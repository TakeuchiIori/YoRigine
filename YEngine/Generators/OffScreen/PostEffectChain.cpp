#include "PostEffectChain.h"

// Engine
#include "WinApp/WinApp.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif


/// <summary>
/// ポストエフェクトを追加してインデックスを返す
/// </summary>
int PostEffectChain::AddEffect(OffScreen::OffScreenEffectType type, const std::string& name)
{
	auto effect = std::make_unique<PostEffectData>();
	effect->type = type;
	effect->name = name.empty() ? GenerateEffectName(type) : name;
	effect->enabled = true;

	// デフォルト値設定
	SetDefaultParameters(*effect);

	effects_.push_back(std::move(effect));
	return static_cast<int>(effects_.size()) - 1;
}


/// <summary>
/// 指定インデックスのエフェクトを削除
/// </summary>
void PostEffectChain::RemoveEffect(int index)
{
	if (IsValidIndex(index)) {
		effects_.erase(effects_.begin() + index);
	}
}


/// <summary>
/// エフェクトの並び順を変更
/// </summary>
void PostEffectChain::MoveEffect(int fromIndex, int toIndex)
{
	if (IsValidIndex(fromIndex) && IsValidIndex(toIndex) && fromIndex != toIndex) {
		auto effect = std::move(effects_[fromIndex]);
		effects_.erase(effects_.begin() + fromIndex);
		effects_.insert(effects_.begin() + toIndex, std::move(effect));
	}
}


/// <summary>
/// エフェクトの有効/無効を切り替える
/// </summary>
void PostEffectChain::SetEffectEnabled(int index, bool enabled)
{
	if (IsValidIndex(index)) {
		effects_[index]->enabled = enabled;
	}
}


/// <summary>
/// すべてのエフェクトを削除
/// </summary>
void PostEffectChain::ClearAllEffects()
{
	effects_.clear();
	effectCounters_.clear();
}


/// <summary>
/// インデックスから PostEffectData を取得
/// </summary>
PostEffectData* PostEffectChain::GetPostEffectData(int index)
{
	if (IsValidIndex(index)) return effects_[index].get();
	return nullptr;
}

const PostEffectData* PostEffectChain::GetPostEffectData(int index) const
{
	if (IsValidIndex(index)) return effects_[index].get();
	return nullptr;
}

PostEffectData* PostEffectChain::GetFirstEffectByType(OffScreen::OffScreenEffectType type)
{
	for (auto& e : effects_) {
		if (e && e->type == type) return e.get();
	}
	return nullptr;
}


/// <summary>
/// 有効なエフェクトのみのインデックス一覧を返す
/// </summary>
std::vector<int> PostEffectChain::GetEnabledEffectIndices() const
{
	std::vector<int> enabledIndices;
	enabledIndices.reserve(effects_.size());

	for (int i = 0; i < static_cast<int>(effects_.size()); ++i) {
		if (effects_[i] && effects_[i]->enabled) {
			enabledIndices.push_back(i);
		}
	}
	return enabledIndices;
}


/// <summary>
/// エフェクトリストをまとめて差し替える
/// </summary>
void PostEffectChain::SetAllEffects(std::vector<std::unique_ptr<PostEffectData>>&& effects)
{
	effects_ = std::move(effects);
}


//==================================================================
// 個別エフェクトのパラメータ設定
//==================================================================

/// <summary>
/// トーンマッピング Exposure 設定
/// </summary>
void PostEffectChain::SetToneMappingExposure(int index, float exposure)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::ToneMapping) {
		effect->params.exposure = exposure;
	}
}


/// <summary>
/// ガウシアンブラー パラメータ設定
/// </summary>
void PostEffectChain::SetGaussianBlurParams(int index, float sigma, int kernelSize)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::GaussSmoothing) {
		effect->params.sigma = sigma;
		effect->params.kernelSize = kernelSize;
	}
}


