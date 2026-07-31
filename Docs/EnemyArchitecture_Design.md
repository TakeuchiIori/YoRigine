# 敵アーキテクチャ再設計 — BaseEnemy / StateMachine 統合 / データ駆動攻撃 / BehaviorTree

対象: `YGame/GameObjects/Enemy/` 配下すべて
目的: ボス実装に耐える骨格へ作り替える。現状のハードコーディングと二重管理を潰す。

---

## 0. 結論

1. **BaseEnemy を作るのは正しい。** ただし「共通の親を作る」だけでは効果は半分。
2. **StateMachine への統合も正しい。ただし順番が逆。** 先に「攻撃をデータ駆動化」しないと、
   状態が増え続けるので enum 方式に寄せた瞬間に破綻する。
3. **BehaviorTree は FSM の置き換えではなく「行動選択層」として上に載せる。**
   実行層(被弾/ダウン/死亡/攻撃モーション)は FSM のまま。混ぜると必ず壊れる。
4. ノード描画は自作不要。**`Externals/imgui/imgui_node_editor.cpp` が既に ImGui.vcxproj で
   コンパイル済みで、プロジェクト内で一度も使われていない。** これを使う。

---

## 1. 現状の棚卸し

### 1.1 状態管理が2系統ある

| | プレイヤー | 敵 |
|---|---|---|
| 基底 | `IState<StateEnum>` (`Player/StateMachine.h`) | `IEnemyState<T>` (`Enemy/IEnemyState.h`) |
| 保持 | `StateMachine<CombatState>` が全 State を事前登録・所有 | オーナーが `unique_ptr<IEnemyState<T>>` を1つ持つ |
| 遷移 | `ChangeState(enum)` — インスタンス使い回し | `ChangeState(make_unique<XxxState>())` — **毎遷移ヒープ確保** |
| オーナー参照 | State がコンストラクタで `PlayerCombat*` を保持 | `Enter/Update/Exit(T& enemy, ...)` で毎回渡す |
| 遷移ロジックの置き場 | State 内 | State 内 **＋ `BattleEnemy::OnEnterCollision` 内** |

敵側の実装は「毎フレーム状態オブジェクトを作り直す」「遷移条件が当たり判定コールバックに漏れている」の2点で
プレイヤー側より明確に劣っている。ここは統一して良い。

### 1.2 BattleEnemy と FieldEnemy の重複

両方に同じものが独立実装されている:

- `stateTimer_` / `ResetStateTimer` / `AddStateTimer` / `GetStateTimer`
- `logicalState_` と `ChangeState`
- `player_` / `GetPlayerPosition()` / `SetPlayer`
- `GetTranslate / SetTranslate / AddTranslate / SetRotationY / GetRotationY`
- `OnEnterCollision` 系4つのオーバーライド

さらに紛らわしいのが体力まわりで、`FieldEnemy::TakeDamage(float)` は**名前に反して HP を一切減らさない**。
`takeDamage_` に代入するだけの「背後からの奇襲ダメージをバトル開始時へ持ち越すための記録値」で、
FieldEnemy には HP という概念自体が無い。`BattleEnemy` 側の `TakeDamage(int)` とは別物なのに同名。
(段階1で `SetCarryOverDamage` / `GetCarryOverDamage` へ改名済み)

回転補間 (`RotateTowards / RotateTowardsPlayer / RotateTowardsDirection`) は `FieldEnemy` にしかなく、
`BattleEnemy` 側の各攻撃 State は `enemy.SetRotationY(std::atan2(dir.x, dir.z))` を直書きしている
(`BattleRushAttackState.cpp:25,54` ほか)。これは典型的な「共通化されていないから毎回書かれる」パターン。

マネージャも `BattleEnemyManager.cpp` 1067行 / `FieldEnemyManager.cpp` 1058行で、
骨格 (データ表の JSON ロード → ID 引き → 実体プール → 範囲検索 → エディタ UI を friend で分離) が同一。

### 1.3 ハードコーディングの実害

**(a) `dynamic_cast` による状態判定** — `BattleEnemy.cpp:503`

```cpp
if (dynamic_cast<BattleRushAttackState*>(GetCurrentState()) != nullptr) {
    if (other->GetTypeID() == kPlayerShield) { ... ChangeState(BattleDownedState) }
}
```

盾でのけぞらせられるのは Rush 攻撃だけ。Spin / Jump / ChargeRush / Combo は盾で止まらない。
`IEnemyState` には既に `IsAttacking()` / `KeepsStateWhenDamaged()` という能力フラグの仕組みがあるのに、
ここだけ具象型判定になっている。**攻撃を1つ増やすたびにこの `dynamic_cast` を書き足す設計。**

**(b) 攻撃を1種類足すのに触るファイルが多すぎる**

`AttackSelector.h` の構造:

