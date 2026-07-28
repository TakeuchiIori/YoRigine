# チュートリアルシステム：遊ばせながら教える仕組み

説明UIを順に表示するだけだったチュートリアルを、**「プレイヤーが実際に行動したら次へ進む」**形にするための資料。

更新日：2026-07-28（Phase 2 まで実装）

---

## 1. 何が問題だったか

従来の `TutorialManager` は、ページ配列を先頭から線形に送るだけだった。

- 完了条件は「決定入力」「n秒経過」「イベント名を待つ」の3種類のみ
- そのイベント待ちも、ゲーム側から `TutorialManager::NotifyEvent("名前")` を手書きで呼ぶ必要があった
- しかも **その呼び出しはゲーム内に1箇所も存在せず**、エディタのテストボタンからしか発火していなかった＝実質使われていなかった
- `pauseGameplay` は既定 true。説明中はゲームが止まるため「遊びながら覚える」ができない

つまり**チュートリアルの本体はUIではなくゲーム状態の制御**なのに、UI表示の仕組みしか無かった。

---

## 2. 構成

| クラス | 場所 | 責務 |
|---|---|---|
| `TutorialSignal` | `YEngine/Utilities/Systems/Tutorial/` | 出来事を名前で流す軽量なバス |
| `TutorialCondition` | 同上 | 完了条件の木（定義） |
| `TutorialConditionRuntime` | 同上 | 条件の評価状態（受信回数など） |
| `TutorialSpotlight` | 同上 | 注目させたい場所以外を暗幕で覆う |
| `TutorialManager` | 同上 | ステップ進行、UI表示、条件と暗幕の駆動 |

設計の要は、**ゲームとチュートリアルが互いを知らないこと**。ゲーム側は「チュートリアルのため」ではなく自分の出来事として `Emit` するだけで、チュートリアルはそれを購読する。

### 配線

`MyGame` から2箇所で繋いでいる。

```cpp
// MyGame::Initialize — エディタパネル（USE_IMGUI のみ）
Editor::GetInstance()->RegisterGameUI(
    "チュートリアル",
    []() { YoRigine::TutorialManager::GetInstance()->DrawEditor(); },
    "AllScene", "システム");

// MyGame::Update — シーン更新の後
Framework::Update();
SceneManager::GetInstance()->Update();
YoRigine::TutorialManager::GetInstance()->Update();
```

**この順序は崩さないこと。** 説明パネルのスプライトは、シーン側UIが呼ぶ `UIManager::UpdateAll()` より後に差し込む必要がある。またチュートリアルの開始は `Editor::Draw()`（`Framework::Update` より前）から起きうるため、テクスチャ更新は `TutorialManager::Update()` まで遅延する作りになっている。

---

## 3. シグナルバス

### 発火

```cpp
TutorialSignal::Emit("guard.success");

TutorialSignalData data;
data.name = "enemy.killed";
data.intValue = 3;
TutorialSignal::Emit(data);
```

### 購読

```cpp
auto handle = TutorialSignal::GetInstance()->Subscribe(
    [](const TutorialSignalData& signal) { /* ... */ });
TutorialSignal::GetInstance()->Unsubscribe(handle);
```

配信中に購読が増減しても壊れないよう、リスナー一覧のスナップショットを回している。

### 内蔵の自動発火源

`ConnectEngineSources()` を呼ぶと、エンジン側の出来事が自動でシグナルになる。

| 発火源 | シグナル名 | ゲーム側のコード |
|---|---|---|
| `InputActionMap` のアクション押下 | `action.triggered.<アクション名>` | **0行** |

`PlayerActions.json` に `AttackLight` を定義してあれば `action.triggered.AttackLight` が流れる。**「攻撃ボタンを3回押す」チュートリアルは、ゲーム側を1行も触らずに JSON だけで作れる。**

接続は冪等で、`TutorialManager::Start()` とエディタ描画の先頭から呼ばれる。

---

## 4. 完了条件の木

### 種類

| type | 成立条件 | 使うキー |
|---|---|---|
| `none` | （未設定。旧 `waitType` へフォールバック） | — |
| `signal` | 指定シグナルが `count` 回届いた | `name` `count` |
| `elapsed` | ステップ開始から `seconds` 秒経過 | `seconds` |
| `confirm` | 決定入力が押された | — |
| `all` | 全ての子が成立 | `children` |
| `any` | いずれかの子が成立 | `children` |
| `not` | 先頭の子が不成立 | `children` |

### JSON

```jsonc
"complete": {
  "type": "signal",
  "name": "action.triggered.AttackLight",
  "count": 3
}
```

