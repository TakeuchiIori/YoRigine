# エフェクト改良 作業ログ

YParticle / VFX 関連の作業を時系列で記録する。方針: **A: facade修正 → B: 新オーサリング → C: 性能 → D: リボン大強化**。

---

## 2026-06-28 — A: EffectHandle を「本当に動く facade」にする（最小A）

### 背景 / 問題
- `EffectHandle::Play(loop=true)` が作る `YParticleEmitter` を**誰も `Update()` していなかった**（tick されるのは `YEmitterGroup` 内のエミッタのみ）。→ ループ再生・追従が機能せず、ゲーム側では GetGroup 呼び出しがコメントアウトされるなど facade が実用に耐えていなかった。

### やったこと（最小A）
`YParticleManager` に「実行中エミッタのレジストリ」を追加し、毎フレーム tick + prune する形にした。

- **`YParticleManager.h`**
  - 前方宣言 `class YParticleEmitter;`（循環インクルード回避）
  - `void RegisterEmitter(const std::shared_ptr<YParticleEmitter>&);` を追加
  - メンバ `std::vector<std::shared_ptr<YParticleEmitter>> activeEmitters_;` を追加
- **`YParticleManager.cpp`**
  - `#include "YParticleEmitter.h"`, `#include <algorithm>`
  - `Update()` 末尾で `activeEmitters_` を tick し、`!IsActive()`（Stop 済み）を `remove_if` で除去
  - `RegisterEmitter()` 実装
- **`EffectHandle.cpp`**
  - `Play(loop=true)` で `mgr->RegisterEmitter(h.emitter_)` を呼ぶように変更。これで継続発生・`SetPosition()` 追従・`Stop()` 停止が機能する。非ループは従来通り撃ちっぱなし（`sys->Emit` 1回）。

### 効くこと / まだ残る制限
- ✅ ループ再生・移動追従（剣の軌跡/松明/オーラ等）・Stop・**ワールド空間**エフェクトの同時多発
- ⚠️ `isRelative=true` の同一 System 同時多発は `parentMatrix` を取り合う（System=定義/インスタンス=粒バッファ の分離が必要）→ B/C で本格対応予定

### 検証状況
- **Develop 構成でビルド成功（0 warning / 0 error）**。
- ⏳ 実機ランタイム検証は未実施。DevelopScene で `EffectHandle::Play("xxx", pos, /*loop*/true)` → `SetPosition` で動かす → `Stop()` で止まる、を確認すること。

### 次の一手
- A の実機確認後、B へ。

---

## 2026-06-28 — B 調査: ソフトパーティクル/HDRの実現可能性（方針見直し）

A は実機確認OK。B 着手前に描画基盤を調査した結果、**ソフトパーティクルは当初見立てより重い**ことが判明し、B の順序を組み直した。

### 調査結果（事実）
- **シーン深度がSRV化されていない**: 深度リソースは `ALLOW_DEPTH_STENCIL` のみで typeless でない（`DsvManager.cpp:251`）→ シェーダから読めない。
- **深度が読める状態になるのはパーティクル描画より後**: `DepthBarrier()` は `MyGame::Draw:229`（ポスト用）。パーティクルはオフスクリーンパス中で、その時点では深度未開放。
- **カメラ near/far が GPU CB に未送信**: `CameraForGPU` は worldPos + VP のみ。深度線形化に near/far が要る。
- **オフスクリーンRTがLDR**: `R8G8B8A8_UNORM_SRGB`（`PostEffectManager.cpp:224`）→ HDRエミッシブの値>1がクランプ。本領発揮にはRGBA16F化が必要。
- ✅ パーティクルPSOは既に `CreateReadOnly`（深度テストON/書込OFF）＝ソフト化で深度書込変更は不要。

### 結論 / 方針
- ソフトパーティクルも本格HDRエミッシブも**コア描画基盤の改修**を伴う（深度 typeless+SRV化／状態遷移／CB拡張／RT float化）。共有パイプラインに手が入るためリスク有り。
- B の優先度を実装コスト基準で組み直し:
  - 低リスク・自己完結: **①フリップブック補間 → ②サブエミッタ/イベント**
  - コア改修(まとめて別案件化推奨): **③ソフトパーティクル ④HDRエミッシブ→Bloom**