```cpp
struct AttackDistanceAffinity {
    float rushVeryClose, rushClose, rushMid, rushFar;
    float spinVeryClose, spinClose, spinMid, spinFar;
    float chargeVeryClose, ...   // 攻撃種別ごとに4フィールドずつ手書き
};
```

新攻撃 `Slam` を足す作業:

1. `AttackPatternType` に `Slam` 追加
2. `AttackPatternToString` / `FromString` に追加 (文字列との手動同期)
3. `AttackDistanceAffinity` に `slamVeryClose..slamFar` 4フィールド追加
4. `AttackHPPriority` に `slamPriority` 追加
5. `GetDistanceWeight` の **switch 4箇所** に case 追加
6. `GetPlayerHPWeight` の switch に case 追加
7. `CreateAttackState` に case 追加
8. `SelectSmartAttack` の距離分岐に追加
9. `BattleSlamAttackState.{h,cpp}` を新規作成
10. `EnemyAttackParams` に `SlamAttackParams slam;` 追加

**10箇所。しかもこれは全敵共有の enum なので、ボス専用攻撃を足すと雑魚敵の重み計算にも case が増える。**
ボスを3体作れば攻撃は30種類。この設計では確実に破綻する。

**(c) 攻撃 State の中身がほぼ同じ**

`BattleRushAttackState.cpp` の構造は「予備動作 → チャージ → 突進 → クールダウン」の時間区間分岐。
`ChargeRush` / `Spin` / `Jump` / `Combo` / `Counter` も全部同じ形。違うのは各フェーズの中身と数値だけ。
600行以上が「同じ時間軸ロジックのコピー」になっている。

### 1.4 一方で、プロジェクトには既に正解のパターンがある

`Player/Magic/` を見ると:

```
MagicActionData.h      … フェーズ/タイムラインイベントをデータで表現
MagicActionDatabase    … JSON から読む
MagicActionRunner      … データを解釈して実行する「1つの」ランナー
MagicActionEditor      … ImGui で編集
```

**魔法は既にデータ駆動になっている。敵の攻撃だけ C++ クラス直書きのまま取り残されている。**
新設計はこれを敵側に横展開するだけなので、学習コストはほぼゼロ。

---

## 2. 設計案の比較

### 案A: プレイヤーの `StateMachine<Enum>` に敵をそのまま寄せる

`BattleEnemy` が `StateMachine<BattleEnemyState>` を持ち、全 State を事前登録。

- ○ プレイヤーと実装が揃う。インスタンス再利用でヒープ確保が消える。
- ○ 登録済み State を列挙できるのでエディタ表示が作れる。
- ✕ **今のままだと enum に攻撃を全部並べる必要がある。** `Attack_Rush, Attack_Spin, ... Boss_Meteor, Boss_Roar` と膨れ、
  ボス専用状態が雑魚敵の enum にも入る。1.3(b) の問題が enum に移動するだけ。
- ✕ 動的な攻撃抽選 (`AttackSelector`) と噛み合わない。

**単体では不採用。ただし攻撃をデータ駆動化した後なら最良になる (後述)。**

### 案B: `IEnemyState` のまま `BaseEnemy` に共通処理だけ集約する (最小手術)

- ○ 変更量が小さく、既存挙動を壊すリスクが低い。1〜2日で終わる。
- ✕ 遷移ロジックの散乱、毎遷移のヒープ確保、攻撃追加コストは何も解決しない。
- ✕ ボスを作る段階で結局もう一度作り直しになる。

**単体では不採用。ただし「移行の第1段階」としては採用する。**

### 案C: 3層アーキテクチャ + 攻撃データ駆動 (採用)

```
┌─────────────────────────────────────────┐
│ 意思決定層  AttackSelector / BehaviorTree │  ← 「次に何をするか」を決める
├─────────────────────────────────────────┤
│ 実行層      StateMachine<StateEnum>      │  ← 「決まった行動を最後までやる」
├─────────────────────────────────────────┤
│ データ層    BaseEnemy + EnemyAttackAction │  ← HP/移動/VFX/攻撃データ
└─────────────────────────────────────────┘
```

- ○ 攻撃追加が JSON 1エントリ。共有 C++ に触らない。
- ○ **攻撃をデータ化した瞬間、State の数が固定小数になる**
  (`Idle / Approach / Attack / Damage / Downed / Recovery / Dead` の7個程度)。
  → 案A の enum 方式が成立する。案A の欠点が消える。
- ○ ボス固有 State が必要なら `enum class BossState` を別に定義すれば良い
  (`StateMachine<Enum>` はテンプレートなので敵クラスごとに別 enum を持てる)。汚染しない。
- ○ BT の導入先が「意思決定層」と明確に定まる。
- ✕ 工数が最大。段階移行が必須。

**採用。** 以降はこの案の詳細。

---

## 3. 採用設計

