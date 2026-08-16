# 敵クラス設計（現状）

`YGame/GameObjects/Enemy/` 配下の現在の構成。
移行計画そのものは `EnemyArchitecture_Design.md`（当初の設計案）を参照。
こちらは**今どうなっているか**を書いたもの。

---

## 全体像

```
BaseEnemy                     … 敵なら必ず持つデータと振る舞い
 ├ FieldEnemy                 … フィールドを巡回し、接触でエンカウントを起こす
 └ BattleEnemy                … 戦闘中の敵。攻撃AIを持つ

各シーンのマネージャが実体を所有する
 ├ FieldEnemyManager          … スポーン / リスポーン / エンカウント仲介
 └ BattleEnemyManager         … 戦闘の進行 / フォーメーション / 攻撃権
```

状態管理は `IEnemyState<T>` のポリモーフィックな State パターン。
`BattleEnemy` と `FieldEnemy` がそれぞれ自分用の State 群を持つ。

---

## 1. BaseEnemy

`BaseEnemy.h/.cpp`（277 / 285 行）。`YoRigine::BaseObject` を継承。

「敵なら必ず持つもの」だけを集約している。

| 分類 | 内容 |
|---|---|
| 体力 | `TakeDamage` / `ApplyDamage` / `Heal` / `SetupHP` / `GetHPRatio` / 生存・無敵フラグ |
| ターゲット | `player_` / `GetPlayerPosition` / `GetDistanceToPlayer` / 最終確認位置 |
| 状態タイマー | `stateTimer_` + アクセサ |
| 移動 | `AddTranslate` / 目標地点 / 速度算出 |
| 回転 | `RotateTowards` / `RotateTowardsPlayer` / `FaceDirection` など |
| 被弾リアクション | ノックバック / 方向別のけぞり / ダメージ点滅 |
| 状態VFX | `AttachStatusVfx` / `UpdateStatusVfx` / `StopStatusVfx` |
| 見た目 | `visualWt_`（描画専用Transform）/ `animation_` / `SetColor` |

**入れていないもの**（派生固有のため）
- 状態管理そのもの（State 集合が派生ごとに違う）
- 体力バーやアラートUI
- NavPathfinder / フォーメーション

### ダメージの2経路

意図的に分けてある。

| 関数 | 動作 | 呼び出し元 |
|---|---|---|
| `TakeDamage(int)` | HPだけ減らす | 魔法2箇所、デバッグボタン、全体ダメージ |
| `ApplyDamage(DamageInfo)` | HP減少 + `OnDamaged()` フック | 武器ヒット |

`ApplyDamage` は**実際にHPが減ったときだけ** `OnDamaged()` を呼ぶ。
無敵で弾かれたのにのけぞりだけ出る、といった不整合が起きない。

魔法ダメージが現状のけぞりを起こさない挙動なので、
それを変えないために `TakeDamage` を残している。
魔法でものけぞらせたくなったら呼び出しを `ApplyDamage` に変えるだけ。

---

## 2. FieldEnemy

`FieldEnemy.h/.cpp`（247 / 432 行）

Patrol / Alert / Chase / Search / Despawn の5状態。
`NavGrid` + `NavPathfinder` で経路探索し、視界（スポットライト）で索敵する。

### 注意点: `IsDespawned()` の命名

```cpp
bool IsDespawned() const { return logicalState_ == FieldEnemyState::Despawn; }
```

以前ここが `IsActive()` という名前だった。
基底の `BaseObject::IsActive()` と同名だが `virtual` ではないため、
`BaseObject*` で保持しているマネージャからは**基底の方が呼ばれ**、
消滅判定が効かないというバグが起きた。

同じ事故を避けるため、あえて基底と衝突しない名前にしている。
**基底と同名の非virtual関数を派生で定義しないこと。**

### `Despawn()` は基底フラグも落とす

```cpp
void Despawn() {
    logicalState_ = FieldEnemyState::Despawn;
    SetActive(false);   // ← これが無いと影だけ残る
}
```