- 推奨ルート: 先に①②で底上げ → その後③④を腰を据えて。（ユーザー選択待ち）

### 補足: 素材調査でフリップブック補間は保留に
`Resources/Effects/` は全て単体スプライト（circle/star/smoke/fire_blue 等）で**連番シート素材が無く**、JSONでも flipbook/subtexture 未使用。フリップブック補間は素材ありきなので**保留**。手持ちスプライトで効く **サブエミッタを B の最初の実装**に決定。

---

## 2026-06-28 — B①: サブエミッタ（UpdateSubEmitter）実装

UEの爆発の肝＝レイヤリングを、手持ちスプライトだけで組めるようにする土台。「親の粒から子エフェクトを発生」させる。

### やったこと
- **`Modules/IParticleModule.h`**: `IUpdateModule` に死亡フック `virtual void OnDeath(attrs, index) {}`（デフォルト no-op）を追加。
- **`YParticleSystem.cpp`**: `Update()` の寿命チェックで、死亡の瞬間に全 UpdateModule の `OnDeath()` を発火してから `isActive=false`。
- **新規 `Modules/Update/Emit/UpdateSubEmitter.{h,cpp}`**:
  - トリガ2種: `OnDeath`（死亡時1回）/ `Continuous`（生存中に `rate_` 個/秒を per-particle 確率発生）
  - パラメータ: 子システム名 / 発生数 `count_` / レート `rate_` / 確率 `probability_`
  - 発生は `YParticleManager::GetInstance().Emit(childSystem_, 親粒position, count_)`
  - エディタ: トリガColombo + 子システムを登録済み一覧から選択 + 各パラメータ
  - JSON保存/読込対応、`REGISTER_UPDATE_MODULE` で自動登録
- premake 再生成（新ファイルを vcxproj に取り込み）

### 効くこと / 制限
- ✅ 「閃光が消えたら小煙」「弾が着弾したら爆発」「飛翔体が煙トレイル」等を手持ち素材で合成可能
- 🛡 暴走防止: 子の同時数は子システムの `maxParticles` で頭打ち（自己参照しても無限増殖しない）
- ⚠️ 子の発生位置は親粒の `position` をそのまま使用＝**ワールド空間運用前提**。`isRelative=true` 親では位置がずれる（A の残課題と同じ。後続のSystem/インスタンス分離で対応）

### 検証状況
- **Develop ビルド成功（0 warning / 0 error）**。
- ⏳ 実機未確認。確認手順: エディタで子用システム（例: 小煙）を用意 → 親システムに Update モジュール「サブエミッタ」を追加 → 子システムを選び OnDeath に設定 → 親を発生させ、死亡時に子が出るか確認。

### 次の一手
- サブエミッタの実機確認。OKなら B② は素材依存のフリップブックを飛ばし、**コア改修（ソフトパーティクル＋HDR）を1案件**として着手するか判断。

---

## 2026-06-28 — B③ ソフトパーティクル 調査＋フェーズ1（配線）

サブエミッタ実機OK。コア改修に着手。

### 基盤調査の結論（前回の訂正含む）
- **深度SRVは既存**: `DsvManager::Create` が `createSRV=true` で MainDepth の SRV を作成済み（`ds->srvIndex`）。`Get("MainDepth")->srvIndex` で取得可。前回「無い」は誤り。
- パーティクル描画時点では MainDepth は `DEPTH_WRITE`（読める状態になるのは `PreDraw()` 以降）。深度の状態は `DirectXCommon::depthCurrentState_` が一元管理 → **遷移は必ず DirectXCommon 経由**。
- 読み取り専用DSVビューは無い。→ DSVヒープ会計（シャドウ等と共有）に触ると波及リスク大。
- カメラ near/far は C++ にあるが GPU CB 未送信。オフスクリーンRTは LDR（`R8G8B8A8_UNORM_SRGB`）。