### 3.1 Layer 1: `BaseEnemy`

`YGame/GameObjects/Enemy/BaseEnemy.{h,cpp}`

```cpp
// 全ての敵の共通基盤。状態管理そのものは持たず、
// 「敵なら必ず持つデータと振る舞い」だけを担当する。
class BaseEnemy : public YoRigine::BaseObject {
public:
    // ── 体力 ──  (int に統一する。FieldEnemy の float takeDamage_ は廃止)
    void TakeDamage(int damage);
    void Heal(int amount);
    int  GetHP() const;
    int  GetMaxHP() const;
    float GetHPRatio() const;
    bool IsAlive() const;
    bool IsInvincible() const;

    // ── ターゲット ──
    void SetPlayer(Player* p);
    Player* GetPlayer() const;
    Vector3 GetPlayerPosition() const;
    float   GetDistanceToPlayer() const;

    // ── 移動・回転 (BattleEnemy 側の atan2 直書きを撲滅) ──
    void AddTranslate(const Vector3& d);
    void RotateTowards(float targetYaw, float speed, float dt);
    void RotateTowardsPlayer(float speed, float dt);
    void RotateTowardsDirection(const Vector3& dir, float speed, float dt);

    // ── 被弾リアクション ──
    void StartKnockback(const Vector3& dir, float power, float duration);
    void StartDirectionalHitReaction(const Vector3& dir);

    // ── 状態VFX (燃焼・感電) ──
    void AttachStatusVfx(const std::string& composite, float duration);
    void StopStatusVfx();

    // ── 派生が実装するフック ──
    virtual void OnDamaged(int damage, const Vector3& fromPos) {}
    virtual void OnDeath() {}

    // ── デバッグ表示用。派生の型付き StateMachine を型消去して見せる ──
    virtual const char* GetStateName() const = 0;

protected:
    void UpdateCommon(float dt);   // ノックバック/のけぞり/VFX/点滅/visualWt_ 同期
    // ...共通メンバ (HP, knockbackData_, hitReaction*, statusVfx_, animation_, visualWt_)
};
```

**入れないもの** (派生固有なので混ぜない):
- `EnemyHealthBarUI` (BattleEnemy 専用)
- `EnemyAlert` / スポットライト / NavPathfinder (FieldEnemy 専用)
- フォーメーション/エンカウント (マネージャの責務)

`BaseObjectManager` には登録しない。メモリ記録どおり、戦闘シーンは意図的に据え置きのままにする。

### 3.2 Layer 2: 実行層 — `StateMachine<Enum>` に統一

`Player/StateMachine.h` を `YGame/GameObjects/Common/StateMachine.h` へ移動し、敵と共有する。
敵用に能力フラグを足した派生インターフェースを定義する。

```cpp
// 敵の State が持つ「今どういう状態か」を外から問い合わせるための能力フラグ。
// dynamic_cast による具象型判定を全廃するために使う。
template<typename StateEnum>
class IEnemyStateBase : public IState<StateEnum> {
public:
    virtual bool IsAttacking()            const { return false; }
    virtual bool IsContactDamageActive()  const { return IsAttacking(); }
    virtual int  GetContactDamageWindow() const { return IsContactDamageActive() ? 0 : -1; }
    virtual bool KeepsStateWhenDamaged()  const { return false; }
    virtual bool CanBeParried()           const { return false; }  // ← 追加。盾判定用
    virtual bool IsFinished()             const { return false; }  // ← 追加。BT の Running 判定用
};
```

これで `BattleEnemy.cpp:503` は次のようになる:

```cpp
// 変更前: if (dynamic_cast<BattleRushAttackState*>(GetCurrentState()) != nullptr)
// 変更後:
if (auto* s = GetCurrentState(); s && s->CanBeParried()) {
    if (other->GetTypeID() == kPlayerShield && guardIsActive) {
        ChangeState(BattleEnemyState::Downed);
    }
}
```

`CanBeParried()` は攻撃データ側の `parriable: true` を読むだけになるので、
どの攻撃を盾で止められるかが JSON で決まる。

**遷移ロジックの集約**: `OnEnterCollision` に埋まっているカウンター遷移 (`BattleEnemy.cpp:472-485`) は
`OnDamaged()` フックへ移す。当たり判定コールバックは「ダメージを渡す」だけにする。

### 3.3 Layer 3: 攻撃のデータ駆動化 (最重要)

`Player/Magic/MagicActionData` と同じ構造を敵に用意する。

