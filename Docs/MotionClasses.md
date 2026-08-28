# Motion関連クラス設計（現状）

「Motion」という名前を共有する**まったく別の2つの系統**が存在する。混同しないこと。

```
1. スケルタルアニメーション（エンジン層）  … YEngine/Model/Motion/
   ボーン単位のキーフレームアニメーション。glTF読み込み、再生、ブレンド。

2. 敵攻撃カーブ系（ゲーム層）              … YGame/GameObjects/Enemy/Attack/
   敵の全身Transform（骨ではない）を9チャンネルのカーブで動かす。
   コミット db25601「敵専用の攻撃エディタ作成」で旧ハードコード攻撃を置換。
```

---

## 1. スケルタルアニメーション（`YEngine/Model/Motion/Core/`）

### 1.1 `Motion`（`Core/Motion.h/.cpp`）

1クリップ分のデータを保持する。

| 構造体 | 内容 |
|---|---|
| `InterpolationType` | `Linear` / `Step` / `CubicSpline` |
| `Keyframe<T>` | `{ float time; T value; }`。`KeyframeVector3`/`KeyframeFloat`/`KeyframeQuaternion` |
| `AnimationCurve<T>` | `std::vector<Keyframe<T>>` |
| `NodeAnimation` | `translate`/`rotate`/`scale` の3カーブ + `interpolationType`（ボーン1本分） |
| `SpeedCurve` | X=正規化時間[0,1]、Y=速度倍率のリマップカーブ。`Evaluate(t)` は範囲外クランプ＋二分探索(`lower_bound`)で線形補間 |
| `AnimationModel` | `duration_` + `map<string, NodeAnimation> nodeAnimations_` + `speedCurve` |

**主要API**
- `LoadFromScene(aiScene*, gltfPath, animName, importUnitScale)` — assimp経由でglTFから読み込み。
  - glTFサンプラーの`interpolation`（LINEAR/STEP/CUBICSPLINE）はassimpの`aiNodeAnim`が公開しないため、`.gltf`の生JSONを別途パースして復元。
  - 座標系ミラーリング（Xを反転、回転クォータニオンのY/Zを反転）をエンジンの左右/Y-up規約に合わせて適用。
  - Mixamo対応: armatureルートノードでスケールがほぼ一様なら1.0に固定（インポーター起因のスケールアーティファクト回避）。
- `SaveBinary`/`LoadBinary` — 独自`.anim`バイナリ形式（マジック`"ANIM"`）。単一クリップのみ。
- `ApplyAnimation(joints, time)` — `Joint`名で`nodeAnimations_`を検索し、T/R/Sを個別評価して`Joint::SetTransform`に反映。該当なしのジョイントはそのまま。
- `PlayerAnimation(time, node)` — スケルトンを持たない単一`Node`向けの簡易版。結果を直接`localMatrix_`に焼き込む。
- `CalculateValue`（float/Vector3/Quaternion版） — 補間種別ごとの評価:
  - `Linear`: Lerp（Quaternionのみ Slerp）
  - `Step`: 直前キーフレーム値を保持
  - `CubicSpline`: Catmull-Rom（float/Vector3のみ実装）
  - **既知の未実装**: QuaternionのCubicSplineはSlerpにフォールバックしている（コメント付き）。glTF側でCubicSplineと指定された回転があっても実際には効かない。

### 1.2 `MotionSystem`（`Core/MotionSystem.h/.cpp`）

`Model`が`unique_ptr<MotionSystem>`として1体につき1つ所有する再生・ブレンドエンジン。

**3本の独立したクロックを毎フレーム進める（`Update(dt)`）**
1. **上半身レイヤー**（`upperAnimation_`）— アクション用の別クリップ。`Once`終了で自動的に0.15秒のブレンドアウトを開始。
2. **ベースブレンド**（`animationBlendState_`）— ブレンド中は`animationTime_`自体は進まず、ブレンド用の`currentTime`のみ進行。
3. **ベースレイヤー**（`animationTime_`）— ブレンド中でない通常再生。`motionSpeed_ × SpeedCurve評価値`で進む。

