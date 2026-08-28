# Tutorial システム：クラスリファレンス

`YEngine/Utilities/Systems/Tutorial/` の各クラスが**何をするクラスで、どう使うのが正しいか**をまとめる。
設計の背景や「なぜこの形なのか」は [TutorialSystem.md](TutorialSystem.md) を参照。こちらは日々の使い方の索引。

作成日：2026-07-29

---

## 0. 全体像

チュートリアルは5つのクラス（＋エディタ）でできている。すべて `namespace YoRigine`。

| クラス | 役割 | 種類 |
|---|---|---|
| `TutorialSignal` | 出来事を名前で流すイベントバス | シングルトン |
| `TutorialCondition` / `TutorialConditionRuntime` | 完了・開始条件の「定義」と「評価状態」 | 値型 / 値型 |
| `TutorialProgress` | 既読ステップの永続化 | シングルトン |
| `TutorialSpotlight` | 注目箇所以外を暗幕で覆う | シングルトン |
| `TutorialManager` | 全体の司令塔（進行・UI・条件・暗幕・ゲートを駆動） | シングルトン |

### 依存関係

```
TutorialManager（司令塔）
  ├─ TutorialConditionRuntime ── OnSignal ── TutorialSignal
  ├─ TutorialSpotlight（暗幕）── Camera / UIManager
  ├─ TutorialProgress（既読）
  └─ UIManager（説明パネル）

TutorialSignal（バス）
  ├← InputActionMap（アクション・軸の自動発火）
  └← CollisionManager（接触の自動発火）
```

**普段ゲーム側が直接触るのは `TutorialManager` と `TutorialSignal` の2つだけ。** 残り3つは
`TutorialManager` が内部で駆動する。`TutorialSpotlight` だけは、ワールド対象を使うときにシーンから登録が要る。

---

## 1. TutorialSignal ── イベントバス

`TutorialSignal.h`

ゲーム内の出来事を**名前**で流す軽量なバス。ゲーム側は「チュートリアルのため」ではなく
**自分の出来事として `Emit` するだけ**で、チュートリアルはそれを購読する。両者は互いを知らない。

### 何を呼ぶか

```cpp
// 発火（ゲーム側の任意の場所）
TutorialSignal::Emit("guard.success");           // 名前だけ

TutorialSignalData data;                          // ペイロード付き
data.name = "enemy.killed";
data.intValue = 3;
TutorialSignal::Emit(data);

// 初期化時に一度だけ：エンジン内蔵の発火源を接続する
TutorialSignal::GetInstance()->ConnectEngineSources();
```

`Emit` は static。`GetInstance()` を書かずに直接呼べる。

### ConnectEngineSources() が繋ぐもの

これを呼ぶと、以下がゲーム側コード**0行**で自動的にシグナルになる。

| 発火源 | シグナル名 |
|---|---|
| `InputActionMap` のアクション押下 | `action.triggered.<アクション名>` |
| `InputActionMap` の軸を倒した（閾値0.5を跨いだ瞬間） | `action.axis.<軸名>` |
| `CollisionManager` の接触開始 | `collision.enter.<型名>` |
| `CollisionManager` の接触終了 | `collision.exit.<型名>` |

名前の組み立てヘルパも用意されている：`ActionTriggeredName()` / `AxisEngagedName()` / `ContactName()`。

### 購読（通常はゲーム側で使わない）

```cpp
auto h = TutorialSignal::GetInstance()->Subscribe(
    [](const TutorialSignalData& s) { /* ... */ });
TutorialSignal::GetInstance()->Unsubscribe(h);
```

購読は主に `TutorialManager` が内部で使う。ゲームのロジックが直接購読する用途は基本ない。

### 正しい使い方 / 注意

- ✅ **発火はゲームの語彙で**。`"tutorial.step2.done"` ではなく `"guard.success"`。
- ⚠️ `ConnectEngineSources()` は冪等（何度呼んでも二重接続しない）。現状 `TutorialManager::Start()` と
  エディタ描画の先頭からも呼ばれる。
- ⚠️ **エンジン側の Observer 枠は各1つ**（`InputActionMap::SetTriggerObserver` /
  `CollisionManager::SetContactObserver`）。`ConnectEngineSources()` がその枠を占有する。
  他に同じ Observer を使いたいものが出たら、そこを複数購読へ分岐させること。