```cpp
// 攻撃フェーズの種別。これが C++ 側に実装される唯一のプリミティブ集合。
// 攻撃そのものはこれらの並びとして JSON で表現する。
enum class AttackPhaseType {
    Anticipation,   // 予備動作 (後退/沈み込み/捻り)
    Charge,         // 溜め (向き追従あり)
    Dash,           // 直進突進
    Spin,           // その場回転
    Leap,           // 放物線ジャンプ
    Projectile,     // 飛び道具生成
    Wait,           // 硬直
    Scripted,       // 逃げ道: C++ 実装のフェーズを ID で差し込む
};

struct AttackPhase {
    AttackPhaseType type = AttackPhaseType::Wait;
    float duration   = 0.5f;
    float distance   = 0.0f;   // Anticipation の後退量 / Dash の到達距離
    float speedMul   = 1.0f;
    float homing     = 0.0f;   // 突進中の追従強度
    float height     = 0.0f;   // Leap の高さ
    float rotations  = 0.0f;   // Spin の回転数
    bool  damageActive = false; // このフェーズで接触ダメージが出るか
    int   damageWindow = -1;    // 多段ヒットの番号 (コンボは段ごとに変える)
    Vector3 scaleMul{1,1,1};    // ObjectAnimation に渡す見た目
    Vector4 color{1,1,1,1};
    std::string vfxAsset;       // EffectHandle で再生する Composite 名
    std::string scriptedId;     // type == Scripted のときだけ使う
};

struct EnemyAttackAction {
    std::string id;                    // "rush", "boss_meteor" など
    std::vector<AttackPhase> phases;
    // ── 選択条件 (AttackSelector の switch 群を置き換える) ──
    float minRange = 0.0f;
    float maxRange = 999.0f;
    float weight   = 1.0f;             // 基本重み
    float cooldown = 0.0f;             // この攻撃自体のクールタイム
    float selfHpBelow  = 1.0f;         // 自分のHPがこれ以下でのみ使う (ボスの追い込み技)
    float targetHpBelow = 1.0f;        // プレイヤーHPがこれ以下でのみ使う (トドメ技)
    int   phaseGate = -1;              // ボスのフェーズ番号。-1 なら常時
    bool  parriable = false;           // 盾でダウンさせられるか
};
```

これを実行する State はただ1つ:

```cpp
// データで定義された攻撃を、フェーズ順に実行する唯一の攻撃ステート。
// BattleRush/Spin/Jump/ChargeRush/Combo/Counter の6クラスをこれ1つで置き換える。
class EnemyAttackState : public IEnemyStateBase<BattleEnemyState> {
public:
    void SetAction(const EnemyAttackAction* action);   // Enter 前に呼ぶ
    void OnEnter() override;
    void Update(float dt) override;
    void OnExit() override;

    bool IsAttacking()           const override { return true; }
    bool IsContactDamageActive() const override { return CurrentPhase().damageActive; }
    int  GetContactDamageWindow()const override { return CurrentPhase().damageWindow; }
    bool CanBeParried()          const override { return action_->parriable; }
    bool IsFinished()            const override { return phaseIndex_ >= action_->phases.size(); }
private:
    const EnemyAttackAction* action_ = nullptr;
    size_t phaseIndex_ = 0;
    float  phaseTimer_ = 0.0f;
};
```

**選択ロジック**は switch が消えて条件フィルタ + 重み抽選になる:

```cpp
const EnemyAttackAction* EnemyAttackPicker::Pick(const BaseEnemy& e, int phase) {
    std::vector<const EnemyAttackAction*> candidates;
    float total = 0.0f;
    for (const auto& a : e.GetAttackActions()) {
        if (a.phaseGate >= 0 && a.phaseGate != phase)         continue;
        if (e.GetDistanceToPlayer() < a.minRange)             continue;
        if (e.GetDistanceToPlayer() > a.maxRange)             continue;
        if (e.GetHPRatio()          > a.selfHpBelow)          continue;
        if (e.GetPlayerHPRatio()    > a.targetHpBelow)        continue;
        if (IsOnCooldown(a.id))                               continue;
        candidates.push_back(&a); total += a.weight;
    }
    // …重み抽選
}
```

**新攻撃を足す作業が「JSON に1エントリ書く」だけになる。** C++ の共有ファイルは1行も触らない。
`AttackPatternType` enum、`AttackDistanceAffinity`、`GetDistanceWeight` の4連 switch、
`CreateAttackState`、`AttackPatternToString/FromString` はすべて削除できる。

エディタは `MagicActionEditor` をほぼそのまま流用できる (タイムライン UI が既にある)。

#### 逃げ道: `Scripted` フェーズ

データ駆動を貫くと必ず「JSON で書けない特殊挙動」が出る (ボスが床を割る、多段ワープなど)。
`AttackPhaseType::Scripted` + `scriptedId` で C++ 実装を名前引きできるようにしておく。
これが無いと、後で必ず「特殊攻撃だけ別クラス」が復活してデータ駆動が骨抜きになる。

```cpp
// 登録: BossEnemy::Initialize などで
ScriptedPhaseRegistry::Register("boss_floor_break",
    [](BaseEnemy& e, float t, float dt) { /* …特殊処理… */ });
```