影は `FieldEnemyManager` ではなく `BaseObjectManager::DrawShadowAll()` から描かれ、
そこは `BaseObject::IsActive()` しか見ない。
`SetAllEnemiesActive()` でも同様に基底フラグを落としている。

### エンカウントの後始末

`FieldEnemyManager::HandleBattleEnd()` は
`lastEncounterInfo_.encounteredEnemy`（**接触した個体のポインタ**）で対象を特定する。

グループ名（＝`enemyId`）で検索すると、同じIDのスポーン点が複数ある場合に
戦っていない個体がヒットし、戦った個体の `hasTriggeredEncounter_` が
true のまま残って二度とエンカウントできなくなる。

---

## 3. BattleEnemy

`BattleEnemy.h/.cpp`（175 / 478 行）

### 状態一覧

```
Idle ──(距離15以内)──> Approach ──(距離10以内)──> 攻撃
                                                    │
     ┌──────────────────────────────────────────────┘
     ↓
  間合い取り（Backstep / Strafe / Observe）
     ↓
  再交戦（Approach or 攻撃）

被弾 ──> Damage ──> 再交戦
盾でパリィ ──> Downed
連続被弾4回 ──> Recovery（無敵） ──> Counter
```

### 間合い取り（Spacing）

`States/Spacing/` の3状態。攻撃が終わると必ずここを通る。

| 状態 | 内容 |
|---|---|
| Backstep | プレイヤーを見たまま後退。序盤速く終わりに減速 |
| Strafe | 距離を保ったまま円周上を左右どちらかに回り込む |
| Observe | 動かずプレイヤーを見続ける。「間」を作る |

`SpacingSelector` が選択を担当。遷移は
`攻撃 → 間合い取り → 再交戦` の**一方向**（無限に間合いを取り続けない）。

### 知覚（Perception）

`AI/EnemyAIContext.h` が「敵がプレイヤーについて観測できること」のスナップショット。
判断のたびに `Capture()` で作り直す。

**プレイヤーの攻撃はオートホーミングで空振りしない**ため、
「振り終わりを狩る」は判断材料にしていない（表示用には残してある）。
代わりにホーミングでは埋まらない情報を使う。

| 隙とみなす条件 | 根拠 |
|---|---|
| こちらを見ていない | プレイヤー正面からの角度が閾値外（既定±70°） |
| 別の敵にロックオン中 | `PlayerCamera::GetLockedTarget()` が自分でない |
| CCが尽きている | `GetCurrentCC() <= 0` |
| のけぞり・スタン中 | `IsHit() \|\| IsStunned()` |

「危険」は `攻撃中 かつ こちらを向いている` のときだけ。
別の敵を斬っている最中は、この敵にとっては好機として扱う。

この構造体は段階7で BehaviorTree を入れるとき**そのまま Blackboard になる**。

### 攻撃権（Attack Token）

`AI/AttackTokenPool.h`。同時に攻撃してよい敵の数を制限する。

- 攻撃権を取れなかった敵は Strafe で周囲を回って待つ
- 攻撃の入口は3箇所（接近から / 間合い取りから / 隙を見つけて）あるが、
  **`SpacingSelector::AttackOrCircle()` に集約**して必ずここを通す
- 返却は `BattleEnemy::ChangeState` で一括。被弾・ダウン・死亡による中断でも漏れない
- 返却後にクールダウンを入れて同じ個体の独占を防ぐ

設定は `enemy_data.json` の `battleSettings`。

### 攻撃の能力フラグ

`IEnemyState` が状態の性質を公開する。
以前 `dynamic_cast` で具象型を判定していた箇所を置き換えたもの。

```cpp
virtual bool IsAttacking() const;
virtual bool IsContactDamageActive() const;
virtual int  GetContactDamageWindow() const;   // 多段ヒットの番号
virtual bool KeepsStateWhenDamaged() const;    // ダウン中は被弾で上書きしない
virtual bool CanBeParried() const;             // 盾で受けるとダウンするか
virtual const char* GetName() const;           // デバッグ表示
```

