# 入力アクションマップ：アクション名による入力の抽象化

生のキーコード／パッドボタンを直接読む実装をやめ、**「攻撃」「ガード」といったアクション名で入力を引く層**を導入した記録。キーコンフィグ、チュートリアルの入力ゲート、カットシーン中の操作制限が、この1層の上に載る。

導入日：2026-07-28

---

## 1. なぜ入れたか

きっかけは汎用チュートリアルシステムの設計だった。「攻撃ボタンを押してほしい」ことを engine 側が知るには、engine が「攻撃」という概念を扱える必要がある。しかし `Input` クラスは `IsPadTriggered(0, GamePadButton::A)` しか提供しておらず、**A ボタンが攻撃であることを知っているのはゲーム側のコードの中だけ**だった。

そのため以下ができなかった。

- チュートリアル中に「回避だけ許可、攻撃は封印」という部分的な入力制限
- キーコンフィグ画面
- 「どのアクションが押されたか」の観測（チュートリアル・実績・アナリティクス）

`Input` に「攻撃」を書き足すのは層の違反になる。そこで **エンジン側にアクション名を知らない汎用マップ、ゲーム側にゲーム固有のアクション名を持つ薄いラッパ** という2層構成にした。

---

## 2. システム構成

| 層 | クラス | 責務 |
|---|---|---|
| Engine | `Input` | デバイスの生の状態（キー、パッド、マウス、振動） |
| Engine | `InputActionMap` | 名前 → バインドの解決、エッジ検出、ゲート、観測 |
| Game | `PlayerInput` | プレイヤー用アクション名の定義と型付きアクセサ |

`InputActionMap` は **ゲーム固有のアクション名を一切知らない**。「AttackLight」という文字列を決めるのは `PlayerInput` の役目で、エンジンから見ればただの辞書のキーでしかない。この分離があるおかげで、別のゲームでも `InputActionMap` はそのまま使い回せる。

- `YEngine/Utilities/Systems/Input/InputActionMap.{h,cpp}`
- `YGame/GameObjects/Player/Input/PlayerInput.{h,cpp}`
- `Resources/Json/Input/PlayerActions.json`

### 更新順序

`Framework::Update()` で、生入力の直後に更新する。

```cpp
input_->Update();                                  // デバイスの状態を取得
YoRigine::InputActionMap::GetInstance()->Update();  // アクションの押下状態を確定
ObjectManager::GetInstance()->Update();            // 以降は全て同じスナップショットを見る
```

`InputActionMap::Update()` の中で1フレーム1回だけ全アクションを評価し、結果をキャッシュする。したがって同じフレーム内で何度問い合わせても値は揺れない。

---

## 3. データ構造

### バインド

```cpp
struct InputActionBinding {
    std::string name;
    std::vector<BYTE> keys;                  // DIK_* のリスト
    std::vector<GamePadButton> padButtons;
    bool leftTrigger  = false;               // LT を割り当てる
    bool rightTrigger = false;               // RT を割り当てる
};

struct InputAxisBinding {
    std::string name;
    InputStickKind stick = InputStickKind::Left;  // None / Left / Right
    BYTE keyUp = 0, keyDown = 0, keyLeft = 0, keyRight = 0;  // 0 は未割り当て
    float deadzone = 0.2f;
    float signalThreshold = 0.5f;  // 「倒した」と通知する倒し量
};
```

1つのアクションに複数のキー／ボタンを割り当てられる。どれか1つでも成立すれば ON。LT／RT はボタンビットではなくアナログ値なので、ボタンリストではなく専用フラグで持つ。

### 軸の評価結果

```cpp
struct InputAxisValue {
    Vector2 value{ 0.0f, 0.0f };   // デッドゾーン適用後
    Vector2 raw{ 0.0f, 0.0f };     // デッドゾーン適用前
    float magnitude = 0.0f;        // value の長さ（0〜1にクランプ）
    InputDeviceKind device = InputDeviceKind::Keyboard;
    bool isAnalog = false;         // スティック由来なら true
};
```

`raw` を分けているのは、呼び出し側が独自の閾値を持つ場合に二重にデッドゾーンを掛けないため。

### 軸の評価規則

1. スティックが割り当てられていてコントローラーが接続済みなら、スティックを先に見る。デッドゾーン適用後の長さが `0.01` を超えていればその値を採用し、`isAnalog = true` で返す。
2. 超えていなければキーボードの4方向キーを合成し、正規化して返す（キーボードにはデッドゾーンを掛けない）。

この優先順位と閾値は、リファクタ前の `PlayerMovement::GetInputState()` の挙動をそのまま移植したもの。

---

## 4. API

### 問い合わせ

```cpp
auto* map = YoRigine::InputActionMap::GetInstance();

map->IsPressed("AttackLight");    // 押しっぱなし
map->IsTriggered("AttackLight");  // 押した瞬間
map->IsReleased("AttackLight");   // 離した瞬間
map->GetAxis("Move");             // InputAxisValue
```

