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

---

## 2026-06-29 — VFXエディタ整理（3点）
ユーザー要望で `VfxMeshEditor` を整理：
- **頂点配置/三日月のメッシュ作成UIを削除**（DrawTrailSection の「立体感・三日月化」＋「カスタムメッシュ形状エディター」canvasを除去。`crescentShape`/`customVertices` データ構造は温存＝TrailMesh等のコンパイル不変）。
- **新規エフェクトダイアログのサイズ固定**: `AlwaysAutoResize`→`NoResize`＋固定420x300。入力でサイズが変わらない。
- **新規Effect名の自動採番**: `MakeUniqueEffectName(base)` 追加（base→base1→base2…空き番号）。「+」押下時に初期名セット＋`CreateNew`でも採番（手入力衝突も吸収）。
- Develop ビルド0/0。

## 2026-06-29 — 新3: 雷（Lightning）プロシージャル稲妻
新ロードマップ③。スモークと同じ VfxMesh 統合パターンで実装。
- 新規 `Vfx/VfxMesh/LightningMesh.{h,cpp}`: start→end を **midpoint displacement** でジグザグ折れ線化（決定的Hash乱数）、**カメラ向きリボン**として描画。`flickerRate` 回/秒で経路再生成＝パチパチ明滅。枝分かれ対応。CB `LightningParamsCB`。
- 新規 `Resources/Shaders/Vfx/VfxMesh/VfxMesh_Lightning.PS.hlsl`: リボン断面中心を細い高輝度の芯に＋`EnergyLines`でチラつき。加算HDR→Bloom。`VfxMesh_Common.hlsli` に `LightningParams`。
- `YPipelineManager`: `CreatePSO_VfxMeshLightning`（NoCull/ReadOnly/Additive）。
- `VfxEffectAsset`: `LightningEffectParam`＋`useLightning`＋JSON。`VfxMeshEditor` に正式統合（previewLightning_/CB/Update/DrawPreview/UpdateLightningCBV/DrawLightningSection/チェックボックス/Finalize）。
- premake再生成＋Develop ビルド0/0。dxc(ps_6_0)検証OK。C++＋新ファイル→再起動。
- 使い方: VFXエディタ→useLightning ON→Play→青白い稲妻が明滅＋枝分かれ。幅/ジグザグ/分割/枝/明滅レート/芯グロー/色を調整。
- 雷に「長さ(length)」パラメータ追加（編集可・JSON対応、preview端点が length 連動）。

## 2026-06-29 — VFXエディタ タブ化（見やすさ改善）
DrawEditPanel の Trail/LightVolume/Smoke/Lightning 縦積み(checkbox+TreeNode)を **TabBar** に変更。一度に1効果のみ表示でスッキリ。有効な効果のタブ名に ● 表示。タブIDは "###tab_xxx" で固定（●付け外しで選択リセット防止）。各タブ先頭に「この効果を有効化」チェック。汎用ラムダ effectTab() で4種を生成。ビルド0/0。

## 2026-06-29 — 新4: 爆発の部品「Shockwave（衝撃波リング）」
ロードマップ④爆発の signature。スモーク/雷と同じVfxMesh統合パターン。
- 新規 `Vfx/VfxMesh/ShockwaveMesh.{h,cpp}`: カメラ向きクワッド1枚。シェーダで中心→外へ広がり消えるリングをアニメ。
- 新規 `Resources/Shaders/Vfx/VfxMesh/VfxMesh_Shockwave.PS.hlsl`: `frac(time/duration)`で膨張phase、リング先端を強調、膨張に伴いフェード。加算HDR。`VfxMesh_Common.hlsli`に`ShockwaveParams`。
- `YPipelineManager`: `CreatePSO_VfxMeshShockwave`(NoCull/ReadOnly/Additive)。
- `VfxEffectAsset`: `ShockwaveEffectParam`+`useShockwave`+JSON。`VfxMeshEditor`にタブ統合(preview/CB/Update/DrawPreview/UpdateShockwaveCBV/DrawShockwaveSection/Finalize)。
- premake再生成＋ビルド0/0、dxc検証OK。