/// <summary>
/// デプスアウトライン パラメータ設定
/// </summary>
void PostEffectChain::SetDepthOutlineParams(int index, int kernelSize, const Vector4& color)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::DepthOutline) {
		effect->params.kernelSize = kernelSize;
		effect->params.outlineColor = color;
	}
}


/// <summary>
/// 放射ブラー設定
/// </summary>
void PostEffectChain::SetRadialBlurParams(int index, const OffScreen::RadialBlurPrams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::RadialBlur) {
		effect->params.radialBlur = params;
	}
}


/// <summary>
/// ディゾルブ設定
/// </summary>
void PostEffectChain::SetDissolveParams(int index, const OffScreen::DissolveParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::Dissolve) {
		effect->params.dissolve = params;
	}
}


/// <summary>
/// 色収差パラメータ設定
/// </summary>
void PostEffectChain::SetChromaticParams(int index, const OffScreen::ChromaticParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::Chromatic) {
		effect->params.chromatic = params;
	}
}


/// <summary>
/// カラー調整パラメータ設定（Tone パラメータ含む）
/// </summary>
void PostEffectChain::SetColorAdjustParams(int index,
	const OffScreen::ColorAdjustParams& colorParams,
	const OffScreen::ToneParams& toneParams)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::ColorAdjust) {
		effect->params.colorAdjust = colorParams;
		effect->params.tone = toneParams;
	}
}


/// <summary>
/// 画面割れ(Shatter)パラメータ設定
/// </summary>
void PostEffectChain::SetShatterTransitionParams(int index, const OffScreen::ShatterTransitionParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::ShatterTransition) {
		effect->params.shatter = params;
	}
}

/// <summary>
/// ブルームパラメータ設定
/// </summary>
void PostEffectChain::SetBloomParams(int index, const OffScreen::BloomParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::Bloom) {
		effect->params.bloom = params;
	}
}

/// <summary>
/// ポスタリゼーションパラメータ設定
/// </summary>
void PostEffectChain::SetPosterizeParams(int index, const OffScreen::PosterizeParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::Posterize) {
		effect->params.posterize = params;
	}
}

void PostEffectChain::SetKuwaharaParams(int index, const OffScreen::KuwaharaParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::Kuwahara) {
		effect->params.kuwahara = params;
	}
}

void PostEffectChain::SetHalftoneParams(int index, const OffScreen::HalftoneParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::Halftone) {
		effect->params.halftone = params;
	}
}

void PostEffectChain::SetCrossHatchParams(int index, const OffScreen::CrossHatchParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::CrossHatch) {
		effect->params.crossHatch = params;
	}
}

void PostEffectChain::SetFogParams(int index, const OffScreen::FogParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::Fog) {
		effect->params.fog = params;
	}
}

void PostEffectChain::SetGodRaysParams(int index, const OffScreen::GodRaysParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::GodRays) {
		effect->params.godRays = params;
	}
}

void PostEffectChain::SetColorGradeParams(int index, const OffScreen::ColorGradeParams& params)
{
	auto* effect = GetPostEffectData(index);
	if (effect && effect->type == OffScreen::OffScreenEffectType::ColorGrade) {
		effect->params.colorGrade = params;
	}
}


//==================================================================
// ImGui
//==================================================================

