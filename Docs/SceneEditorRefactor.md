# シーンエディタ改修：責務分割・パネル化・マテリアル上書き

シーンエディタを Unreal / Unity 相当の扱いやすさへ近づけるための改修記録。
肥大化していた `SceneEditor`（約 700 行）と `SceneEditorUI`（約 640 行）を
責務ごとに分割し、メッシュ単位のマテリアル（色・テクスチャ）上書きを追加した。

対象は Debug 専用（`USE_IMGUI`）のエディタ機能。Release では従来どおり
描画系のみが残る。

---

## 1. 全体構成

```
YEngine/Generators/SceneEditor/
├─ SceneEditor.{h,cpp}          … ファサード（所有と取り次ぎのみ）
├─ SceneEditorUI.{h,cpp}        … パネルのホスト（表示フラグと描画順のみ）
├─ SceneSerializer.{h,cpp}      … Save/Load（AutoJson ベース）
├─ SceneSerializerLegacy.cpp    … 旧形式(version<=14)の読み込み専用
├─ SceneJsonBinding.{h,cpp}     … JSON フィールド登録の一本化
├─ Core/                        … サブシステム共有の状態
├─ Render/                      … 描画パス
├─ Edit/                        … 編集操作
└─ Panels/                      … 各 ImGui パネル
```

`SceneEditor` は実体を所有するだけで、ロジックはすべてサブシステムへ委譲する。
機能追加は Render/ Edit/ Panels/ のいずれかに置き、`SceneEditor` からは呼ぶだけ。

---

## 2. SceneEditor の責務分割

### Core/（サブシステム共有の状態）

| ファイル | 役割 |
|---|---|
| `SceneViewSettings.h` | 表示・カリング設定を 1 構造体へ集約（`bool*` セッターの列挙を廃止） |
| `SceneEditorContext.h` | 各サブシステムが必要とする借用ポインタ束。`Set〜()` を並べる代わりに参照 1 本を渡す |

### Render/（描画パス）

| クラス | 役割 |
|---|---|
| `SceneObjectRenderer` | カラー / シャドウ / ピック（ObjectID 焼き込み）パス。視錐台カリング、非アニメのインスタンシング集約、カメラ遮蔽ディザーフェードを内包 |
| `SceneDebugDrawer` | 選択枠（Blender 風オレンジ）、コライダー形状、BroadPhase グリッド。AABB/OBB は `InstancedCube`、Sphere は `InstancedSphere` に集約しドローコールを最小化 |

### Edit/（編集操作）

| クラス | 役割 |
|---|---|
| `SceneClipboard` | 選択オブジェクトのコピー / 貼り付け（トランスフォーム・コライダー・マテリアル・親子まで引き継ぐ） |
| `ScenePlacementService` | モデル配置、地面吸着（真下 Raycast）、グリッド整列、回転整列 |
| `SceneLoadController` | シーン切替時の退避 / 復元（同一シーンは JSON 再パースを省略して D3D12 リソース再確保を回避） |
| `SceneGizmoLayer` | 選択集合から `IGizmable` を組んで `GizmoController` へ渡す |
| `SceneEditorShortcuts` | キーボードショートカットの割り当てを 1 箇所へ集約（下表） |

#### ショートカット一覧

| キー | 動作 | キー | 動作 |
|---|---|---|---|
| Ctrl+S | 保存 | Delete | 選択を削除 |
| Ctrl+C / Ctrl+V | コピー / 貼り付け | F | 選択にフォーカス |
| Ctrl+D | 選択を複製 | B | スタンプ配置を開始 |
| Ctrl+G | 地面に吸着 | Esc | スタンプ配置を終了 |

---

## 3. SceneEditorUI のパネル化

`SceneEditorUI` は全ウィンドウの描画を直接持っていたが、各ウィンドウを
`Panels/` 配下の専用クラスへ委譲し、表示フラグとパネルの所有だけを持つ形にした。

| パネル | 内容 |
|---|---|
| `OutlinerPanel` | オブジェクト一覧。検索・親子ツリー・表示トグル（目）・選択ロック・右クリックメニュー（名前変更 / 複製 / 削除）。ID 昇順ソートで行順を安定化 |
| `InspectorPanel` | 選択オブジェクトを **トランスフォーム / 描画 / マテリアル / コライダー / 階層** のタブで編集 |
| `MaterialPanel` | メッシュ（マテリアルスロット）単位の色・テクスチャ上書き（第 5 章） |
| `ColliderPanel` | インスペクタ埋め込みの設定 UI と、シーン全体のコライダー一覧ウィンドウ |
| `DuplicatePanel` | 等間隔の連続複製（柵・柱・階段向け） |
| `PrefabPanel` | プレファブの作成 / 一覧 / 配置 / 削除 |
| `SceneMenuBar` | ファイル / ウィンドウ / 編集 / 表示メニュー |
| `ScenePanelContext.h` | パネル共通の依存束（シーン参照＋サービス＋スタンプ操作のコールバック） |