```jsonc
"complete": {
  "type": "any",
  "children": [
    { "type": "signal",  "name": "action.triggered.StyleToggle", "count": 1 },
    { "type": "elapsed", "seconds": 8.0 }
  ]
}
```

### 評価の作り

定義（`TutorialCondition`）は読み取り専用のデータで、受信回数のような実行時の状態は `TutorialConditionRuntime` が同じ形の木として別に持つ。ステップをやり直すときは `Reset` するだけでよい。

`confirm` と `elapsed` は押した瞬間・跨いだ瞬間しか判定できないため、`Update()` で成立を**ラッチ**する。`all` の中に `confirm` を入れても、他の条件が後から揃えば成立する。

子が空の `all` は「常に成立」ではなく**不成立**として扱う。設定漏れのステップが一瞬で飛ぶのを防ぐため。

---

## 5. 後方互換

既存の JSON は1バイトも変えずに動く。

- `complete` キーが無い → `type` は `none` のまま → 従来の `waitType` で進む
- `NotifyEvent(名前)` は残っており、内部で `TutorialSignal::Emit` へ流す。旧 `waitType=Event` も、新しい条件木の `signal` も、同じ名前で拾える
- `pauseGameplay` の既定値は `true` のまま。遊ばせながら教えたいステップだけ `false` にする

---

## 6. エディタ

ステップインスペクタに「完了条件」セクションを追加した。

- 条件の種類をコンボで選択
- `signal` はシグナル名を候補一覧から選択（直接入力も可）＋必要回数
- `all` / `any` / `not` は子条件を追加・削除でき、最大4段まで入れ子にできる
- 完了条件を設定すると、旧 `waitType` の設定欄はグレーアウトする

候補一覧には、発火実績のある名前に加えて `InputActionMap` に登録済みの全アクション名が最初から並ぶ。

---

## 7. スポットライト

### やっていること

暗幕を1枚のくり抜き画像で作るのではなく、**「穴を避けた矩形の集合」へ分解して敷き詰める**。穴の部分にはスプライトが1枚も置かれないので、下にあるUIも3Dの画もそのまま明るく残る。シェーダは不要。

分解は横帯の走査で行う。

1. 全ての穴の上辺・下辺で画面を横帯に切る
2. 帯ごとに、その帯を丸ごと跨ぐ穴の x 区間を集めて左から走査する
3. 穴と穴の隙間を暗幕の矩形として切り出す

穴が1つなら上下左右の4枚になり、複数あっても破綻しない。重なった穴は走査中に自然に結合される。

暗幕は `style.layer - 1` に敷く。説明パネル（`style.layer`）より下、ゲームUIより上になる。

### 対象の指定

穴を「画面矩形」として抽象化してあるので、3種類を同じ経路で扱える。

| kind | 指定するもの | 用途 |
|---|---|---|
| `ui` | `id`（UIManager のID） | HUDのボタンアイコンなど |
| `rect` | `center` `size`（画面座標） | 座標直打ち。依存が無く確実に出る |
| `world` | `id`（登録名）`radius`（ワールド単位） | 敵や宝箱など3D空間上の対象 |

```jsonc
"spotlight": {
  "enabled": true,
  "dimColor": [0.0, 0.0, 0.0, 0.65],
  "padding": 16.0,
  "fadeSeconds": 0.25,
  "targets": [
    { "kind": "ui",    "id": "ControlUI_ButtonA" },
    { "kind": "world", "id": "NearestEnemy", "radius": 1.5 }
  ]
}
```

### ワールド対象の登録

`world` を使うにはゲーム側で2つ渡す必要がある。

```cpp
auto* spotlight = YoRigine::TutorialSpotlight::GetInstance();
spotlight->SetCamera(camera);
spotlight->RegisterWorldTarget("NearestEnemy",
    [this]() { return enemy_->GetWorldPosition(); });
```

位置を**関数で**受け取るので、動き回る敵にもそのまま追従する（矩形は毎フレーム組み直される）。カメラ未設定、名前未登録、対象がカメラの後ろ、のいずれでも黙って無視され、他の穴だけで暗幕が作られる。

半径のピクセル換算は、対象の中心と XYZ 各方向へ半径ぶんずらした点を投影し、最も大きな見かけの距離を採用している。カメラの向きで特定の軸が潰れても破綻しないため。

### 制限

- 穴は**軸並行の矩形のみ**。円形や角丸にしたい場合は、穴の上に枠テクスチャを重ねる想定。
- **フェードインのみ**。`Clear()` は即座に消える。ステップ間の暗幕は毎回フェードインし直す。