**`Apply()` — ボーンごとの合成（骨単位でポーズソースを選択）**
- 上半身ボーン＋アクション再生中 → `upperAnimation_`から直接評価
- 上半身ボーン＋ブレンドアウト中 → 旧アクションポーズとベースレイヤー（それ自体がブレンド中の可能性あり）の間を補間
- それ以外のボーン → ベースレイヤー（`from`/`to`間のper-boneブレンド、`BlendAndApplyAnimation`）
- 最後に`Skeleton::Update()`（階層行列再計算）→ `SkinCluster::UpdateMatrixPalette(joints)`（GPUパレット更新）

ボーン名の突き合わせはglTF由来の命名ゆれを吸収する正規化ステップを経て`normalizedNameCache_`にキャッシュ。

**再生制御API**: `PlayOnce`/`PlayLoop`/`Stop`/`Resume`/`SetPlayMode`/`SetAnimationTime`/`ResetPlaybackState`（ブレンド・上半身状態を全リセットしてフレーム0から再開）。

**ブレンド**: `StartBlend(toAnimation, duration)`
- 事前に、スケルトンの全ボーン名が遷移先`Motion`のチャンネルに存在するか検証。欠けていると**`std::runtime_error`を投げる**（無視リストの外なら実行時に落ちるハード仕様）。新規アニメをブレンド対象にする場合はスケルトン全体をカバーする必要がある。
- 現在の`Motion`を値としてスナップショット（`from`）、遷移先も同様に（`to`）。
- `animation_`ポインタは即座に`to`側を指すようになる（`GetAnimation()`は補間途中でも遷移先を返す）。

**上半身（アクション）レイヤー**: `PlayUpperAnimation`/`StopUpperAnimation`/`SetUpperBodyMask` — 攻撃モーションを上半身だけに適用しつつ下半身の移動アニメを継続、といった用途。マスクは通常`Skeleton::GetDescendantBones(rootBoneName)`で構築。

**速度**: `motionSpeed_`（外部指定、例: コンボタイミング）× `currentAnimationSpeed_`（アニメ駆動）= `GetEffectiveSpeed()`。

**単体ルックアップ**: `HasTransformAnimation`/`GetTransformAnimation` — 再生状態と無関係に任意の`Motion`から任意ボーンの任意時刻の姿勢を取得可能（例: 剣振りモーションに合わせたVFXアタッチ点の同期）。

**呼び出し元確認箇所**: `Player.cpp`（コンボタイミングでの`motionSpeed_`設定など）、`AttackingCombatState`、`HitCombatState`、`DeadCombatState`、`PlayerMagicController`、`DemoPlayer`。取得は`obj_->GetModel()->GetMotionSystem()`。

### 1.3 補助クラス（`YEngine/Model/Skeleton/`）

| クラス | 役割 |
|---|---|
| `Joint` | 1ボーン。`transform_`(ローカルTRS) / `localMatrix_` / `skeletonSpaceMatrix_` / `children_`・`parent_`（`Skeleton::joints_`へのインデックス）。`Update`で親から順に再計算 |
| `Skeleton` | `vector<Joint> joints_` + 名前→インデックスの`jointMap_`。`Create(Node&)`で構築、`Update()`で全ボーン再計算、`GetDescendantBones(root)`で子孫ボーン名集合を取得（上半身マスク構築に使用）、`Draw()`でデバッグ描画 |
| `SkinCluster` | 頂点ボーンウェイトとGPUスキニング行列パレットを保持。`UpdateMatrixPalette(joints)`を毎フレーム`MotionSystem::Apply()`から呼ばれる |
| `BoneGizmable` | Debug用。`MotionEditor`から使う3Dギズモでのボーン姿勢編集 |

### 1.4 `MotionEditor`（`YEngine/Model/Motion/Editor/`、`USE_IMGUI`のみ）

`SceneEditor`が`motionEditor`メンバとして保持。997行。

**表示ON/OFFゲート**（コミット `3eaadce` で追加）:
以前は`SceneEditor::Update()`が選択中オブジェクトを無条件で追跡し、ボーン線・ギズモを常時オーバーレイしていた（通常のシーン編集の邪魔になっていた）。
- `bool isEnabled_ = false`（デフォルトOFF）+ `SetEnabled`/`IsEnabled`
- `SceneEditor::Update()`は`IsEnabled()`のときだけ`SetTargetObjectId`/`Update`を呼ぶ
- ON/OFFの入口は2箇所:
  1. InspectorパネルのチェックボックスDraw（対象オブジェクトが`MotionSystem`を持つ場合のみ表示）
  2. ウィンドウメニューの「モーションエディタ」項目