### 採用方式（リスク最小）
**「深度を PS 読み取り状態へ遷移 ＋ 深度なしで RT 再バインド ＋ PS で手動遮蔽＆ソフトフェード」**。
読み取り専用DSV不要＝DSVヒープ会計に触らない。粒は加算半透明で深度書込不要なので成立。システム単位オプトイン（既定OFF）で既存に影響なし。

### フェーズ1でやったこと（配線・見た目変化なし）
- **`DirectXCommon.h/.cpp`**: `BeginParticleSoftDepth()` / `EndParticleSoftDepth()` を追加。
  - Begin: MainDepth を `PIXEL_SHADER_RESOURCE` へ遷移し、OffScreen を深度なしで再バインド。
  - End: `DEPTH_WRITE` に戻し OffScreen+MainDepth を再バインド。`depthCurrentState_` を整合維持。
- 既存パス関数は未変更・新ヘルパーは未呼び出し → **既存描画は完全に従来通り**。
- **Develop ビルド成功（0/0）**。

### フェーズ2（次回）
- SoftParticleCB（near/far/invScreenSize/fadeDistance/enabled）を粒PSへ。near/far getter 確認/追加。
- `YParticle.PS.hlsl`: 深度サンプル→線形化→奥なら discard（遮蔽）＋手前距離でα減衰（ソフト化）。
- `YParticleSystem` に `softParticle_` フラグ（JSON/エディタ）、RenderBatch に伝播。
- `YParticleManager::Draw` で Begin/End ヘルパーを粒描画前後に呼び、深度SRV＋CBをバインド。
- ⚠️ 深度線形化は射影規約依存 → **ユーザー実機での見た目チューニング往復が必要**。

---

## 2026-06-28 — B③ ソフトパーティクル フェーズ2（実装完了・要実機チューニング）

### 設計（クリーン重視・影響範囲最小）
- **ソフト専用 PSO ＋ 専用 PS に分離**。`softParticle` を有効にしたシステムのバッチ**だけ**新経路を通る。非ソフト粒は現行のハードウェア深度経路を **100% 維持**（見た目不変）。
- 深度は既存の MainDepth SRV を**再利用**（新DSV/typeless化なし＝DSVヒープ会計に触らない）。
- ソフト経路は「深度なしRT＋PSで手動遮蔽＋接地フェード」。

### 変更ファイル
- `Camera.h`: `GetNearClip()` / `GetFarClip()` 追加。
- `YParticleSystem.h/.cpp`: `softParticle_` / `softFadeDistance_` + アクセサ、ShowEditor に「ソフトパーティクル」チェック＋フェード距離スライダ。
- `YParticleEditor.cpp`: システムJSONに `softParticle` / `softFadeDistance` 保存・読込。
- **新規 `Resources/Shaders/Particle/YParticleSoft.PS.hlsl`**: `gSceneDepth`(t2) + `gSoftParticle`(b4: near/far/fade) を追加。シーン深度を線形化し、奥なら discard（手動遮蔽）＋接地距離で α 減衰。
- `YPipelineManager.cpp`: `CreatePSO_YParticleAllBlendModes` を整理し、共通VS＋PS差し替えで `YParticle` と `YParticleSoft` の全ブレンドPSOをリフレクション生成。
- `YParticleRenderer.h/.cpp`: ソフト用CB（バッチ256Bスロット）+ `ApplySoftParticle()`。EndFrame でソフト時は専用PSO/RootSig選択＋深度SRV＋CBをバインド。
- `YParticleManager.h/.cpp`: RenderBatch に soft 情報、バッチ分類キーに追加。Draw を **2フェーズ化**（非ソフト→従来描画 / ソフト→`Begin/EndParticleSoftDepth` で囲んで描画）。

