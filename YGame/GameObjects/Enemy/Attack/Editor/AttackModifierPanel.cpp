#include "AttackModifierPanel.h"

#ifdef USE_IMGUI

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace {

// 種別ごとの表示色。役割が一目で分かるように分ける。
DopeSheet::Color GetModifierColor(AttackModifierType type) {
	switch (type) {
	case AttackModifierType::Hitbox:       return DopeSheet::Color::Red();
	case AttackModifierType::Invincible:   return DopeSheet::Color::Cyan();
	case AttackModifierType::FaceTarget:   return DopeSheet::Color::Green();
	case AttackModifierType::HomingOffset: return DopeSheet::Color::Orange();
	default:                               return DopeSheet::Color::Purple();
	}
}

const char *GetModifierLabel(AttackModifierType type) {
	switch (type) {
	case AttackModifierType::Hitbox:       return "攻撃判定";
	case AttackModifierType::Invincible:   return "無敵";
	case AttackModifierType::FaceTarget:   return "相手を向く";
	case AttackModifierType::HomingOffset: return "追尾";
	default:                               return "弾を撃つ";
	}
}

// トラックの並び順。重要度が高いものを上に置く。
constexpr AttackModifierType kTrackOrder[] = {
	AttackModifierType::Hitbox,
	AttackModifierType::Invincible,
	AttackModifierType::FaceTarget,
	AttackModifierType::HomingOffset,
	AttackModifierType::EmitProjectile,
};
constexpr int kTrackCount = static_cast<int>(std::size(kTrackOrder));

} // namespace

int AttackModifierPanel::ToFrames(float seconds) const {
	return static_cast<int>(std::lround(seconds * static_cast<float>(editorFps_)));
}

float AttackModifierPanel::ToSeconds(int frames) const {
	return static_cast<float>(frames) / static_cast<float>(std::max(1, editorFps_));
}

// ============================================================
// トラックの構築
//
// 種別ごとに1トラック。同じ種別が複数あっても同じ行に並ぶので、
// 多段攻撃の判定の前後関係がそのまま見える。
// ============================================================
void AttackModifierPanel::BuildTracks(const EnemyAttack &attack) {
	using namespace DopeSheet;

	tracks_.clear();
	keyToModifier_.assign(kTrackCount, {});

	for (int t = 0; t < kTrackCount; ++t) {
		const AttackModifierType type = kTrackOrder[t];

		DopeTrack track;
		track.label = GetModifierLabel(type);
		track.color = GetModifierColor(type);

		for (size_t i = 0; i < attack.modifiers.size(); ++i) {
			const auto &modifier = attack.modifiers[i];
			if (modifier.type != type) continue;

			const int begin = ToFrames(modifier.startTime);
			// 瞬間発火は長さ0のキー、区間は長さ付きのバー
			const int duration =
				modifier.IsInstant() ? 0 : std::max(0, ToFrames(modifier.endTime) - begin);

			DopeKey key(begin, 0.0f, 0, duration);
			key.shape = modifier.IsInstant() ? KeyShape::Diamond : KeyShape::Bar;
			track.AddKey(key);

			keyToModifier_[t].push_back(static_cast<int>(i));
		}
		tracks_.push_back(std::move(track));
	}

	builtAttack_ = &attack;
	builtDuration_ = attack.duration;
	builtCount_ = attack.modifiers.size();
	dirty_ = false;
}

// ============================================================
// ドープシートの編集結果を書き戻す
// ============================================================
void AttackModifierPanel::ApplyTracks(EnemyAttack &attack) {
	for (int t = 0; t < kTrackCount && t < static_cast<int>(tracks_.size()); ++t) {
		const auto &track = tracks_[t];

		for (size_t k = 0; k < track.keys.size(); ++k) {
			if (k >= keyToModifier_[t].size()) break;

			const int index = keyToModifier_[t][k];
			if (index < 0 || index >= static_cast<int>(attack.modifiers.size())) continue;

			auto &modifier = attack.modifiers[index];
			const auto &key = track.keys[k];

			modifier.startTime = ToSeconds(key.frame);
			modifier.endTime = modifier.IsInstant()
			                       ? modifier.startTime
			                       : ToSeconds(key.frame + key.duration);

			// 攻撃時間からはみ出さないよう丸める
			modifier.startTime = std::clamp(modifier.startTime, 0.0f, attack.duration);
			modifier.endTime = std::clamp(modifier.endTime, modifier.startTime, attack.duration);
		}
	}
}