---

## 8. サンプル

`Resources/Json/Tutorials/AttackBasics.json`

1. 軽攻撃を3回（`signal` × count 3、`pauseGameplay: false`）
2. ガードを1回（`signal`）
3. スタイル切替、または8秒経過（`any` の入れ子）

`TutorialManager::GetInstance()->LoadAndStart("Resources/Json/Tutorials/AttackBasics.json")` で開始できる。

---

## 8. 確認済みの範囲

Debug / Develop / Release の3構成でビルド成功（両プロジェクトとも `fatalwarnings "All"`）。

**実機での動作確認は未実施。** Debug / Develop で起動し、エディタの「システム > チュートリアル」パネルから
`Resources/Json/Tutorials/AttackBasics.json` を読み込んで再生すると確認できる。見るべき点は以下。

- 軽攻撃を3回振るとページが送られるか（決定入力ではなく行動で進むか）
- `pauseGameplay: false` なのでゲームが動いたまま説明が出るか
- 3ページ目が、スタイル切替でも8秒経過でも進むか（`any` の確認）
- 完了条件を未設定にしたステップが、従来通り決定入力で進むか（後方互換の確認）

---

## 9. Gotchas

- **軸（移動）はシグナルを出さない。** 自動発火するのはボタン系アクションの押下だけで、`Move` / `Look` のような軸は対象外。「スティックで移動してみよう」を完了条件にはまだできない。閾値超えでシグナル化する仕組みが要る。
- **`gameplaySpeed`（スロー再生）は入れていない。** `GameTime::Update()` が毎フレーム `timeScale_` を hitstop / slowmo から再計算して上書きするため、`SetChannelScale` は次フレームで消える（`GameTime.cpp` の Gameplay チャンネル更新）。実装するには `GameTime` 側に「持続スケール」の枠を足し、hitstop と合成する形にする必要がある。
- **`InputActionMap::SetTriggerObserver` は1枠しかない。** `ConnectEngineSources()` がその枠を占有する。他にアクション押下を購読したいものが出たら、ここを複数購読に分岐させること。
- **`TutorialConditionRuntime` は定義へのポインタを持つ。** 指す先は `TutorialManager::currentData_` の中なので、`Start` / `Stop` のタイミングで必ず張り直している。条件木を別の場所で使う場合は寿命に注意。
- **当たり判定はまだシグナルを出さない。** `CollisionManager` に購読フックが無く、「敵に近づいたら開始」のような条件は書けない。開始条件（trigger）を入れる Phase 3 と同時に対応する。
- **`ConnectEngineSources()` の呼び出し場所が暫定。** 現在は `TutorialManager::Start()` とエディタ描画から呼んでいる（冪等）。本来は初期化時に一度で足りるので、エンジンの初期化順が固まったら `MyGame::Initialize` へ移す。
- **ゲームからチュートリアルを開始する導線がまだ無い。** `LoadAndStart()` を呼ぶのは現状エディタパネルだけで、シーン側から自動で始まる仕組みは入れていない。開始条件（trigger）を扱う Phase 3 で用意する。
- **スポットライトのカメラとワールド対象は未登録。** `TutorialSpotlight::SetCamera` / `RegisterWorldTarget` をゲーム側から呼んでいる箇所がまだ無いため、`kind: "world"` は現状すべて無視される。`ui` と `rect` は動く。
- **暗幕は `style.layer - 1` 固定。** ゲームUIがそれ以上のレイヤーを使っていると暗幕の上に出てしまう。HUD 側のレイヤー帯と衝突しないか確認すること。

---

## 10. 今後の予定

| Phase | 内容 | 状態 |
|---|---|---|
| 0 | `InputActionMap` + `PlayerInput`（[InputActionMap.md](InputActionMap.md)） | **完了** |
| 1 | `TutorialSignal`、`TutorialCondition`、非ポーズ進行 | **完了** |
| 2 | `TutorialSpotlight`（暗幕の矩形くり抜き、UI／ワールド両対応） | **完了** |
| 3 | 開始条件（trigger）による非線形起動、既読フラグの永続化、入力ゲートの実効化、当たり判定シグナル | 未着手 |
| 4 | エディタの拡充（条件のプレビュー、シグナルのログ表示） | 未着手 |

Phase 3 でステップの進行を「線形カーソル」から「待機プール → トリガ成立でアクティブ → 完了で終了」の状態機械へ変える。`trigger` 未設定のステップは「前のステップ完了で起動」に落ちるので、既存 JSON は従来の線形挙動のまま動く。