### 検証状況
- **Develop ビルド成功（0/0）**。
- ⏳ **実機チューニング必須**。確認手順: エディタでスモーク等のシステムの「ソフトパーティクル」をON→フェード距離調整→地面に近い煙の硬い切り口が消えるか確認。
- ⚠️ **既知の調整ポイント**: 深度線形化は標準的な透視射影（深度[0,1]・非reversed）を仮定。もし**ソフト粒が消える/壁を透ける**なら射影規約が違う（reversed-Z等）→ `YParticleSoft.PS.hlsl` の `LinearizeDepth` か diff 符号を反転して調整する。
- 非ソフト粒は経路未変更＝従来通りのはず（要確認）。

### 修正: D3D12 ERROR #615 DEPTH_STENCIL_FORMAT_MISMATCH（実機初回）
- 症状: ソフトON時 `DrawIndexedInstanced` で「null DSV を bind できるのは PSO の深度フォーマットが UNKNOWN の時だけ」エラー（ソフトPSOが D24 を持っていた）。
- 修正: `YPipelineManager` のソフトPSOを `DepthStencilPresets::CreateDisabled()` ＋ `SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN)` でビルド（手動遮蔽するのでハード深度テスト不要）。非ソフトPSOは従来通り D24/ReadOnly。ビルド0/0。

### 修正: 加算合成でフェードが効かない（実機2回目）
- 症状: 通常(アルファ)ブレンドでは接地フェードが見えるが、加算合成(SrcBlend=ONE)だと効かない。
- 原因: 加算は alpha を使わないため、`baseColor.a *= fade` だけでは寄与が減らない。
- 修正: `YParticleSoft.PS.hlsl` で **rgb にも fade を掛ける**（premultiplied 方式）。加算は rgb 減衰で、通常は rgb*alpha 減衰で、どちらも正しくフェード。discard 判定も `fade < 0.001` に変更。
- シェーダのみ変更 → Develop は実行時コンパイルなので **C++再ビルド不要・再起動で反映**。

### 次の一手
- 実機でソフトの見た目を確認＆数式チューニング。未コミットなので確認後にコミット。

---

## 2026-06-28 — 方針転換：粒子の外（メッシュ＋シェーダ＋HDR）へ

粒子だけでは爆発・雷・**Valorant Omenのめらめらボリュームスモーク**は不可能とユーザーと合意。主戦場を `VfxMesh`＋シェーダ＋HDR に移す。新ロードマップ：**1.HDR/Bloom → 2.ボリューム風スモーク → 3.雷リボン → 4.爆発合成**。ソフトパーティクルの深度配線はボリュームメッシュのソフト接地で再利用＝無駄でない。詳細はメモ `project_yparticle_improvement`。

## 2026-06-28 — 新1: HDR/Bloom（安い版＝LDR Bloom＋エミッシブ強度）

### 調査
- オフスクリーンRT・中間RTとも LDR（`R8G8B8A8_UNORM_SRGB`）。CS対応で「TYPELESS実体＋SRGB(RTV/SRV)＋UNORM(UAV)三系統ビュー」（`PostEffectManager.cpp:215`）。本格HDR=全RTを RGBA16F 化＋UAVビュー再設計＝大改修。
- Bloom/ToneMapping は CS版が既存（`PostEffectBloomCS`）。シーンは preset ロード（GameScene/Title="TestGame"、Clear="ClearScene"）。**DevelopScene は preset 未ロード**＝テスト時はエディタ「ポストエフェクト」で Bloom 追加が必要。
- 判断：まず**安い版**（LDRのまま Bloom＋エミッシブ強度）。本格HDR(float RT)は必要になったら後で。

### やったこと（最小コスト＝新CB/シェーダ/PSO 変更なし）
- `YParticleSystem.h/.cpp`: `emissiveIntensity_`（既定1.0）+ アクセサ、ShowEditor に「エミッシブ強度」DragFloat。
- `YParticleEditor.cpp`: JSON 保存/読込に `emissiveIntensity`。
- `YParticleRenderer.cpp` `AddSystem`: インスタンスカラーの **rgb に emissive を掛ける**（a はそのまま）。色は float4 なので >1 を保持→RTでクランプされても Bloom しきい値は越える。
- **Develop ビルド成功（0/0）**。