### 大爆発の作り方 / 残課題
- エディタで Shockwave + Smoke(炎色) + Lightning + Bloom(自動) を1エフェクトに重ねれば大爆発の絵。
- ⚠️ 現状エディタは**ループ再生**(調整用)。実ゲーム用の**ワンショット発火**(`EffectHandle::Explosion(pos)`的にドカンと1回再生して消える)は未実装＝次の仕上げ。各メッシュに再生進捗(0..1)を外部駆動する仕組み＋ワールド配置トリガが必要。

## 2026-06-29 — 爆発感ゼロ→ワンショット破裂エンベロープ追加 / プレビューUI整理
ユーザー: 「全然爆発感が無い」「プレビューの軌道アニメはTrailだけに出すべき」「破裂＋煙＋衝撃波が欲しい」。
- **爆発感の本質**: ループ継続だと持続球に見える。爆発は"一発膨張して消える"ワンショットのタイミングが命。
- **ワンショット破裂エンベロープ実装**:
  - 編集側: `oneShot_`/`burstDuration_`/`burstProgress_` 追加。プレビューに「爆発ワンショット再生」チェック＋破裂時間＋「もう一度」。Update で進捗0→1を自動リピート（-1=継続モード）。
  - Smoke: 進捗で半径を素早く膨張（ポップ）＋シェーダでエンベロープ `saturate(p/0.12)*(1-smoothstep(0.35,1,p))` で立ち上がり速→フェード。CB/hlsli に `burst` 追加。
  - Shockwave: phase をワンショット時は外部進捗(0→1で1回)、継続時は frac ループ。CB/hlsli に `burst`（旧_pad）。
  - SmokeParamsCB に burst+_pad2[3] 追加（5行目）。
- **#2 プレビューUI**: 「軌道アニメ」「剣の長さ」を `useTrail` 時のみ表示。
- ビルド0/0、dxc検証OK（Smoke/Shockwave）。
- レシピ: Smoke(炎色HDR)+Shockwave(暖色HDR,半径大)+Lightning(枝多)+爆発ワンショットON。
- 次の盛り: 火花パーティクルのワンショット同期、閃光フラッシュ、実ゲームのワールド発火トリガ。

## 2026-06-29 — UE5風「爆発後の煙」: 火→煙遷移＋上昇＋長い余韻
ユーザー: UE5の爆発を再現したい、特に爆発後の煙。
- **タイムライン化**（ワンショット時）:
  - Smoke `burst` 進捗で **火球色→煙色** へ lerp（smoothstep 0.04→0.30）。シェーダ: stepCol/rim を baseRGB に。
  - エンベロープを「破裂(pop, p/0.07)→長い余韻(linger, pow(1-p,0.7))」に。火球は一瞬、煙は長く残る。
  - 煙の**上昇(浮力)**: editor で center.y += riseSpeed*previewTimer_。半径も p で膨張継続。CB center/radius を上昇・膨張後の値に一致(smokeCenter_/smokeRadius_ メンバ)。
  - **衝撃波のタイムスケール分離**: shockwave burst = previewTimer_/sw.duration（煙の burstProgress_ とは別。速く膨張して終わる）。
- 新パラメータ: SmokeEffectParam に `smokeColor`(煙色) `riseSpeed`。CB/hlsli に `smokeColor`。editorに火球色/煙色/上昇速度スライダ＋serialize。
- 既定 burstDuration_ 0.8→**2.0**（煙の漂う時間）。
- ビルド0/0、dxc検証OK。
- UE5風レシピ: Smoke(火球色=明オレンジHDR/煙色=暗灰/上昇1.5/密度高)+Shockwave(暖色HDR/半径大/膨張0.4)+Lightning(任意)+ワンショットON/破裂時間2.5。
- 残: 火花パーティクル同期・画面フラッシュ・熱揺らぎ・ワールド発火トリガ。

---

## 2026-06-30 — 新オーサリング設計確定: YParticle を「Effect」単位に集約（設計のみ・未実装）

