#include "PostEffectManager.h"
#include "DirectXCommon.h"
#include "RTVManager.h"
#include "OffScreen.h"
#include "WinApp/WinApp.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <json.hpp>
//=========================================================================
// JSON処理のための簡易ユーティリティ関数群
// ※外部ライブラリを使わず最低限のJSON読み取りを行う
//=========================================================================
namespace JsonUtil {

	/// <summary>
	/// JSON向けに文字列をエスケープする
	/// </summary>
	std::string EscapeString(const std::string& str) {
		std::ostringstream escaped;
		for (char c : str) {
			switch (c) {
			case '"':  escaped << "\\\""; break;
			case '\\': escaped << "\\\\"; break;
			case '\b': escaped << "\\b";  break;
			case '\f': escaped << "\\f";  break;
			case '\n': escaped << "\\n";  break;
			case '\r': escaped << "\\r";  break;
			case '\t': escaped << "\\t";  break;
			default:   escaped << c;      break;
			}
		}
		return escaped.str();
	}

	/// <summary>
	/// 数値文字列を任意型にパースする（float/int など）
	/// </summary>
	template<typename T>
	T ParseNumber(const std::string& str) {
		std::istringstream iss(str);
		T value;
		iss >> value;
		return value;
	}

	/// <summary>
	/// JSON文字列から "key" : "value" を取り出す（文字列専用）
	/// </summary>
	std::string GetStringValue(const std::string& json, const std::string& key) {
		std::string searchKey = "\"" + key + "\":";
		size_t pos = json.find(searchKey);
		if (pos == std::string::npos) return "";

		pos = json.find("\"", pos + searchKey.length());
		if (pos == std::string::npos) return "";

		size_t endPos = json.find("\"", pos + 1);
		if (endPos == std::string::npos) return "";

		return json.substr(pos + 1, endPos - pos - 1);
	}

	/// <summary>
	/// JSON文字列から数値を取り出す
	/// </summary>
	template<typename T>
	T GetNumberValue(const std::string& json, const std::string& key, T defaultValue = T()) {
		std::string searchKey = "\"" + key + "\":";
		size_t pos = json.find(searchKey);
		if (pos == std::string::npos) return defaultValue;

		pos += searchKey.length();
		while (pos < json.length() && std::isspace(json[pos])) pos++;

		std::string numStr;
		while (pos < json.length() &&
			(std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-' ||
				json[pos] == 'e' || json[pos] == 'E')) {
			numStr += json[pos++];
		}

		if (numStr.empty()) return defaultValue;
		return ParseNumber<T>(numStr);
	}

	/// <summary>
	/// JSON文字列から bool 値を取得する
	/// </summary>
	bool GetBoolValue(const std::string& json, const std::string& key, bool defaultValue = false) {
		std::string searchKey = "\"" + key + "\":";
		size_t pos = json.find(searchKey);
		if (pos == std::string::npos) return defaultValue;

		pos += searchKey.length();
		while (pos < json.length() && std::isspace(json[pos])) pos++;

		if (json.substr(pos, 4) == "true") return true;
		if (json.substr(pos, 5) == "false") return false;
		return defaultValue;
	}

	/// <summary>
	/// JSON配列 [1,2,3] を float 配列にパース
	/// </summary>
	std::vector<float> GetArrayValue(const std::string& json, const std::string& key) {
		std::vector<float> result;

		std::string searchKey = "\"" + key + "\":";
		size_t pos = json.find(searchKey);
		if (pos == std::string::npos) return result;

		pos = json.find("[", pos);
		if (pos == std::string::npos) return result;

		size_t endPos = json.find("]", pos);
		if (endPos == std::string::npos) return result;

		std::string arrayContent = json.substr(pos + 1, endPos - pos - 1);
		std::istringstream iss(arrayContent);
		std::string token;

		while (std::getline(iss, token, ',')) {
			result.push_back(ParseNumber<float>(token));
		}
		return result;
	}

	/// <summary>
	/// JSONオブジェクト {...} を文字列として取得する
	/// </summary>
	std::string GetObjectValue(const std::string& json, const std::string& key) {
		std::string searchKey = "\"" + key + "\":";
		size_t pos = json.find(searchKey);
		if (pos == std::string::npos) return "";

		pos = json.find("{", pos);
		if (pos == std::string::npos) return "";

		int braceCount = 1;
		size_t endPos = pos + 1;
		while (endPos < json.length() && braceCount > 0) {
			if (json[endPos] == '{') braceCount++;
			else if (json[endPos] == '}') braceCount--;
			endPos++;
		}

		return json.substr(pos, endPos - pos);
	}
}

/// <summary>
/// PostEffectManager の初期化
/// ・RTV、OffScreen、EffectChain の準備
/// ・中間レンダーターゲット作成
/// ・デフォルトで Copy を一つ追加
/// </summary>
void PostEffectManager::Initialize()
{
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();
	rtvManager_ = dxCommon_->GetRTVManager();
	offScreen_ = OffScreen::GetInstance();

	// エフェクトチェーン（複数のエフェクトを管理）
	effectChain_ = std::make_unique<PostEffectChain>();

	// 中間レンダーターゲット生成
	InitializeRenderTargets();

	// 初期状態として Copy エフェクトを一つ追加
	AddEffect(OffScreen::OffScreenEffectType::Copy, "Default");
}

/// <summary>
/// 終了処理（全リソース解放）
/// </summary>
void PostEffectManager::Finalize()
{
	// エフェクト削除
	ClearAllEffects();

	// 中間RTの管理情報クリア
	intermediateRTNames_.clear();
	rtStates_.clear();

	// OffScreen の GPU リソース解放
	if (offScreen_) {
		offScreen_->ReleaseResources();
	}

	effectChain_.reset();
}

/// <summary>
/// 全エフェクトをリセット（Copy だけの状態に戻す）
/// </summary>
void PostEffectManager::Reset()
{
	ClearAllEffects();
	AddEffect(OffScreen::OffScreenEffectType::Copy, "Default");
}