### 登録

```cpp
YoRigine::InputActionBinding guard;
guard.name       = "Guard";
guard.padButtons = { GamePadButton::X };
guard.keys       = { DIK_N };
map->AddAction(guard);
```

同名のバインドが既にあれば上書きされる。登録順は `GetActionNames()` / `GetAxisNames()` で保たれるので、エディタ表示やチュートリアルの候補一覧にそのまま使える。

### ゲート

```cpp
map->SetEnabled("AttackLight", false);                 // 個別に封じる
map->SetExclusivelyEnabled({ "Move", "AttackLight" }); // これ以外を全て封じる
map->EnableAll();                                      // 解除
```

無効化されたアクション／軸は、`IsPressed` / `IsTriggered` / `IsReleased` が `false` を、`GetAxis` が空の値を返す。**呼び出し側のコードを一切変更せずに操作を止められる**のが要点で、チュートリアルやカットシーンはこれだけで成立する。

ゲートはアクションと軸を区別せず1つの集合で管理している。`SetExclusivelyEnabled` に `"Move"` を渡せば移動軸だけ生かせる。

### 観測

```cpp
map->SetTriggerObserver([](const std::string& name, InputActionMap::EventKind kind) {
    // ActionTriggered : ボタンを押した瞬間
    // AxisEngaged     : 軸の倒し量が signalThreshold を超えた瞬間
});
```

軸は「倒し続けている間ずっと」ではなく、閾値を跨いだ瞬間だけ通知する。閾値は `InputAxisBinding::signalThreshold`（既定 `0.5`）で軸ごとに指定でき、JSON にも書ける。

ゲートで無効化されている名前は通知されない（封じられている操作を「押した」ことにしないため）。チュートリアルのシグナルバスがここに接続されている。

### デバイス判定

```cpp
map->LastDevice();  // InputDeviceKind::Keyboard / Gamepad
```

そのフレームに入力のあったデバイスを記録する。同時に触られた場合はゲームパッドを優先し、入力が全く無いフレームでは直前の値を保持する。UI のボタン表示切り替えに使う。

---

## 5. JSON によるバインド定義

`Resources/Json/Input/PlayerActions.json`

```jsonc
{
  "version": 1,
  "actions": [
    { "name": "AttackLight", "key": [],        "pad": ["A"] },
    { "name": "Guard",       "key": ["N"],     "pad": ["X"] },
    { "name": "Run",         "key": ["LSHIFT"],"pad": [] }
  ],
  "axes": [
    { "name": "Move", "stick": "Left", "deadzone": 0.2,
      "keyUp": "W", "keyDown": "S", "keyLeft": "A", "keyRight": "D" }
  ]
}
```

`PlayerInput::Initialize()` は **まずコードで既定バインドを登録し、その後 JSON で上書きする**。JSON が存在しない／壊れている場合は `LoadFromFile` が `false` を返すだけで既定バインドが残るため、**設定ファイルの不備で操作不能になることはない**。

### 使える名前

| 種別 | 名前 |
|---|---|
| キー | `A`〜`Z`、`0`〜`9`、`SPACE` `ENTER` `ESC` `TAB` `BACKSPACE`、`LSHIFT` `RSHIFT` `LCTRL` `RCTRL` `LALT` `RALT`、`UP` `DOWN` `LEFT` `RIGHT`、`F1`〜`F12` |
| パッド | `A` `B` `X` `Y` `LB` `RB` `START` `BACK` `L_STICK` `R_STICK` `DPAD_UP` `DPAD_DOWN` `DPAD_LEFT` `DPAD_RIGHT`、および `LT` `RT` |
| スティック | `None` `Left` `Right` |

未知の名前は無視される（キーコード `0` は未割り当て扱い）。名前とコードの相互変換は `InputActionMap::KeyCodeFromName` / `KeyNameFromCode` / `PadButtonFromName` / `PadButtonName` で公開しており、キーコンフィグ UI からも使える。

`SaveToFile()` で現在のバインドを書き戻せる。

---

## 6. PlayerInput

アクション名の文字列は `PlayerAction` 名前空間に集約し、綴り間違いをコンパイル時に潰す。

```cpp
namespace PlayerAction {
    inline constexpr const char* kMove        = "Move";
    inline constexpr const char* kLook        = "Look";
    inline constexpr const char* kRun         = "Run";
    inline constexpr const char* kAttackLight = "AttackLight";
    inline constexpr const char* kAttackHeavy = "AttackHeavy";
    inline constexpr const char* kGuard       = "Guard";
    inline constexpr const char* kStyleToggle = "StyleToggle";
    inline constexpr const char* kLockOn      = "LockOn";
}
```

ゲーム側は型付きのアクセサだけを使う。

```cpp
playerInput_->AttackLightTriggered();
playerInput_->GuardHeld();
playerInput_->MoveAxis();
playerInput_->RunHeld();
```