- ⚠️ 接触シグナルは**型ID単位**。`collision.enter.Enemy` は「敵に何かが触れた」の意味で、
  誰が触れたかは区別できない。

---

## 2. TutorialCondition / TutorialConditionRuntime ── 条件の木

`TutorialCondition.h`

完了条件・開始条件を**木構造**で表す。「攻撃を3回」「攻撃かつ敵に当てた」を JSON で書けるようにする。

**定義（`TutorialCondition`）と評価状態（`TutorialConditionRuntime`）が分かれている**のが要点。
定義は読み取り専用データ、受信回数などの実行時状態はランタイム側が同じ形の木として別に持つ。

### 定義（値型 struct）

```cpp
enum class TutorialConditionType {
  None,     // 未設定。従来の waitType へフォールバック
  Signal,   // 指定シグナルが requiredCount 回届いたら成立
  Elapsed,  // seconds 秒経過したら成立
  Confirm,  // 決定入力が押されたら成立
  All, Any, Not,   // 子条件の論理合成
};

struct TutorialCondition {
  TutorialConditionType type = TutorialConditionType::None;
  std::string signalName;   // Signal 用
  int requiredCount = 1;    // Signal 用
  float seconds = 0.0f;     // Elapsed 用
  std::vector<TutorialCondition> children;  // All/Any/Not 用
};
```

通常は JSON から読まれる（`TutorialStep::complete` / `trigger`）。手書きコードで組むことはまれ。

### ランタイム（評価状態クラス）

```cpp
void Reset(const TutorialCondition& def);  // 定義を束ね、状態リセット（ステップ開始時）
void Clear();                              // 定義との結び付きを解除
void OnSignal(const TutorialSignalData&);  // 届いたシグナルを木全体へ配る
void Update(float elapsedSeconds, bool confirmTriggered);  // 毎フレーム。時間/決定をラッチ
bool IsSatisfied() const;                  // 現在成立しているか
bool HasDefinition() const;                // 有効な定義があるか（None なら false）
```

`Confirm` と `Elapsed` は「押した瞬間」「跨いだ瞬間」しか来ないので、`Update()` で成立を**ラッチ**する。
だから `all` の中に `confirm` を入れても、他が後から揃えば成立する。

### 正しい使い方 / 注意

- ✅ 使い回すときは `Reset` で状態を初期化する。子が空の `all`/`any` は**不成立**扱い（設定漏れ対策）。
- ⚠️ **`Reset` に渡した定義の寿命に注意。** ランタイムは定義への**ポインタ**を保持する
  （`definition_`）。`TutorialManager` では定義が `currentData_` 内にあるため、`Start`/`Stop` の
  タイミングで必ず張り直している。別の場所で使うなら、定義がランタイムより長生きすることを保証すること。

---

## 3. TutorialProgress ── 既読の永続化

`TutorialProgress.h`

一度見たステップを記録し、次回以降は出さないための永続データ。キーは `"<チュートリアル名>/<ステップ名>"`。
保存先の既定は `Resources/Json/Tutorials/Progress.json`。

```cpp
static TutorialProgress* GetInstance();

bool Load();                     // 明示読込（問い合わせ時にも自動で読まれる）
bool Save() const;

bool IsSeen(const std::string& tutorialName, const std::string& stepName) const;
void MarkSeen(const std::string& tutorialName, const std::string& stepName);  // 記録して即保存
void ClearAll();                 // 全消去（デバッグ用）。ファイルへも反映

void SetFilePath(const std::string& path);
std::size_t GetSeenCount() const;
```

### 正しい使い方 / 注意

- ✅ 普段は**直接触らない**。ステップに `"once": true` を付ければ `TutorialManager` が完了時に
  `MarkSeen` を呼ぶ。`ClearAll()` はデバッグで「もう一度全部見たい」ときに使う。
- ✅ `MarkSeen` は**即座に保存**する。途中でゲームが落ちても記録が残る。
- ⚠️ ステップの管理名を変えると**別物**として扱われ、また表示される（作り直したステップを見せ直す意図）。

---