ユーザー要望: ①複数Systemを組み合わせて1つのエフェクトを完成させ名前を付けたい ②ゲーム側から超簡単に再生したい ③Editorが扱いにくいので何とかしたい。

### 使いにくさ診断（現状）
- **入口が2つに割れている**: 単一System→`EffectHandle::Play(name)` 1行 / 複数System束ね(Group)→`YEmitterGroupManager::GetGroup()` 生API 6行＋nullチェック。`EffectHandle` がGroup非対応なのが主因。
- **手動ロード羅列が壊れやすい（実害あり）**: `MyGame.cpp` で `LoadSystemsFromFile` を1個ずつ手書き。`EnemyHit` グループは EnemyHit1/2/**3** を参照するが MyGame は 1/2 しかロードしておらず、**EnemyHit3 が無言で出ていない**。
- **エディタが2ウィンドウ往復**: System編集=`YParticleEditor` / 束ね=`YEmitterGroupEditor`。後者は **System名を文字列で手打ち**（`newEmitterSystemName_[128]`）して繋ぐ→タイプミスでサイレント失敗。保存もSystem/Group/Bundleの3系統で迷う。「エフェクト」という第一級単位がエディタに存在しない。

### 決定事項
- **「Effect」を第一級概念にする**。Effect = 名前 + 構成System群（modules同梱）+ 各Systemのoffset。System1個だけのEffectも可。
- **データ構造 = 同梱方式**（ユーザー選択）。`Resources/Json/YEffects/<名前>.json` 1ファイルに systems の定義(modules)も配置(offset)も全部入れる。参照切れが原理的に起きない（EnemyHit3問題が構造的に消える）。既存 `LoadEffectBundle`/`SaveEffectBundle`(systems+groups in 1 file) が土台。
  ```json
  { "name":"EnemyHit",
    "systems":[ {"name":"EnemyHit1","modules":{...},"offset":[0,0,0]},
                {"name":"EnemyHit2","modules":{...},"offset":[0,1,0]},
                {"name":"EnemyHit3","modules":{...},"offset":[0,0,0]} ] }
  ```
- **ランタイム（ゲーム側）= 名前1発**。`EffectHandle::PlayOneShot("EnemyHit", pos)` / `Play("SlashTrail", pos, true)`。内部解決順: Effect(Group相当)を名前で引く→無ければSystem単体fallback。同名衝突はEffect優先＋警告ログ。EffectHandle 内部表現を「System経路(emitter_) or Group/Effect経路」のどちらか持つ形に拡張。
- **自動ロード**: `MyGame` の手動羅列を廃止し `ScanDirectory("Resources/Json/YEffects/")` で全自動ロード（VfxMeshSpawner と同手法）。
- **エディタ刷新 = 1画面 Effect Editor**: 左=Effect一覧 / 中=構成System一覧（追加は**ドロップダウン選択**、手打ち廃止 / offset編集）/ 右=選択Systemのモジュール編集をインライン / 上=[▶プレビュー再生] / 下=[このEffectを保存]1ボタン → `YEffects/<名前>.json`。

### 移行方針
- 既存 `YParticleSystems/*.json` `YEmitterGroups/*.json` は読めるまま残す（後方互換）。新規は `YEffects/` に集約。
- 着手したら PlayerSword の `GetGroup("EnemyHit")` 6行を `EffectHandle::PlayOneShot("EnemyHit", hitPos)` に置換。BattleEnemy.cpp:312 の同コードはコメントアウト済み（死にコード）。

### 状態
- 設計確定 → **フェーズ1（入口統一＋自動ロード）実装完了・Debugビルド成功**（下記）。フェーズ2(YEffects同梱フォーマット)・3(Effect Editor 1画面化)は未着手。

---

## 2026-06-30 — フェーズ1実装: EffectHandle 入口統一 ＋ 自動ロード（ビルド成功）

設計の施策1+2を実装。**既存JSONフォーマットのまま**、ゲーム側の入口統一とロード自動化を実現。

### 変更点
- **`YParticleManager::ScanDirectory(dir)`** 追加（`.cpp`/`.h`）: ディレクトリ内 `*.json` を `LoadSystemsFromFile` で再帰自動ロード。`<filesystem>`/`<fstream>` include 追加。
- **`YEmitterGroupManager::ScanDirectory(dir)`** 追加: 同様に `LoadGroupFromFile` で自動ロード。**System を先に Scan → Group を後に Scan**（GroupはSystem名参照のため）。
- **`EffectHandle` を Group 対応に拡張**: `Play/PlayOneShot` が名前をまず `YEmitterGroupManager::GetGroup` で解決→あればGroup経路（SetPosition/SetActive/loop時SetAutoEmitAll/非loop時EmitAll）、無ければ従来のSystem単体経路。`group_`(借用ptr)メンバ追加、`SetPosition/Stop/IsActive` を経路でディスパッチ。同名衝突は Group優先＋Loggerで警告。ループ追従は `YEmitterGroupManager::Update`(YParticleManager::Update内246行から毎フレーム呼ばれる)で機能。
- **`MyGame.cpp`**: 手動 `LoadSystemsFromFile`/`LoadGroupFromFile` 羅列(8行) → `ScanDirectory` 2発に置換。
- **`PlayerSword.cpp:233`**: 生グループAPI 6行 → `hitEffect_ = EffectHandle::PlayOneShot("EnemyHit", hitPos, 10);` 1行。`hitEffect_` メンバは元から宣言済みだが未使用だった。不要になった `YEmitterGroupManager.h`/`YEmitterGroup.h` include を整理。

### 効果
- ✅ **EnemyHit3 ロード漏れが自動解消**: `Resources/Json/YParticleSystems/EnemyHit3.json` は存在したが MyGame が EnemyHit1/2 しか手動ロードしておらず無言で出ていなかった。Scan化で全自動ロード。
- ✅ ゲーム側は中身がGroupか単発System かを意識せず名前1つで再生（`EffectHandle::PlayOneShot("EnemyHit", pos)`）。
- ✅ エフェクト追加=JSON置くだけ。MyGame修正不要。

### 検証
- premake5 再生成（前ターンのVfxMesh新規ファイル反映）→ **MSBuild Debug x64 ビルド成功、YMain.exe 生成**。0 error。
- ⏳ 実機ランタイム検証は未（剣ヒットで EnemyHit1/2/3 が出るか、ループ追従の確認）。

### 次の一手
- フェーズ2: `YEffects/<名前>.json` 同梱フォーマット（systems定義+offset を1ファイル）。既存 `LoadEffectBundle`/`SaveEffectBundle` が土台。
- フェーズ3: Effect Editor 1画面化（System追加はドロップダウン選択・保存1ボタン・プレビュー再生）。

---

## 2026-06-30 — フェーズ2実装: YEffects 同梱フォーマット採用（ビルド成功）

設計の施策3。**同梱フォーマットは既存 `LoadEffectBundle`/`SaveEffectBundle` が既に提供していた**（`{"systems":[{name,modules...}], "groups":[{groupName,emitters:[{systemName,offset}]}]}`）。新フォーマットは作らず、これを `YEffects/` 単位の正式エフェクトとして採用するだけでフェーズ2が成立。

### 変更点
- **`YParticleManager::ScanEffectBundles(dir)`** 追加: `YEffects/*.json` を `LoadEffectBundle` で再帰自動ロード。1ファイルで systems(modules込み)＋groups(offset込み) が完結 → 参照切れ原理消滅。
- **`MyGame.cpp`**: `ScanDirectory(Systems)` → `ScanDirectory(Groups)` → **`ScanEffectBundles("Resources/Json/YEffects/")`** を追加。旧フォルダ形式と YEffects 形式を共存させ、段階移行できる。
- **EnemyHit を YEffects に移行**: `YParticleSystems/EnemyHit1,2,3.json` ＋ `YEmitterGroups/EnemyHit.json` を統合して **`Resources/Json/YEffects/EnemyHit.json`** を新規作成。旧4ファイルは削除（情報は完全移行）。

### 重要な制約（二重ロード）
- `LoadSystemsFromJson` は既存 System に対しても無条件で `AddSpawnModule`/`AddUpdateModule` する（`CreateSystem` は同名なら既存を返すだけ・クリアしない）。→ **同名 System を旧フォルダと YEffects の両方に置くとモジュールが二重追加されて壊れる**。移行したら旧ファイルを必ず消すこと。共存安全化（再ロード時クリア＝ホットリロード対応）は将来フェーズ3で別途検討。

### 検証
- **MSBuild Debug x64 ビルド成功、YMain.exe 生成**。0 error。
- EnemyHit は `YEffects/EnemyHit.json` 1ファイルから systems+group がロードされ、`EffectHandle::PlayOneShot("EnemyHit", hitPos, 10)`（PlayerSword）で解決される構成。
- ⏳ 実機での EnemyHit 表示確認は未。

### 次の一手
- フェーズ3: Effect Editor 1画面化。System追加はドロップダウン選択（手打ち廃止）・保存は `SaveEffectBundle` で `YEffects/` に1ボタン・プレビュー再生。これで Clear/Title/BattleArea も順次 YEffects へ移行でき、旧2フォルダを最終的に廃止。
- フェーズ3で「再ロード時クリア（同名Systemのモジュール作り直し）」を入れると、エディタの上書き保存→即反映（ホットリロード）が安全になる。

---

## 2026-06-30 — フェーズ3実装: Effect Editor 改善（既存 YEmitterGroupEditor を育成、ビルド成功）

**前提修正**: 現状エディタを読んだら想定よりずっと出来ていた。System選択リストもバンドル保存(YEffects へ `SaveEffectBundle`)も**既に実装済み**だった（「手打ちのみ・バンドル無し」という以前の診断は誤り）。新規エディタは作らず既存を育てる方針に変更。ユーザーが触って報告した不満を直接潰した。

### 対応した不満と実装
1. **保存/読込ボタンが多すぎる** → `ShowFileButtons` 再編。先頭に主導線「エフェクトを保存」「エフェクトを開く」(YEffects/ バンドル)。旧 YEmitterGroups 分離形式の4ボタンは `if(!CollapsingHeader("旧形式…")) return;` で折りたたみ。バンドル読込 `loadBundleBrowser_`(YEffects/, `LoadEffectBundle`)を新設(.h+コンストラクタ)。
2. **System編集の往復** → `ShowSelectedEmitterDetail` 末尾に CollapsingHeader「システム "xxx" を編集」を追加し `sys->ShowEditor()` をインライン展開。`YParticleEditor` 別ウィンドウ不要に。
3. **一覧が見づらい/新規作成が分からない** → `ShowGroupList` ヘッダを「エフェクト一覧」に。先頭に緑の「＋ 新規エフェクト」ボタン(NewEffect 自動ユニーク名→選択)。右クリックに「複製」(SaveToJson→ユニーク名→CreateGroup+LoadFromJson)。
4. **プレビューが見えない**（実機で判明）→ 原因は **EmitAll がグループの保存位置で発生**し、EnemyHit は位置Y=33(ユーザー編集)で上空に出て画面外だった。`previewPos_`(既定{0,2,0}, DragFloat3+原点リセット)を追加し、グループ全体 Emit(x1/10/100)は保存位置を一時上書き→発生→復元する `emitAtPreview` 経由に。System プレビューも emitter の `FollowEmit(previewPos_, n)` で同位置に発生。これで保存位置に関係なく必ず見える。

### つまずき
- StepB/C とフェーズ3記録は一度ユーザーに中断され、その時の編集が**ファイルに適用されていなかった**（ラベルが「エミッターグループ」のままだった）。Grep でファイルの実状態を確認して再適用。**ツール結果を過信せず現物確認**する教訓。

### 検証
- **MSBuild Debug x64 ビルド成功**。0 error。
- ⏳ 実機での確認は未（新規エフェクト作成→エミッタ追加→プレビュー再生→System インライン編集→保存の一連）。

### 残
- プレビュー: 再生は previewPos_ で出るが、カメラがそこを向いていないと依然見えない可能性。必要なら「カメラ前で再生」を追加。
- 再ロード時クリア（ホットリロード安全化）。旧2フォルダ(YParticleSystems/YEmitterGroups)の最終廃止。

---

## 2026-07-03 — 設計: 複合エフェクト（Particle+VfxMesh+GPU）＋ サウンド付きハンドル（設計のみ・未実装）

ユーザー要望: 「エフェクト合成」と「サウンド付きハンドル」をまとめて設計する。実装前の方針確認フェーズ。

### 現状診断（3系統が名前空間ごと分断）

| 系統 | 発生源JSON | ゲーム側ハンドル | 備考 |
|---|---|---|---|
| Particle | `YEffects/*.json`（systems+groups同梱） | `EffectHandle::Play/PlayOneShot(name)` | フェーズ1〜3で入口統一済み |
| VfxMesh | `Resources/Json/VfxMesh/*.json`（Trail/Volume/Smoke/Lightning/Shockwave） | `VfxMeshHandle::Play/PlayOneShot/PlayBolt(assetName)` | 既に EffectHandle と同型の facade が実装済み（未使用に近い） |
| GPU Particle | `GpuEmitters/*.json`（Emitter/Group） | ハンドルなし。`GpuEmitManager::EmitGroups/PlayEmitterGroup` 生API直叩き | facade 不在 |
| Sound | なし | `Audio::GetInstance()->Play/PlayOneShot(filepath,...)` | エフェクトと無関係に独立して呼ぶしかない |

→ 「爆発 = 火花(Particle) + 衝撃波(VfxMesh) + 破片(GPU) + 爆発音(Audio)」を1つとして扱う手段が無く、**ゲームコード側で4系統を手動で同期して呼ぶ**ことになっている（呼び忘れ・位置ズレのリスクは EnemyHit3 ロード漏れ問題と同種）。

### 決定事項（案）

**新しい合成レイヤーを1枚追加する。既存3系統のJSON/ハンドルは変更しない**（Trail断面形状やParticleモジュールのような内部フォーマットに手を入れると波及リスクが大きいため、疎結合な「名前を束ねるだけ」の薄いレイヤーにする）。

- **`Resources/Json/YComposites/<名前>.json`** を新設。中身は既存アセット名の参照リストのみ:
  ```json
  {
    "name": "Explosion",
    "particleEffect": "ExplosionSparks",
    "vfxMeshAssets": [
      { "asset": "ExplosionShockwave", "offset": [0,0,0], "scale": 1.5 },
      { "asset": "ExplosionSmoke",     "offset": [0,0,0], "scale": 2.0 }
    ],
    "gpuEmitterGroup": "ExplosionDebris",
    "sounds": [
      { "path": "Resources/Audio/SE/explosion.wav", "volume": 1.0, "category": "SE" }
    ]
  }
  ```
  各フィールドは省略可（例: 音だけ足したい場合は `sounds` のみでも成立）。
- **ランタイム**: `EffectHandle` の名前解決チェーンの**先頭**に Composite 解決を追加（Composite→Group→System の順）。`EffectHandle::PlayOneShot("Explosion", pos)` 1行のままで4系統すべてが連動する。
  - Composite 内部では既存API をそのまま呼ぶだけ（新規の描画/更新コードは書かない）:
    - `particleEffect` → 既存 `EffectHandle` 経路
    - `vfxMeshAssets` 各要素 → 既存 `VfxMeshHandle::Play/PlayOneShot`
    - `gpuEmitterGroup` → 既存 `GpuEmitManager::EmitGroups`（PlayOneShot相当）/ `PlayEmitterGroup`・`StopEmitterGroup`（loop相当）
    - `sounds` 各要素 → 既存 `Audio::PlayOneShot`（非loop）/ `Audio::Play`（loop、返り値の `SoundHandle` を保持）
- **サウンド付きハンドル**: `EffectHandle` に `SoundHandle soundHandle_`（moveのみ、Audio.h の型そのまま）を追加。
  - `PlayOneShot` 経由の音は fire-and-forget（`Audio::PlayOneShot`、ハンドル保持不要）。
  - `Play(loop=true)` 経由の音は `Audio::Play(...,loop=true)` の戻り値を `soundHandle_` に保持し、`EffectHandle::Stop()` で `soundHandle_.Stop()` も呼ぶ（例: 剣の唸り音つきスラッシュトレイルが `Stop()` で音ごと止まる）。
  - 位置追従は `SetPosition()` では音量/ピッチは変えない（Audio.h に3D減衰パラメータが無いため）。3D位置オーディオが要るなら Audio 側の拡張が別途必要＝**今回のスコープ外**として明記。

### 実装ステップ案（低リスク・自己完結 — コア描画/オーディオパイプライン変更なし）

1. `CompositeEffectAsset`（name+4フィールド）+ JSON Save/Load。`YComposites/` を `ScanDirectory` 対応（既存 `VfxMeshSpawner::ScanDirectory` 等と同パターン）。
2. `CompositeEffectManager`(singleton) 新設。名前→アセットのマップのみ保持（描画は一切しない。既存 Handle を呼ぶだけの薄い層）。
3. `EffectHandle::Play/PlayOneShot` の解決順に Composite を追加。内部で `VfxMeshHandle`/`GpuEmitManager`/`Audio` の子ハンドルを `EffectHandle` にぶら下げて `Stop()` で連鎖停止できるようにする（`std::vector<VfxMeshHandle>` 等を追加）。
4. `EffectHandle` に `SoundHandle` 追加。Stop連鎖を実装。
5. VfxMeshEditor / GpuEmitManager エディタ側は変更不要（Composite は既存アセット名を選ぶだけの薄いエディタを別途作るか、当面は手書きJSONで運用）。

### リスク / 保留事項
- GPU Emitter の「ループ→Stop」semantics は `PlayEmitterGroup`/`StopEmitterGroup` が groupName ベースの状態管理（同名グループの多重再生を想定していない）。Composite からの多重発生（同時に2つ爆発）で同名GPUグループを取り合う可能性 → 要検証、必要なら GPU側もインスタンスID方式に寄せる（EffectHandle 側で以前対応した `parentMatrix` 取り合い問題と同種）。
- Composite 専用の編集UIは今回のステップ案に含めていない（手書きJSON運用でスタートし、需要が出たらフェーズ2でエディタ化）。
- 3D位置オーディオ（距離減衰/パン）は Audio.h に無いため対象外。欲しくなったら Audio 側の別案件。

### 状態
設計のみ・未実装。実装着手はユーザー承認待ち。

---

## 2026-07-03 — GPUパーティクルを「ゲームで使える」状態にする（Phase1+2 実装・Debugビルド成功）

ユーザー要望: 「GPUパーティクルが扱い悪すぎてゲームで使える状態でない。使える程度にしたい」。スコープは**大**（最小＋per-emitterオフセット＆複数同時再生＋エディタ刷新＆Composite連携）で合意。まず土台のPhase1・2を実装。

### 現状診断（コードから特定した「使えない」理由）
1. **🔴 起動時にグループが一切ロードされない（致命）**: `CreateEmitterGroup` 先頭が `selectedJsonFilePath_`（ImGuiで手選択したときだけ入るエディタ専用フィールド）必須で、空だと `ThrowError`。起動時は空→`LoadFromFile` の catch が例外を握りつぶし、**groups_ が空のまま**。ゲームが `EmitGroups` を呼んでも「見つかりません」で無言no-op。エディタ操作前提の状態がコアのデータモデルに漏れていた。
2. **🔴 ゲーム向けハンドルが無い**: `EffectHandle`/`VfxMeshHandle` 相当が無く生API直叩きのみ。YGame側の再生呼び出しは実際ゼロ。
3. **🟠 EmitGroups が全エミッタを同座標・同数で上書き**（グループ内の相対配置が1点に潰れる）。
4. **🟠 emission と simulation が未分離**: 発生が interval 駆動で `isPlaying` と無関係、停止時は `Reset()` で粒子が即消滅。→ ワンショットも「停止後フェード」も表現不能。
5. `Initialize()` が `LoadAllEmitters`/`LoadModel` を2回重複。

### Phase1: 致命バグ修正（`GpuEmitManager`）
- `CreateEmitterGroup(groupName, sourceFilePath="")` にシグネチャ変更。`selectedJsonFilePath_` 必須の `ThrowError` を撤去し、由来パスは引数優先→無ければ選択中にフォールバック。`LoadFromFile→FromJson(json, filepath)→CreateEmitterGroup(name, filepath)` と由来パスを引き回し、**起動時ロードでも確実にグループ生成**。
- `DeleteEmitterGroup` の `ThrowError` 2箇所を非致命 `Logger` 化。ファイル一致ガードは `selectedJsonFilePath_` 非空時（エディタ文脈）のみ。
- `Initialize()` の重複ロード/LoadModel を1回に。

### Phase2: ゲーム向けハンドル ＋ emission/simulation 分離 ＋ per-emitterオフセット
- **`GPUEmitter`**: 発生制御を追加。`continuousEmit_`（interval継続発生ON/OFF）と `burstRequest_`（ワンショット1回）を分離。`UpdateEmitters()` を `isEmit = burst || (continuousEmit_ && intervalHit)` に再構成。`SetContinuousEmit`/`RequestBurst`/`SetEmitWorldPosition`(translateのみ追従上書き) 追加。`EmitAtPosition` は translate/count設定＋`RequestBurst()`（従来の isEmit=1 直書きは UpdateEmitters に上書きされ無意味だった）。`Reset()` でフラグ클リア。
- **`GpuEmitManager`**: 
  - `Update()` を**再生中(isPlaying)＋停止後余韻(lingerTimer>0)の間シミュレーション継続**する形に。emission は `SetContinuousEmit(isPlaying)`。→ 停止後も既存粒子が寿命で自然消滅（剣トレイル等がプツッと消えない）。
  - `EmitGroups`: 各エミッタを `position + ローカルオフセット` で発生（相対配置維持）、`count<0` で各エミッタ設定値を使用、`lingerTimer = max(既存, グループ最大寿命)` を立ててワンショットが寿命を全うするまで回す。
  - `StopEmitterGroup`: `Reset()` 廃止し `lingerTimer` セットで自然消滅待ちに。
  - 追加: `SetGroupPosition`/`HasGroup`/`IsGroupPlaying`/`GetEmitterLocalOffset`/`GetGroupMaxLifetime`。
- **新規 `GpuParticleHandle.{h,cpp}`**（`EffectHandle`/`VfxMeshHandle` と同型・global名前空間）: `Play(group,pos)`（ループ）/`PlayOneShot(group,pos,count=-1)`/`SetPosition`/`Stop`/`IsActive`。ゲームから `GpuParticleHandle::PlayOneShot("ExplosionDebris", pos);` の1行で撃てる。

### 検証
- premake再生成＋**MSBuild Debug x64 ビルド成功（0 error / 0 warning）**。新規 `GpuParticleHandle.obj` コンパイル、`YEngine.lib`＋`YGame.dll` 再リンク確認。
- ⏳ **実機未確認**。確認手順: 起動して既存グループ（`GpuEmitters/emrs.json` の "ss" 等）が**ロードされ表示されるか**（Phase1の証明）／テストで `GpuParticleHandle::PlayOneShot("ss", pos)` が1発出て消えるか／`Play`→`SetPosition`追従→`Stop`後に自然消滅するか。

### 残（大スコープの続き）
- **Phase3**: グループが名前ベース単一状態＝**同じエフェクトを2箇所同時再生できない**。インスタンス方式（VfxMeshHandle の id レジストリ相当）へ寄せる。
- **Phase4**: エディタのファイル手選択ワークフロー撤廃・自動スキャン化・保存導線整理。
- **Phase5**: 07-03設計の Composite レイヤーへ GPU を統合（`EffectHandle` 解決チェーンから連動）。