### テスト手順
1. Develop 起動 → エディタ「ポストエフェクト」で **Bloom を追加**（threshold/intensity調整）。
2. 適当なパーティクルシステムの **エミッシブ強度を 3〜5** に。
3. 明るく光って Bloom が乗れば成功。
- ⚠️ 安い版の限界：芯は白飛び（色がクランプ）。色付きの強い発光まで欲しくなったら本格HDR(float RT)へ。

### 追記: DevelopScene に Bloom が無く「変わらない」（実機）
- 原因: DevelopScene は preset 未ロード＝Bloom無し。エミッシブで明るくしても白くなるだけで発光しない。
- 対応: `DevelopScene::Initialize` で `PostEffectManager::GetInstance()->LoadPreset("TitleBloom")` を読み込み（Bloom入り type12）。C++変更＝**再起動が必要**。ビルド0/0。
- 使い方: 再起動→エミッシブ強度3〜5→光る。物足りなければエディタ「ポストエフェクト」で Bloom の threshold↓/intensity↑。

### 次の一手
- Bloom＋エミッシブの見た目確認。良ければコミット。

---

## 2026-06-29 — 新2: Omen風ボリュームスモーク Stage1（球＋FBMノイズ、画面表示）

### 設計（VfxMesh族に新タイプ追加・既存の作りに倣う）
- 既存 `LightVolumeMesh`/`VfxMesh_Volume.PS` と同じ枠組み。共通VS(`VfxMesh.VS`)はワールド頂点前提、共通hlsliに **FBM/Fresnel/EnergyLines(=将来の雷用)** が既に揃っている。
- スモーク＝**球メッシュ＋FBMノイズ(渦巻き)×視線フレネル(中心濃く縁柔らか)**。アルファブレンド。