### 3.4 ボス

```cpp
enum class BossState {
    Intro, Idle, Approach, Attack, Damage, Stagger,
    PhaseTransition,   // 演出中は無敵・入力受付なし
    Enraged, Dead,
};

class BossEnemy : public BaseEnemy {
    StateMachine<BossState> fsm_;
    std::unique_ptr<BTRunner> brain_;   // 意思決定は BT に任せる
    int phase_ = 0;
    std::vector<float> phaseHpThresholds_;  // 例 {0.7f, 0.35f}
};
```

- フェーズ切替は `TakeDamage` 後に閾値を跨いだら `PhaseTransition` へ強制遷移。
- フェーズごとの技構成は `EnemyAttackAction::phaseGate` で表現。BT を書き換えなくて済む。
- 多部位判定 (角/尻尾に弱点) が要るなら、`BaseObject` のコライダースロット3種では足りないので
  子オブジェクトを別 `BaseObject` として持ち、`wt_` の親子付けで追従させる。これは別issue。

### 3.5 マネージャの共通化

```cpp
// JSON のデータ表 (ID → データ) を扱う部分だけを切り出す
template<class TData> class EnemyDataTable {
    bool Load(const std::string& path);  bool Save(const std::string& path) const;
    const TData* Find(const std::string& id) const;
    std::unordered_map<std::string, TData> map_;
};

// 実体プールと空間クエリ
template<class TEnemy> class EnemyPool {
    TEnemy* Spawn(...);  void Remove(...);
    std::vector<TEnemy*> GetInRange(const Vector3& c, float r);
    TEnemy* GetNearest(const Vector3& p);
    size_t  GetActiveCount() const;
};
```

`BattleEnemyManager` / `FieldEnemyManager` はこの2つを持ち、
「エンカウント/リスポーン」「フォーメーション/戦闘結果」という固有ドメインだけを残す。
1000行 → 500行程度に落ちる想定。エディタ UI が既に別クラスに分離されているのでここは安全にやれる。

---

## 4. BehaviorTree

### 4.1 責務の分界 — ここを間違えると必ず壊れる

| 層 | 担当 | 実装 |
|---|---|---|
| 意思決定 | 「次に何をするか」を毎 tick 判断 | **BehaviorTree** |
| 実行 | 「決めた行動を最後までやり切る」 | **StateMachine** |
| 割り込み | 被弾・死亡・フェーズ移行 | **FSM が BT の外から強制遷移** |

被弾/死亡を BT の中で表現しようとするのが一番ありがちな失敗。
「攻撃モーション中に被弾したらどう中断するか」を BT の abort ルールで解こうとすると
デバッグ不能になる。**被弾と死亡は BT を通さず FSM が直接遷移させ、BT には
「今 FSM が Attack で走っているか」だけ見せる。**

適用範囲の判断:

- `FieldEnemy` (Patrol/Alert/Chase/Search) → **BT にしない。** 4状態の FSM で足りている。
- `BattleEnemy` (雑魚) → 小さい BT。もしくは今の重み抽選のままでも可。
- `BossEnemy` → **ここが BT の本命。** フェーズ別サブツリー、連携技のシーケンス、
  「距離が遠い かつ 大技クールダウン明け なら 突進で詰めてから薙ぎ払い」のような
  複合条件が FSM だと爆発する。

### 4.2 最小実装 (これで足りる。外部ライブラリは不要、実質300行)

`YGame/GameObjects/Enemy/AI/BT/` に置く。

```cpp
// ── 実行結果 ──
enum class BTStatus { Success, Failure, Running };

// ── Blackboard ──
// 文字列キーの汎用マップにしない。型安全と補完が効く素の構造体で始める。
// 汎用マップが必要になるのは、エディタから任意変数を触りたくなった時だけ。
struct EnemyAIContext {
    BaseEnemy* self   = nullptr;
    Player*    player = nullptr;
    float distance      = 0.0f;   // プレイヤーまでの距離
    float hpRatio       = 1.0f;
    float playerHpRatio = 1.0f;
    float timeSinceLastAttack = 0.0f;
    bool  playerIsAttacking   = false;
    int   phase = 0;              // ボスのフェーズ番号
};

// ── ノード基底 ──
class BTNode {
public:
    virtual ~BTNode() = default;
    virtual BTStatus Tick(EnemyAIContext& ctx) = 0;
    virtual void     Abort(EnemyAIContext& ctx) {}   // 上位優先ノードに割り込まれた時の後始末

    const std::string& GetName() const { return name_; }
    BTStatus GetLastStatus() const { return lastStatus_; } // 可視化用
    const std::vector<std::unique_ptr<BTNode>>& GetChildren() const { return children_; }
protected:
    std::string name_;
    BTStatus    lastStatus_ = BTStatus::Failure;
    std::vector<std::unique_ptr<BTNode>> children_;
};
```