/// <summary>
/// ポストエフェクト用の中間レンダーターゲットを生成
/// （各エフェクトの出力を次のエフェクトの入力にするため）
/// </summary>
void PostEffectManager::InitializeRenderTargets()
{
	// 中間バッファを複数枚確保（ping-pong 的に使用）
	// CS書き込み対応のため、TYPELESS実体 + SRGB(RTV/SRV) + UNORM(UAV) の三系統ビューで生成
	for (int i = 0; i < MAX_INTERMEDIATE_BUFFERS; ++i) {

		std::string rtName = "PostEffect_Intermediate" + std::to_string(i);

		rtvManager_->Create(
			rtName,
			WinApp::kClientWidth,
			WinApp::kClientHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			{ 0.0f, 0.0f, 0.0f, 1.0f },
			/*createSRV*/ true,
			/*createUAV*/ true
		);

		intermediateRTNames_.push_back(rtName);

		// 初期状態を記録
		rtStates_[rtName] = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	// バックバッファの初期状態を記録
	for (UINT i = 0; i < dxCommon_->GetBackBufferCount(); ++i) {
		std::string backBufferName = "BackBuffer" + std::to_string(i);
		rtStates_[backBufferName] = D3D12_RESOURCE_STATE_PRESENT;
	}

	// OffScreen は入力側のため READ にしておく
	rtStates_["OffScreen"] = D3D12_RESOURCE_STATE_GENERIC_READ;
}

/// <summary>
/// PostEffectManager の ImGui 操作画面
/// ・エフェクト追加/削除
/// ・プリセット保存/読み込み
/// ・パラメータ編集
/// </summary>
void PostEffectManager::ImGui()
{
#ifdef USE_IMGUI
	float windowWidth = ImGui::GetWindowWidth();
	float buttonWidth = (std::max)(80.0f, windowWidth * 0.3f);

	// -------------------------
	// エフェクト追加ボタン
	// -------------------------
	if (ImGui::Button("Add Effect", ImVec2(buttonWidth, 0))) {
		ImGui::OpenPopup("AddEffectPopup");
	}

	// エフェクト種類選択ポップアップ
	if (ImGui::BeginPopup("AddEffectPopup")) {

		const char* effectNames[] = {
			"Copy", "GaussSmoothing", "DepthOutline", "Sepia",
			"Grayscale", "Vignette", "RadialBlur", "ToneMapping",
			"Dissolve", "Chromatic", "ColorAdjust", "ShatterTransition",
			"Bloom", "Posterize", "Kuwahara", "Halftone", "CrossHatch", "ColorGrade",
			"Fog", "GodRays"
		};

		for (int i = 0; i < IM_ARRAYSIZE(effectNames); ++i) {
			if (ImGui::Selectable(effectNames[i])) {
				AddEffect(static_cast<OffScreen::OffScreenEffectType>(i));
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
	if (windowWidth > 200.0f) {
		ImGui::SameLine();
	}

	// -------------------------
	// Clear All ボタン（全エフェクト削除）
	// -------------------------
	if (ImGui::Button("Clear All", ImVec2(buttonWidth, 0))) {
		ClearAllEffects();
	}

	ImGui::Separator();

	// =======================================================
	// 🔽 プリセット管理 UI
	// =======================================================
	if (ImGui::CollapsingHeader("Presets")) {

		static char presetNameBuffer[256] = "";
		static std::string saveStatusMessage = "";
		static float messageTimer = 0.0f;

		// -------------------------
		// 🔹 Save preset
		// -------------------------
		ImGui::Text("Save Current Effect Chain:");

		float inputWidth = (std::max)(100.0f, windowWidth - 120.0f);
		ImGui::SetNextItemWidth(inputWidth);

		// プリセット名入力
		ImGui::InputText("##PresetName", presetNameBuffer, sizeof(presetNameBuffer));

		if (windowWidth > 250.0f) {
			ImGui::SameLine();
		}

		// 保存ボタン
		if (ImGui::Button("Save Preset")) {

			if (strlen(presetNameBuffer) > 0) {

				if (SavePreset(presetNameBuffer)) {
					saveStatusMessage = "Saved: " + std::string(presetNameBuffer);
					messageTimer = 3.0f;   // メッセージ3秒表示
					memset(presetNameBuffer, 0, sizeof(presetNameBuffer));
				} else {
					saveStatusMessage = "Failed to save preset!";
					messageTimer = 3.0f;
				}
			} else {
				saveStatusMessage = "Please enter a preset name!";
				messageTimer = 2.0f;
			}
		}

		// 保存結果メッセージ（徐々に消える）
		if (messageTimer > 0.0f) {
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", saveStatusMessage.c_str());
			messageTimer -= ImGui::GetIO().DeltaTime;
		}

		ImGui::Separator();

		// -------------------------
		// 🔹 Load preset
		// -------------------------
		ImGui::Text("Load Preset:");

		auto presets = GetAvailablePresets();

		if (presets.empty()) {
			ImGui::TextDisabled("No presets available");
		} else {

			static int selectedPresetIndex = -1;
			static std::string loadStatusMessage = "";
			static float loadMessageTimer = 0.0f;

			std::vector<const char*> presetItems;
			for (const auto& preset : presets) {
				presetItems.push_back(preset.c_str());
			}

			float comboWidth = (std::max)(120.0f, windowWidth - 100.0f);
			ImGui::SetNextItemWidth(comboWidth);

			ImGui::Combo("##PresetList", &selectedPresetIndex,
				presetItems.data(), static_cast<int>(presetItems.size()));

			if (windowWidth > 200.0f) {
				ImGui::SameLine();
			}

			bool canLoad = selectedPresetIndex >= 0 &&
				selectedPresetIndex < static_cast<int>(presets.size());

			if (!canLoad) {
				ImGui::BeginDisabled();
			}

			// 読み込みボタン
			if (ImGui::Button("Load Preset")) {

				if (canLoad) {
					if (LoadPreset(presets[selectedPresetIndex])) {
						loadStatusMessage = "Loaded: " + presets[selectedPresetIndex];
						loadMessageTimer = 3.0f;
					} else {
						loadStatusMessage = "Failed to load preset!";
						loadMessageTimer = 3.0f;
					}
				}
			}

			if (!canLoad) {
				ImGui::EndDisabled();
			}

			// ロードメッセージ表示
			if (loadMessageTimer > 0.0f) {
				ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s", loadStatusMessage.c_str());
				loadMessageTimer -= ImGui::GetIO().DeltaTime;
			}

			ImGui::Separator();

			// -------------------------
			// 🔹 プリセットの管理（削除など）
			// -------------------------
			ImGui::Text("Manage Presets:");

			// 削除はループ外で実行するので一時的に記録
			static int presetToDelete = -1;

			for (size_t i = 0; i < presets.size(); ++i) {

				const auto& preset = presets[i];
				ImGui::PushID(static_cast<int>(i));

				std::string displayName = preset;
				if (displayName.length() > 25) {
					displayName = displayName.substr(0, 22) + "...";
				}

				ImGui::Text("%s", displayName.c_str());

				if (ImGui::IsItemHovered() && preset != displayName) {
					ImGui::SetTooltip("%s", preset.c_str());
				}

				ImGui::SameLine();

				float remainingWidth = ImGui::GetContentRegionAvail().x;
				float loadButtonWidth = (std::min)(50.0f, remainingWidth * 0.4f);
				float deleteButtonWidth = (std::min)(60.0f, remainingWidth * 0.4f);

				// 小さな Load ボタン
				if (ImGui::Button("Load", ImVec2(loadButtonWidth, 0))) {
					LoadPreset(preset);
				}

				ImGui::SameLine();

				// 削除ボタン
				if (ImGui::Button("Delete", ImVec2(deleteButtonWidth, 0))) {
					ImGui::OpenPopup(("Confirm Delete##" + preset).c_str());
				}

				// 削除確認ダイアログ
				if (ImGui::BeginPopupModal(("Confirm Delete##" + preset).c_str(),
					nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

					ImGui::Text("Delete preset '%s'?", preset.c_str());
					ImGui::Text("This action cannot be undone.");
					ImGui::Separator();

					if (ImGui::Button("Delete", ImVec2(120, 0))) {
						std::filesystem::remove(GetPresetFilePath(preset));
						presetToDelete = (int)i;  // 後で消す
						ImGui::CloseCurrentPopup();
					}

					ImGui::SameLine();

					if (ImGui::Button("Cancel", ImVec2(120, 0))) {
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				ImGui::PopID();
			}
		}
	}

	ImGui::Separator();
	// =======================================================
	// Effect List（エフェクト並べ替え / 選択）
	// =======================================================
	if (ImGui::CollapsingHeader("Effect List", ImGuiTreeNodeFlags_DefaultOpen)) {
		effectChain_->DrawEffectListImGui(selectedEffectIndex_);
	}

	ImGui::Separator();
	// =======================================================
	// Effect Parameters（選択中のエフェクト編集）
	// =======================================================
	if (selectedEffectIndex_ >= 0) {
		if (ImGui::CollapsingHeader("Effect Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
			effectChain_->DrawEffectParametersImGui(selectedEffectIndex_);
		}
	} else {
		ImGui::TextDisabled("No effect selected");
	}

#endif // USE_IMGUI
}

/// <summary>
/// PostEffectManager の描画入口
/// ※ここから RenderEffectChain() を呼び、
///   すべてのポストエフェクトをチェーン処理で実行する
/// </summary>
void PostEffectManager::Draw()
{
	auto commandList = dxCommon_->GetCommandList();
	const UINT backBufferIndex = dxCommon_->GetCurrentBackBufferIndex();
	const std::string bbName = "BackBuffer" + std::to_string(backBufferIndex);

	// バックバッファの状態を RT に設定
	rtStates_[bbName] = D3D12_RESOURCE_STATE_RENDER_TARGET;

	// ポストエフェクト実行
	RenderEffectChain();
}

/// <summary>
/// ポストエフェクトのチェーン実行
/// ・入力テクスチャ → エフェクト処理 → 中間RT → 次のエフェクト...
/// ・最後のエフェクトはバックバッファに書き込み
/// </summary>
void PostEffectManager::RenderEffectChain()
{
	auto commandList = dxCommon_->GetCommandList();
	const UINT backBufferIndex = dxCommon_->GetCurrentBackBufferIndex();
	const std::string bbName = "BackBuffer" + std::to_string(backBufferIndex);

	// 有効なエフェクトのみを抽出
	std::vector<int> enabledIndices = effectChain_->GetEnabledEffectIndices();

	// 最初の入力は OffScreen
	std::string inputRT = "OffScreen";
	rtStates_["OffScreen"] = D3D12_RESOURCE_STATE_GENERIC_READ;

	// エフェクトなし → Copy だけでバックバッファへ
	if (enabledIndices.empty()) {

		TransitionResource(inputRT, D3D12_RESOURCE_STATE_GENERIC_READ);
		TransitionResource(bbName, D3D12_RESOURCE_STATE_RENDER_TARGET);

		rtvManager_->SetRenderTargets(commandList.Get(), { bbName }, nullptr);
		SetViewportAndScissor();

		offScreen_->RenderEffect(
			OffScreen::OffScreenEffectType::Copy,
			rtvManager_->Get(inputRT)->srvHandleGPU
		);

		return;
	}

	// -------------------------------
	// 全エフェクトをCSで中間バッファにチェーン処理
	// 最後に1回 PS Copy でバックバッファへ blit (バックバッファはUAV非対応のため)
	// -------------------------------
	for (size_t idx = 0; idx < enabledIndices.size(); ++idx) {

		const int effectIndex = enabledIndices[idx];
		auto* effectData = effectChain_->GetPostEffectData(effectIndex);
		if (!effectData) continue;

		std::string outputRT =
			intermediateRTNames_[idx % intermediateRTNames_.size()];

		TransitionResource(inputRT, D3D12_RESOURCE_STATE_GENERIC_READ);
		TransitionResource(outputRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		ApplyEffectParametersToOffScreen(*effectData);

		const auto* outRT = rtvManager_->Get(outputRT);
		offScreen_->RenderEffectCompute(
			effectData->type,
			rtvManager_->Get(inputRT)->srvHandleGPU,
			outRT->uavHandleGPU,
			outRT->width,
			outRT->height
		);

		TransitionResource(outputRT, D3D12_RESOURCE_STATE_GENERIC_READ);
		inputRT = outputRT;
	}

	// 最終 blit: 最後の中間バッファ → バックバッファ (PS Copyで)
	TransitionResource(inputRT, D3D12_RESOURCE_STATE_GENERIC_READ);
	TransitionResource(bbName, D3D12_RESOURCE_STATE_RENDER_TARGET);
	rtvManager_->SetRenderTargets(commandList.Get(), { bbName }, nullptr);
	SetViewportAndScissor();

	offScreen_->RenderEffect(
		OffScreen::OffScreenEffectType::Copy,
		rtvManager_->Get(inputRT)->srvHandleGPU
	);
}

/// <summary>
/// 各ポストエフェクトに応じて、OffScreen の GPU定数バッファを更新する
/// ※エフェクト適用前に必ず呼ばれる
/// </summary>
void PostEffectManager::ApplyEffectParametersToOffScreen(const PostEffectData& effect)
{
	switch (effect.type) {

	case OffScreen::OffScreenEffectType::ToneMapping:
		// 露光量（明るさ調整）
		offScreen_->SetToneMappingExposure(effect.params.exposure);
		break;

	case OffScreen::OffScreenEffectType::GaussSmoothing:
		// ガウスぼかしの強度とカーネルサイズ
		offScreen_->SetGaussianBlurParams(
			effect.params.sigma,
			effect.params.kernelSize
		);
		break;

	case OffScreen::OffScreenEffectType::DepthOutline:
		// 輪郭線の太さと色
		offScreen_->SetDepthOutlineParams(
			effect.params.kernelSize,
			effect.params.outlineColor
		);
		break;

	case OffScreen::OffScreenEffectType::RadialBlur:
		// 放射状ブラー
		offScreen_->SetRadialBlurParams(effect.params.radialBlur);
		break;

	case OffScreen::OffScreenEffectType::Dissolve:
		// ディゾルブ（マスクテクスチャ + 閾値など）
		offScreen_->SetDissolveParams(effect.params.dissolve);
		break;

	case OffScreen::OffScreenEffectType::Chromatic:
		// 色収差（Chromatic Aberration）
		offScreen_->SetChromaticParams(effect.params.chromatic);
		break;

	case OffScreen::OffScreenEffectType::ColorAdjust:
		// Brightness/Contrast/Saturation/Hue とトーン補正
		offScreen_->SetColorAdjustParams(
			effect.params.colorAdjust,
			effect.params.tone
		);
		break;

	case OffScreen::OffScreenEffectType::ShatterTransition:
		offScreen_->SetShatterTransitionParams(effect.params.shatter);
		break;

	case OffScreen::OffScreenEffectType::Bloom:
		offScreen_->SetBloomParams(effect.params.bloom);
		break;

	case OffScreen::OffScreenEffectType::Posterize:
		offScreen_->SetPosterizeParams(effect.params.posterize);
		break;

	case OffScreen::OffScreenEffectType::Kuwahara:
		offScreen_->SetKuwaharaParams(effect.params.kuwahara);
		break;

	case OffScreen::OffScreenEffectType::Halftone:
		offScreen_->SetHalftoneParams(effect.params.halftone);
		break;

	case OffScreen::OffScreenEffectType::CrossHatch:
		offScreen_->SetCrossHatchParams(effect.params.crossHatch);
		break;

	case OffScreen::OffScreenEffectType::ColorGrade:
		offScreen_->SetColorGradeParams(effect.params.colorGrade);
		break;

	case OffScreen::OffScreenEffectType::Fog:
		// 色・密度等は毎フレーム流し込む。
		// CameraPos/InvVP/LightDir は別経路 (BaseScene::UpdateCommon) で既に書き込み済み。
		offScreen_->SetFogParams(effect.params.fog);
		break;

	case OffScreen::OffScreenEffectType::GodRays:
		// sunUV / sunVisibility は LightManager::UpdateShadowMatrix で書き込み済み
		offScreen_->SetGodRaysParams(effect.params.godRays);
		break;

	default:
		// パラメータ不要のエフェクトは何も更新しない
		break;
	}
}

/// <summary>
/// 指定された RenderTarget のリソース状態を切り替える
/// ※不要なバリアの乱発を避けるため、現在の状態を記録している
/// </summary>
void PostEffectManager::TransitionResource(const std::string& rtName,
	D3D12_RESOURCE_STATES newState)
{
	auto commandList = dxCommon_->GetCommandList();

	// 現在の状態を取得
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_GENERIC_READ;

	auto it = rtStates_.find(rtName);
	if (it != rtStates_.end()) {
		currentState = it->second;
	}

	// 状態が変わる場合のみバリア発行（最適化）
	if (currentState != newState) {
		rtvManager_->TransitionBarrier(
			commandList.Get(),
			rtName,
			currentState,
			newState
		);

		// 新しい状態に更新
		rtStates_[rtName] = newState;
	}
}

/// <summary>
/// ビューポートとシザー領域を画面全体にリセットする
/// ポストエフェクトは全画面描画なので毎回これでOK
/// </summary>
void PostEffectManager::SetViewportAndScissor()
{
	auto commandList = dxCommon_->GetCommandList();

	// ビューポート (画面全体)
	D3D12_VIEWPORT vp{};
	vp.Width = static_cast<float>(WinApp::kClientWidth);
	vp.Height = static_cast<float>(WinApp::kClientHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &vp);

	// シザー (画面全体)
	D3D12_RECT sc{};
	sc.left = 0;
	sc.top = 0;
	sc.right = WinApp::kClientWidth;
	sc.bottom = WinApp::kClientHeight;
	commandList->RSSetScissorRects(1, &sc);
}

/// <summary>
/// チェーンにエフェクトを追加
/// </summary>
int PostEffectManager::AddEffect(OffScreen::OffScreenEffectType type,
	const std::string& name)
{
	return effectChain_->AddEffect(type, name);
}

/// <summary>
/// エフェクト削除
/// </summary>
void PostEffectManager::RemoveEffect(int index)
{
	effectChain_->RemoveEffect(index);
}

/// <summary>
/// エフェクト並べ替え
/// </summary>
void PostEffectManager::MoveEffect(int fromIndex, int toIndex)
{
	effectChain_->MoveEffect(fromIndex, toIndex);
}

/// <summary>
/// 有効/無効の切替え
/// </summary>
void PostEffectManager::SetEffectEnabled(int index, bool enabled)
{
	effectChain_->SetEffectEnabled(index, enabled);
}

/// <summary>
/// 全エフェクト削除（デフォルト状態へ戻す）
/// </summary>
void PostEffectManager::ClearAllEffects()
{
	effectChain_->ClearAllEffects();
	selectedEffectIndex_ = -1; // 選択もリセット
}

/// <summary>
/// エフェクト数取得
/// </summary>
size_t PostEffectManager::GetEffectCount() const
{
	return effectChain_->GetEffectCount();
}

/// <summary>
/// エフェクトデータ取得
/// </summary>
PostEffectData* PostEffectManager::GetEffectData(int index)
{
	return effectChain_->GetPostEffectData(index);
}

/// <summary>
/// 現在のポストエフェクトチェーンを JSON 形式で保存
/// </summary>
bool PostEffectManager::SaveEffectChain(const std::string& filename)
{
	try {
		std::string jsonStr = EffectChainToJson();

		std::ofstream file(filename);
		if (!file.is_open()) {
			return false;   // 書き込み失敗
		}

		file << jsonStr;
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Error saving effect chain: " << e.what() << std::endl;
		return false;
	}
}

/// <summary>
/// エフェクトチェーンを JSON ファイルから読み込む
/// </summary>
/// <param name="filename">読み込むファイルパス</param>
/// <returns>成功したら true</returns>
bool PostEffectManager::LoadEffectChain(const std::string& filename)
{
	try {
		// ファイルを開く
		std::ifstream file(filename);
		if (!file.is_open()) {
			std::cerr << "Failed to open effect chain file: " << filename << std::endl;
			return false;
		}

		// 内容を文字列として読み込む
		std::string jsonText((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());

		file.close();

		// JSON → エフェクトチェーン復元
		return JsonToEffectChain(jsonText);
	}
	catch (const std::exception& e) {
		std::cerr << "Error loading effect chain: " << e.what() << std::endl;
		return false;
	}
}


/// <summary>
/// 現在のエフェクトチェーンをプリセットとして保存する
/// </summary>
bool PostEffectManager::SavePreset(const std::string& presetName)
{
	// 保存先フォルダがなければ作成
	if (!EnsureDirectoryExists(GetPresetDirectory())) {
		return false;
	}

	// フルパスを作成
	std::string filepath = GetPresetFilePath(presetName);

	// 通常の JSON 保存処理を使用
	return SaveEffectChain(filepath);
}

/// <summary>
/// 指定プリセットを読み込む
/// </summary>
bool PostEffectManager::LoadPreset(const std::string& presetName)
{
	std::string filepath = GetPresetFilePath(presetName);
	return LoadEffectChain(filepath);
}

/// <summary>
/// プリセットフォルダ内の .preset ファイル一覧を取得する
/// </summary>
std::vector<std::string> PostEffectManager::GetAvailablePresets() const
{
	std::vector<std::string> presets;
	std::string presetDir = GetPresetDirectory();

	try {
		if (std::filesystem::exists(presetDir)) {
			for (const auto& entry : std::filesystem::directory_iterator(presetDir)) {
				if (entry.is_regular_file() && entry.path().extension() == FILE_EXTENSION) {
					presets.push_back(entry.path().stem().string());
				}
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error getting preset list: " << e.what() << std::endl;
	}

	return presets;
}


/// <summary>
/// 現在のエフェクトチェーンを JSON テキストに変換する
/// エディター保存用
/// </summary>
std::string PostEffectManager::EffectChainToJson() const
{
	std::ostringstream json;

	// 数値は小数点6桁で統一
	json << std::fixed << std::setprecision(6);

	json << "{\n";
	json << "  \"version\": \"1.0\",\n";
	json << "  \"effectCount\": " << effectChain_->GetEffectCount() << ",\n";
	json << "  \"effects\": [\n";

	const auto& effects = effectChain_->GetAllEffects();

	// 全ポストエフェクトを列挙
	for (size_t i = 0; i < effects.size(); ++i) {
		const auto& effect = effects[i];
		if (!effect) continue;

		json << "    {\n";

		// 種類
		json << "      \"type\": " << static_cast<int>(effect->type) << ",\n";

		// 名前
		json << "      \"name\": \""
			<< JsonUtil::EscapeString(effect->name) << "\",\n";

		// 有効無効
		json << "      \"enabled\": "
			<< (effect->enabled ? "true" : "false") << ",\n";

		// ---- パラメータ ----
		json << "      \"parameters\": {\n";

		// 共通パラメータ
		json << "        \"exposure\": " << effect->params.exposure << ",\n";
		json << "        \"sigma\": " << effect->params.sigma << ",\n";
		json << "        \"kernelSize\": " << effect->params.kernelSize << ",\n";

		// アウトライン
		json << "        \"outlineColor\": ["
			<< effect->params.outlineColor.x << ","
			<< effect->params.outlineColor.y << ","
			<< effect->params.outlineColor.z << ","
			<< effect->params.outlineColor.w << "],\n";

		// ラジアルブラー
		json << "        \"radialBlur\": {\n";
		json << "          \"direction\": ["
			<< effect->params.radialBlur.direction.x << ","
			<< effect->params.radialBlur.direction.y << "],\n";
		json << "          \"center\": ["
			<< effect->params.radialBlur.center.x << ","
			<< effect->params.radialBlur.center.y << "],\n";
		json << "          \"width\": " << effect->params.radialBlur.width << ",\n";
		json << "          \"sampleCount\": " << effect->params.radialBlur.sampleCount << ",\n";
		json << "          \"isRadial\": "
			<< (effect->params.radialBlur.isRadial ? "true" : "false") << "\n";
		json << "        },\n";

		// ディゾルブ
		json << "        \"dissolve\": {\n";
		json << "          \"threshold\": " << effect->params.dissolve.threshold << ",\n";
		json << "          \"edgeWidth\": " << effect->params.dissolve.edgeWidth << ",\n";
		json << "          \"edgeColor\": ["
			<< effect->params.dissolve.edgeColor.x << ","
			<< effect->params.dissolve.edgeColor.y << ","
			<< effect->params.dissolve.edgeColor.z << "],\n";
		json << "          \"invert\": " << effect->params.dissolve.invert << "\n";
		json << "        },\n";

		// 色収差
		json << "        \"chromatic\": {\n";
		json << "          \"aberrationStrength\": "
			<< effect->params.chromatic.aberrationStrength << ",\n";
		json << "          \"screenSize\": ["
			<< effect->params.chromatic.screenSize.x << ","
			<< effect->params.chromatic.screenSize.y << "],\n";
		json << "          \"edgeStrength\": "
			<< effect->params.chromatic.edgeStrength << "\n";
		json << "        },\n";

		// カラー調整
		json << "        \"colorAdjust\": {\n";
		json << "          \"brightness\": " << effect->params.colorAdjust.brightness << ",\n";
		json << "          \"contrast\": " << effect->params.colorAdjust.contrast << ",\n";
		json << "          \"saturation\": " << effect->params.colorAdjust.saturation << ",\n";
		json << "          \"hue\": " << effect->params.colorAdjust.hue << "\n";
		json << "        },\n";

		// トーン調整
		json << "        \"tone\": {\n";
		json << "          \"gamma\": " << effect->params.tone.gamma << ",\n";
		json << "          \"exposure\": " << effect->params.tone.exposure << "\n";
		json << "        },\n";

		// 画面割れ（Shatter）
		json << "        \"shatter\": {\n";
		json << "          \"progress\": " << effect->params.shatter.progress << ",\n";
		json << "          \"resolution\": ["
			<< effect->params.shatter.resolution.x << ","
			<< effect->params.shatter.resolution.y << "],\n";
		json << "          \"time\": " << effect->params.shatter.time << "\n";
		json << "        },\n";

		// ブルーム
		json << "        \"bloom\": {\n";
		json << "          \"threshold\": " << effect->params.bloom.threshold << ",\n";
		json << "          \"intensity\": " << effect->params.bloom.intensity << ",\n";
		json << "          \"spread\": " << effect->params.bloom.spread << ",\n";
		json << "          \"colorTemperature\": " << effect->params.bloom.colorTemperature << "\n";
		json << "        },\n";

		// ポスタリゼーション
		json << "        \"posterize\": {\n";
		json << "          \"steps\": " << effect->params.posterize.steps << ",\n";
		json << "          \"saturationBoost\": " << effect->params.posterize.saturationBoost << "\n";
		json << "        },\n";

		// クワハラ
		json << "        \"kuwahara\": {\n";
		json << "          \"radius\": " << effect->params.kuwahara.radius << ",\n";
		json << "          \"sharpness\": " << effect->params.kuwahara.sharpness << "\n";
		json << "        },\n";

		// ハーフトーン
		json << "        \"halftone\": {\n";
		json << "          \"dotSize\": " << effect->params.halftone.dotSize << ",\n";
		json << "          \"angle\": " << effect->params.halftone.angle << ",\n";
		json << "          \"strength\": " << effect->params.halftone.strength << ",\n";
		json << "          \"threshold\": " << effect->params.halftone.threshold << "\n";
		json << "        },\n";

		// クロスハッチング
		json << "        \"crossHatch\": {\n";
		json << "          \"lineSpacing\": " << effect->params.crossHatch.lineSpacing << ",\n";
		json << "          \"lineWidth\": " << effect->params.crossHatch.lineWidth << ",\n";
		json << "          \"strength\": " << effect->params.crossHatch.strength << "\n";
		json << "        },\n";

		// カラーグレーディング
		json << "        \"colorGrade\": {\n";
		json << "          \"shadowColor\": ["
			<< effect->params.colorGrade.shadowColor.x << ","
			<< effect->params.colorGrade.shadowColor.y << ","
			<< effect->params.colorGrade.shadowColor.z << "],\n";
		json << "          \"splitBalance\": " << effect->params.colorGrade.splitBalance << ",\n";
		json << "          \"highlightColor\": ["
			<< effect->params.colorGrade.highlightColor.x << ","
			<< effect->params.colorGrade.highlightColor.y << ","
			<< effect->params.colorGrade.highlightColor.z << "],\n";
		json << "          \"splitStrength\": " << effect->params.colorGrade.splitStrength << ",\n";
		json << "          \"vibrance\": " << effect->params.colorGrade.vibrance << ",\n";
		json << "          \"colorTemp\": " << effect->params.colorGrade.colorTemp << ",\n";
		json << "          \"colorTint\": " << effect->params.colorGrade.colorTint << "\n";
		json << "        },\n";

		// 大気フォグ
		json << "        \"fog\": {\n";
		json << "          \"fogColor\": ["
			<< effect->params.fog.fogColor.x << ","
			<< effect->params.fog.fogColor.y << ","
			<< effect->params.fog.fogColor.z << "],\n";
		json << "          \"fogDensity\": " << effect->params.fog.fogDensity << ",\n";
		json << "          \"fogStart\": " << effect->params.fog.fogStart << ",\n";
		json << "          \"skyFogClamp\": " << effect->params.fog.skyFogClamp << ",\n";
		json << "          \"sunInscatterStrength\": " << effect->params.fog.sunInscatterStrength << ",\n";
		json << "          \"sunColor\": ["
			<< effect->params.fog.sunColor.x << ","
			<< effect->params.fog.sunColor.y << ","
			<< effect->params.fog.sunColor.z << "],\n";
		json << "          \"heightFogTop\": " << effect->params.fog.heightFogTop << ",\n";
		json << "          \"heightFogBottom\": " << effect->params.fog.heightFogBottom << ",\n";
		json << "          \"heightFogDensity\": " << effect->params.fog.heightFogDensity << ",\n";
		json << "          \"heightFogDistanceScale\": " << effect->params.fog.heightFogDistanceScale << "\n";
		json << "        },\n";

		// God Rays
		json << "        \"godRays\": {\n";
		json << "          \"sunColor\": ["
			<< effect->params.godRays.sunColor.x << ","
			<< effect->params.godRays.sunColor.y << ","
			<< effect->params.godRays.sunColor.z << "],\n";
		json << "          \"density\": " << effect->params.godRays.density << ",\n";
		json << "          \"weight\": " << effect->params.godRays.weight << ",\n";
		json << "          \"decay\": " << effect->params.godRays.decay << ",\n";
		json << "          \"exposure\": " << effect->params.godRays.exposure << ",\n";
		json << "          \"numSamples\": " << effect->params.godRays.numSamples << ",\n";
		json << "          \"skyThreshold\": " << effect->params.godRays.skyThreshold << "\n";
		json << "        }\n";

		json << "      }\n";  // parameters
		json << "    }";

		if (i < effects.size() - 1) json << ",";
		json << "\n";
	}

	json << "  ]\n"; // effects[]
	json << "}\n";

	return json.str();
}

/// <summary>
/// JSON → ポストエフェクトチェーンへ復元
/// </summary>
bool PostEffectManager::JsonToEffectChain(const std::string& jsonStr)
{
	try {
		// まず全消去
		effectChain_->ClearAllEffects();

		// エフェクト数を取得
		int effectCount = JsonUtil::GetNumberValue<int>(jsonStr, "effectCount", 0);
		if (effectCount == 0) {
			std::cerr << "No effects found in JSON\n";
			return false;
		}

		// "effects": [...] を探す
		size_t posEffects = jsonStr.find("\"effects\":");
		if (posEffects == std::string::npos) return false;

		size_t arrayStart = jsonStr.find("[", posEffects);
		size_t arrayEnd = jsonStr.rfind("]");
		if (arrayStart == std::string::npos || arrayEnd == std::string::npos)
			return false;

		// 配列の中身を切り出す
		std::string effectsContent =
			jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

		// --- 個々の { ... } を分割 ---
		std::vector<std::string> effectObjects;
		int braceCount = 0;
		size_t objStart = 0;
		bool inObject = false;

		for (size_t i = 0; i < effectsContent.length(); ++i) {

			if (effectsContent[i] == '{') {
				if (braceCount == 0) {
					objStart = i;
					inObject = true;
				}
				braceCount++;
			} else if (effectsContent[i] == '}') {
				braceCount--;
				if (braceCount == 0 && inObject) {
					effectObjects.push_back(
						effectsContent.substr(objStart, i - objStart + 1)
					);
					inObject = false;
				}
			}
		}

		// --- 個別のエフェクトを復元 ---
		for (const auto& effectJson : effectObjects) {

			// 種類
			int typeInt = JsonUtil::GetNumberValue<int>(effectJson, "type");
			auto type = static_cast<OffScreen::OffScreenEffectType>(typeInt);

			// 名前
			std::string name = JsonUtil::GetStringValue(effectJson, "name");

			// 追加
			int index = effectChain_->AddEffect(type, name);
			auto* effect = effectChain_->GetPostEffectData(index);
			if (!effect) continue;

			// 有効フラグ
			effect->enabled = JsonUtil::GetBoolValue(effectJson, "enabled", true);

			// パラメータ抽出
			std::string paramsJson = JsonUtil::GetObjectValue(effectJson, "parameters");
			if (paramsJson.empty()) continue;

			// ----- 共通 -----
			effect->params.exposure = JsonUtil::GetNumberValue<float>(paramsJson, "exposure", 0.25f);
			effect->params.sigma = JsonUtil::GetNumberValue<float>(paramsJson, "sigma", 2.0f);
			effect->params.kernelSize = JsonUtil::GetNumberValue<int>(paramsJson, "kernelSize", 3);

			// ----- outlineColor -----
			auto outlineColor = JsonUtil::GetArrayValue(paramsJson, "outlineColor");
			if (outlineColor.size() >= 4) {
				effect->params.outlineColor =
				{ outlineColor[0], outlineColor[1], outlineColor[2], outlineColor[3] };
			}

			// ----- radialBlur -----
			std::string radialJson = JsonUtil::GetObjectValue(paramsJson, "radialBlur");
			if (!radialJson.empty()) {
				auto dir = JsonUtil::GetArrayValue(radialJson, "direction");
				if (dir.size() >= 2) {
					effect->params.radialBlur.direction = { dir[0], dir[1] };
				}
				auto center = JsonUtil::GetArrayValue(radialJson, "center");
				if (center.size() >= 2) {
					effect->params.radialBlur.center = { center[0], center[1] };
				}

				effect->params.radialBlur.width =
					JsonUtil::GetNumberValue<float>(radialJson, "width", 0.01f);

				effect->params.radialBlur.sampleCount =
					JsonUtil::GetNumberValue<int>(radialJson, "sampleCount", 10);

				effect->params.radialBlur.isRadial =
					JsonUtil::GetBoolValue(radialJson, "isRadial", true);
			}

			// ----- dissolve -----
			std::string disJson = JsonUtil::GetObjectValue(paramsJson, "dissolve");
			if (!disJson.empty()) {
				effect->params.dissolve.threshold =
					JsonUtil::GetNumberValue<float>(disJson, "threshold", 0.5f);

				effect->params.dissolve.edgeWidth =
					JsonUtil::GetNumberValue<float>(disJson, "edgeWidth", 0.1f);

				auto edgeColor = JsonUtil::GetArrayValue(disJson, "edgeColor");
				if (edgeColor.size() >= 3) {
					effect->params.dissolve.edgeColor =
					{ edgeColor[0], edgeColor[1], edgeColor[2] };
				}

				effect->params.dissolve.invert =
					JsonUtil::GetNumberValue<float>(disJson, "invert", 0.0f);
			}

			// ----- chromatic -----
			std::string chromJson = JsonUtil::GetObjectValue(paramsJson, "chromatic");
			if (!chromJson.empty()) {
				effect->params.chromatic.aberrationStrength =
					JsonUtil::GetNumberValue<float>(chromJson, "aberrationStrength", 0.02f);

				auto size = JsonUtil::GetArrayValue(chromJson, "screenSize");
				if (size.size() >= 2) {
					effect->params.chromatic.screenSize = { size[0], size[1] };
				}

				effect->params.chromatic.edgeStrength =
					JsonUtil::GetNumberValue<float>(chromJson, "edgeStrength", 1.0f);
			}

			// ----- colorAdjust -----
			std::string colJson = JsonUtil::GetObjectValue(paramsJson, "colorAdjust");
			if (!colJson.empty()) {
				effect->params.colorAdjust.brightness =
					JsonUtil::GetNumberValue<float>(colJson, "brightness", 0.0f);

				effect->params.colorAdjust.contrast =
					JsonUtil::GetNumberValue<float>(colJson, "contrast", 1.0f);

				effect->params.colorAdjust.saturation =
					JsonUtil::GetNumberValue<float>(colJson, "saturation", 1.0f);

				effect->params.colorAdjust.hue =
					JsonUtil::GetNumberValue<float>(colJson, "hue", 0.0f);
			}

			// ----- tone -----
			std::string toneJson = JsonUtil::GetObjectValue(paramsJson, "tone");
			if (!toneJson.empty()) {
				effect->params.tone.gamma =
					JsonUtil::GetNumberValue<float>(toneJson, "gamma", 2.2f);

				effect->params.tone.exposure =
					JsonUtil::GetNumberValue<float>(toneJson, "exposure", 1.0f);
			}

			// ----- shatter -----
			std::string shatterJson = JsonUtil::GetObjectValue(paramsJson, "shatter");
			if (!shatterJson.empty()) {
				effect->params.shatter.progress =
					JsonUtil::GetNumberValue<float>(shatterJson, "progress", 0.0f);

				auto resolution = JsonUtil::GetArrayValue(shatterJson, "resolution");
				if (resolution.size() >= 2) {
					effect->params.shatter.resolution = { resolution[0], resolution[1] };
				}

				effect->params.shatter.time =
					JsonUtil::GetNumberValue<float>(shatterJson, "time", 0.0f);
			}

			// ----- bloom -----
			std::string bloomJson = JsonUtil::GetObjectValue(paramsJson, "bloom");
			if (!bloomJson.empty()) {
				effect->params.bloom.threshold =
					JsonUtil::GetNumberValue<float>(bloomJson, "threshold", 0.6f);
				effect->params.bloom.intensity =
					JsonUtil::GetNumberValue<float>(bloomJson, "intensity", 0.5f);
				effect->params.bloom.spread =
					JsonUtil::GetNumberValue<float>(bloomJson, "spread", 6.0f);
				effect->params.bloom.colorTemperature =
					JsonUtil::GetNumberValue<float>(bloomJson, "colorTemperature", 0.3f);
			}

			// ----- posterize -----
			std::string posterizeJson = JsonUtil::GetObjectValue(paramsJson, "posterize");
			if (!posterizeJson.empty()) {
				effect->params.posterize.steps =
					JsonUtil::GetNumberValue<int>(posterizeJson, "steps", 5);
				effect->params.posterize.saturationBoost =
					JsonUtil::GetNumberValue<float>(posterizeJson, "saturationBoost", 1.2f);
			}

			// ----- kuwahara -----
			std::string kuwaharaJson = JsonUtil::GetObjectValue(paramsJson, "kuwahara");
			if (!kuwaharaJson.empty()) {
				effect->params.kuwahara.radius =
					JsonUtil::GetNumberValue<int>(kuwaharaJson, "radius", 4);
				effect->params.kuwahara.sharpness =
					JsonUtil::GetNumberValue<float>(kuwaharaJson, "sharpness", 4.0f);
			}

			// ----- halftone -----
			std::string halftoneJson = JsonUtil::GetObjectValue(paramsJson, "halftone");
			if (!halftoneJson.empty()) {
				effect->params.halftone.dotSize =
					JsonUtil::GetNumberValue<float>(halftoneJson, "dotSize", 6.0f);
				effect->params.halftone.angle =
					JsonUtil::GetNumberValue<float>(halftoneJson, "angle", 45.0f);
				effect->params.halftone.strength =
					JsonUtil::GetNumberValue<float>(halftoneJson, "strength", 0.85f);
				effect->params.halftone.threshold =
					JsonUtil::GetNumberValue<float>(halftoneJson, "threshold", 0.75f);
			}

			// ----- crossHatch -----
			std::string crossHatchJson = JsonUtil::GetObjectValue(paramsJson, "crossHatch");
			if (!crossHatchJson.empty()) {
				effect->params.crossHatch.lineSpacing =
					JsonUtil::GetNumberValue<float>(crossHatchJson, "lineSpacing", 8.0f);
				effect->params.crossHatch.lineWidth =
					JsonUtil::GetNumberValue<float>(crossHatchJson, "lineWidth", 1.0f);
				effect->params.crossHatch.strength =
					JsonUtil::GetNumberValue<float>(crossHatchJson, "strength", 0.7f);
			}

			// ----- colorGrade -----
			std::string colorGradeJson = JsonUtil::GetObjectValue(paramsJson, "colorGrade");
			if (!colorGradeJson.empty()) {
				auto sc = JsonUtil::GetArrayValue(colorGradeJson, "shadowColor");
				if (sc.size() >= 3)
					effect->params.colorGrade.shadowColor = { sc[0], sc[1], sc[2] };

				effect->params.colorGrade.splitBalance =
					JsonUtil::GetNumberValue<float>(colorGradeJson, "splitBalance", 0.45f);

				auto hc = JsonUtil::GetArrayValue(colorGradeJson, "highlightColor");
				if (hc.size() >= 3)
					effect->params.colorGrade.highlightColor = { hc[0], hc[1], hc[2] };

				effect->params.colorGrade.splitStrength =
					JsonUtil::GetNumberValue<float>(colorGradeJson, "splitStrength", 0.25f);
				effect->params.colorGrade.vibrance =
					JsonUtil::GetNumberValue<float>(colorGradeJson, "vibrance", 0.3f);
				effect->params.colorGrade.colorTemp =
					JsonUtil::GetNumberValue<float>(colorGradeJson, "colorTemp", 0.08f);
				effect->params.colorGrade.colorTint =
					JsonUtil::GetNumberValue<float>(colorGradeJson, "colorTint", 0.0f);
			}

			// ----- fog -----
			std::string fogJson = JsonUtil::GetObjectValue(paramsJson, "fog");
			if (!fogJson.empty()) {
				auto fc = JsonUtil::GetArrayValue(fogJson, "fogColor");
				if (fc.size() >= 3)
					effect->params.fog.fogColor = { fc[0], fc[1], fc[2] };

				effect->params.fog.fogDensity =
					JsonUtil::GetNumberValue<float>(fogJson, "fogDensity", 0.005f);
				effect->params.fog.fogStart =
					JsonUtil::GetNumberValue<float>(fogJson, "fogStart", 10.0f);
				effect->params.fog.skyFogClamp =
					JsonUtil::GetNumberValue<float>(fogJson, "skyFogClamp", 0.7f);
				effect->params.fog.sunInscatterStrength =
					JsonUtil::GetNumberValue<float>(fogJson, "sunInscatterStrength", 0.5f);

				auto sunc = JsonUtil::GetArrayValue(fogJson, "sunColor");
				if (sunc.size() >= 3)
					effect->params.fog.sunColor = { sunc[0], sunc[1], sunc[2] };

				effect->params.fog.heightFogTop =
					JsonUtil::GetNumberValue<float>(fogJson, "heightFogTop", 20.0f);
				effect->params.fog.heightFogBottom =
					JsonUtil::GetNumberValue<float>(fogJson, "heightFogBottom", -5.0f);
				effect->params.fog.heightFogDensity =
					JsonUtil::GetNumberValue<float>(fogJson, "heightFogDensity", 0.3f);
				effect->params.fog.heightFogDistanceScale =
					JsonUtil::GetNumberValue<float>(fogJson, "heightFogDistanceScale", 0.01f);
			}

			// ----- godRays -----
			std::string grJson = JsonUtil::GetObjectValue(paramsJson, "godRays");
			if (!grJson.empty()) {
				auto sc = JsonUtil::GetArrayValue(grJson, "sunColor");
				if (sc.size() >= 3)
					effect->params.godRays.sunColor = { sc[0], sc[1], sc[2] };

				effect->params.godRays.density =
					JsonUtil::GetNumberValue<float>(grJson, "density", 1.0f);
				effect->params.godRays.weight =
					JsonUtil::GetNumberValue<float>(grJson, "weight", 0.25f);
				effect->params.godRays.decay =
					JsonUtil::GetNumberValue<float>(grJson, "decay", 0.95f);
				effect->params.godRays.exposure =
					JsonUtil::GetNumberValue<float>(grJson, "exposure", 0.12f);
				effect->params.godRays.numSamples =
					JsonUtil::GetNumberValue<int>(grJson, "numSamples", 48);
				effect->params.godRays.skyThreshold =
					JsonUtil::GetNumberValue<float>(grJson, "skyThreshold", 0.999f);
			}
		}

		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Error parsing JSON: " << e.what() << std::endl;
		return false;
	}
}

/// <summary>
/// プリセット保存フォルダのパスを返す
/// </summary>
std::string PostEffectManager::GetPresetDirectory() const
{
	return PRESET_DIRECTORY;
}

/// <summary>
/// presetName → "Preset/presetName.json" を返す
/// </summary>
std::string PostEffectManager::GetPresetFilePath(const std::string& presetName) const
{
	return GetPresetDirectory() + presetName + FILE_EXTENSION;
}

/// <summary>
/// フォルダが無ければ作成する
/// </summary>
bool PostEffectManager::EnsureDirectoryExists(const std::string& dirPath) const
{
	try {
		std::filesystem::create_directories(dirPath);
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Error creating directory: " << e.what() << std::endl;
		return false;
	}
}
