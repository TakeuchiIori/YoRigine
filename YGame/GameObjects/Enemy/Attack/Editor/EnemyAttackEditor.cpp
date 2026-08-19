#include "EnemyAttackEditor.h"

#ifdef USE_IMGUI

#include "../../BattleEnemy/BattleEnemy.h"
#include "../../BattleEnemy/BattleEnemyManager.h"
#include "Systems/GameTime/GameTime.h"

#include <algorithm>
#include <cstdio>

EnemyAttack *EnemyAttackEditor::GetSelectedAttack() {
	auto &attacks = EnemyAttackLibrary::GetInstance().GetAll();
	if (selectedAttack_ < 0 || selectedAttack_ >= static_cast<int>(attacks.size())) {
		return nullptr;
	}
	return &attacks[selectedAttack_];
}

// ============================================================
// プレビュー対象の敵
// ============================================================
BattleEnemy *EnemyAttackEditor::GetPreviewEnemy() const {
	if (!manager_) return nullptr;

	auto enemies = manager_->GetActiveBattleEnemies();
	if (enemies.empty()) return nullptr;

	const int index = std::clamp(previewEnemyIndex_, 0, static_cast<int>(enemies.size()) - 1);
	return enemies[index];
}

// ============================================================
// プレビュー停止
//
// 姿勢を再生前へ戻す。対象が既に消えている場合は
// 書き戻す先が無いので状態だけ落とす。
// ============================================================
void EnemyAttackEditor::StopPreview() {
	if (!previewPlaying_) return;

	if (previewTarget_) {
		auto enemies = manager_ ? manager_->GetActiveBattleEnemies()
		                        : std::vector<BattleEnemy *>{};
		const bool stillAlive =
			std::find(enemies.begin(), enemies.end(), previewTarget_) != enemies.end();

		if (stillAlive) {
			preview_.Stop(*previewTarget_);
		}
	}

	previewPlaying_ = false;
	previewTarget_ = nullptr;
}

// ============================================================
// 攻撃一覧
// ============================================================
void EnemyAttackEditor::DrawAttackList() {
	auto &library = EnemyAttackLibrary::GetInstance();
	auto &attacks = library.GetAll();

	ImGui::SeparatorText("攻撃一覧");

	if (ImGui::BeginChild("attackList", ImVec2(0, 110), ImGuiChildFlags_Borders)) {
		for (int i = 0; i < static_cast<int>(attacks.size()); ++i) {
			char label[192];
			std::snprintf(label, sizeof(label), "%s  [%.2f秒]##atk%d",
			              attacks[i].displayName.c_str(), attacks[i].duration, i);

			if (ImGui::Selectable(label, selectedAttack_ == i)) {
				StopPreview();
				selectedAttack_ = i;
			}
		}
	}
	ImGui::EndChild();

	if (ImGui::Button("新規作成")) {
		StopPreview();
		selectedAttack_ = library.Add("attack");
	}
	ImGui::SameLine();
	if (ImGui::Button("複製") && selectedAttack_ >= 0) {
		StopPreview();
		selectedAttack_ = library.Duplicate(selectedAttack_);
	}
	ImGui::SameLine();
	if (ImGui::Button("削除") && selectedAttack_ >= 0) {
		StopPreview();
		library.Remove(selectedAttack_);
		selectedAttack_ = -1;
	}
	ImGui::SameLine();
	if (ImGui::Button("JSONに保存")) {
		library.Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("再読み込み")) {
		StopPreview();
		library.Load();
		selectedAttack_ = -1;
	}
}

// ============================================================
// 基本設定
// ============================================================
void EnemyAttackEditor::DrawBasicSettings(EnemyAttack &attack) {
	ImGui::SeparatorText("基本設定");

	std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", attack.id.c_str());
	if (ImGui::InputText("ID", nameBuffer_, sizeof(nameBuffer_))) {
		attack.id = nameBuffer_;
		attack.displayName = attack.id;
	}
	ImGui::TextDisabled("敵データの attackIds にこのIDを書くと、その敵が使えるようになります");

	ImGui::DragFloat("攻撃時間", &attack.duration, 0.05f, 0.1f, 10.0f, "%.2f秒");
	ImGui::TextDisabled("カーブの横軸 0〜1 がこの時間に対応します");

	ImGui::DragFloat("最短射程", &attack.minRange, 0.1f, 0.0f, 100.0f, "%.1fm");
	ImGui::DragFloat("最長射程", &attack.maxRange, 0.1f, 0.0f, 999.0f, "%.1fm");
	ImGui::DragFloat("抽選の重み", &attack.weight, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("0にすると抽選に出なくなる。\nカウンターのようにID指定で出す技に使う。");
		ImGui::EndTooltip();
	}

	ImGui::Checkbox("盾で止められる", &attack.parriable);
	ImGui::SameLine();
	ImGui::Checkbox("発生が速い技", &attack.fast);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::TextUnformatted("相手が隙を晒したときに優先される。\n溜めの長い技はOFFにする。");
		ImGui::EndTooltip();
	}

	// 位置の入力元
	int sourceIndex = static_cast<int>(attack.positionSource);
	const char *sources[] = {"カーブで作る", "経路ライブラリを使う"};
	if (ImGui::Combo("位置の作り方", &sourceIndex, sources, IM_ARRAYSIZE(sources))) {
		attack.positionSource = static_cast<AttackPositionSource>(sourceIndex);
	}
	if (attack.positionSource == AttackPositionSource::Path) {
		char pathBuffer[128];
		std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", attack.pathName.c_str());
		if (ImGui::InputText("経路名", pathBuffer, sizeof(pathBuffer))) {
			attack.pathName = pathBuffer;
		}
		ImGui::TextDisabled("攻撃経路エディタで作った経路の名前を指定します");
	}

	if (!attack.HasHitbox()) {
		ImGui::TextColored({1.0f, 0.5f, 0.4f, 1.0f}, "攻撃判定がありません（当たりません）");
	}
}