**パネル構成**（`IMotionEditorPanel`実装、いずれも`MotionEditorContext`共有）:
`MenuBarPanel` / `SavePanel` / `ToolbarPanel`（再生・ループ・逆再生・ピンポン・速度）/ `BoneListPanel`（階層ツリー選択）/ `PropertyPanel`（選択ボーンのT/R/S数値編集）/ `TimelinePanel`（キーフレームタイムライン）/ `SpeedCurvePanel`（`Motion::SpeedCurve`専用エディタ）。

**編集操作**: `SavePose`/`InsertKeyframeFromTransform`/`SetJointTransform`/`SyncBufferToJoint`（ImGui入力とライブ`Joint::transform_`のブリッジ）/`RestoreLiveBoneOriginal`（Escapeキャンセル時のロールバック）。

**Undo/Redo**: `MotionEditorContext::history`（`CommandHistory`）にキーフレーム追加・削除をラムダコマンドとして登録。フィールド種別ごとの特別扱い不要。

このエディタはスケルタル`Motion`/`Joint`データのみを扱う。名前が似ている`MotionPathEditor`（後述）とは無関係。

---

## 2. 敵攻撃カーブ系（`YGame/GameObjects/Enemy/Attack/`）

**骨ではなく敵の`WorldTransform`全体**（攻撃開始時の姿勢を基準とした相対位置/回転/スケール）をカーブで動かす仕組み。`MotionSystem`/`Motion`とは無関係。

コミット `db25601` で旧`EnemyAttackAction`/`EnemyAttackDatabase`/旧`EnemyAttackState`（動作タイプごとに手書きC++クラス）を全廃し、`Data/` `Runtime/` `Editor/` `Motion/` の4層構成に置き換えた。

### 2.1 データ層（`Attack/Data/`）

**`EnemyAttack`** — 1攻撃の定義一式。`id`/`displayName`/`duration`、`positionSource`（`Curves` or `Path`）、`pathName`（Path時のみ）、`AttackTracks tracks`、`vector<AttackModifier> modifiers`、選択用ゲート（`minRange`/`maxRange`/`weight`/`cooldown`/`selfHpBelow`/`targetHpBelow`）、フラグ（`parriable`/`fast`）。
永続化先: `Resources/Json/BattleEnemies/enemy_attacks.json`（`EnemyAttackIO::Load`/`Save`）。

**`AttackTracks`** — `PositionX/Y/Z` `RotationX/Y/Z` `ScaleX/Y/Z`の9チャンネル`CurveChannel`。
- `EvaluatePositionOffset(t)`/`EvaluateRotationOffset(t)`/`EvaluateScaleMultiplier(t)` — 正規化進行度`t∈[0,1]`で評価
- 未使用チャンネルは既定値にフォールバック（位置/回転=0、スケール=1）
- すべて攻撃開始姿勢を基準に、攻撃者のローカルZ前方基準で評価（「前方に突進」＝ワールド向きに関係なくZ位置カーブが正値、というだけで済む）

**`AttackModifier`** — カーブ以外の要素。`AttackModifierType`:

| 種別 | 内容 |
|---|---|
| `FaceTarget` | `strength` rad/sで対象方向へ継続的に旋回 |
| `HomingOffset` | カーブ軌道を`strength`だけ対象方向へ寄せる（固定カーブのままオートエイム可） |
| `Hitbox` | 接触ダメージ有効ウィンドウ。`damageWindow`idで多段ヒットを区別 |
| `Invincible` | 無敵フレーム区間 |
| `EmitProjectile` | 発射物。`projectileId`/`count`/`spreadDeg`/`offset`/`aimAtTarget` |

各モディファイアは`[startTime, endTime]`区間で`IsActiveAt(t)`。`EmitProjectile`のみ`IsInstant()`（`startTime`一度だけ発火）。

### 2.2 ランタイム層（`Attack/Runtime/`）

