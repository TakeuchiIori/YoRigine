#include "AttackCurvePanel.h"

#ifdef USE_IMGUI

#include <ImCurveEdit.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

// チャンネルごとの色。XYZ を RGB に対応させて直感的にする。
constexpr uint32_t kColorX = 0xFF4444FF; // 赤
constexpr uint32_t kColorY = 0xFF44FF44; // 緑
constexpr uint32_t kColorZ = 0xFFFF8844; // 青

const char *GetAxisLabel(int axis) {
	switch (axis) {
	case 0:  return "X";
	case 1:  return "Y";
	default: return "Z";
	}
}

uint32_t GetAxisColor(int axis) {
	switch (axis) {
	case 0:  return kColorX;
	case 1:  return kColorY;
	default: return kColorZ;
	}
}

} // namespace

// ============================================================
// グループに属する3チャンネル
// ============================================================
void AttackCurvePanel::GetGroupChannels(AttackChannel outChannels[3]) const {
	switch (group_) {
	case Group::Rotation:
		outChannels[0] = AttackChannel::RotationX;
		outChannels[1] = AttackChannel::RotationY;
		outChannels[2] = AttackChannel::RotationZ;
		break;
	case Group::Scale:
		outChannels[0] = AttackChannel::ScaleX;
		outChannels[1] = AttackChannel::ScaleY;
		outChannels[2] = AttackChannel::ScaleZ;
		break;
	default:
		outChannels[0] = AttackChannel::PositionX;
		outChannels[1] = AttackChannel::PositionY;
		outChannels[2] = AttackChannel::PositionZ;
		break;
	}
}

// ============================================================
// Delegate の構築
// ============================================================
void AttackCurvePanel::RebuildDelegate(EnemyAttack &attack) {
	if (!delegate_) {
		delegate_ = std::make_unique<CurveDelegate>();
	}
	delegate_->ClearChannels();

	AttackChannel channels[3];
	GetGroupChannels(channels);

	for (int i = 0; i < 3; ++i) {
		delegate_->AddChannel(&attack.tracks.GetChannel(channels[i]), GetAxisColor(i),
		                      GetAxisLabel(i));
	}

	builtGroup_ = group_;
	builtAttack_ = &attack;
	built_ = true;
}

// ============================================================
// 縦軸の範囲
// ============================================================
void AttackCurvePanel::UpdateViewRange(const EnemyAttack &attack) {
	if (!autoRange_) {
		delegate_->SetViewRange({0.0f, manualMin_}, {1.0f, manualMax_});
		return;
	}

	// キーの実値から範囲を決める。全チャンネル空なら適当な既定値。
	AttackChannel channels[3];
	GetGroupChannels(channels);

	float minValue = 0.0f;
	float maxValue = 0.0f;
	bool any = false;

	for (int i = 0; i < 3; ++i) {
		const CurveChannel &curve = attack.tracks.GetChannel(channels[i]);
		for (const auto &key : curve.GetKeys()) {
			minValue = any ? std::min(minValue, key.value) : key.value;
			maxValue = any ? std::max(maxValue, key.value) : key.value;
			any = true;
		}
	}

	if (!any) {
		minValue = (group_ == Group::Scale) ? 0.0f : -2.0f;
		maxValue = (group_ == Group::Scale) ? 2.0f : 8.0f;
	}

	// 上下に余白を入れて端のキーを掴みやすくする
	const float margin = std::max(0.5f, (maxValue - minValue) * 0.15f);
	delegate_->SetViewRange({0.0f, minValue - margin}, {1.0f, maxValue + margin});
}

// ============================================================
// カーブ以外の操作
// ============================================================
void AttackCurvePanel::DrawChannelControls(EnemyAttack &attack) {
	AttackChannel channels[3];
	GetGroupChannels(channels);

	for (int i = 0; i < 3; ++i) {
		ImGui::PushID(i);

		CurveChannel &curve = attack.tracks.GetChannel(channels[i]);
		const ImVec4 color = ImGui::ColorConvertU32ToFloat4(GetAxisColor(i));

		ImGui::TextColored(color, "%s", GetAxisLabel(i));
		ImGui::SameLine();
		ImGui::TextDisabled("%dキー", curve.GetKeyCount());

		// キーが1つも無いチャンネルは評価されない（＝変化しない）
		ImGui::SameLine();
		if (ImGui::SmallButton("始点と終点を作る")) {
			curve.Clear();
			const float defaultValue = GetChannelDefaultValue(channels[i]);
			curve.AddKey(0.0f, defaultValue);
			curve.AddKey(1.0f, defaultValue);
			built_ = false;
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("空にする")) {
			curve.Clear();
			built_ = false;
		}

		ImGui::PopID();
	}

	if (group_ == Group::Rotation) {
		ImGui::TextDisabled("回転はラジアン。1回転 = %.2f", 2.0f * std::numbers::pi_v<float>);
	} else if (group_ == Group::Scale) {
		ImGui::TextDisabled("スケールは倍率。1.0 が等倍");
	} else {
		ImGui::TextDisabled("位置は開始時の向き基準。Z が前方、Y が上");
	}
}

// ============================================================
// 描画
// ============================================================
void AttackCurvePanel::Draw(EnemyAttack &attack) {
	// グループ切り替え
	int groupIndex = static_cast<int>(group_);
	const char *groupNames[] = {"位置", "回転", "スケール"};
	if (ImGui::Combo("編集対象", &groupIndex, groupNames, IM_ARRAYSIZE(groupNames))) {
		group_ = static_cast<Group>(groupIndex);
		built_ = false;
	}

	ImGui::SameLine();
	ImGui::Checkbox("縦軸を自動", &autoRange_);
	if (!autoRange_) {
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragFloat("下限", &manualMin_, 0.1f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragFloat("上限", &manualMax_, 0.1f);
	}

	// 対象や表示グループが変わったら組み直す
	if (!built_ || builtAttack_ != &attack || builtGroup_ != group_) {
		RebuildDelegate(attack);
	}

	DrawChannelControls(attack);
	UpdateViewRange(attack);

	// 外部でキーを触った可能性があるので毎フレーム同期する
	delegate_->SyncAll();

	const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 220.0f);
	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();

	ImCurveEdit::Edit(*delegate_, canvasSize, static_cast<unsigned int>(group_) + 100);

	// 再生ヘッドをカーブの上に重ねる。
	// 「この時刻でどの値になっているか」を目で追えるようにする。
	if (playhead_ >= 0.0f) {
		const float x = canvasPos.x + canvasSize.x * std::clamp(playhead_, 0.0f, 1.0f);
		ImGui::GetWindowDrawList()->AddLine({x, canvasPos.y}, {x, canvasPos.y + canvasSize.y},
		                                    IM_COL32(255, 230, 60, 200), 1.5f);
	}
}

#endif // USE_IMGUI