`CanBeParried()` は各攻撃データの `parriable` を返す。
どの攻撃を盾で止められるかは**JSONで決まる**。

### 被弾リアクション

`enemy_data.json` の `damageReaction` で調整。

硬直時間は**ノックバックが終わってから**数え始める。
実際に動けない時間は `ノックバック時間 + 硬直時間`。
`waitForKnockback` を false にすると被弾した瞬間から数える。

---

## 4. 攻撃システム（移行中）

ここが現在作業中の領域。**3世代が同居している。**

### 第1世代: ハードコード（現役）

`States/Attack/Battle*AttackState.{h,cpp}` の6クラス。
Rush / ChargeRush / Spin / Jump / Combo / Counter。

いま実際に動いているのはこれ。
各クラスが「予備動作 → 溜め → 本体 → 硬直」を時間で分岐している。

**問題**: 攻撃を1種類増やすのに10箇所触る必要がある
（enum、文字列変換2つ、重み用フィールド4つ、switch 4連、生成関数、新規クラス、パラメータ構造体）。

### 第2世代: フェーズ列（廃止予定）

`Attack/EnemyAttackAction.h` / `EnemyAttackDatabase.h` / `States/Attack/EnemyAttackState.h`
＋ `Resources/Json/BattleEnemies/attack_actions.json`

`AttackPhaseType` の enum（Anticipation / Charge / Dash / Spin / Leap / Wait / Scripted）で
攻撃をフェーズの並びとして表現する試み。エディタのトグルで切り替え可能な状態まで作った。

**廃止理由**: 動きの形が enum と C++ の更新関数に固定されており、
新しい動きを作るには結局コードを書く必要がある。第1世代と同じ構造の問題。

**→ 削除予定。**

### 第3世代: カーブ + モディファイア（構築中）

現在の設計。攻撃の種別という概念をコードから消す。

```
Attack/
  Data/
    AttackTracks.h/.cpp     … 位置/回転/スケールの9チャンネル（CurveChannel を使用）
    AttackModifier.h/.cpp   … 相手依存の5要素
    EnemyAttack.h           … 攻撃1つ分
    EnemyAttackIO.cpp       … JSON 入出力
  Runtime/
    AttackPlayer.h/.cpp     … 再生器（本番とプレビューが共用）
  Motion/                   … 制御点スプライン経路（位置の入力元として選択可）
  Projectile/               … 弾の定義・実体・管理
```

#### 中心となる考え方

攻撃の「形」は**位置・回転・スケールの9本のカーブ**で表現する。

| 作りたい動き | やること |
|---|---|
| 突進 | 位置Z を 0→8 |
| 跳躍 | 位置Y に山、位置Z を 0→8 |
| 回転攻撃 | 回転Y を 0→4π |
| 後退から突進 | 位置Z を 0→-1.5→8 |
| 回りながら跳んで縮む | 3つとも同時に引く |

チャンネルが独立しているので**組み合わせは自動で成立する**。
「突進」「跳躍」といった種別クラスは1つも要らない。

#### カーブとモディファイアの振り分け

判断基準は「相手の状態に依存するか」だけ。

| | 置き場所 | 例 |
|---|---|---|
| 依存しない（形が決まっている） | **カーブ** | 移動・回転・スケール |
| 依存する（実行時に決まる） | **モディファイア** | 向き追従・追尾・攻撃判定・無敵・弾 |

この基準がある限りモディファイアの種類は増え続けない
（相手依存の要素は本質的に数えるほどしかない）。

#### すべて相対量

位置・回転・スケールは**攻撃開始時の姿勢からの相対量**で持つ。

- どこに立っていても同じ攻撃が成立する
- プレビュー停止時の復帰が「基準姿勢を書き戻すだけ」
- 攻撃が中断されても元に戻せる

```cpp
void AttackPlayer::Stop(BaseEnemy& enemy) {
    enemy.SetTranslate(basePosition_);
    enemy.SetRotate(baseRotation_);
    enemy.SetScale(baseScale_);
}
```