---

## 4. 再利用ウィジェット（YEngine/Core/Editor/Widgets）

パネルから使う共通ウィジェットを追加し、アンブレラヘッダ `YEditorWidget.h` に登録。

| ファイル | 主なもの |
|---|---|
| `YEditorWidget_Transform` | `TransformFields`（位置 / 回転(度表示) / スケール、一様編集）、`DragEulerDegrees` |
| `YEditorWidget_Material` | `MaterialSlotEditor`（1 スロットの色 + テクスチャ）、`OverrideColor3`（上書きトグル付き入力） |
| `YEditorWidget_AssetPicker` | `SearchBox`（大小無視の部分一致）、`AssetCombo`、`ScanAssetFiles` |
| `YEditorWidget_Hierarchy` | `DrawHierarchyRow`（アウトライナ 1 行） |
| `YEditorWidget_Toolbar` | `ToolbarSelector`（排他選択）、`ToolbarToggle`、`ToolbarSeparator` |

---

## 5. メッシュ単位のマテリアル上書き（色・テクスチャ）

モデル（`Material`）は複数オブジェクトで共有されるため、`Material` を直接
書き換えると同じモデルを使う全オブジェクトへ波及する。そこで **上書きセット**
をオブジェクト側に持たせ、描画時だけモデル本来の値へ重ねる方式にした。
Unreal のマテリアルインスタンスと同じ考え方。

### 型

| 型 | 役割 |
|---|---|
| `MeshMaterialOverride` | 1 スロットぶんの上書き。**ベースカラー(Kd)** と **テクスチャパス** の 2 つ |
| `MaterialOverrideSet` | 1 オブジェクトの全スロット分。256 バイト境界で並べた定数バッファを 1 本持ち、`Apply()` でモデル本来の値とマージして書き込む |

各項目は「上書きするか」のチェックとセット。OFF ならモデル本来の値（読み込んだ Kd /
テクスチャ）がそのまま使われる。

### 配線

- `Object3d` / `ObjectManager` が上書きセットを所有。`Model::Draw` /
  `Model::DrawInstanced` に上書きを渡し、スロットごとにテクスチャと定数バッファの
  バインド先を差し替える。
- 静的オブジェクト（インスタンシング）は `InstancedObject3d` の `BatchKey` に
  上書きセットを含め、設定が違うものだけ別バッチにする（同じ設定同士はまとまるので
  インスタンシングは維持）。実際に上書きが無いセットは `nullptr` に潰してバッチ分裂を防ぐ。
- テクスチャ選択は共通の **`FileBrowser`**（`MaterialPanel` が所有）。サムネイル
  走査時にテクスチャを読み込む＝ GPU アップロード済みになるため、選んだ直後に
  白くならない。フォルダを辿れるのでモデル付属テクスチャにも共通テクスチャにも届く。

### PBR（粗さ・メタリック）について

Blender の PBR パラメータをそのまま反映する試みを一度入れたが、**撤回した**。
glTF は仕様上 `metallicFactor` の既定が 1.0 で多くのマテリアルが金属扱いになり、
金属は拡散反射を持たないため、環境マップ（IBL）を持たない本プロジェクトのシーンでは
モデルが真っ黒になった。現状は色 + テクスチャの上書きのみ。再挑戦する場合は
環境キューブマップをシーンに用意することが前提。

---

## 6. SceneSerializer の AutoJson 化

- フィールドの登録を `SceneJsonBinding` に一本化（`Add` を 1 行足すだけで
  Save 側 Load 側の両方へ反映される＝片方書き忘れによる値消失を防止）。
- 新形式は **version 15**（ベクトルは `{"x":..,"y":..}` のオブジェクト）。
  version 14 以下（`[x,y,z]` 配列）は `SceneSerializerLegacy.cpp` の読み込み専用
  パスで処理し、次の保存で自動的に新形式へ移行する。
- マテリアル上書きは各オブジェクトの `materialOverrides` 配列として保存
  （実際に上書きしているスロットのみ）。

### 付随して追加した永続フィールド

- `PlacedObject::visible` … アウトライナの目トグル。描画・影・ピックから外れるが
  一覧には残る（当たり判定には影響しない、エディタ専用の見た目フラグ）。

---

## 7. 主要ファイル早見表

| 変更/追加 | パス |
|---|---|
| ファサード | `YEngine/Generators/SceneEditor/SceneEditor.{h,cpp}` |
| 共有状態 | `.../SceneEditor/Core/` |
| 描画 | `.../SceneEditor/Render/` |
| 編集 | `.../SceneEditor/Edit/` |
| パネル | `.../SceneEditor/Panels/` |
| ウィジェット | `YEngine/Core/Editor/Widgets/YEditorWidget_*` |
| マテリアル上書き | `YEngine/Model/Material/MaterialOverrideSet.{h,cpp}` |
| シリアライズ | `.../SceneEditor/SceneJsonBinding.*`、`SceneSerializerLegacy.cpp` |