**Selector** (上から試して最初に成功/実行中になった子を採用):

```cpp
class BTSelector : public BTNode {
public:
    BTStatus Tick(EnemyAIContext& ctx) override {
        for (size_t i = 0; i < children_.size(); ++i) {
            BTStatus s = children_[i]->Tick(ctx);
            if (s == BTStatus::Failure) continue;
            // より優先度の高い子が動き出したら、走っていた下位の子を中断する
            if (running_ >= 0 && running_ != static_cast<int>(i)) {
                children_[running_]->Abort(ctx);
            }
            running_ = (s == BTStatus::Running) ? static_cast<int>(i) : -1;
            return lastStatus_ = s;
        }
        running_ = -1;
        return lastStatus_ = BTStatus::Failure;
    }
private:
    int running_ = -1;
};
```

**Sequence** (全部順に成功したら Success):

```cpp
class BTSequence : public BTNode {
public:
    BTStatus Tick(EnemyAIContext& ctx) override {
        while (index_ < children_.size()) {
            BTStatus s = children_[index_]->Tick(ctx);
            if (s == BTStatus::Running) return lastStatus_ = BTStatus::Running;
            if (s == BTStatus::Failure) { index_ = 0; return lastStatus_ = BTStatus::Failure; }
            ++index_;
        }
        index_ = 0;
        return lastStatus_ = BTStatus::Success;
    }
    void Abort(EnemyAIContext& ctx) override {
        if (index_ < children_.size()) children_[index_]->Abort(ctx);
        index_ = 0;
    }
private:
    size_t index_ = 0;
};
```

**Condition デコレータ** (名前でファクトリ登録し、JSON から引く):

```cpp
class BTCondition : public BTNode {
public:
    using Pred = std::function<bool(const EnemyAIContext&)>;
    BTCondition(std::string name, Pred p) { name_ = std::move(name); pred_ = std::move(p); }
    BTStatus Tick(EnemyAIContext& ctx) override {
        if (!pred_(ctx)) return lastStatus_ = BTStatus::Failure;
        if (children_.empty()) return lastStatus_ = BTStatus::Success;
        return lastStatus_ = children_[0]->Tick(ctx);
    }
private:
    Pred pred_;
};
```

**Cooldown デコレータ**:

```cpp
class BTCooldown : public BTNode {
public:
    BTStatus Tick(EnemyAIContext& ctx) override {
        if (timer_ > 0.0f) return lastStatus_ = BTStatus::Failure;
        BTStatus s = children_[0]->Tick(ctx);
        if (s == BTStatus::Success) timer_ = seconds_;
        return lastStatus_ = s;
    }
    void Advance(float dt) { timer_ = std::max(0.0f, timer_ - dt); }
private:
    float seconds_ = 5.0f, timer_ = 0.0f;
};
```

**Action 葉ノード** — ここが FSM との接続点。**FSM に状態を要求し、終わるまで Running を返す**:

```cpp
// 指定 ID の攻撃を FSM に要求し、その攻撃が終わるまで Running を返す。
class BTPlayAttack : public BTNode {
public:
    explicit BTPlayAttack(std::string id) : id_(std::move(id)) { name_ = "Attack:" + id_; }
    BTStatus Tick(EnemyAIContext& ctx) override {
        if (!started_) {
            if (!ctx.self->RequestAttack(id_)) return lastStatus_ = BTStatus::Failure;
            started_ = true;
            return lastStatus_ = BTStatus::Running;
        }
        if (ctx.self->IsCurrentActionFinished()) {
            started_ = false;
            return lastStatus_ = BTStatus::Success;
        }
        return lastStatus_ = BTStatus::Running;
    }
    void Abort(EnemyAIContext& ctx) override { started_ = false; }
private:
    std::string id_;
    bool started_ = false;
};
```

### 4.3 tick の頻度と割り込み

- **毎フレーム tick しない。** `BattleEnemyManager.cpp:89-92` に既に
  `aiUpdateTimer_ / aiUpdateInterval_ (0.1s)` の枠があり、中身が空のまま放置されている。
  ここが BT tick の置き場所。0.1s 間隔で十分で、判断のもたつきがキャラの「間」にもなる。
- **被弾・死亡・フェーズ移行は BT を通さない。** `BaseEnemy::OnDamaged` から FSM を直接
  `Damage` / `Stagger` へ飛ばし、BT 側は次 tick で `BTPlayAttack` が
  `IsCurrentActionFinished()` を見て Failure を返す → 自然に別枝へ流れる。
- **FSM が「割り込み不可状態」の間は tick をスキップする。**
  `if (fsm_.Current()->IsUninterruptible()) return;`

### 4.4 JSON 形式