#### 弾

`Projectile/` は経路の抽象（`IAttackMotion`）を敵本体と共用する。
スプラインを渡せば曲がる弾、円運動を渡せば渦を巻く弾になる。

弾のダメージは `Player::ApplyDamage()` を通すので、
**ガード／パリィがそのまま弾にも効く**。

---

## 5. データファイル

| ファイル | 内容 |
|---|---|
| `Resources/Json/BattleEnemies/enemy_data.json` | 敵ごとのHP・攻撃力・攻撃パラメータ・間合い取り・知覚・被弾リアクション ＋ `battleSettings`（攻撃権） |
| `Resources/Json/BattleEnemies/motion_paths.json` | 名前付きの経路（制御点スプライン） |
| `Resources/Json/BattleEnemies/attack_actions.json` | 第2世代のフェーズ列（**削除予定**） |
| `Resources/Json/Objects/BattleEnemies/<id>.json` | コライダーオフセット・ディゾルブ設定 |
| `Resources/Json/Player/guard_config.json` | ガード／パリィのタイムラインと結果 |

---

## 6. エディタ

すべて `#ifdef USE_IMGUI`（Debug のみ）。

| パネル | 場所 | 内容 |
|---|---|---|
| バトルモード:デバッグ情報 | `BattleEnemyEditorUI` | 敵データ全般、攻撃権、状態モニタ、知覚モニタ |
| 攻撃経路エディタ | `MotionPathEditor` | 制御点をギズモで置いて経路を作る。3Dプレビュー付き |
| ガード設定 | `GuardEditor` | ドープシートで発生/持続/パリィ/硬直を編集 |
| フィールドエネミーエディター | `FieldEnemyEditorUI` | スポーン点の配置とパラメータ |

### 状態モニタ

`logicalState_` は `Dead` にしか代入されておらず状態表示に使えなかったため、
`IEnemyState::GetName()` と `BattleEnemy` の遷移ログを追加した。

```
現在: Spacing:Strafe (0.83秒)
  12.4s  Spacing:Strafe    (前の状態 0.45秒)   ← 青
  11.9s  Spacing:Backstep  (前の状態 3.20秒)   ← 青
   8.7s  Attack:Rush       (前の状態 1.10秒)   ← 赤
```

赤（攻撃）と赤の間に青（間合い取り）が挟まっているかで、
間合い取りが機能しているか目視で確認できる。

---

## 7. 既知の課題

| 課題 | 内容 |
|---|---|
| **敵ごとの攻撃リストが無い** | 第3世代は全敵共通の攻撃プールになる想定。`enemy_data.json` の `attackPatterns`（敵ごとに違う攻撃セット）に相当するものが未設計。**このまま移行すると敵の個性が消える** |
| 第2世代の残骸 | `EnemyAttackAction` 系一式。削除予定 |
| `BattleEnemyState` enum が死んでいる | `logicalState_` は `Dead` にしか代入されない。`GetName()` に役目を譲ったので削除候補 |
| `DebugSpawnEnemy` が未使用 | エディタから外したので呼び出し元なし |
| マネージャの重複 | `BattleEnemyManager`（1223行）と `FieldEnemyManager`（1096行）の骨格が同じ。`EnemyDataTable` + `EnemyPool` への共通化が未着手 |
| `ProjectileManager` が未接続 | BattleScene への初期化・更新・描画の登録が未実施 |

---

## 8. 今後の予定

1. **第3世代の完成** — 攻撃の保管庫と選択 → 新ランナー → エディタ
2. **既存6攻撃の再現** — エディタで作って挙動を比較
3. **第1・第2世代の削除** — 2で一致を確認してから
4. **敵ごとの攻撃リスト** — 個性が消える問題への対応
5. ボス（フェーズ制）
6. BehaviorTree（行動選択層。実行層はFSMのまま）

BT の責務分界は「BT＝次に何をするか / FSM＝決めた行動をやり切る」。
被弾・死亡・フェーズ移行は BT を通さず FSM が直接割り込む。