**`EnemyAttackLibrary`**（シングルトン） — `enemy_attacks.json`のロード/セーブ、`Find(id)`、編集用CRUD。
- **`IsEnabled()`/`SetEnabled(bool)`という生死スイッチ**を持つ。OFF中は`AttackSelector`が旧ハードコード攻撃（`BattleRushAttackState`等）に完全フォールバック。移行期間中、コード変更なしで新旧を切り替え比較できるA/Bスイッチ。
- `EnemyAttackPicker::Pick(enemy, attackIds, ctx, perception)` — 射程/HP/クールダウンでフィルタした重み付きランダム選択。

**`AttackPlayer`** — 毎フレームのカーブ評価器。**実戦闘再生と`EnemyAttackEditor`のプレビューが同一クラス**を使うため、エディタで見た通りの挙動が実戦闘で保証される(WYSIWYG)。
- `Play(enemy, attack)` — 現在のposition/rotation/scaleを基準姿勢としてスナップショット
- `Update(enemy, dt)` — `ApplyPose`（カーブ or `MotionPathLibrary`経由のPath評価）→ `ApplyRangeModifiers`（FaceTarget/HomingOffset）→ `FireInstantModifiers`（`instantFired_`ビットセットでフレームまたぎでも二重発火しない）
- `Stop(enemy)` — 基準姿勢に完全スナップバック
- `StopKeepPosition(enemy)` — 回転/スケールのみ復元、**位置は攻撃で得た分を保持**（突進/跳躍攻撃終了時に元位置へワープしないように）
- `GetActiveDamageWindow()`/`IsInvincibleNow()` — `CurveAttackState`が毎フレーム参照
- `Seek(enemy, time)` — エディタのタイムラインスクラブ用

### 2.3 モーションパス サブライブラリ（`Attack/Motion/`）

`positionSource == Path`時のみ使用。

**`IAttackMotion`** — 「進行度→ワールド座標」の抽象カーブIF。`Evaluate(t, MotionContext&)`、`EvaluateDirection`（既定実装は`Evaluate`の有限差分）。`MotionContext`は攻撃者の開始位置/向きと対象位置を保持（`"targetRelative"`空間のパス解決に使用）。

実装されている2形状（`Resources/Json/BattleEnemies/motion_paths.json`のサンプルデータより）:
- **`SplineMotion`** — `constantSpeed`（弧長再パラメータ化で制御点間隔に関わらず速度一定）、`space: "targetRelative"`、Catmull-Rom風の制御点列。制御点は`SplinePointGizmable`でビューポート上ドラッグ編集想定
- **`OrbitMotion`** — `startAngleDeg`/`sweepDeg`（サンプルで964°観測=1周超のスパイラルも可）、`startRadius`/`endRadius`、`startHeight`/`endHeight`。対象を中心に半径・高さが連続変化する円/螺旋軌道（コークスクリュー状の旋回攻撃など）

**`AttackMotionFactory`** — `type`フィールドに基づく型レジストリ。`Create(json)`/`ToJson`/`CreateDefault(typeName)`/`GetTypeNames()`。新形状追加はここへの登録のみで済み、`AttackPlayer`等は`IAttackMotion`インターフェースしか見ないため無改修。

**`MotionPathLibrary`**（シングルトン） — 名前付きパスのレジストリ。`Resources/Json/BattleEnemies/motion_paths.json`に永続化。`Find`/`FindShared`を`AttackPlayer`が`EnemyAttack::pathName`経由で呼ぶ。

**`MotionPathPreview`** — `MotionPathEditor`専用のプレビュー評価器。生の`BattleEnemy`インスタンスなしでパス単体をプレビューするため`AttackPlayer`とは別クラス。

### 2.4 エディタ層（`Attack/Editor/`、`USE_IMGUI`のみ）

**`EnemyAttackEditor`** — `EnemyAttackLibrary`由来の`EnemyAttack`を編集。
- `SetManager(BattleEnemyManager*)`で実戦闘中の敵にアクセスし、**シーン内プレビュー**を実現（自前の`AttackPlayer preview_`を実際の`BattleEnemy`に対して再生、`StopPreview()`でプレビュー前の姿勢へ復元）。
- サブパネル: `AttackCurvePanel`（9チャンネルカーブ編集）、`AttackModifierPanel`（モディファイアCRUD、種別ごとにフィールド表示切替）。
- 登録: `BattleScene.cpp` にて `"攻撃エディタ"` として `"Game"`/`"ゲームプレイ"` カテゴリに登録。