```json
{
  "name": "boss_phase_tree",
  "root": {
    "type": "Selector",
    "children": [
      { "type": "Condition", "cond": "HpBelow", "value": 0.35,
        "child": { "type": "PlayAttack", "action": "boss_enraged_meteor" } },
      { "type": "Sequence", "name": "詰めてから薙ぎ払い", "children": [
        { "type": "Condition", "cond": "DistanceGreater", "value": 8.0 },
        { "type": "PlayAttack", "action": "boss_dash" },
        { "type": "PlayAttack", "action": "boss_sweep" }
      ]},
      { "type": "Cooldown", "seconds": 12.0,
        "child": { "type": "PlayAttack", "action": "boss_roar" } },
      { "type": "PlayAttack", "action": "boss_basic" }
    ]
  }
}
```

条件は**式文字列をパースしない**。`"cond": "HpBelow", "value": 0.35` のように
名前 + パラメータでファクトリ登録する。式パーサを書き始めると本題から3日消える。

```cpp
BTConditionRegistry::Register("HpBelow",
    [](float v){ return [v](const EnemyAIContext& c){ return c.hpRatio <= v; }; });
```

保存先: `Resources/Json/EnemyAI/*.json`

### 4.5 ノードの描画

**段階1 (先にこれをやる。半日で済み、効果の8割はここ)** — `ImGui::TreeNodeEx` で実行状態を色分け表示。

```cpp
#ifdef USE_IMGUI
static void DrawBTNode(const BTNode* n) {
    ImVec4 col;
    switch (n->GetLastStatus()) {
    case BTStatus::Running: col = {0.3f, 1.0f, 0.3f, 1.0f}; break;  // 緑 = 実行中
    case BTStatus::Success: col = {0.6f, 0.8f, 1.0f, 1.0f}; break;  // 青 = 成功
    default:                col = {0.5f, 0.5f, 0.5f, 1.0f}; break;  // 灰 = 不成立
    }
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    bool open = ImGui::TreeNodeEx(n, n->GetChildren().empty()
                    ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_DefaultOpen,
                    "%s", n->GetName().c_str());
    ImGui::PopStyleColor();
    if (open) {
        for (const auto& c : n->GetChildren()) DrawBTNode(c.get());
        ImGui::TreePop();
    }
}
#endif
```

これを `Editor::RegisterGameUI("BossAI", ...)` で登録すれば、
「今どの枝が走っているか」がリアルタイムで見える。**BT デバッグの本体はこれ。**

**段階2 — ノードグラフ編集。ライブラリは既にある。**

`Externals/imgui/imgui_node_editor.cpp` / `imgui_node_editor_api.cpp` は
`ImGui.vcxproj` でコンパイル済み (`crude_json.cpp`, `imgui_canvas.cpp` も込み) だが、
**プロジェクト内で一度も include されていない**。thedmd 版 imgui-node-editor がそのまま使える。
`Externals/imgui` は既にインクルードルートなので premake の変更も不要。

```cpp
#ifdef USE_IMGUI
#include <imgui_node_editor.h>
namespace ed = ax::NodeEditor;

// 初期化 (ノード位置は SettingsFile に自動保存される)
ed::Config cfg;
cfg.SettingsFile = "Resources/Json/EnemyAI/boss_bt_layout.json";
ctx_ = ed::CreateEditor(&cfg);

// 描画
ed::SetCurrentEditor(ctx_);
ed::Begin("BTEditor");
for (auto& n : flatNodes_) {                 // ツリーを事前にフラット化して一意 ID を振る
    ed::BeginNode(n.id);
      ImGui::TextUnformatted(n.name.c_str());
      ed::BeginPin(n.inPin,  ed::PinKind::Input);  ImGui::Text("->"); ed::EndPin();
      ImGui::SameLine();
      ed::BeginPin(n.outPin, ed::PinKind::Output); ImGui::Text("->"); ed::EndPin();
    ed::EndNode();
}
for (auto& l : links_) ed::Link(l.id, l.from, l.to);
ed::End();
ed::SetCurrentEditor(nullptr);
#endif
```

注意点:

- ノード/ピン/リンクの ID は **1 始まりの一意な整数**。0 は無効値扱いなので使わない。
- ノード位置は `SettingsFile` に勝手に保存される。BT 本体の JSON とは別ファイルにする。
- **`USE_IMGUI` は Debug のみ。** BT の構築・実行コードは絶対に `#ifdef USE_IMGUI` の中に入れない。
  エディタは JSON を吐くだけ、実行は JSON を読むだけ、という分離を守る。
- `GraphEditor.cpp` (ImGuizmo 付属) もビルドされているが、機能が貧弱なので使わない。

---

## 5. 移行手順

既に動いているゲームなので、各段階で**挙動が変わらないこと**を確認しながら進める。