## 4. TutorialSpotlight ── 暗幕（スポットライト）

`TutorialSpotlight.h`

注目させたい場所**以外**を暗幕で覆う。暗幕は1枚のくり抜き画像ではなく
**「穴を避けた矩形の集合」へ分解**して敷き詰めるので、穴の下のUIも3Dの画もそのまま明るく残る（シェーダ不要）。

### 対象の3種類

```cpp
enum class TutorialSpotlightTargetKind { Ui, Rect, World };
```

| kind | 指定するもの | 用途 |
|---|---|---|
| `Ui` | `id`（UIManager のID） | HUDのボタンアイコンなど |
| `Rect` | `center` `size`（画面座標） | 座標直打ち。依存が無く確実に出る |
| `World` | `id`（登録名）`radius`（ワールド単位） | 敵・宝箱など3D空間上の対象 |

### API

```cpp
static TutorialSpotlight* GetInstance();

void Apply(const TutorialSpotlightConfig& config, int layer);  // 適用（layer は暗幕を敷く描画レイヤー）
void Clear();          // fadeSeconds を掛けて薄くしてから消す
void ClearImmediate(); // 即座に消す（シーン終了など次フレームが無い場面用）
void Update(float deltaTime);  // 毎フレーム。フェードと動く対象への矩形追従
void Draw();           // 暗幕を描画
bool IsActive() const; // 消えかけの間も true

// ワールド対象を使うとき
void SetCamera(Camera* camera);
void RegisterWorldTarget(const std::string& name, std::function<Vector3()> provider);
void UnregisterWorldTarget(const std::string& name);
void ClearWorldTargets();
```

`Apply`/`Clear`/`Update`/`Draw` は **`TutorialManager` が内部で呼ぶ**ので、ゲーム側は普通触らない。

### ワールド対象の登録（シーンからの責務）

`World` 種別を使うには、シーン側でカメラと位置プロバイダを渡す必要がある。

```cpp
// GameScene::Initialize
auto* sp = YoRigine::TutorialSpotlight::GetInstance();
sp->SetCamera(camera);
sp->RegisterWorldTarget("Player", [this]() { return player_->GetWorldPosition(); });

// GameScene::Finalize ── 必ず解除する
sp->ClearWorldTargets();
sp->SetCamera(nullptr);
```

位置を**関数で**受け取るので、動き回る対象にも追従する（矩形は毎フレーム組み直す）。

### 正しい使い方 / 注意

- ⚠️ **登録した関数はシーンの寿命に紐づく。** シーン破棄時の `ClearWorldTargets()` /
  `SetCamera(nullptr)` は必須。シングルトン側に残すと解放済みシーンを参照する。
- ⚠️ カメラ未設定・名前未登録・対象がカメラの後ろ、のいずれでも黙って無視され、他の穴だけで暗幕が作られる。
- ⚠️ 穴は**軸並行の矩形のみ**（円・角丸は不可）。
- ⚠️ `Clear()` はフェード完了まで `IsActive()` が true のまま。消え終わるまで `Update`/`Draw` を回すこと。
- 💡 現在ワールド対象を登録しているのは `GameScene` だけ。Title/Clear では `kind: "world"` は無視される。

---

## 5. TutorialManager ── 司令塔

`TutorialManager.h`

ステップ進行・UI表示・条件評価・暗幕・入力ゲート・ゲーム速度をまとめて駆動する。
**ゲーム側が最もよく触るクラス。**

### ライフサイクルAPI

```cpp
static TutorialManager* GetInstance();

// 保存・読込
bool Save(const TutorialData& data, const std::string& path) const;
bool Load(TutorialData& data, const std::string& path);
bool LoadAndStart(const std::string& path, std::size_t startStep = 0);  // 読込＋再生開始

// 再生制御
void Start(const TutorialData& data, std::size_t startStep = 0);
void Stop();       // 暗幕・ゲート・ゲーム速度をすべてリセットして停止
void Update();     // 毎フレーム。条件評価・UI更新・フェード
void Draw();       // 暗幕→説明パネルの順に自前描画

// 状態
bool IsPlaying() const;
bool HasActiveStep() const;              // 開始条件待ちで何も出していない間は false
std::size_t GetCurrentStepIndex() const;
const TutorialData& GetCurrentData() const;

// 互換・その他
void NotifyEvent(const std::string& eventName);  // 旧イベント通知（内部で Emit へ転送）
void Advance();                                  // 現在ステップを強制で進める

#ifdef USE_IMGUI
void DrawEditor();
#endif
```

