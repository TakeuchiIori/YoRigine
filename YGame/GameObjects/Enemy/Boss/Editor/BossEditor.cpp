#include "BossEditor.h"

#ifdef USE_IMGUI

#include "../../Attack/Runtime/EnemyAttackLibrary.h"
#include "../../BattleEnemy/BattleEnemyManager.h"
#include "../BossEnemy.h"
#include "Systems/GameTime/GameTime.h"
#include <Loaders/Json/JsonManager.h>

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace {

// 区切り見出し＋説明（ツールチップ）。ボスの調整値は項目が多いので、
// 何のための値かをその場で読めるようにしておく。
void HelpMarker(const char *text) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

bool Contains(const std::vector<std::string> &list, const std::string &value) {
  return std::find(list.begin(), list.end(), value) != list.end();
}

} // namespace

// ============================================================
// 描画
// ============================================================
void BossEditor::Draw() {
  BossEnemy *boss = FindBoss();

  // プレビュー中のボスが消えた（撃破・出し直し）なら、触らずに状態だけ落とす
  if (previewPlaying_ && previewTarget_ != boss) {
    previewPlaying_ = false;
    previewTarget_ = nullptr;
  }

  if (ImGui::BeginTabBar("BossEditorTabs")) {
    if (ImGui::BeginTabItem("ボス")) {
      DrawBossTab(boss);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("攻撃")) {
      DrawAttackTab(boss);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("出現演出")) {
      DrawAppearanceTab(boss);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

BossEnemy *BossEditor::FindBoss() const {
  if (!manager_)
    return nullptr;
  for (auto *enemy : manager_->GetActiveBattleEnemies()) {
    if (auto *boss = dynamic_cast<BossEnemy *>(enemy)) {
      return boss;
    }
  }
  return nullptr;
}

// ============================================================
// タブ: ボス
// ============================================================
void BossEditor::DrawBossTab(BossEnemy *boss) {
  // ── 出す ──
  if (hooks_.spawnBossNow && ImGui::Button("ボスを目の前に出す（演出なし）")) {
    StopPreview();
    hooks_.spawnBossNow();
    return; // 生成直後のボスは次のフレームから触る
  }
  ImGui::SameLine();
  if (hooks_.replayAppearance && ImGui::Button("出現演出で出す")) {
    StopPreview();
    hooks_.replayAppearance();
    return;
  }

  if (!boss) {
    ImGui::TextDisabled("ボスがいません。バトル中に上のボタンで出してください");
    return;
  }

  DrawStatus(*boss);

  ImGui::SeparatorText("調整値");
  DrawParams(*boss);
  DrawCollider(*boss);

  ImGui::Separator();
  if (ImGui::Button("ボスの調整値を保存")) {
    boss->SaveTuning();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Resources/Json/Objects/BattleEnemies/%s.json",
                      boss->GetEnemyData().enemyId.c_str());
}

void BossEditor::DrawStatus(BossEnemy &boss) {
  ImGui::SeparatorText("状態");
  ImGui::Text("状態: %s   フェーズ: %d   身長(カメラ用): %.1fm", boss.GetStateName(),
              boss.GetPhase() + 1, boss.GetStandingHeight());

  // HP。スライダーで直接いじれるようにしておくと、フェーズ移行の確認が速い
  int hp = boss.GetCurrentHP();
  if (ImGui::SliderInt("HP", &hp, 0, boss.GetMaxHP())) {
    const int diff = hp - boss.GetCurrentHP();
    if (diff > 0) {
      boss.Heal(diff);
    } else if (diff < 0) {
      const bool wasInvincible = boss.IsInvincible();
      boss.IsInvincible() = false; // 無敵中でも削れるように一瞬だけ外す
      boss.TakeDamage(-diff);
      boss.IsInvincible() = wasInvincible;
    }
  }

  const float poiseMax = std::max(boss.GetParams().poiseMax, 1.0f);
  char poiseLabel[64];
  std::snprintf(poiseLabel, sizeof(poiseLabel), "怯み値 %.0f / %.0f", boss.GetPoise(), poiseMax);
  ImGui::ProgressBar(boss.GetPoise() / poiseMax, ImVec2(-1.0f, 0.0f), poiseLabel);

  bool paused = boss.IsAIPaused();
  if (ImGui::Checkbox("AI を止める", &paused)) {
    boss.SetAIPaused(paused);
  }
  HelpMarker("状態遷移だけを止める。アニメと判定の表示は動き続ける。\n"
             "攻撃のプレビュー中は自動で止まる。");

  if (ImGui::Button("怯ませる")) boss.ForceStagger();
  ImGui::SameLine();
  if (ImGui::Button("フェーズ移行")) boss.ForcePhaseShift();
  ImGui::SameLine();
  if (ImGui::Button("全快")) boss.Heal(boss.GetMaxHP());
  ImGui::SameLine();
  if (ImGui::Button("撃破")) {
    boss.IsInvincible() = false;
    boss.TakeDamage(boss.GetCurrentHP());
  }
}

void BossEditor::DrawParams(BossEnemy &boss) {
  BossParams &p = boss.GetParamsMutable();
  const auto &clips = boss.GetAnimationClipNames();
  const std::vector<std::string> joints = boss.GetJointNames();


  if (ImGui::TreeNode("見た目")) {
    float scale = p.modelScale;
    if (ImGui::DragFloat("モデル倍率", &scale, 0.05f, 0.1f, 50.0f, "%.2f")) {
      boss.ApplyModelScale(scale);
    }
    HelpMarker("素材の大きさの補正。攻撃判定の半径はこの倍率では大きくならない（メートルで作る）。");
    ImGui::TreePop();
  }


  if (ImGui::TreeNode("クリップ")) {
    NameCombo("待機", p.idleClip, clips, false);
    NameCombo("歩き", p.walkClip, clips, false);
    NameCombo("走り", p.runClip, clips, false);
    NameCombo("登場", p.introClip, clips, false);
    NameCombo("フェーズ移行", p.phaseShiftClip, clips, false);
    NameCombo("死亡", p.deathClip, clips, false);
    NameCombo("ルートジョイント", p.rootJoint, joints, false);
    HelpMarker("攻撃アニメの前進を本体の移動に置き換えるジョイント（Mixamo 系なら Hips）。");
    ImGui::TreePop();
  }


  if (ImGui::TreeNode("移動と思考")) {
    ImGui::DragFloat("旋回速度(rad/s)", &p.turnSpeed, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("走り出す距離", &p.runDistance, 0.1f, 0.0f, 100.0f, "%.1fm");
    ImGui::DragFloat("走り速度倍率", &p.runSpeedMultiplier, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("思考時間 最短", &p.thinkTimeMin, 0.05f, 0.0f, 10.0f, "%.2f秒");
    ImGui::DragFloat("思考時間 最長", &p.thinkTimeMax, 0.05f, 0.0f, 10.0f, "%.2f秒");
    ImGui::DragFloat("追跡を諦める秒数", &p.chaseGiveUpTime, 0.1f, 0.5f, 30.0f, "%.1f秒");
    ImGui::TreePop();
  }


  if (ImGui::TreeNode("怯みとフェーズ")) {
    ImGui::DragFloat("怯み値 上限", &p.poiseMax, 1.0f, 1.0f, 9999.0f);
    HelpMarker("受けたダメージを溜め、これを超えたら怯む。毎ヒットでは怯まない。");
    ImGui::DragFloat("怯み値 回復/秒", &p.poiseRecoverPerSec, 0.5f, 0.0f, 999.0f);
    ImGui::DragFloat("怯み時間", &p.staggerDuration, 0.05f, 0.0f, 10.0f, "%.2f秒");
    ImGui::DragFloat("パリィ時の怯み時間", &p.parryStaggerDuration, 0.05f, 0.0f, 10.0f, "%.2f秒");
    ImGui::SliderFloat("第2フェーズ HP割合", &p.phase2HpRatio, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("第3フェーズ HP割合", &p.phase3HpRatio, 0.0f, 1.0f, "%.2f");
    HelpMarker("HP 割合がこれを下回った瞬間に PowerUp 演出を挟む。0 で無効。\n"
               "フェーズごとの技の出し分けは、攻撃タブの「自分のHPがこれ以下で使う」で行う。");
    ImGui::Checkbox("登場時に咆哮する", &p.playIntro);
    ImGui::TreePop();
  }


  if (ImGui::TreeNode("カメラと当たりの基準")) {
    NameCombo("カメラが見る胸のジョイント", p.cameraFocusJoint, joints, false);
    NameCombo("身長を測る頭のジョイント", p.cameraTopJoint, joints, false);
    ImGui::DragFloat("プレイヤーの半径", &p.playerHitRadius, 0.01f, 0.05f, 5.0f, "%.2fm");
    ImGui::DragFloat("プレイヤーの高さ", &p.playerHitHeight, 0.01f, 0.1f, 10.0f, "%.2fm");
    HelpMarker("ボスの攻撃判定は、プレイヤーをこの大きさの縦長カプセルとして当てる。");
    ImGui::TreePop();
  }
}

void BossEditor::DrawCollider(BossEnemy &boss) {
  OBBCollider *obb = boss.GetOBBCollider();
  if (!obb || !ImGui::TreeNode("本体の当たり判定（被弾・押し合い）"))
    return;

  // obbOffset_ はモデルのローカル単位で、ボスのスケールが掛かった大きさになる
  ImGui::DragFloat3("中心", &obb->obbOffset_.center.x, 0.01f);
  ImGui::DragFloat3("大きさ", &obb->obbOffset_.size.x, 0.01f, 0.01f, 100.0f);
  ImGui::DragFloat3("回転(度)", &obb->obbEulerOffset_.x, 0.5f);
  const Vector3 scale = boss.GetScale();
  ImGui::TextDisabled("実寸: %.1f x %.1f x %.1f m（ボスのスケール %.1f を掛けた大きさ）",
                      obb->obbOffset_.size.x * scale.x, obb->obbOffset_.size.y * scale.y,
                      obb->obbOffset_.size.z * scale.z, scale.y);
  ImGui::TextDisabled("剣・魔法が当たる範囲で、プレイヤーとの押し合いにも使われる");
  ImGui::TreePop();
}

// ============================================================
// タブ: 攻撃
// ============================================================
void BossEditor::DrawAttackTab(BossEnemy *boss) {
  if (!boss) {
    ImGui::TextDisabled("ボスがいません。「ボス」タブから出してください");
    return;
  }

  // 判定の付け先の候補を毎回モデルから取る（モデルを差し替えても追従する）
  jointNames_ = boss->GetJointNames();
  std::sort(jointNames_.begin(), jointNames_.end());

  DrawAttackList(*boss);

  EnemyAttack *attack = FindSelectedAttack();
  if (!attack) {
    ImGui::TextDisabled("上の一覧から技を選んでください");
    return;
  }

  DrawAttackPreview(*boss, *attack);
  DrawAttackSettings(*boss, *attack);
}

void BossEditor::DrawAttackList(BossEnemy &boss) {
  auto &library = EnemyAttackLibrary::GetInstance();
  const auto &used = boss.GetEnemyData().attackIds;

  ImGui::SeparatorText("このボスの技");
  if (ImGui::BeginListBox("##bossAttacks", ImVec2(-1.0f, 6.0f * ImGui::GetTextLineHeightWithSpacing()))) {
    for (const auto &attack : library.GetAll()) {
      if (!Contains(used, attack.id))
        continue;
      char label[160];
      std::snprintf(label, sizeof(label), "%s  [%s / %.1f秒 / %.0f〜%.0fm]", attack.id.c_str(),
                    attack.animation.HasClip() ? attack.animation.clip.c_str() : "クリップなし",
                    attack.duration, attack.minRange, attack.maxRange);
      if (ImGui::Selectable(label, selectedAttackId_ == attack.id)) {
        if (selectedAttackId_ != attack.id)
          StopPreview();
        selectedAttackId_ = attack.id;
      }
    }
    ImGui::EndListBox();
  }

  // 追加・外す
  if (!selectedAttackId_.empty() && Contains(used, selectedAttackId_)) {
    if (ImGui::Button("選択中の技をボスから外す")) {
      StopPreview();
      SetAttackUsed(boss, selectedAttackId_, false);
    }
    ImGui::SameLine();
  }
  if (ImGui::BeginCombo("##addAttack", "既存の技を追加...")) {
    for (const auto &attack : library.GetAll()) {
      if (Contains(used, attack.id))
        continue;
      // 骨格アニメ前提の技を先に、雑魚用（カーブだけの技）は薄く表示
      if (!attack.RequiresSkeleton())
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      if (ImGui::Selectable(attack.id.c_str())) {
        SetAttackUsed(boss, attack.id, true);
        selectedAttackId_ = attack.id;
      }
      if (!attack.RequiresSkeleton())
        ImGui::PopStyleColor();
    }
    ImGui::EndCombo();
  }

  ImGui::SetNextItemWidth(160.0f);
  ImGui::InputText("##newAttackName", newAttackName_, sizeof(newAttackName_));
  ImGui::SameLine();
  if (ImGui::Button("新しい技を作る")) {
    CreateBossAttack(boss);
  }

  if (ImGui::Button("技と技リストを保存")) {
    library.Save();
    if (manager_)
      manager_->SaveEnemyDataToDefault();
  }
  HelpMarker("技の中身は enemy_attacks.json、ボスが使う技の一覧は enemy_data.json に保存する。");
}

EnemyAttack *BossEditor::FindSelectedAttack() {
  if (selectedAttackId_.empty())
    return nullptr;
  for (auto &attack : EnemyAttackLibrary::GetInstance().GetAll()) {
    if (attack.id == selectedAttackId_)
      return &attack;
  }
  return nullptr;
}

void BossEditor::SetAttackUsed(BossEnemy &boss, const std::string &id, bool used) {
  auto apply = [&](std::vector<std::string> &ids) {
    auto it = std::find(ids.begin(), ids.end(), id);
    if (used && it == ids.end())
      ids.push_back(id);
    if (!used && it != ids.end())
      ids.erase(it);
  };
  // 生成中の個体（すぐ効く）と、敵データ表（保存して次回以降も効く）の両方
  apply(boss.GetEnemyData().attackIds);
  if (manager_) {
    if (auto *data = manager_->FindEnemyData(boss.GetEnemyData().enemyId)) {
      apply(data->attackIds);
    }
  }
}

void BossEditor::CreateBossAttack(BossEnemy &boss) {
  auto &library = EnemyAttackLibrary::GetInstance();
  const int index = library.Add(newAttackName_);
  auto &attack = library.GetAll()[index];

  // 保管庫の既定は「前へ出るカーブ＋本体の判定」の雑魚向けなので、ボス向けに作り直す
  attack.tracks.Clear();
  attack.modifiers.clear();
  const auto &clips = boss.GetAnimationClipNames();
  attack.animation.clip = clips.empty() ? std::string() : boss.GetParams().idleClip;
  attack.animation.speed = 1.0f;
  attack.duration = 1.5f;
  attack.maxRange = 12.0f;

  AttackModifier hitbox;
  hitbox.type = AttackModifierType::Hitbox;
  hitbox.startTime = 0.5f;
  hitbox.endTime = 0.8f;
  hitbox.joint = Contains(jointNames_, "RightHand") ? "RightHand" : std::string();
  hitbox.radius = 1.3f;
  attack.modifiers.push_back(hitbox);

  SetAttackUsed(boss, attack.id, true);
  selectedAttackId_ = attack.id;
}

void BossEditor::DrawAttackSettings(BossEnemy &boss, EnemyAttack &attack) {
  ImGui::SeparatorText("アニメーション");
  NameCombo("クリップ", attack.animation.clip, boss.GetAnimationClipNames(), true);
  ImGui::DragFloat("再生速度", &attack.animation.speed, 0.01f, 0.1f, 5.0f, "%.2f倍");
  ImGui::DragFloat("攻撃時間", &attack.duration, 0.01f, 0.1f, 20.0f, "%.2f秒");
  const float clipDuration = previewPlaying_ ? preview_.GetClipPlayDuration() : 0.0f;
  if (clipDuration > 0.0f) {
    ImGui::SameLine();
    char label[64];
    std::snprintf(label, sizeof(label), "クリップ長に合わせる(%.2f秒)", clipDuration);
    if (ImGui::SmallButton(label)) {
      attack.duration = clipDuration;
    }
  }

  ImGui::SeparatorText("いつ出すか");
  ImGui::DragFloat("最短射程", &attack.minRange, 0.1f, 0.0f, 200.0f, "%.1fm");
  ImGui::DragFloat("最長射程", &attack.maxRange, 0.1f, 0.0f, 999.0f, "%.1fm");
  HelpMarker("ボスの中心からプレイヤーまでの距離。巨体だと押し合いで近づけないので、\n"
             "近接技でも最長射程は体の大きさ分だけ広く取る。");
  ImGui::DragFloat("抽選の重み", &attack.weight, 0.05f, 0.0f, 10.0f, "%.2f");
  ImGui::DragFloat("クールダウン", &attack.cooldown, 0.1f, 0.0f, 60.0f, "%.1f秒");
  ImGui::SliderFloat("自分のHPがこれ以下で使う", &attack.selfHpBelow, 0.0f, 1.0f, "%.2f");
  HelpMarker("1.0 なら常に使う。0.6 にすると第2フェーズ（HP60%以下）からの技になる。");
  ImGui::SliderFloat("相手のHPがこれ以下で使う", &attack.targetHpBelow, 0.0f, 1.0f, "%.2f");
  ImGui::Checkbox("盾で崩せる", &attack.parriable);
  ImGui::SameLine();
  ImGui::Checkbox("発生が速い", &attack.fast);

  ImGui::SeparatorText("判定とタイミング");
  ImGui::TextDisabled("攻撃判定を選ぶと、ジョイント・半径・区間を編集できます");
  modifierPanel_.SetJointNames(&jointNames_);
  modifierPanel_.SetPlayheadTime(previewPlaying_ ? preview_.GetTime() : -1.0f);
  modifierPanel_.Draw(attack);
}

void BossEditor::DrawAttackPreview(BossEnemy &boss, EnemyAttack &attack) {
  ImGui::SeparatorText("プレビュー");

  if (!previewPlaying_) {
    if (ImGui::Button("その場で再生")) {
      // AI を止めてから再生する。止めないと次のフレームで AI が別の行動を始め、
      // 姿勢を上書きしてしまう。攻撃中だったなら待機へ戻してから。
      // 本番の攻撃が途中で止まったまま残ると、その判定が生き続けてしまう
      if (boss.GetState() == BossState::Attack) {
        boss.ChangeState(BossState::Idle);
      }
      aiWasPaused_ = boss.IsAIPaused();
      boss.SetAIPaused(true);
      preview_.SetDriveMotionClock(true);
      preview_.Play(boss, &attack);
      boss.SetPreviewAttack(&preview_);
      previewPlaying_ = true;
      previewTarget_ = &boss;
    }
  } else if (ImGui::Button("停止")) {
    StopPreview();
  }
  ImGui::SameLine();
  ImGui::Checkbox("繰り返す", &previewLoop_);
  ImGui::SameLine();
  if (ImGui::Button("実戦で出させる")) {
    StopPreview();
    boss.ForceAttack(&attack);
  }
  HelpMarker("その場で再生 … AI を止めて動きと判定の球だけを見る（ダメージなし・その場から動かない）\n"
             "実戦で出させる … AI に今すぐこの技を出させる（前進・ダメージ・パリィも本番どおり）");

  if (!previewPlaying_)
    return;

  float time = preview_.GetTime();
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::SliderFloat("##scrub", &time, 0.0f, attack.duration, "%.2f秒")) {
    preview_.Seek(boss, time);
  }
  ImGui::TextDisabled("赤い球 = 今有効な攻撃判定（当たり判定の表示を ON にすると見えます）");

  // ゲームを止めていても編集できるよう実時間で進める
  preview_.Update(boss, YoRigine::GameTime::GetUnscaledDeltaTime());
  if (preview_.IsFinished()) {
    if (previewLoop_) {
      preview_.Seek(boss, 0.0f);
    } else {
      StopPreview();
    }
  }
}

void BossEditor::StopPreview() {
  if (!previewPlaying_)
    return;
  if (previewTarget_ && previewTarget_ == FindBoss()) {
    preview_.Stop(*previewTarget_);
    previewTarget_->SetPreviewAttack(nullptr);
    previewTarget_->ReturnToIdlePose();
    previewTarget_->SetAIPaused(aiWasPaused_);
  }
  previewPlaying_ = false;
  previewTarget_ = nullptr;
}

// ============================================================
// タブ: 出現演出
// ============================================================
void BossEditor::DrawAppearanceTab([[maybe_unused]] BossEnemy *boss) {
  BossAppearanceSettings *s = hooks_.appearance;
  if (!s) {
    ImGui::TextDisabled("出現演出の設定がありません");
    return;
  }

  // ── 出すボス ──
  char idBuffer[64];
  std::snprintf(idBuffer, sizeof(idBuffer), "%s", s->bossEnemyId.c_str());
  if (ImGui::InputText("出現させる敵ID", idBuffer, sizeof(idBuffer))) {
    s->bossEnemyId = idBuffer;
  }
  if (s->bossEnemyId.empty()) {
    ImGui::TextDisabled("空なので、最終戦の取り巻きを倒すとそのままクリアへ進みます");
  } else if (manager_ && !manager_->HasEnemyData(s->bossEnemyId)) {
    ImGui::TextColored({1.0f, 0.5f, 0.4f, 1.0f}, "敵データにこの ID がありません（ボス戦が飛ばされます）");
  }
  ImGui::DragFloat("出現距離", &s->spawnDistance, 0.5f, 5.0f, 200.0f, "%.1fm");
  HelpMarker("プレイヤーから、今カメラが向いている方向へこの距離だけ先に出す。");

  // ── 構図 ──
  ImGui::SeparatorText("構図");
  ImGui::DragFloat("プレイヤーの立ち位置", &s->playerStandDistance, 0.5f, 0.0f, 200.0f, "手前 %.1fm");
  HelpMarker("演出中、プレイヤーは操作を受け付けずに出現位置へ向かって歩き、\n"
             "この距離だけ手前で立ち止まってボスの方を向く。\n"
             "出現距離以上にすると歩かずにその場で向きだけ変える。");
  ImGui::SliderFloat("カメラの肩ずらし", &s->cameraShoulderYaw, -1.0f, 1.0f, "%.2f rad");
  HelpMarker("プレイヤー→ボスの軸から、カメラを横へずらす角度。\n"
             "0 だと真後ろからになり、プレイヤーの背中がボスを隠す。符号で左右の肩を選ぶ。");
  ImGui::SliderFloat("カメラの見下ろし", &s->cameraPitch, -0.3f, 0.8f, "%.2f rad");
  HelpMarker("小さいほどカメラが低くなり、ボスを見上げる煽りの構図になる。");
  ImGui::DragFloat("出現前の注視の高さ", &s->cameraLookHeight, 0.1f, 0.0f, 30.0f, "%.1fm");
  HelpMarker("ボスが生成されるまでは、出現位置のこの高さを画面中央に置いておく。\n"
             "生成後はボスの胸へ滑らかに移る。");
  ImGui::DragFloat("カメラの寄る速さ", &s->cameraBlendSpeed, 0.05f, 0.1f, 20.0f, "%.2f");

  // ── タイムライン ──
  ImGui::SeparatorText("タイムライン");
  {
    // 区間を帯で並べて、重なり（霧が濃いうちに実体化が始まるか等）を一目で見せる
    const float width = ImGui::GetContentRegionAvail().x;
    const float rowH = ImGui::GetTextLineHeight() + 4.0f;
    const float total = std::max(s->totalDuration, 0.1f);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    struct Row { const char *label; float start; float end; ImU32 color; };
    const Row rows[] = {
        {"霧が覆う", 0.0f, s->fogInDuration, IM_COL32(110, 70, 160, 255)},
        {"実体化", s->spawnTime, s->spawnTime + s->emergenceDuration, IM_COL32(200, 90, 230, 255)},
        {"霧が晴れる", s->fogLiftStart, s->fogLiftStart + s->fogLiftDuration, IM_COL32(120, 150, 200, 255)},
        {"操作停止", 0.0f, s->totalDuration, IM_COL32(90, 90, 90, 255)},
    };
    const float labelW = 80.0f;
    const float barW = std::max(width - labelW, 50.0f);
    for (int i = 0; i < static_cast<int>(std::size(rows)); ++i) {
      const float y = origin.y + i * rowH;
      dl->AddText({origin.x, y}, IM_COL32(220, 220, 220, 255), rows[i].label);
      const float x0 = origin.x + labelW + barW * std::clamp(rows[i].start / total, 0.0f, 1.0f);
      const float x1 = origin.x + labelW + barW * std::clamp(rows[i].end / total, 0.0f, 1.0f);
      dl->AddRectFilled({x0, y + 2.0f}, {std::max(x1, x0 + 2.0f), y + rowH - 2.0f}, rows[i].color, 3.0f);
    }
    ImGui::Dummy({width, rowH * std::size(rows)});
    ImGui::TextDisabled("0秒 ─ %.1f秒（ボスの生成は %.1f秒）", total, s->spawnTime);
  }
  ImGui::DragFloat("霧が覆うまで", &s->fogInDuration, 0.05f, 0.0f, 20.0f, "%.2f秒");
  ImGui::DragFloat("ボス生成時刻", &s->spawnTime, 0.05f, 0.0f, 20.0f, "%.2f秒");
  ImGui::DragFloat("実体化の時間", &s->emergenceDuration, 0.05f, 0.0f, 20.0f, "%.2f秒");
  ImGui::DragFloat("霧が晴れ始める時刻", &s->fogLiftStart, 0.05f, 0.0f, 30.0f, "%.2f秒");
  ImGui::DragFloat("霧が晴れる時間", &s->fogLiftDuration, 0.05f, 0.0f, 20.0f, "%.2f秒");
  ImGui::DragFloat("演出全体", &s->totalDuration, 0.05f, 0.5f, 60.0f, "%.2f秒");
  HelpMarker("この時間が経つと黒帯が引いて操作が戻る。ボスの咆哮が終わる頃に合わせる。");

  // ── 霧 ──
  ImGui::SeparatorText("霧");
  bool fogChanged = false;
  if (ImGui::TreeNodeEx("最も濃い霧", ImGuiTreeNodeFlags_DefaultOpen)) {
    fogChanged |= ImGui::ColorEdit3("色##dark", &s->darkFogColor.x);
    fogChanged |= ImGui::DragFloat("濃度##dark", &s->darkFogDensity, 0.001f, 0.0f, 1.0f, "%.3f");
    fogChanged |= ImGui::DragFloat("開始距離##dark", &s->darkFogStart, 0.1f, 0.0f, 500.0f, "%.1f");
    fogChanged |= ImGui::DragFloat("高さフォグ##dark", &s->darkHeightFogDensity, 0.05f, 0.0f, 20.0f);
    fogChanged |= ImGui::SliderFloat("空のフォグ上限##dark", &s->darkSkyFogClamp, 0.0f, 1.0f);
    ImGui::TreePop();
  }
  if (ImGui::TreeNodeEx("ボス戦中の霧", ImGuiTreeNodeFlags_DefaultOpen)) {
    fogChanged |= ImGui::ColorEdit3("色##battle", &s->battleFogColor.x);
    fogChanged |= ImGui::DragFloat("濃度##battle", &s->battleFogDensity, 0.001f, 0.0f, 1.0f, "%.3f");
    fogChanged |= ImGui::DragFloat("開始距離##battle", &s->battleFogStart, 0.1f, 0.0f, 500.0f, "%.1f");
    fogChanged |= ImGui::DragFloat("高さフォグ##battle", &s->battleHeightFogDensity, 0.05f, 0.0f, 20.0f);
    fogChanged |= ImGui::SliderFloat("空のフォグ上限##battle", &s->battleSkyFogClamp, 0.0f, 1.0f);
    ImGui::TreePop();
  }

  // 霧だけのプレビュー。演出を毎回通しで流さなくても色と濃さを詰められる
  if (hooks_.previewFog) {
    bool sliderMoved = ImGui::SliderFloat("霧のプレビュー", &fogPreviewT_, 0.0f, 2.0f, "%.2f");
    HelpMarker("0 = 演出前の霧 / 1 = 最も濃い霧 / 2 = ボス戦中の霧。\n動かすと画面の霧が即座に変わる。");
    if (sliderMoved || (fogPreviewActive_ && fogChanged)) {
      hooks_.previewFog(fogPreviewT_);
      fogPreviewActive_ = true;
    }
    if (fogPreviewActive_) {
      ImGui::SameLine();
      if (ImGui::Button("霧を元に戻す") && hooks_.endFogPreview) {
        hooks_.endFogPreview();
        fogPreviewActive_ = false;
      }
    }
  }

  ImGui::Separator();
  if (hooks_.replayAppearance && ImGui::Button("演出を通しで再生")) {
    StopPreview();
    fogPreviewActive_ = false; // 再生側で霧を退避し直すので、プレビュー状態は捨てる
    hooks_.replayAppearance();
  }
  ImGui::SameLine();
  if (hooks_.appearanceJson && ImGui::Button("出現演出を保存")) {
    hooks_.appearanceJson->Save();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Resources/Json/Scenes/BossAppearance.json");
}

// ============================================================
// 名前のプルダウン
// ============================================================
bool BossEditor::NameCombo(const char *label, std::string &value,
                           const std::vector<std::string> &names, bool allowEmpty) {
  // 候補が取れない（モデル未読込など）時は手入力に落とす
  if (names.empty()) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    if (ImGui::InputText(label, buffer, sizeof(buffer))) {
      value = buffer;
      return true;
    }
    return false;
  }

  bool changed = false;
  const bool missing = !value.empty() && !Contains(names, value);
  const char *preview = value.empty() ? "(なし)" : value.c_str();
  if (missing)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.4f, 1.0f)); // モデルに無い名前は赤
  if (ImGui::BeginCombo(label, preview)) {
    if (missing)
      ImGui::PopStyleColor();
    if (allowEmpty && ImGui::Selectable("(なし)", value.empty())) {
      value.clear();
      changed = true;
    }
    for (const auto &name : names) {
      if (ImGui::Selectable(name.c_str(), value == name)) {
        value = name;
        changed = true;
      }
    }
    ImGui::EndCombo();
  } else if (missing) {
    ImGui::PopStyleColor();
  }
  return changed;
}

#endif // USE_IMGUI