`Player` が `std::unique_ptr<PlayerInput>` として所有し、`Player::GetPlayerInput()` で公開する。`PlayerMovement` は `owner_->GetPlayerInput()` 経由で参照する。

### 既定バインド

| アクション | パッド | キーボード |
|---|---|---|
| `AttackLight` | A | — |
| `AttackHeavy` | B | — |
| `Guard` | X | N |
| `StyleToggle` | Y | — |
| `LockOn` | R_STICK | — |
| `Run` | —（注） | LSHIFT |
| `Move`（軸） | 左スティック | W / S / A / D |
| `Look`（軸） | 右スティック | — |

注：ゲームパッドの走りはボタンではなくスティックの倒し量で判定する。`MovementConfig::analogRunThreshold` が担当するため `Run` にパッドボタンを割り当てていない。

---

## 7. 置き換えた箇所

| 場所 | 変更内容 |
|---|---|
| `Player::HandleCombatInput` | 生ボタン読み8行を削除し `PlayerInput` 経由へ |
| `Player::HandleSwordInput` / `HandleMagicInput` | bool 引数6個のリレーを廃止し、内部で `PlayerInput` を直接参照 |
| `Player::IsAttackPressedA` / `IsAttackPressedB` | コンボ先行入力の判定も `PlayerInput` 経由（ゲートが効くようになる） |
| `PlayerMovement::GetInputState` | `GetKeyboardInput` / `GetControllerInput` / `ApplyDeadzone` の3関数を削除し1本化 |

`PlayerMovement` はキーボード用とコントローラー用で入力取得をほぼ同じ形で二重に持っていたが、軸バインドがデバイス差を吸収するため1本に畳めた。

### デッドゾーンの扱い

調整元は `MovementConfig::analogDeadzone` のまま残した。`PlayerMovement::Update()` が毎フレーム `PlayerInput::SetMoveDeadzone()` でバインド側へ同期するので、エディタで値を変えるとその場で効く。設定の置き場所を移動していないため、既存の Player.json とも互換がある。

デッドゾーンの計算式（閾値未満を0に落とし、超えた分を0〜1へ引き伸ばす）は `PlayerMovement::ApplyDeadzone` から `InputActionMap` へそのまま移した。

---

## 8. 確認済みの範囲

Debug / Develop / Release の3構成でビルド成功。両プロジェクトとも `fatalwarnings "All"` なので警告ゼロでもある。

挙動の等価性はコード上の移植で担保しており、実機での操作確認は未実施。特に確認すべきは以下の2点。

- キーボードでの斜め移動（正規化の結果が従来と一致するか）
- スティックの走り判定（`analogRunThreshold` との比較タイミング）

---

## 9. Gotchas

- **`PlayerCamera` はまだ生入力のまま。** `PlayerCamera.cpp` のロックオン判定（`IsPadTriggered(0, R_Stick)`）と視点操作（`GetJoystickState` の直参照）が残っている。`LockOn` のバインドは登録済みだが接続していないため、**現状 `LockOn` をゲートで塞いでもロックオンは止まらない**。視点操作は `GetJoystickState` を生で使っており、軸バインドへ寄せるには別途対応が要る。
- **`Input` は消していない。** 振動（`StartVibration`）やマウスなど、アクション名で抽象化する意味がない機能は `Input` を直接使う。`Player` も振動用に `input_` を保持したまま。
- **ゲートは名前空間を持たない。** アクション名と軸名が同じ集合で管理されるので、両者で名前が衝突しないようにすること。
- **`DemoPlayer` は未対応。** 生入力のまま残している。

---

## 10. 今後の予定

この層は汎用チュートリアルシステムの土台として入れたもので、以下が続く。

| Phase | 内容 | 状態 |
|---|---|---|
| 0 | `InputActionMap` + `PlayerInput` | **完了** |
| 1 | `TutorialSignal`（シグナルバス）、`TutorialCondition`（条件ツリー）、ポーズせず進行 | **完了** |
| 2 | `TutorialSpotlight`（暗幕の矩形くり抜き、UI／ワールド両対応） | 未着手 |
| 3 | 開始条件による非線形起動、既読フラグの永続化、ゲートの実効化 | 未着手 |
| 4 | エディタの条件ピッカー、シグナル一覧 | 未着手 |

詳細は [TutorialSystem.md](TutorialSystem.md) を参照。

Phase 1 のシグナルバスは `InputActionMap::SetTriggerObserver` に接続するだけで `action.triggered.AttackLight` が流れる。**ボタン押下を完了条件にするチュートリアルは、ゲーム側のコードを1行も書かずに作れる**というのが、この層を先に入れた理由。

なお `SetTriggerObserver` は1枠しかなく、現在は `TutorialSignal::ConnectEngineSources()` が占有している。他に購読者が必要になったら複数購読へ拡張すること。