### 組み込み方（MyGame への配線）

順序が重要。詳細は [TutorialSystem.md §2](TutorialSystem.md) を参照。

```cpp
// MyGame::Initialize
YoRigine::TutorialSignal::GetInstance()->ConnectEngineSources();

// MyGame::Update ── シーン更新の後
SceneManager::GetInstance()->Update();
YoRigine::TutorialManager::GetInstance()->Update();

// MyGame::Draw ── シーン描画とレターボックスの後
YoRigine::TutorialManager::GetInstance()->Draw();
```

### チュートリアルを開始する

```cpp
TutorialManager::GetInstance()->LoadAndStart("Resources/Json/Tutorials/AttackBasics.json");
```

または `Load` してから `Start(data, startStep)`。

### データモデル（JSON で書く部分）

`TutorialData` → `TutorialStyle`（全ステップ共通）＋ `TutorialStep[]`。
各 `TutorialStep` が持つ主なもの：

| フィールド | 意味 |
|---|---|
| `complete` (`TutorialCondition`) | 完了条件。`None` なら旧 `waitType` へフォールバック |
| `trigger` (`TutorialCondition`) | 開始条件。`None` なら前ステップ完了で順番に起動（＝線形進行） |
| `spotlight` | 暗幕設定 |
| `highlight` | 対象UIを揺らして強調（暗幕とは別系統。併用可） |
| `gate` | 表示中の入力制限（`allow` のアクション以外を封じる） |
| `once` | 一度見たら出さない（`TutorialProgress` へ記録） |
| `pauseGameplay` / `gameplaySpeed` | ゲーム停止 / 停止しないときの速度 |
| `layout` / `additionalUIs` | 説明UIの配置と補足画像 |

各条件・レイアウトの JSON 詳細は [TutorialSystem.md](TutorialSystem.md) の §4・§7・§9 に一覧がある。

### 正しい使い方 / 注意

- ✅ **`Update` はシーン更新の後、`Draw` はレターボックスの後**に呼ぶ。順序を崩さない。
- ✅ 停止は必ず `Stop()`。暗幕・入力ゲート・ゲーム速度スケールをまとめて元へ戻す。
- ⚠️ **開始条件（`trigger`）はチュートリアル再生中しか評価されない。** 「ゲーム中いつでも、敵に初めて会ったら出す」
  は、シーン開始時に `LoadAndStart` しておく運用で実現する。
- ⚠️ **チュートリアルは常に最前面。** 自前描画をレターボックスの後に置いているため、カットシーン中でも
  説明パネルが乗る。演出中に隠したいなら `MyGame::Draw` の呼び出しを条件付きにする。
- ⚠️ **ゲーム内からの開始導線はまだエディタパネルだけ。** どのシーンでどのファイルを読むかはゲーム設計の話。
- ⚠️ `GameTime::SetGameplaySustainedScale`（`gameplaySpeed` の実装）も**枠は1つ**。他がスロー再生を
  使い始めると取り合いになる。

---

## 6. よくある作業別インデックス

| やりたいこと | 触るもの |
|---|---|
| 「ボタンを3回押したら次へ」を作る | JSON の `complete` に `signal`。コードは不要 |
| ゲームの出来事を条件に使いたい | ゲーム側で `TutorialSignal::Emit("...")`、JSON の `signal.name` で拾う |
| 3D空間の敵を暗幕でくり抜く | シーンで `RegisterWorldTarget` ＋ JSON `spotlight` に `kind: "world"` |
| 一度見たら二度と出さない | JSON ステップに `"once": true` |
| もう一度全部見たい（デバッグ） | `TutorialProgress::GetInstance()->ClearAll()` |
| チュートリアルを止める | `TutorialManager::GetInstance()->Stop()` |

---

関連ドキュメント：[TutorialSystem.md](TutorialSystem.md)（設計思想）・[InputActionMap.md](InputActionMap.md)（入力の発火源）