// ============================================================
// プレビュー操作
// ============================================================
void EnemyAttackEditor::DrawPreviewControls(EnemyAttack &attack) {
	ImGui::SeparatorText("プレビュー");

	BattleEnemy *enemy = GetPreviewEnemy();
	if (!enemy) {
		ImGui::TextDisabled("戦闘中の敵がいません。バトルを開始してください");
		previewPlaying_ = false;
		previewTarget_ = nullptr;
		return;
	}

	auto enemies = manager_->GetActiveBattleEnemies();
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::SliderInt("対象の敵", &previewEnemyIndex_, 0,
	                     static_cast<int>(enemies.size()) - 1)) {
		StopPreview();
	}

	// 再生／停止
	if (!previewPlaying_) {
		if (ImGui::Button("再生")) {
			preview_.Play(*enemy, &attack);
			previewPlaying_ = true;
			previewTarget_ = enemy;
		}
	} else {
		if (ImGui::Button("停止（元の位置へ戻る）")) {
			StopPreview();
		}
	}

	ImGui::SameLine();
	ImGui::Checkbox("繰り返す", &previewLoop_);

	// 再生位置のスクラブ
	if (previewPlaying_ && previewTarget_) {
		float time = preview_.GetTime();
		ImGui::SetNextItemWidth(240.0f);
		if (ImGui::SliderFloat("再生位置", &time, 0.0f, attack.duration, "%.2f秒")) {
			preview_.Seek(*previewTarget_, time);
		}

		ImGui::SameLine();
		ImGui::TextDisabled("判定 %d / %s", preview_.GetActiveDamageWindow(),
		                    preview_.IsInvincibleNow() ? "無敵" : "通常");
	}

	// 時間を進める。ゲームが止まっていても編集したいので実時間を使う。
	if (previewPlaying_ && previewTarget_) {
		preview_.Update(*previewTarget_, YoRigine::GameTime::GetUnscaledDeltaTime());

		if (preview_.IsFinished()) {
			if (previewLoop_) {
				preview_.Seek(*previewTarget_, 0.0f);
			} else {
				StopPreview();
			}
		}
	}
}

// ============================================================
// 描画
// ============================================================
void EnemyAttackEditor::Draw() {
	// 初回だけ読み込む。マネージャ側でも読むが、
	// バトル外でエディタを開いた場合はこちらが効く。
	if (!loadedOnce_) {
		if (EnemyAttackLibrary::GetInstance().GetAll().empty()) {
			EnemyAttackLibrary::GetInstance().Load();
		}
		loadedOnce_ = true;
	}

	ImGui::Checkbox("カーブ定義の攻撃を使う", EnemyAttackLibrary::GetEnabledPtr());
	ImGui::SameLine();
	ImGui::TextDisabled("OFFなら従来のハードコード攻撃で動きます");

	DrawAttackList();

	EnemyAttack *attack = GetSelectedAttack();
	if (!attack) {
		ImGui::TextDisabled("攻撃を選択してください");
		return;
	}

	DrawBasicSettings(*attack);
	DrawPreviewControls(*attack);

	// 再生ヘッドを両パネルへ渡して、時間軸を揃えて見られるようにする
	const float normalized = previewPlaying_ ? preview_.GetNormalizedTime() : -1.0f;
	const float seconds = previewPlaying_ ? preview_.GetTime() : -1.0f;
	curvePanel_.SetPlayhead(normalized);
	modifierPanel_.SetPlayheadTime(seconds);

	ImGui::SeparatorText("動き（カーブ）");
	curvePanel_.Draw(*attack);

	ImGui::SeparatorText("タイミング（モディファイア）");
	modifierPanel_.Draw(*attack);
}

#endif // USE_IMGUI