/// <summary>
/// エフェクト一覧（ImGui GUI）
/// </summary>
bool PostEffectChain::DrawEffectListImGui([[maybe_unused]] int& selectedIndex)
{
#ifdef USE_IMGUI
	bool changed = false;

	ImGui::Text("Effects: %zu", effects_.size());

	int effectToRemove = -1;

	for (int i = 0; i < static_cast<int>(effects_.size()); ++i) {
		auto& effect = effects_[i];
		ImGui::PushID(i);

		// 有効フラグ
		bool enabled = effect->enabled;
		if (ImGui::Checkbox("##enabled", &enabled)) {
			SetEffectEnabled(i, enabled);
			changed = true;
		}

		ImGui::SameLine();

		// 選択
		bool selected = (selectedIndex == i);
		if (ImGui::Selectable(effect->name.c_str(), selected)) {
			selectedIndex = i;
		}
		ImGui::SetNextItemAllowOverlap();

		// 右クリックメニュー
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::Selectable("Remove")) {
				effectToRemove = i;
				changed = true;
				ImGui::CloseCurrentPopup();
			}
			if (i > 0 && ImGui::Selectable("Move Up")) {
				MoveEffect(i, i - 1);
				if (selectedIndex == i) selectedIndex--;
				changed = true;
			}
			if (i < static_cast<int>(effects_.size()) - 1 &&
				ImGui::Selectable("Move Down"))
			{
				MoveEffect(i, i + 1);
				if (selectedIndex == i) selectedIndex++;
				changed = true;
			}
			ImGui::EndPopup();
		}

		// ドラッグ&ドロップ並び替え
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("EFFECT_REORDER", &i, sizeof(int));
			ImGui::Text("Moving %s", effect->name.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EFFECT_REORDER")) {
				int draggedIndex = *(int*)payload->Data;
				MoveEffect(draggedIndex, i);
				if (selectedIndex == draggedIndex)
					selectedIndex = i;
				changed = true;
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();
	}

	// 削除処理をループ外で
	if (effectToRemove >= 0) {
		RemoveEffect(effectToRemove);
		if (selectedIndex == effectToRemove) selectedIndex = -1;
		else if (selectedIndex > effectToRemove) selectedIndex--;
	}

	return changed;
#else
	return false;
#endif
}