| # | 内容 | 挙動変化 | 目安 |
|---|---|---|---|
| 1 | **✅完了** `BaseEnemy` 抽出。HP/移動/回転/ノックバック/のけぞり/状態VFX/アニメを集約 | なし | 済 |
| 2 | `StateMachine.h` を `Common/` へ移動。`IEnemyStateBase` 定義。`BattleEnemy`/`FieldEnemy` を enum 版 FSM へ移行 | なし | 2日 |
| 3 | `dynamic_cast` 撲滅 (`CanBeParried`)、`OnEnterCollision` の遷移ロジックを `OnDamaged` へ移動 | 盾で止まる攻撃が増える (仕様変更。要調整) | 半日 |
| 4 | `EnemyAttackAction` + `EnemyAttackState` 実装。既存6攻撃を JSON 化して**同じ数値**を再現 | なし (数値を写経すれば同一) | 3〜4日 |
| 5 | `AttackSelector` を `EnemyAttackPicker` (条件フィルタ+重み) へ置換。旧 enum と switch 群を削除 | 抽選の質が変わる。要プレイ確認 | 1日 |
| 6 | マネージャ共通化 (`EnemyDataTable` / `EnemyPool`) | なし | 2日 |
| 7 | `BossEnemy` + フェーズ + BT 導入 + ツリー可視化 UI | 新機能 | 4〜5日 |
| 8 | (任意) `imgui_node_editor` によるノード編集 | 新機能 | 3日〜 |

**4 を飛ばして 2 を完成形にしようとしないこと。** 攻撃がクラスのままだと enum が膨れる。
**7 を先にやらないこと。** BT の葉が要求する「データで定義された攻撃」が無いと、
結局 BT から `make_unique<BattleXxxAttackState>()` を呼ぶことになり、何も解決しない。

---

## 6. 想定される弱点と対策

| 弱点 | 対策 |
|---|---|
| State インスタンスを使い回すと、前回の残骸 (`dirLocked_`, `anticipationStartPos_` 等) が残る | State のメンバは構造体1つにまとめて `OnEnter` 冒頭で `data_ = {};` する規約にする |
| データ駆動で表現できない特殊挙動が必ず出る | `AttackPhaseType::Scripted` + `ScriptedPhaseRegistry` を最初から入れておく |
| Blackboard を文字列キーにすると型安全が死ぬ | `EnemyAIContext` は素の構造体で始める。汎用マップは必要になってから |
| BT を全敵に入れるとデバッグ対象が増えて逆に遅くなる | FieldEnemy は FSM のまま。BT はボス(+必要なら雑魚)だけ |
| 条件を式文字列で書きたくなる | 名前+パラメータのファクトリ登録に固定。パーサは書かない |
| JSON が1ファイルに集中して壊れる | 敵IDごとにファイル分割 (`Resources/Json/Objects/BattleEnemies/` の既存構成を踏襲) |
| ボス専用 State が共有 enum を汚染する | `StateMachine<Enum>` はテンプレート。`BossState` を別に定義する |
| `USE_IMGUI` が Debug のみ | BT の構築・実行は必ずガード外。エディタは JSON を吐くだけ |
| 移行途中で Release ビルドが壊れる | 各段階で `msbuild /p:Configuration=Release` を通す。YGame は Release で static lib なので `GAME_API` の付け忘れに注意 |

### 命名について

`Behavior` は Behavior Tree と紛らわしいのでクラス名に使わない方針だが、
**BT ノードは文字通り Behavior Tree なので `BT` プレフィックスは可**
(`BTNode`, `BTSelector`, `BTPlayAttack`)。
意思決定層のクラスは `EnemyBehavior` ではなく `BTRunner` / `EnemyAIContext` とする。

---

## 7. 着手前チェックリスト

- [x] 段階1着手前に Debug/Release 両方のビルドが通ることを確認
- 新規ファイルを足したら `Tools/premake.bat` で再生成する。`Tools/premake5.lua` は
  `YGame/**.cpp` をグロブしているので自動で拾われる。再生成しない場合は
  `YGame/YGame.vcxproj` と `.filters` へ手動追加が必要 (どちらも .gitignore 対象の生成物)
- [ ] `Resources/Json/Objects/BattleEnemies/*.json` と `field_enemy_data.json` をバックアップ
- [ ] 既存6攻撃の数値 (`BattleEnemyData.h` の各 `*AttackParams`) を JSON へ写経する対応表を作る
- [ ] 段階3で「盾で止まる攻撃が増える」ため、どの攻撃を `parriable: true` にするか事前に決める
- [ ] 段階5の抽選置換後、旧 `SelectSmartAttack` の距離分岐 (遠=Jump/ChargeRush, 中=Rush, 近=Spin/Combo) を
      `minRange`/`maxRange` で再現できているか実機確認
- [ ] BT 導入後、`aiUpdateInterval_` を変えて「判断の間」が不自然でないか確認