### やったこと
- 新規 `Vfx/VfxMesh/VolumeSmokeMesh.{h,cpp}`: UV球をワールド生成（TRIANGLELIST、姿勢変化時のみ再構築）。CB構造体 `SmokeParamsCB` をヘッダに定義。
- 新規 `Resources/Shaders/Vfx/VfxMesh/VfxMesh_Smoke.PS.hlsl`: FBM2層スクロールで渦巻き、`dot(法線,視線)`で中心濃く縁フェード、`color.rgb`明るくでBloom。`VfxMesh_Common.hlsli` に `SmokeParams` 構造体追加。
- `YPipelineManager`: `CreatePSO_VfxMeshSmoke`（NoCull/ReadOnly深度/**AlphaBlend**）追加＋呼び出し＋宣言。
- `DevelopScene`: テスト表示（(0,1.5,0)に紫の球、ハードコードparam）。Bloom preset も自動ロード済み。
- premake再生成＋**Develop ビルド成功（0/0）**。C++変更＝再起動で反映。

### 検証状況 / 次
- ⏳ 実機で見た目確認＆パラメータ調整（scrollSpeed/noiseStrength/noiseScale/fresnelPower/density/色）。現状ハードコード。
- Stage2: 見た目が決まったら VfxMeshEditor へ組み込み ImGui 調整化、ゲーム向けハンドル化。
- 既知の注意: NoCull＋アルファで前後両半球がブレンド（ボリューム感が出る一方むしろ濁る可能性）。要観察。

### 実機1回目: 「ただの紫球」「エディタで調整不可」→ 2点改善
- 原因①: UVに2Dノイズ＝球面に平面テクスチャ貼った見た目で渦巻かない。
  - 対応: `VfxMesh_Smoke.PS.hlsl` を**オブジェクト空間の擬似3Dノイズ(3平面合成)＋ドメインワープ＋turbulence(abs加算)**に作り替え。時間で3Dドリフト。pow でコントラスト。
- 原因②: DevelopSceneハードコードで調整不可。
  - 対応: DevelopScene に `Editor::RegisterGameUI("スモーク(Volume)")` でリアルタイム調整パネル追加（色/密度/ノイズスケール/渦巻き強さ/スクロール速度/縁の柔らかさ/オクターブ/中心/半径）。
- Develop ビルド0/0。C++変更＝再起動。
- 限界: 表面シェーダ方式なのでレイマーチ体積煙ほどの奥行きは出ない。turbulence版で不足なら raymarch / 多層を検討。
- Stage2(本来): VfxMeshEditor へ正式統合＋VfxEffectAssetでsave/load＋ゲーム向けハンドル化。今はDevelopScene上の暫定パネル。

### 実機2回目: 「なぜ別エディタ？」→ VfxMeshEditor へ正式統合（Stage2実施）
ユーザー指摘: スモークはVfxMeshなのでDevelopSceneの仮パネルでなく `VfxMeshEditor`(Trail/LightVolumeと同居)に入れるべき。対応:
- `VfxEffectAsset` に `SmokeEffectParam smoke` + `bool useSmoke(既定false)` 追加、Save/LoadのJSON対応。
- `VfxMeshEditor`: `previewSmoke_`(VolumeSmokeMesh) + `smokeCBResource_`(SmokeParamsCB) 追加。Initialize/InitCBVs/Update/DrawPreview/Finalize に smoke 分岐、`UpdateSmokeCBV`/`DrawSmokeSection` 追加、編集パネルに「Volume Smoke (Omen風)」チェック+セクション(LightVolumeと同パターン、CommitChangeでUndo対応)。
- `VolumeSmokeMesh` の頂点色を白に（色はCB駆動）。
- DevelopScene の仮実装(メンバ/init/update/draw/パネル/include)を全撤去。
- Develop ビルド0/0。使い方: VFXエディタ→useSmoke ON→Play→セクションで調整→保存でJSON永続化。
- 既知: 表面シェーダ方式の限界は据え置き（必要ならraymarch/多層）。ゲーム向けハンドル化は今後。

### 実機3回目: 中に入ると消える / テクスチャ感 / 太陽フレア欲しい
- ①「球の中に入ると消える」: フレネル `facing=dot(法線,視線)` が内側で負→discard が原因。→ `abs(dot(...))` で両面対応。
- ③「太陽フレア風の光輪」: リム発光を追加。`rimIntensity` パラメータ新設（CB `_pad`→`rimIntensity` に。hlsli/asset/editor/serialize 全部通し）。シェーダで `pow(1-facing,3)*rimIntensity` を color.rgb(HDR)で加算→Bloomが外へ滲んでフレア。エディタに「リム発光(フレア)」スライダ。
- ②「テクスチャ感が残る」: 表面シェーダの本質的限界。真の体積感は**レイマーチ**（ピクセル毎に球内を貫き3Dノイズ積分）が必要。重いので3060向けにステップ数設計要。**次の大物として保留**（surface版は残して切替式にする案）。
- Develop ビルド0/0。シェーダ＋C++（param追加）→再起動。

### 実機4回目: 「テクスチャ感を消したい」→ レイマーチ版に作り替え
- `VfxMesh_Smoke.PS.hlsl` を**ボリュームレイマーチ**に全面書き換え。カメラ→球内をレイで貫き、24ステップで 3D FBM(ドメインワープ＋時間ドリフト)密度を front-to-back 積分。視点移動で内部が視差で動く＝表面テクスチャ感が消える。
- NoCullのまま**奥側フェイスのみ処理**(`dot(surfN, toCam)>0 で discard`)＝二重積分回避＆カメラが球内でも成立。レイ×球は解析交差、tStart=max(入口,0)。
- `fresnelPower`→“縁の柔らかさ(shell falloff)”に流用。リムは最接近距離(graze)ベースのフレアに。
- CBパラメータ不変＝**C++変更なし・シェーダのみ**。dxc(ps_6_0)でコンパイル検証OK→再起動で反映。
- 注意: 24ステップ×octをピクセル毎＝球が画面を覆うと重い(3060)。要観察→ステップ削減/品質ノブ。遮蔽は奥側深度の近似。
- 密度の効き方が積分式で変わるのでエディタで再調整想定。