// ============================================================
// 追加ボタン
// ============================================================
void AttackModifierPanel::DrawAddButtons(EnemyAttack &attack) {
	for (int t = 0; t < kTrackCount; ++t) {
		const AttackModifierType type = kTrackOrder[t];

		char label[64];
		std::snprintf(label, sizeof(label), "+ %s", GetModifierLabel(type));

		if (ImGui::Button(label)) {
			AttackModifier modifier;
			modifier.type = type;
			// 攻撃の中盤に置く。端に置くと見つけにくい。
			modifier.startTime = attack.duration * 0.4f;
			modifier.endTime = attack.duration * 0.6f;

			if (type == AttackModifierType::Hitbox) {
				// 既存の判定と番号が被ると1回しか当たらなくなる
				int maxWindow = -1;
				for (const auto &m : attack.modifiers) {
					if (m.type == AttackModifierType::Hitbox) {
						maxWindow = std::max(maxWindow, m.damageWindow);
					}
				}
				modifier.damageWindow = maxWindow + 1;
			}

			attack.modifiers.push_back(modifier);
			selectedModifier_ = static_cast<int>(attack.modifiers.size()) - 1;
			dirty_ = true;
		}
		if (t < kTrackCount - 1) ImGui::SameLine();
	}
}

// ============================================================
// 選択中モディファイアの編集
// ============================================================
void AttackModifierPanel::DrawInspector(EnemyAttack &attack) {
	if (selectedModifier_ < 0 || selectedModifier_ >= static_cast<int>(attack.modifiers.size())) {
		ImGui::TextDisabled("一覧から選ぶと詳細を編集できます");
		return;
	}

	auto &modifier = attack.modifiers[selectedModifier_];

	ImGui::SeparatorText(GetModifierLabel(modifier.type));
	ImGui::TextDisabled("%s", GetAttackModifierDescription(modifier.type));

	if (ImGui::DragFloat("開始", &modifier.startTime, 0.01f, 0.0f, attack.duration, "%.2f秒")) {
		dirty_ = true;
	}
	if (!modifier.IsInstant()) {
		if (ImGui::DragFloat("終了", &modifier.endTime, 0.01f, modifier.startTime, attack.duration,
		                     "%.2f秒")) {
			dirty_ = true;
		}
	}

	switch (modifier.type) {
	case AttackModifierType::FaceTarget:
		ImGui::DragFloat("回転速度", &modifier.strength, 0.1f, 0.0f, 30.0f, "%.1frad/s");
		break;

	case AttackModifierType::HomingOffset:
		ImGui::DragFloat("寄せる強さ", &modifier.strength, 0.1f, 0.0f, 30.0f, "%.1f");
		ImGui::TextDisabled("カーブで作った軌道を相手方向へ曲げます");
		break;

	case AttackModifierType::Hitbox:
		ImGui::DragInt("判定番号", &modifier.damageWindow, 1, 0, 16);
		ImGui::TextDisabled("同じ番号の間は1回しか当たりません。多段は番号を分けます");
		break;

	case AttackModifierType::EmitProjectile: {
		char buffer[128];
		std::snprintf(buffer, sizeof(buffer), "%s", modifier.projectileId.c_str());
		if (ImGui::InputText("弾のID", buffer, sizeof(buffer))) {
			modifier.projectileId = buffer;
		}
		ImGui::DragInt("発射数", &modifier.count, 1, 1, 32);
		ImGui::DragFloat("拡散角度", &modifier.spreadDeg, 1.0f, 0.0f, 360.0f, "%.0f度");
		ImGui::DragFloat3("発射位置", &modifier.offset.x, 0.1f);
		ImGui::Checkbox("相手を狙う", &modifier.aimAtTarget);
		break;
	}

	default:
		break;
	}

	if (ImGui::Button("このモディファイアを削除")) {
		attack.modifiers.erase(attack.modifiers.begin() + selectedModifier_);
		selectedModifier_ = -1;
		dirty_ = true;
	}
}

// ============================================================
// 描画
// ============================================================
void AttackModifierPanel::Draw(EnemyAttack &attack) {
	// 個数や長さが変わったら組み直す
	if (dirty_ || builtAttack_ != &attack || builtCount_ != attack.modifiers.size() ||
		builtDuration_ != attack.duration) {
		BuildTracks(attack);
	}

	DrawAddButtons(attack);

	// 再生ヘッド
	if (playheadTime_ >= 0.0f) {
		dope_.SetSeekFrame(ToFrames(playheadTime_));
	}

	const int totalFrames = std::max(1, ToFrames(attack.duration));
	if (dope_.Draw("##attack_modifiers", tracks_, totalFrames, editorFps_)) {
		ApplyTracks(attack);
	}

	// 一覧からの選択。ドープシート上のクリック選択より確実に選べる。
	ImGui::SeparatorText("モディファイア一覧");
	for (int i = 0; i < static_cast<int>(attack.modifiers.size()); ++i) {
		const auto &modifier = attack.modifiers[i];

		char label[160];
		std::snprintf(label, sizeof(label), "%s  %.2f - %.2f秒##mod%d",
		              GetModifierLabel(modifier.type), modifier.startTime, modifier.endTime, i);

		if (ImGui::Selectable(label, selectedModifier_ == i)) {
			selectedModifier_ = i;
		}
	}

	DrawInspector(attack);
}

#endif // USE_IMGUI
