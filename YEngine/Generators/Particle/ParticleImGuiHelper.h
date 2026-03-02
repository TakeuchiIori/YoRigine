#pragma once

#ifdef USE_IMGUI
#include "imgui.h"
#include "../../Utilities/Debugger/ImGuiControlsHelper.h"
#endif

#include "ParticleSetting.h"
#include <string>
#include <functional>

/// <summary>
/// パーティクル用のImGuiヘルパークラス
/// </summary>
class ParticleImGuiHelper {
public:
	using ChangeCallback = std::function<void()>;

	ParticleImGuiHelper(ParticleSetting* settings);

	// 設定変更コールバック
	void SetChangeCallback(ChangeCallback callback) { changeCallback_ = callback; }

	// カテゴリ別表示（日本語）
	void ShowBasicSettings();       // 基本設定
	void ShowPhysicsSettings();     // 物理設定
	void ShowColorSettings();       // 色設定
	void ShowRandomColorSettings(); // ランダム色設定
	void ShowVelocitySettings();    // 速度設定
	void ShowTransformSettings();   // 変形設定
	void ShowEmissionSettings();    // 発生設定
	void ShowRenderSettings();      // 描画設定
	void ShowTrailSettings();       // トレイル設定
	void ShowForceSettings();       // 力設定
	void ShowAdvancedSettings();    // 高度な設定
	void ShowRotationSettings();    // 回転設定
	void ShowCollisionSettings();   // 衝突設定
	void ShowMassSettings();        // 質量設定
	void ShowLightingSettings();    // ライティング設定

	// 全設定表示
	void ShowAllSettings();

	// プリセット用
	void ApplyFirePreset();        // 炎プリセット
	void ApplySmokePreset();       // 煙プリセット
	void ApplyMagicPreset();       // 魔法プリセット
	void ApplyExplosionPreset();   // 爆発プリセット
	void ApplyRainPreset();        // 雨プリセット
	void ApplySnowPreset();        // 雪プリセット

private:
	ParticleSetting* settings_;
	ChangeCallback changeCallback_;

	// 設定変更通知
	void OnSettingChanged();

	// ユーティリティ
	const char* GetEnumName(ParticleManagerEnums::ColorChangeType type);
	const char* GetEnumName(ParticleManagerEnums::EmissionType type);
	const char* GetEnumName(BlendMode mode);
};