**`MotionPathEditor`**（配置は`Attack/Motion/`配下だがエディタクラス） — `MotionPathLibrary`（名前付きパス自体）を、特定の攻撃と切り離して編集。
- 実際の敵/対象が存在しないため、`previewOrigin_`/`previewYawDeg_`/`previewTarget_`/`previewScale_`という**仮想プレビュー状況**を自前で用意。
- `DrawSplineEditor`/`DrawOrbitEditor`。
- 登録: `BattleScene.cpp` にて `"攻撃経路エディタ"` として登録。ギズモコールバックとプレビュー描画も別途登録。

### 2.5 敵FSMへの組み込み

**`CurveAttackState`**（`Enemy/BattleEnemy/States/Attack/`） — 旧6クラス（Rush/ChargeRush/Spin/Jump/Combo/Counter）を置換した単一の`IEnemyState<BattleEnemy>`実装。
- `SetAttack(const EnemyAttack*)`を`Enter`前に呼ぶ必要あり
- `Enter`→`player_.Play`、`Update`→`player_.Update`、`Exit`→`player_.Stop`または`StopKeepPosition`
- `GetContactDamageWindow`/`IsContactDamageActive`/`CanBeParried`は`player_`と`attack_->parriable`へ委譲

**`AttackSelector`**:
- `SelectCurveAttack` — `EnemyAttackLibrary::IsEnabled()`がゲート、`EnemyAttackPicker::Pick`を使用
- `SelectSmartAttack` — まず`SelectCurveAttack`を試し、無効時 or 条件に合う攻撃なしの場合のみ旧ハードコード攻撃（距離/HP加重選択、`db25601`以前からロジック不変）にフォールバック

---

## 3. まとめ表

| クラス | 場所 | 系統 | エディタ登録名 |
|---|---|---|---|
| `Motion` | `YEngine/Model/Motion/Core/` | スケルタル | — |
| `MotionSystem` | `YEngine/Model/Motion/Core/` | スケルタル | — |
| `Skeleton`/`Joint`/`SkinCluster` | `YEngine/Model/Skeleton/` | スケルタル | — |
| `MotionEditor`+パネル群 | `YEngine/Model/Motion/Editor/` | スケルタル | Inspectorチェックボックス／「モーションエディタ」 |
| `EnemyAttack`/`AttackTracks`/`AttackModifier` | `YGame/.../Attack/Data/` | 敵攻撃カーブ | — |
| `EnemyAttackLibrary`/`AttackPlayer` | `YGame/.../Attack/Runtime/` | 敵攻撃カーブ | — |
| `EnemyAttackEditor`+パネル | `YGame/.../Attack/Editor/` | 敵攻撃カーブ | **「攻撃エディタ」** |
| `IAttackMotion`/`OrbitMotion`/`SplineMotion`/`AttackMotionFactory`/`MotionPathLibrary` | `YGame/.../Attack/Motion/` | 攻撃用パス | — |
| `MotionPathEditor` | `YGame/.../Attack/Motion/` | 攻撃用パス | **「攻撃経路エディタ」** |
| `CurveAttackState` | `YGame/.../BattleEnemy/States/Attack/` | 敵FSM連携 | — |

---

## 4. 既知の注意点

- **QuaternionのCubicSpline未実装**（`Motion.cpp`内、Slerpにフォールバック）。glTF側でCubicSpline指定の回転キーがあっても実際には効かない。
- **`StartBlend`は欠落ボーンで例外を投げる**。新規モーションをブレンド遷移先にする際はスケルトン全体をカバーする必要がある（無視リスト外の場合）。
- **`EnemyAttackLibrary::IsEnabled()`が新旧攻撃系のA/Bスイッチ**。デバッグ中に片方だけ検証したい場合はここを確認。
- `MotionEditor`と`MotionPathEditor`は名前が似ているが完全に別系統（前者=骨、後者=攻撃パス）。ドキュメント上も混同しないこと。

※本ドキュメントはコード調査エージェントの報告を基に作成。行番号は目安であり、実装を直接触る前に該当ファイルで最新の状態を確認すること。