/// <summary>
/// エフェクト個別パラメータ編集（ImGui）
/// </summary>
bool PostEffectChain::DrawEffectParametersImGui([[maybe_unused]] int selectedIndex)
{
#ifdef USE_IMGUI
	if (!IsValidIndex(selectedIndex)) {
		return false;
	}

	bool changed = false;
	auto& effect = effects_[selectedIndex];

	ImGui::Text("Edit: %s", effect->name.c_str());

	switch (effect->type) {

		// --------------------------------------------------------
		// トーンマッピング
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::ToneMapping:
		if (ImGui::DragFloat("Exposure", &effect->params.exposure, 0.01f, 0.0f, 5.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// ガウシアンブラー
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::GaussSmoothing:
		if (ImGui::DragFloat("Sigma", &effect->params.sigma, 0.01f, 0.1f, 10.0f)) {
			changed = true;
		}
		if (ImGui::DragInt("Kernel Size", &effect->params.kernelSize, 1, 1, 25)) {
			if (effect->params.kernelSize % 2 == 0) effect->params.kernelSize++;
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// デプスアウトライン
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::DepthOutline:
		if (ImGui::DragInt("Kernel Size", &effect->params.kernelSize, 1, 1, 25)) {
			if (effect->params.kernelSize % 2 == 0) effect->params.kernelSize++;
			changed = true;
		}
		if (ImGui::ColorEdit4("Outline Color", &effect->params.outlineColor.x)) {
			changed = true;
		}
		{
			auto& d = effect->params.depthOutline;
			ImGui::SeparatorText("エッジ要因");
			if (ImGui::Checkbox("深度", &d.useDepth)) changed = true;
			ImGui::SameLine();
			if (ImGui::Checkbox("法線", &d.useNormal)) changed = true;
			ImGui::SameLine();
			if (ImGui::Checkbox("輝度", &d.useLuminance)) changed = true;

			if (ImGui::DragFloat("全体強度", &d.edgeStrength, 0.01f, 0.0f, 10.0f)) changed = true;

			if (d.useDepth) {
				if (ImGui::DragFloat("深度 重み", &d.depthWeight, 0.01f, 0.0f, 10.0f)) changed = true;
				if (ImGui::DragFloat("深度 閾値", &d.depthThreshold, 0.001f, 0.0001f, 10.0f)) changed = true;
			}
			if (d.useNormal) {
				if (ImGui::DragFloat("法線 重み", &d.normalWeight, 0.01f, 0.0f, 10.0f)) changed = true;
				if (ImGui::DragFloat("法線 閾値", &d.normalThreshold, 0.001f, 0.0001f, 10.0f)) changed = true;
				if (ImGui::DragFloat("法線 太さ", &d.normalWidth, 0.05f, 1.0f, 16.0f)) changed = true;
			}
			if (d.useLuminance) {
				if (ImGui::DragFloat("輝度 重み", &d.luminanceWeight, 0.01f, 0.0f, 10.0f)) changed = true;
				if (ImGui::DragFloat("輝度 閾値", &d.luminanceThreshold, 0.001f, 0.0001f, 10.0f)) changed = true;
				if (ImGui::DragFloat("輝度 太さ", &d.luminanceWidth, 0.05f, 1.0f, 16.0f)) changed = true;
			}

			ImGui::SeparatorText("距離フェード（遠景の真っ黒化防止）");
			if (ImGui::Checkbox("距離フェード有効", &d.useDistanceFade)) changed = true;
			if (d.useDistanceFade) {
				if (ImGui::DragFloat("フェード開始", &d.distanceFadeStart, 0.5f, 0.0f, 1000.0f)) changed = true;
				if (ImGui::DragFloat("フェード終了", &d.distanceFadeEnd, 0.5f, 0.0f, 1000.0f)) changed = true;
			}
		}
		break;

		// --------------------------------------------------------
		// 法線可視化（パラメータなし）
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::NormalVisualize:
		ImGui::TextUnformatted("法線 G-buffer をそのまま色表示します（調整項目なし）");
		break;

		// --------------------------------------------------------
		// 放射ブラー
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::RadialBlur:
		if (ImGui::DragFloat2("Direction", &effect->params.radialBlur.direction.x, 0.01f, -1.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::DragFloat2("Center", &effect->params.radialBlur.center.x, 0.01f, 0.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::DragFloat("Width", &effect->params.radialBlur.width, 0.001f, 0.0f, 0.2f)) {
			changed = true;
		}
		if (ImGui::DragInt("Sample Count", &effect->params.radialBlur.sampleCount, 1, 1, 64)) {
			changed = true;
		}
		if (ImGui::Checkbox("Is Radial", &effect->params.radialBlur.isRadial)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// ディゾルブ
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::Dissolve:
		if (ImGui::DragFloat("Threshold", &effect->params.dissolve.threshold, 0.01f, 0.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::DragFloat("Edge Width", &effect->params.dissolve.edgeWidth, 0.001f, 0.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::ColorEdit3("Edge Color", &effect->params.dissolve.edgeColor.x)) {
			changed = true;
		}
		if (ImGui::DragFloat("Invert", &effect->params.dissolve.invert, 0.01f, 0.0f, 1.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// 色収差
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::Chromatic:
		if (ImGui::DragFloat("Aberration Strength", &effect->params.chromatic.aberrationStrength, 0.001f, 0.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::DragFloat("Edge Strength", &effect->params.chromatic.edgeStrength, 0.001f, 0.0f, 5.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// カラー調整
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::ColorAdjust:
		if (ImGui::SliderFloat("Brightness", &effect->params.colorAdjust.brightness, -1.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::DragFloat("Contrast", &effect->params.colorAdjust.contrast, 0.01f, 0.1f, 3.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Saturation", &effect->params.colorAdjust.saturation, 0.0f, 3.0f)) {
			changed = true;
		}
		if (ImGui::DragFloat("Hue", &effect->params.colorAdjust.hue, 1.0f, -180.0f, 180.0f, "%.0f°")) {
			changed = true;
		}

		ImGui::Separator();

		if (ImGui::SliderFloat("Gamma", &effect->params.tone.gamma, 0.1f, 5.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Exposure", &effect->params.tone.exposure, -3.0f, 3.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// 画面割れ (Shatter)
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::ShatterTransition:
		if (ImGui::DragFloat("Progress", &effect->params.shatter.progress, 0.01f, 0.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::DragFloat("Time", &effect->params.shatter.time, 0.01f, 0.0f, 10.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// ブルーム
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::Bloom:
		if (ImGui::SliderFloat("Threshold", &effect->params.bloom.threshold, 0.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Intensity", &effect->params.bloom.intensity, 0.0f, 3.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Spread", &effect->params.bloom.spread, 1.0f, 20.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Color Temp (warm +)", &effect->params.bloom.colorTemperature, -1.0f, 1.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// ポスタリゼーション
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::Posterize:
		if (ImGui::SliderInt("Steps", &effect->params.posterize.steps, 2, 128)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Saturation Boost", &effect->params.posterize.saturationBoost, 0.0f, 2.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// クワハラ（油絵）フィルター
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::Kuwahara:
		if (ImGui::SliderInt("Radius", &effect->params.kuwahara.radius, 1, 8)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Sharpness", &effect->params.kuwahara.sharpness, 1.0f, 16.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// ハーフトーン
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::Halftone:
		if (ImGui::SliderFloat("Dot Size", &effect->params.halftone.dotSize, 2.0f, 16.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Angle", &effect->params.halftone.angle, 0.0f, 90.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Strength##halftone", &effect->params.halftone.strength, 0.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Shadow Threshold", &effect->params.halftone.threshold, 0.0f, 1.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// カラーグレーディング
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::ColorGrade:
	{
		ImGui::TextDisabled("--- Split Toning ---");
		float sc[3] = { effect->params.colorGrade.shadowColor.x, effect->params.colorGrade.shadowColor.y, effect->params.colorGrade.shadowColor.z };
		if (ImGui::ColorEdit3("Shadow Color", sc)) {
			effect->params.colorGrade.shadowColor = { sc[0], sc[1], sc[2] };
			changed = true;
		}
		float hc[3] = { effect->params.colorGrade.highlightColor.x, effect->params.colorGrade.highlightColor.y, effect->params.colorGrade.highlightColor.z };
		if (ImGui::ColorEdit3("Highlight Color", hc)) {
			effect->params.colorGrade.highlightColor = { hc[0], hc[1], hc[2] };
			changed = true;
		}
		if (ImGui::SliderFloat("Balance", &effect->params.colorGrade.splitBalance, 0.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Split Strength", &effect->params.colorGrade.splitStrength, 0.0f, 1.0f)) {
			changed = true;
		}
		ImGui::Separator();
		ImGui::TextDisabled("--- Vibrance / Temperature ---");
		if (ImGui::SliderFloat("Vibrance", &effect->params.colorGrade.vibrance, -1.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Color Temp (warm +)", &effect->params.colorGrade.colorTemp, -1.0f, 1.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Tint (magenta +)", &effect->params.colorGrade.colorTint, -1.0f, 1.0f)) {
			changed = true;
		}
		break;
	}

		// --------------------------------------------------------
		// クロスハッチング
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::CrossHatch:
		if (ImGui::SliderFloat("Line Spacing", &effect->params.crossHatch.lineSpacing, 2.0f, 20.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Line Width", &effect->params.crossHatch.lineWidth, 0.3f, 4.0f)) {
			changed = true;
		}
		if (ImGui::SliderFloat("Strength##crosshatch", &effect->params.crossHatch.strength, 0.0f, 1.0f)) {
			changed = true;
		}
		break;

		// --------------------------------------------------------
		// 大気フォグ
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::Fog:
	{
		auto& f = effect->params.fog;
		ImGui::TextDisabled("--- Distance Fog ---");
		float fc[3] = { f.fogColor.x, f.fogColor.y, f.fogColor.z };
		if (ImGui::ColorEdit3("Fog Color", fc)) {
			f.fogColor = { fc[0], fc[1], fc[2] };
			changed = true;
		}
		if (ImGui::DragFloat("Density", &f.fogDensity, 0.0005f, 0.0f, 0.5f, "%.4f")) changed = true;
		if (ImGui::DragFloat("Start Distance", &f.fogStart, 0.5f, 0.0f, 500.0f)) changed = true;
		if (ImGui::SliderFloat("Sky Fog Max", &f.skyFogClamp, 0.0f, 1.0f)) changed = true;

		ImGui::Separator();
		ImGui::TextDisabled("--- Sun Inscatter ---");
		if (ImGui::SliderFloat("Sun Strength", &f.sunInscatterStrength, 0.0f, 4.0f)) changed = true;
		float sc[3] = { f.sunColor.x, f.sunColor.y, f.sunColor.z };
		if (ImGui::ColorEdit3("Sun Color", sc)) {
			f.sunColor = { sc[0], sc[1], sc[2] };
			changed = true;
		}

		ImGui::Separator();
		ImGui::TextDisabled("--- Height Fog ---");
		if (ImGui::DragFloat("Top Y", &f.heightFogTop, 0.5f, -100.0f, 500.0f)) changed = true;
		if (ImGui::DragFloat("Bottom Y", &f.heightFogBottom, 0.5f, -500.0f, 500.0f)) changed = true;
		if (ImGui::SliderFloat("Height Density", &f.heightFogDensity, 0.0f, 5.0f)) changed = true;
		if (ImGui::DragFloat("Distance Scale", &f.heightFogDistanceScale, 0.001f, 0.0f, 1.0f, "%.4f")) changed = true;
		break;
	}

		// --------------------------------------------------------
		// God Rays
		// --------------------------------------------------------
	case OffScreen::OffScreenEffectType::GodRays:
	{
		auto& g = effect->params.godRays;
		ImGui::TextDisabled("加算合成。空(=深度1.0)の方向に光が伸びる。");
		ImGui::TextDisabled("Exposure を下げると色変化が見えやすい。");

		float sc[3] = { g.sunColor.x, g.sunColor.y, g.sunColor.z };
		if (ImGui::ColorEdit3("Sun Color##gr", sc)) {
			g.sunColor = { sc[0], sc[1], sc[2] };
			changed = true;
		}
		ImGui::TextDisabled("  ※スカイボックス内の「太陽の絵」は変わりません");
		ImGui::TextDisabled("  ※DirectionalLight (LightingEditor) とも別物");

		if (ImGui::SliderFloat("Exposure", &g.exposure, 0.0f, 1.0f)) changed = true;
		ImGui::TextDisabled("  全体強度。> 0.3 で白飛びしがち");

		if (ImGui::SliderFloat("Weight##gr", &g.weight, 0.0f, 0.6f)) changed = true;
		ImGui::TextDisabled("  1サンプル寄与。0.2〜0.3 が無難");

		if (ImGui::SliderFloat("Decay##gr", &g.decay, 0.8f, 1.0f)) changed = true;
		ImGui::TextDisabled("  距離減衰。高いほど遠くまで光が伸びる");

		if (ImGui::SliderFloat("Density##gr", &g.density, 0.0f, 4.0f)) changed = true;
		ImGui::TextDisabled("  サンプル間隔。大きいほど太い光芒");

		if (ImGui::SliderInt("Samples##gr", &g.numSamples, 8, 64)) changed = true;
		if (ImGui::SliderFloat("Sky Threshold", &g.skyThreshold, 0.99f, 1.0f, "%.5f")) changed = true;

		ImGui::Separator();
		if (ImGui::Button("Reset GodRays Defaults")) {
			g = OffScreen::GodRaysParams{};
			changed = true;
		}
		break;
	}

	default:
		ImGui::Text("No editable parameters for this effect.");
		break;
	}

	return changed;

#else
	return false;
#endif
}

/// <summary>
/// エフェクト名を自動生成する（例：GaussSmoothing 1, GaussSmoothing 2）
/// </summary>
std::string PostEffectChain::GenerateEffectName(OffScreen::OffScreenEffectType type)
{
	// 同じタイプの生成回数をカウント
	int& counter = effectCounters_[type];
	counter++;

	// 種類名を取得して連番をつける
	std::string baseName = GetEffectTypeName(type);
	return baseName + " " + std::to_string(counter);
}

/// <summary>
/// エフェクトの種類を文字列で返す
/// </summary>
const char* PostEffectChain::GetEffectTypeName(OffScreen::OffScreenEffectType type) const
{
	switch (type) {
	case OffScreen::OffScreenEffectType::Copy:              return "Copy";
	case OffScreen::OffScreenEffectType::GaussSmoothing:    return "GaussSmoothing";
	case OffScreen::OffScreenEffectType::DepthOutline:      return "DepthOutline";
	case OffScreen::OffScreenEffectType::Sepia:             return "Sepia";
	case OffScreen::OffScreenEffectType::Grayscale:         return "Grayscale";
	case OffScreen::OffScreenEffectType::Vignette:          return "Vignette";
	case OffScreen::OffScreenEffectType::RadialBlur:        return "RadialBlur";
	case OffScreen::OffScreenEffectType::ToneMapping:       return "ToneMapping";
	case OffScreen::OffScreenEffectType::Dissolve:          return "Dissolve";
	case OffScreen::OffScreenEffectType::Chromatic:         return "Chromatic";
	case OffScreen::OffScreenEffectType::ColorAdjust:       return "ColorAdjust";
	case OffScreen::OffScreenEffectType::ShatterTransition: return "ShatterTransition";
	case OffScreen::OffScreenEffectType::Bloom:             return "Bloom";
	case OffScreen::OffScreenEffectType::Posterize:         return "Posterize";
	case OffScreen::OffScreenEffectType::Kuwahara:          return "Kuwahara";
	case OffScreen::OffScreenEffectType::Halftone:          return "Halftone";
	case OffScreen::OffScreenEffectType::CrossHatch:        return "CrossHatch";
	case OffScreen::OffScreenEffectType::ColorGrade:        return "ColorGrade";
	case OffScreen::OffScreenEffectType::Fog:               return "Fog";
	case OffScreen::OffScreenEffectType::GodRays:           return "GodRays";
	case OffScreen::OffScreenEffectType::NormalVisualize:   return "NormalVisualize";
	default:                                                return "Unknown";
	}
}

/// <summary>
/// 新規追加されたエフェクトにデフォルトパラメータを設定する
/// </summary>
void PostEffectChain::SetDefaultParameters(PostEffectData& effect)
{
	switch (effect.type) {

		// ----------------------------- トーンマッピング　-----------------------------//
	case OffScreen::OffScreenEffectType::ToneMapping:
		effect.params.exposure = 0.25f;
		break;

		// ----------------------------- ガウスブラー　-----------------------------//
	case OffScreen::OffScreenEffectType::GaussSmoothing:
		effect.params.sigma = 2.0f;
		effect.params.kernelSize = 3;
		break;

		// ----------------------------- 深度アウトライン　-----------------------------//
	case OffScreen::OffScreenEffectType::DepthOutline:
		effect.params.kernelSize = 3;
		effect.params.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		break;

		// ----------------------------- 放射ブラー　-----------------------------//
	case OffScreen::OffScreenEffectType::RadialBlur:
		effect.params.radialBlur = { {0.0f, 0.0f}, {0.5f, 0.5f}, 0.001f, 10, true };
		break;

		// ----------------------------- ディゾルブ　-----------------------------//
	case OffScreen::OffScreenEffectType::Dissolve:
		effect.params.dissolve.threshold = 0.5f;
		effect.params.dissolve.edgeWidth = 0.1f;
		effect.params.dissolve.edgeColor = { 1.0f, 1.0f, 1.0f };
		effect.params.dissolve.invert = 0.0f;
		break;

		// ----------------------------- 色収差　-----------------------------//
	case OffScreen::OffScreenEffectType::Chromatic:
		effect.params.chromatic.aberrationStrength = 0.02f;
		effect.params.chromatic.screenSize = { WinApp::kClientWidth, WinApp::kClientHeight };
		effect.params.chromatic.edgeStrength = 1.0f;
		break;

		// ----------------------------- カラー調整　-----------------------------//
	case OffScreen::OffScreenEffectType::ColorAdjust:
		effect.params.colorAdjust = { 0.0f, 1.0f, 1.0f, 0.0f }; // 明るさ/コントラスト/彩度/色相
		effect.params.tone = { 2.2f, 1.0f };					// ガンマ/エクスポージャ
		break;

		// ----------------------------- 画面割れトランジション　-----------------------------//
	case OffScreen::OffScreenEffectType::ShatterTransition:
		effect.params.shatter = { 0.0f, { WinApp::kClientWidth, WinApp::kClientHeight }, 1.0f };
		break;

		// ----------------------------- ブルーム　-----------------------------//
	case OffScreen::OffScreenEffectType::Bloom:
		effect.params.bloom.threshold = 0.6f;
		effect.params.bloom.intensity = 0.5f;
		effect.params.bloom.spread = 6.0f;
		effect.params.bloom.colorTemperature = 0.3f;
		break;

		// ----------------------------- ポスタリゼーション　-----------------------------//
	case OffScreen::OffScreenEffectType::Posterize:
		effect.params.posterize.steps = 5;
		effect.params.posterize.saturationBoost = 1.2f;
		break;

		// ----------------------------- クワハラ（油絵）フィルター　-----------------------------//
	case OffScreen::OffScreenEffectType::Kuwahara:
		effect.params.kuwahara.radius = 4;
		effect.params.kuwahara.sharpness = 4.0f;
		break;

		// ----------------------------- ハーフトーン　-----------------------------//
	case OffScreen::OffScreenEffectType::Halftone:
		effect.params.halftone.dotSize = 6.0f;
		effect.params.halftone.angle = 45.0f;
		effect.params.halftone.strength = 0.85f;
		effect.params.halftone.threshold = 0.75f;
		break;

		// ----------------------------- クロスハッチング　-----------------------------//
	case OffScreen::OffScreenEffectType::CrossHatch:
		effect.params.crossHatch.lineSpacing = 8.0f;
		effect.params.crossHatch.lineWidth = 1.0f;
		effect.params.crossHatch.strength = 0.7f;
		break;

		// ----------------------------- カラーグレーディング　-----------------------------//
	case OffScreen::OffScreenEffectType::ColorGrade:
		effect.params.colorGrade.shadowColor    = { 0.0f, 0.02f, 0.08f };
		effect.params.colorGrade.splitBalance   = 0.45f;
		effect.params.colorGrade.highlightColor = { 0.10f, 0.06f, -0.02f };
		effect.params.colorGrade.splitStrength  = 0.25f;
		effect.params.colorGrade.vibrance       = 0.30f;
		effect.params.colorGrade.colorTemp      = 0.08f;
		effect.params.colorGrade.colorTint      = 0.0f;
		break;

		// ----------------------------- 大気フォグ　-----------------------------//
	case OffScreen::OffScreenEffectType::Fog:
		effect.params.fog = OffScreen::FogParams{}; // FogParams のメンバ既定値
		break;

		// ----------------------------- God Rays　-----------------------------//
	case OffScreen::OffScreenEffectType::GodRays:
		effect.params.godRays = OffScreen::GodRaysParams{};
		break;

	default:
		// 特に設定するものが無いタイプはスルー
		break;
	}
}

/// <summary>
/// インデックスが配列の範囲内かどうかを判定する
/// </summary>
bool PostEffectChain::IsValidIndex(int index) const
{
	return index >= 0 && index < static_cast<int>(effects_.size());
}

