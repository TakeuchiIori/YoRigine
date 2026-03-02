#include "ParticleImGuiHelper.h"
#include <algorithm>

ParticleImGuiHelper::ParticleImGuiHelper(ParticleSetting* settings)
	: settings_(settings), changeCallback_(nullptr) {
}

void ParticleImGuiHelper::ShowBasicSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("基本設定", ImGuiTreeNodeFlags_DefaultOpen)) return;

	// 最大パーティクル数（スマートスライダー）
	int maxParticles = settings_->GetMaxParticles();
	if (ImGuiControlsHelper::SmartSliderInt("最大パーティクル数", &maxParticles, 1, 10000, 1000)) {
		settings_->SetMaxParticles(maxParticles);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("同時に存在できるパーティクルの最大数");

	// エミッション率（対数スライダー）
	float emissionRate = settings_->GetEmissionRate();
	if (ImGuiControlsHelper::LogSliderFloat("エミッション率", &emissionRate, 0.1f, 1000.0f, 10.0f, "%.1f個/秒")) {
		settings_->SetEmissionRate(emissionRate);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("1秒あたりに発生するパーティクル数");

	// 寿命範囲（範囲入力）
	Vector2 lifeTime = settings_->GetLifeTimeRange();
	ImGui::PushID("lifetime_range");
	if (ImGuiControlsHelper::RangeInputFloat("寿命範囲", &lifeTime.x, &lifeTime.y, 0.1f, 10.0f, "%.1f秒")) {
		settings_->SetLifeTimeRange(lifeTime);
		OnSettingChanged();
	}
	ImGui::PopID();
	ImGuiControlsHelper::ShowTooltip("パーティクルの寿命の最小値と最大値");

	// ループ
	bool looping = settings_->GetLooping();
	if (ImGuiControlsHelper::CheckboxWithReset("ループ再生", &looping, true)) {
		settings_->SetLooping(looping);
		OnSettingChanged();
	}

	// 持続時間（時間入力）
	float duration = settings_->GetDuration();
	if (ImGuiControlsHelper::TimeInput("持続時間", &duration, 5.0f)) {
		settings_->SetDuration(duration);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("パーティクルシステムの持続時間");

	// 開始遅延（時間入力）
	float startDelay = settings_->GetStartDelay();
	if (ImGuiControlsHelper::TimeInput("開始遅延", &startDelay, 0.0f)) {
		settings_->SetStartDelay(startDelay);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("パーティクル発生開始までの遅延時間");
#endif
}


void ParticleImGuiHelper::ShowPhysicsSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("物理設定")) return;

	bool physicsEnabled = settings_->GetIsPhysicsEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("物理演算有効", &physicsEnabled, true)) {
		settings_->SetIsPhysicsEnabled(physicsEnabled);
		OnSettingChanged();
	}

	if (physicsEnabled) {
		// 重力（ドラッグ入力）
		Vector3 gravity = settings_->GetGravity();
		if (ImGuiControlsHelper::DragFloat3WithReset("重力", &gravity, 0.1f, -50.0f, 50.0f, Vector3{ 0.0f, -9.8f, 0.0f })) {
			settings_->SetGravity(gravity);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("パーティクルに適用される重力ベクトル");

		// 空気抵抗（ドラッグ入力）
		float drag = settings_->GetDrag();
		if (ImGuiControlsHelper::DragFloatWithReset("空気抵抗", &drag, 0.01f, 0.0f, 10.0f, 0.1f)) {
			settings_->SetDrag(drag);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("パーティクルの速度減衰率");

		// 質量（プリセット付き）
		float mass = settings_->GetMass();
		const float massPresets[] = { 0.1f, 0.5f, 1.0f, 2.0f, 5.0f };
		const char* massPresetNames[] = { "軽い", "やや軽い", "標準", "重い", "とても重い" };
		if (ImGuiControlsHelper::FloatWithPresets("質量", &mass, massPresets, massPresetNames, 5, 1.0f)) {
			settings_->SetMass(mass);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("パーティクルの質量（物理演算に影響）");

		// 反発力（パーセンテージ）
		float bounciness = settings_->GetBounciness();
		if (ImGuiControlsHelper::PercentageSlider("反発力", &bounciness, 0.0f)) {
			settings_->SetBounciness(bounciness);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("衝突時の反発の強さ（0%=反発なし、100%=完全反発）");

		// 摩擦（ドラッグ入力）
		float friction = settings_->GetFriction();
		if (ImGuiControlsHelper::DragFloatWithReset("摩擦", &friction, 0.01f, 0.0f, 10.0f, 0.0f)) {
			settings_->SetFriction(friction);
			OnSettingChanged();
		}

		// 衝突判定
		bool collisionEnabled = settings_->GetCollisionEnabled();
		if (ImGuiControlsHelper::CheckboxWithReset("衝突判定", &collisionEnabled, false)) {
			settings_->SetCollisionEnabled(collisionEnabled);
			OnSettingChanged();
		}

		if (collisionEnabled) {
			float collisionRadius = settings_->GetCollisionRadius();
			if (ImGuiControlsHelper::DragFloatWithReset("衝突半径", &collisionRadius, 0.01f, 0.1f, 5.0f, 0.5f)) {
				settings_->SetCollisionRadius(collisionRadius);
				OnSettingChanged();
			}
		}
	}


	ImGui::Separator();
	ImGui::Text("乱流・ノイズ");

	// 乱流有効
	bool turbulenceEnabled = settings_->GetTurbulenceEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("乱流有効", &turbulenceEnabled, false)) {
		settings_->SetTurbulenceEnabled(turbulenceEnabled);
		OnSettingChanged();
	}

	if (turbulenceEnabled) {
		float turbulenceStrength = settings_->GetTurbulenceStrength();
		if (ImGuiControlsHelper::DragFloatWithReset("乱流強度", &turbulenceStrength, 0.1f, 0.0f, 10.0f, 1.0f)) {
			settings_->SetTurbulenceStrength(turbulenceStrength);
			OnSettingChanged();
		}

		float turbulenceFrequency = settings_->GetTurbulenceFrequency();
		if (ImGuiControlsHelper::DragFloatWithReset("乱流周波数", &turbulenceFrequency, 0.1f, 0.1f, 10.0f, 1.0f)) {
			settings_->SetTurbulenceFrequency(turbulenceFrequency);
			OnSettingChanged();
		}

		Vector3 noiseScale = settings_->GetNoiseScale();
		if (ImGuiControlsHelper::DragFloat3WithReset("ノイズスケール", &noiseScale, 0.01f, 0.1f, 10.0f, Vector3{ 1.0f, 1.0f, 1.0f })) {
			settings_->SetNoiseScale(noiseScale);
			OnSettingChanged();
		}

		float noiseSpeed = settings_->GetNoiseSpeed();
		if (ImGuiControlsHelper::DragFloatWithReset("ノイズ速度", &noiseSpeed, 0.1f, 0.0f, 5.0f, 1.0f)) {
			settings_->SetNoiseSpeed(noiseSpeed);
			OnSettingChanged();
		}
	}
#endif
}

void ParticleImGuiHelper::ShowColorSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("色設定", ImGuiTreeNodeFlags_DefaultOpen)) return;

	// カラープリセット付きピッカー
	Vector4 startColor = settings_->GetStartColor();
	ImGui::PushID("start_color");
	if (ImGuiControlsHelper::ColorPresets("開始色", &startColor, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f })) {
		settings_->SetStartColor(startColor);
		OnSettingChanged();
	}
	ImGui::PopID();
	ImGuiControlsHelper::ShowTooltip("パーティクルの初期色");

	Vector4 endColor = settings_->GetEndColor();
	ImGui::PushID("end_color");
	if (ImGuiControlsHelper::ColorPresets("終了色", &endColor, Vector4{ 1.0f, 1.0f, 1.0f, 0.0f })) {
		settings_->SetEndColor(endColor);
		OnSettingChanged();
	}
	ImGui::PopID();
	ImGuiControlsHelper::ShowTooltip("パーティクルの最終色");

	// 色変化タイプ（既存のCombo）
	const char* colorTypes[] = { "なし", "フェード", "炎", "虹色", "点滅", "グラデーション", "電気" };
	int colorType = static_cast<int>(settings_->GetColorType());
	if (ImGui::Combo("色変化タイプ", &colorType, colorTypes, 7)) {
		settings_->SetColorType(static_cast<ParticleManagerEnums::ColorChangeType>(colorType));
		OnSettingChanged();
	}

	ImGui::Separator();
	ImGui::Text("アルファ設定");

	// アルファフェードイン時間
	float alphaFadeInTime = settings_->GetAlphaFadeInTime();
	ImGui::PushID("alpha_fade_in");
	if (ImGuiControlsHelper::TimeInput("フェードイン時間", &alphaFadeInTime, 0.0f)) {
		settings_->SetAlphaFadeInTime(alphaFadeInTime);
		OnSettingChanged();
	}
	ImGui::PopID();

	// アルファフェードアウト時間
	float alphaFadeOutTime = settings_->GetAlphaFadeOutTime();
	ImGui::PushID("alpha_fade_out");
	if (ImGuiControlsHelper::TimeInput("フェードアウト時間", &alphaFadeOutTime, 1.0f)) {
		settings_->SetAlphaFadeOutTime(alphaFadeOutTime);
		OnSettingChanged();
	}
	ImGui::PopID();

	// ランダム開始色（高度な色設定から移動）
	ImGui::Separator();
	ImGui::Text("高度な色設定");

	bool randomStartColor = settings_->GetRandomStartColor();
	if (ImGuiControlsHelper::CheckboxWithReset("ランダム開始色", &randomStartColor, false)) {
		settings_->SetRandomStartColor(randomStartColor);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("パーティクルごとに異なる開始色を使用");

	if (randomStartColor) {
		ImGui::Indent();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		ImGui::Text("各パーティクルがランダムな色で生成されます");
		ImGui::PopStyleColor();
		ImGui::Unindent();
	}
#endif
}



void ParticleImGuiHelper::ShowRandomColorSettings()
{

}

void ParticleImGuiHelper::ShowVelocitySettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("速度設定")) return;

	// 方向ベクトル（専用制御）
	Vector3 baseVelocity = settings_->GetBaseVelocity();
	if (ImGuiControlsHelper::DirectionVector("基本方向", &baseVelocity, Vector3{ 0, 0, 0 })) {
		settings_->SetBaseVelocity(baseVelocity);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("パーティクルの基本移動方向");

	// 速度バリエーション
	Vector3 velocityVariation = settings_->GetVelocityVariation();
	if (ImGuiControlsHelper::DragFloat3WithReset("速度バリエーション", &velocityVariation, 0.1f, 0.0f, 20.0f, Vector3{ 1, 1, 1 })) {
		settings_->SetVelocityVariation(velocityVariation);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("基本速度に加えるランダムなバリエーション");

	// ランダム方向
	bool randomDirection = settings_->GetRandomDirection();
	if (ImGuiControlsHelper::CheckboxWithReset("ランダム方向", &randomDirection, false)) {
		settings_->SetRandomDirection(randomDirection);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("ランダムな方向に発射するか");

	// 速度（プリセット付き）
	float speed = settings_->GetSpeed();
	const float speedPresets[] = { 0.1f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f };
	const char* speedPresetNames[] = { "極低速", "低速", "標準", "高速", "超高速", "最高速", "光速" };
	if (ImGuiControlsHelper::FloatWithPresets("速度倍率", &speed, speedPresets, speedPresetNames, 7, 1.0f)) {
		settings_->SetSpeed(speed);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("パーティクルの移動速度の倍率");

	// 速度バリエーション値
	float speedVariation = settings_->GetSpeedVariation();
	if (ImGuiControlsHelper::DragFloatWithReset("速度ランダム性", &speedVariation, 0.1f, 0.0f, 10.0f, 0.0f)) {
		settings_->SetSpeedVariation(speedVariation);
		OnSettingChanged();
	}

	ImGui::Separator();
	ImGui::Text("時間経過による変化");

	// 時間経過速度変化
	bool velocityOverTime = settings_->GetVelocityOverTime();
	if (ImGuiControlsHelper::CheckboxWithReset("速度時間変化", &velocityOverTime, false)) {
		settings_->SetVelocityOverTime(velocityOverTime);
		OnSettingChanged();
	}

	if (velocityOverTime) {
		Vector3 velocityMultiplier = settings_->GetVelocityOverTimeMultiplier();
		if (ImGuiControlsHelper::DragFloat3WithReset("速度変化倍率", &velocityMultiplier, 0.01f, 0.0f, 5.0f, Vector3{ 1.0f, 1.0f, 1.0f })) {
			settings_->SetVelocityOverTimeMultiplier(velocityMultiplier);
			OnSettingChanged();
		}
	}
#endif
}

void ParticleImGuiHelper::ShowTransformSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("変形設定")) return;

	ImGui::Text("スケール設定");

	// スケール範囲
	Vector3 scaleMin = settings_->GetScaleMin();
	Vector3 scaleMax = settings_->GetScaleMax();

	if (ImGuiControlsHelper::DragFloat3WithReset("スケール最小", &scaleMin, 0.01f, 0.01f, 10.0f, Vector3{ 1, 1, 1 })) {
		settings_->SetScaleMin(scaleMin);
		OnSettingChanged();
	}

	if (ImGuiControlsHelper::DragFloat3WithReset("スケール最大", &scaleMax, 0.01f, 0.01f, 10.0f, Vector3{ 1, 1, 1 })) {
		settings_->SetScaleMax(scaleMax);
		OnSettingChanged();
	}

	// サイズアニメーション
	bool sizeOverTime = settings_->GetSizeOverTime();
	if (ImGuiControlsHelper::CheckboxWithReset("サイズアニメーション", &sizeOverTime, false)) {
		settings_->SetSizeOverTime(sizeOverTime);
		OnSettingChanged();
	}

	if (sizeOverTime) {
		float sizeMultiplierStart = settings_->GetSizeMultiplierStart();
		if (ImGuiControlsHelper::DragFloatWithReset("サイズ倍率(開始)", &sizeMultiplierStart, 0.01f, 0.0f, 100.0f, 1.0f)) {
			settings_->SetSizeMultiplierStart(sizeMultiplierStart);
			OnSettingChanged();
		}

		float sizeMultiplierEnd = settings_->GetSizeMultiplierEnd();
		if (ImGuiControlsHelper::DragFloatWithReset("サイズ倍率(終了)", &sizeMultiplierEnd, 0.01f, 0.0f, 100.0f, 1.0f)) {
			settings_->SetSizeMultiplierEnd(sizeMultiplierEnd);
			OnSettingChanged();
		}
	}

	ImGui::Separator();
	ImGui::Text("回転設定");

	// 回転（角度入力）
	Vector3 rotateMin = settings_->GetRotateMin();
	Vector3 rotateMax = settings_->GetRotateMax();

	// ラジアンから度数に変換して表示
	Vector3 rotateMinDegrees = { rotateMin.x * 180.0f / 3.14159f, rotateMin.y * 180.0f / 3.14159f, rotateMin.z * 180.0f / 3.14159f };
	Vector3 rotateMaxDegrees = { rotateMax.x * 180.0f / 3.14159f, rotateMax.y * 180.0f / 3.14159f, rotateMax.z * 180.0f / 3.14159f };

	if (ImGuiControlsHelper::DragFloat3WithReset("回転最小 (度)", &rotateMinDegrees, 1.0f, -360.0f, 360.0f, Vector3{ 0, 0, 0 })) {
		rotateMin = { rotateMinDegrees.x * 3.14159f / 180.0f, rotateMinDegrees.y * 3.14159f / 180.0f, rotateMinDegrees.z * 3.14159f / 180.0f };
		settings_->SetRotateMin(rotateMin);
		OnSettingChanged();
	}

	if (ImGuiControlsHelper::DragFloat3WithReset("回転最大 (度)", &rotateMaxDegrees, 1.0f, -360.0f, 360.0f, Vector3{ 0, 0, 0 })) {
		rotateMax = { rotateMaxDegrees.x * 3.14159f / 180.0f, rotateMaxDegrees.y * 3.14159f / 180.0f, rotateMaxDegrees.z * 3.14159f / 180.0f };
		settings_->SetRotateMax(rotateMax);
		OnSettingChanged();
	}

	// 角速度
	float angularVelocityMin = settings_->GetAngularVelocityMin();
	if (ImGuiControlsHelper::DragFloatWithReset("角速度最小", &angularVelocityMin, 0.1f, -10.0f, 10.0f, 0.0f)) {
		settings_->SetAngularVelocityMin(angularVelocityMin);
		OnSettingChanged();
	}

	float angularVelocityMax = settings_->GetAngularVelocityMax();
	if (ImGuiControlsHelper::DragFloatWithReset("角速度最大", &angularVelocityMax, 0.1f, -10.0f, 10.0f, 0.0f)) {
		settings_->SetAngularVelocityMax(angularVelocityMax);
		OnSettingChanged();
	}
#endif
}

void ParticleImGuiHelper::ShowEmissionSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("発生設定")) return;

	// 発生形状（既存のCombo）
	const char* emissionTypes[] = { "点", "球", "ボックス", "円", "リング", "コーン", "ライン", "半球" };
	int emissionType = static_cast<int>(settings_->GetEmissionType());
	if (ImGui::Combo("発生形状", &emissionType, emissionTypes, 8)) {
		settings_->SetEmissionType(static_cast<ParticleManagerEnums::EmissionType>(emissionType));
		OnSettingChanged();
	}

	// 発生半径
	float emissionRadius = settings_->GetEmissionRadius();
	if (ImGuiControlsHelper::DragFloatWithReset("発生半径", &emissionRadius, 0.1f, 0.0f, 50.0f, 1.0f)) {
		settings_->SetEmissionRadius(emissionRadius);
		OnSettingChanged();
	}

	// 発生サイズ
	Vector3 emissionSize = settings_->GetEmissionSize();
	if (ImGuiControlsHelper::DragFloat3WithReset("発生サイズ", &emissionSize, 0.1f, 0.0f, 50.0f, Vector3{ 1, 1, 1 })) {
		settings_->SetEmissionSize(emissionSize);
		OnSettingChanged();
	}

	// 発生角度（角度入力）
	float emissionAngle = settings_->GetEmissionAngle();
	if (ImGuiControlsHelper::AngleDegrees("発生角度", &emissionAngle, 25.0f * 3.14159f / 180.0f)) {
		settings_->SetEmissionAngle(emissionAngle);
		OnSettingChanged();
	}

	// 発生高さ
	float emissionHeight = settings_->GetEmissionHeight();
	if (ImGuiControlsHelper::DragFloatWithReset("発生高さ", &emissionHeight, 0.1f, 0.0f, 20.0f, 1.0f)) {
		settings_->SetEmissionHeight(emissionHeight);
		OnSettingChanged();
	}

	ImGui::Separator();
	ImGui::Text("バースト設定");

	// バースト有効
	bool burstEnabled = settings_->GetBurstEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("バースト有効", &burstEnabled, false)) {
		settings_->SetBurstEnabled(burstEnabled);
		OnSettingChanged();
	}

	if (burstEnabled) {
		int burstCount = settings_->GetBurstCount();
		if (ImGuiControlsHelper::DragIntWithReset("バースト数", &burstCount, 1, 1, 1000, 30)) {
			settings_->SetBurstCount(burstCount);
			OnSettingChanged();
		}

		float burstInterval = settings_->GetBurstInterval();
		if (ImGuiControlsHelper::TimeInput("バースト間隔", &burstInterval, 2.0f)) {
			settings_->SetBurstInterval(burstInterval);
			OnSettingChanged();
		}
	}

	// コーンエミッション用の角度設定
	if (settings_->GetEmissionType() == ParticleManagerEnums::EmissionType::Cone) {
		float coneAngle = settings_->GetConeAngle();
		float coneAngleDegrees = coneAngle * 180.0f / 3.14159f; // ラジアンから度数に変換

		if (ImGuiControlsHelper::DragFloatWithReset("コーン角度", &coneAngleDegrees, 1.0f, 0.0f, 180.0f, 30.0f, "%.1f°")) {
			float coneAngleRadians = coneAngleDegrees * 3.14159f / 180.0f; // 度数からラジアンに変換
			settings_->SetConeAngle(coneAngleRadians);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("コーン状エミッションの広がり角度");

		// コーン角度プリセット
		ImGui::Text("角度プリセット:");
		if (ImGui::Button("狭い(15°)")) {
			settings_->SetConeAngle(15.0f * 3.14159f / 180.0f);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("普通(30°)")) {
			settings_->SetConeAngle(30.0f * 3.14159f / 180.0f);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("広い(60°)")) {
			settings_->SetConeAngle(60.0f * 3.14159f / 180.0f);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("最大(90°)")) {
			settings_->SetConeAngle(90.0f * 3.14159f / 180.0f);
			OnSettingChanged();
		}
	}
#endif
}

void ParticleImGuiHelper::ShowRenderSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("描画設定")) return;

	// ブレンドモード（既存のCombo）
	const char* blendModes[] = { "なし", "通常", "加算", "減算", "乗算", "スクリーン" };
	int blendMode = static_cast<int>(settings_->GetBlendMode());
	if (ImGui::Combo("ブレンドモード", &blendMode, blendModes, 6)) {
		settings_->SetBlendMode(static_cast<BlendMode>(blendMode));
		OnSettingChanged();
	}

	// ビルボード
	bool enableBillboard = settings_->GetEnableBillboard();
	if (ImGuiControlsHelper::CheckboxWithReset("ビルボード", &enableBillboard, true)) {
		settings_->SetEnableBillboard(enableBillboard);
		OnSettingChanged();
	}

	// オフセット
	Vector3 offset = settings_->GetOffset();
	if (ImGuiControlsHelper::DragFloat3WithReset("オフセット", &offset, 0.1f, -10.0f, 10.0f, Vector3{ 0, 0, 0 })) {
		settings_->SetOffset(offset);
		OnSettingChanged();
	}

	ImGui::Separator();
	ImGui::Text("UV設定");

	// UVスケール
	Vector2 uvScale = settings_->GetUVScale();
	if (ImGuiControlsHelper::DragFloat2WithReset("UVスケール", &uvScale, 0.01f, 0.1f, 5.0f, Vector2{ 1, 1 })) {
		settings_->SetUVScale(uvScale);
		OnSettingChanged();
	}

	// UV移動
	Vector2 uvTranslate = settings_->GetUVTranslate();
	if (ImGuiControlsHelper::DragFloat2WithReset("UV移動", &uvTranslate, 0.01f, -2.0f, 2.0f, Vector2{ 0, 0 })) {
		settings_->SetUVTranslate(uvTranslate);
		OnSettingChanged();
	}

	// UV回転（角度入力）
	float uvRotate = settings_->GetUVRotate();
	if (ImGuiControlsHelper::AngleDegrees("UV回転", &uvRotate, 0.0f)) {
		settings_->SetUVRotate(uvRotate);
		OnSettingChanged();
	}

	// UVアニメーション
	bool uvAnimationEnabled = settings_->GetUVAnimationEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("UVアニメーション", &uvAnimationEnabled, false)) {
		settings_->SetUVAnimationEnabled(uvAnimationEnabled);
		OnSettingChanged();
	}

	if (uvAnimationEnabled) {
		Vector2 uvAnimationSpeed = settings_->GetUVAnimationSpeed();
		if (ImGuiControlsHelper::DragFloat2WithReset("UVアニメーション速度", &uvAnimationSpeed, 0.1f, -10.0f, 10.0f, Vector2{ 0, 0 })) {
			settings_->SetUVAnimationSpeed(uvAnimationSpeed);
			OnSettingChanged();
		}
	}

	ImGui::Separator();
	ImGui::Text("テクスチャシート");

	// テクスチャシート有効
	bool textureSheetEnabled = settings_->GetTextureSheetEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("テクスチャシート", &textureSheetEnabled, false)) {
		settings_->SetTextureSheetEnabled(textureSheetEnabled);
		OnSettingChanged();
	}

	if (textureSheetEnabled) {
		Vector2 textureSheetTiles = settings_->GetTextureSheetTiles();
		if (ImGuiControlsHelper::DragFloat2WithReset("タイル数", &textureSheetTiles, 0.1f, 1.0f, 16.0f, Vector2{ 1, 1 })) {
			settings_->SetTextureSheetTiles(textureSheetTiles);
			OnSettingChanged();
		}

		float textureSheetFrameRate = settings_->GetTextureSheetFrameRate();
		if (ImGuiControlsHelper::DragFloatWithReset("フレームレート", &textureSheetFrameRate, 1.0f, 1.0f, 120.0f, 30.0f)) {
			settings_->SetTextureSheetFrameRate(textureSheetFrameRate);
			OnSettingChanged();
		}
	}
#endif
}

void ParticleImGuiHelper::ShowTrailSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("トレイル設定")) return;

	// トレイル有効
	bool trailEnabled = settings_->GetTrailEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("トレイル有効", &trailEnabled, false)) {
		settings_->SetTrailEnabled(trailEnabled);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("パーティクルの軌跡を描画する");

	if (trailEnabled) {
		// トレイル長（プリセット付き）
		int trailLength = settings_->GetTrailLength();
		const int lengthPresets[] = { 5, 10, 20, 50, 100 };
		const char* lengthPresetNames[] = { "短い", "標準", "長い", "とても長い", "最長" };
		if (ImGuiControlsHelper::DragIntWithReset("トレイル長", &trailLength, 1, 2, 100, 10)) {
			settings_->SetTrailLength(trailLength);
			OnSettingChanged();
		}

		// プリセットボタン
		ImGui::Text("長さプリセット:");
		for (int i = 0; i < 5; ++i) {
			ImGui::PushID(i);
			if (ImGui::Button(lengthPresetNames[i])) {
				settings_->SetTrailLength(lengthPresets[i]);
				OnSettingChanged();
			}
			if (i < 4) ImGui::SameLine();
			ImGui::PopID();
		}
		ImGuiControlsHelper::ShowTooltip("トレイルの長さ（セグメント数）");

		// トレイル幅
		float trailWidth = settings_->GetTrailWidth();
		if (ImGuiControlsHelper::DragFloatWithReset("トレイル幅", &trailWidth, 0.01f, 0.01f, 5.0f, 0.1f)) {
			settings_->SetTrailWidth(trailWidth);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("トレイルの太さ");

		// トレイル色（プリセット付き）
		Vector4 trailColor = settings_->GetTrailColor();
		if (ImGuiControlsHelper::ColorPresets("トレイル色", &trailColor, Vector4{ 1, 1, 1, 0.5f })) {
			settings_->SetTrailColor(trailColor);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("トレイルの色と透明度");
	}
#endif
}

void ParticleImGuiHelper::ShowForceSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("力設定")) return;

	// 時間経過力
	bool forceOverTime = settings_->GetForceOverTime();
	if (ImGuiControlsHelper::CheckboxWithReset("時間経過力", &forceOverTime, false)) {
		settings_->SetForceOverTime(forceOverTime);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("時間の経過と共に力を適用する");

	if (forceOverTime) {
		Vector3 forceVector = settings_->GetForceVector();
		if (ImGuiControlsHelper::DragFloat3WithReset("力ベクトル", &forceVector, 0.1f, -20.0f, 20.0f, Vector3{ 0, 0, 0 })) {
			settings_->SetForceVector(forceVector);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("適用する力の方向と強さ");
	}

	ImGui::Separator();
	ImGui::Text("渦力設定");

	// 渦力有効
	bool vortexEnabled = settings_->GetVortexEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("渦力有効", &vortexEnabled, false)) {
		settings_->SetVortexEnabled(vortexEnabled);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("渦状の力場を作成する");

	if (vortexEnabled) {
		Vector3 vortexCenter = settings_->GetVortexCenter();
		if (ImGuiControlsHelper::PositionVector("渦中心", &vortexCenter, Vector3{ 0, 0, 0 })) {
			settings_->SetVortexCenter(vortexCenter);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("渦の中心位置");

		// 渦強度（プリセット付き）
		float vortexStrength = settings_->GetVortexStrength();
		const float strengthPresets[] = { 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
		const char* strengthPresetNames[] = { "弱い", "標準", "強い", "とても強い", "最強" };
		if (ImGuiControlsHelper::FloatWithPresets("渦強度", &vortexStrength, strengthPresets, strengthPresetNames, 5, 1.0f)) {
			settings_->SetVortexStrength(vortexStrength);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("渦力の強さ");

		float vortexRadius = settings_->GetVortexRadius();
		if (ImGuiControlsHelper::DragFloatWithReset("渦半径", &vortexRadius, 0.1f, 1.0f, 50.0f, 5.0f)) {
			settings_->SetVortexRadius(vortexRadius);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("渦力が影響する範囲");
	}
#endif
}

void ParticleImGuiHelper::ShowAdvancedSettings() {
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("高度な設定")) return;

	ImGui::Text("継承設定");

	// 変換速度継承
	bool inheritTransformVelocity = settings_->GetInheritTransformVelocity();
	if (ImGuiControlsHelper::CheckboxWithReset("変換速度継承", &inheritTransformVelocity, false)) {
		settings_->SetInheritTransformVelocity(inheritTransformVelocity);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("エミッターの移動速度をパーティクルが継承する");

	if (inheritTransformVelocity) {
		float inheritVelocityMultiplier = settings_->GetInheritVelocityMultiplier();
		if (ImGuiControlsHelper::PercentageSlider("速度継承倍率", &inheritVelocityMultiplier, 1.0f)) {
			settings_->SetInheritVelocityMultiplier(inheritVelocityMultiplier);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("継承する速度の倍率");
	}

	ImGui::Separator();
	ImGui::Text("最適化設定");

	// カリング有効
	bool cullingEnabled = settings_->GetCullingEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("カリング有効", &cullingEnabled, true)) {
		settings_->SetCullingEnabled(cullingEnabled);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("距離によるパーティクルの描画制限");

	if (cullingEnabled) {
		float cullingDistance = settings_->GetCullingDistance();
		// 対数スケールで距離を調整（1～1000の範囲を扱いやすく）
		if (ImGuiControlsHelper::LogSliderFloat("カリング距離", &cullingDistance, 1.0f, 1000.0f, 100.0f, "%.0fm")) {
			settings_->SetCullingDistance(cullingDistance);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("この距離を超えると描画しない");
	}

	// LOD有効
	bool lodEnabled = settings_->GetLODEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("LOD有効", &lodEnabled, false)) {
		settings_->SetLODEnabled(lodEnabled);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("距離に応じて詳細度を調整");

	if (lodEnabled) {
		float lodDistance1 = settings_->GetLODDistance1();
		float lodDistance2 = settings_->GetLODDistance2();

		// 範囲スライダーでLOD距離を設定
		if (ImGuiControlsHelper::RangeSliderFloat("LOD距離", &lodDistance1, &lodDistance2, 1.0f, 200.0f, 25.0f, 50.0f)) {
			settings_->SetLODDistance1(lodDistance1);
			settings_->SetLODDistance2(lodDistance2);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("LOD切り替え距離（近い距離/遠い距離）");

		// LOD説明
		ImGui::Text("LOD段階:");
		ImGui::Text("  高品質: 0 ～ %.0fm", lodDistance1);
		ImGui::Text("  中品質: %.0f ～ %.0fm", lodDistance1, lodDistance2);
		ImGui::Text("  低品質: %.0fm ～", lodDistance2);
	}

	ImGui::Separator();
	ImGui::Text("デバッグ設定");

	// デバッグ表示用の追加情報
	ImGui::Text("現在の設定概要:");
	ImGui::Text("  パーティクル数: %d", settings_->GetMaxParticles());
	ImGui::Text("  エミッション率: %.1f/秒", settings_->GetEmissionRate());
	ImGui::Text("  平均寿命: %.1f秒", (settings_->GetLifeTimeRange().x + settings_->GetLifeTimeRange().y) * 0.5f);

	float estimatedParticles = settings_->GetEmissionRate() * (settings_->GetLifeTimeRange().x + settings_->GetLifeTimeRange().y) * 0.5f;
	ImGui::Text("  推定アクティブ数: %.0f", estimatedParticles);

	if (estimatedParticles > settings_->GetMaxParticles()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
		ImGui::Text("  警告: 推定数が最大数を超過！");
		ImGui::PopStyleColor();
	}
#endif
}

void ParticleImGuiHelper::ShowRotationSettings()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("ランダム回転設定")) return;

	// ランダム回転有効
	bool randomRotationEnabled = settings_->GetRandomRotationEnabled();
	if (ImGuiControlsHelper::CheckboxWithReset("ランダム回転有効", &randomRotationEnabled, false)) {
		settings_->SetRandomRotationEnabled(randomRotationEnabled);
		OnSettingChanged();
	}
	ImGuiControlsHelper::ShowTooltip("パーティクル生成時に各軸をランダムに回転させる");

	if (randomRotationEnabled) {
		ImGui::Separator();
		ImGui::Text("初期回転設定");

		// 軸ごとに独立したランダム化
		bool randomRotationPerAxis = settings_->GetRandomRotationPerAxis();
		if (ImGuiControlsHelper::CheckboxWithReset("軸ごとに独立", &randomRotationPerAxis, true)) {
			settings_->SetRandomRotationPerAxis(randomRotationPerAxis);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("チェック時：各軸が独立してランダム化、未チェック時：統一的なランダム回転");

		// ランダム回転範囲（度数で表示）
		Vector3 randomRotationRange = settings_->GetRandomRotationRange();
		if (randomRotationPerAxis) {
			// 各軸個別設定
			if (ImGuiControlsHelper::AngleDegrees("X軸回転範囲", &randomRotationRange.x, 0.0f)) {
				settings_->SetRandomRotationRange(randomRotationRange);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("X軸周りのランダム回転範囲（±この角度内）");

			if (ImGuiControlsHelper::AngleDegrees("Y軸回転範囲", &randomRotationRange.y, 0.0f)) {
				settings_->SetRandomRotationRange(randomRotationRange);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("Y軸周りのランダム回転範囲（±この角度内）");

			if (ImGuiControlsHelper::AngleDegrees("Z軸回転範囲", &randomRotationRange.z, 0.0f)) {
				settings_->SetRandomRotationRange(randomRotationRange);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("Z軸周りのランダム回転範囲（±この角度内）");
		} else {
			// 統一設定
			float uniformRange = randomRotationRange.x;
			if (ImGuiControlsHelper::AngleDegrees("統一回転範囲", &uniformRange, 0.0f)) {
				randomRotationRange = Vector3{ uniformRange, uniformRange, uniformRange };
				settings_->SetRandomRotationRange(randomRotationRange);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("全軸共通のランダム回転範囲");
		}

		// プリセットボタン
		ImGui::Text("回転範囲プリセット:");
		if (ImGui::Button("微小")) {
			Vector3 preset = randomRotationPerAxis ? Vector3{ 15.0f, 15.0f, 15.0f } : Vector3{ 15.0f, 15.0f, 15.0f };
			settings_->SetRandomRotationRange(preset);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("小")) {
			Vector3 preset = randomRotationPerAxis ? Vector3{ 45.0f, 45.0f, 45.0f } : Vector3{ 45.0f, 45.0f, 45.0f };
			settings_->SetRandomRotationRange(preset);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("中")) {
			Vector3 preset = randomRotationPerAxis ? Vector3{ 90.0f, 90.0f, 90.0f } : Vector3{ 90.0f, 90.0f, 90.0f };
			settings_->SetRandomRotationRange(preset);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("大")) {
			Vector3 preset = randomRotationPerAxis ? Vector3{ 180.0f, 180.0f, 180.0f } : Vector3{ 180.0f, 180.0f, 180.0f };
			settings_->SetRandomRotationRange(preset);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("完全")) {
			Vector3 preset = randomRotationPerAxis ? Vector3{ 360.0f, 360.0f, 360.0f } : Vector3{ 360.0f, 360.0f, 360.0f };
			settings_->SetRandomRotationRange(preset);
			OnSettingChanged();
		}

		ImGui::Separator();
		ImGui::Text("回転速度設定");

		// ランダム回転速度
		Vector3 randomRotationSpeed = settings_->GetRandomRotationSpeed();
		if (randomRotationPerAxis) {
			// 各軸個別の回転速度
			if (ImGuiControlsHelper::DragFloatWithReset("X軸回転速度", &randomRotationSpeed.x, 0.1f, -10.0f, 10.0f, 0.0f, "%.2f rad/s")) {
				settings_->SetRandomRotationSpeed(randomRotationSpeed);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("X軸周りの回転速度（ランダム範囲の最大値）");

			if (ImGuiControlsHelper::DragFloatWithReset("Y軸回転速度", &randomRotationSpeed.y, 0.1f, -10.0f, 10.0f, 0.0f, "%.2f rad/s")) {
				settings_->SetRandomRotationSpeed(randomRotationSpeed);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("Y軸周りの回転速度（ランダム範囲の最大値）");

			if (ImGuiControlsHelper::DragFloatWithReset("Z軸回転速度", &randomRotationSpeed.z, 0.1f, -10.0f, 10.0f, 0.0f, "%.2f rad/s")) {
				settings_->SetRandomRotationSpeed(randomRotationSpeed);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("Z軸周りの回転速度（ランダム範囲の最大値）");
		} else {
			// 統一回転速度
			float uniformSpeed = randomRotationSpeed.x;
			if (ImGuiControlsHelper::DragFloatWithReset("統一回転速度", &uniformSpeed, 0.1f, -10.0f, 10.0f, 0.0f, "%.2f rad/s")) {
				randomRotationSpeed = Vector3{ uniformSpeed, uniformSpeed, uniformSpeed };
				settings_->SetRandomRotationSpeed(randomRotationSpeed);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("全軸共通の回転速度");
		}

		// 回転速度プリセット
		ImGui::Text("回転速度プリセット:");
		if (ImGui::Button("停止")) {
			settings_->SetRandomRotationSpeed(Vector3{ 0.0f, 0.0f, 0.0f });
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("ゆっくり")) {
			Vector3 preset = randomRotationPerAxis ? Vector3{ 0.5f, 0.5f, 0.5f } : Vector3{ 0.5f, 0.5f, 0.5f };
			settings_->SetRandomRotationSpeed(preset);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("普通")) {
			Vector3 preset = randomRotationPerAxis ? Vector3{ 1.0f, 1.0f, 1.0f } : Vector3{ 1.0f, 1.0f, 1.0f };
			settings_->SetRandomRotationSpeed(preset);
			OnSettingChanged();
		}
		ImGui::SameLine();
		if (ImGui::Button("高速")) {
			Vector3 preset = randomRotationPerAxis ? Vector3{ 3.0f, 3.0f, 3.0f } : Vector3{ 3.0f, 3.0f, 3.0f };
			settings_->SetRandomRotationSpeed(preset);
			OnSettingChanged();
		}

		ImGui::Separator();
		ImGui::Text("継承とオプション");

		// 初期回転継承
		bool inheritInitialRotation = settings_->GetInheritInitialRotation();
		if (ImGuiControlsHelper::CheckboxWithReset("初期回転継承", &inheritInitialRotation, false)) {
			settings_->SetInheritInitialRotation(inheritInitialRotation);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("パーティクルシステムの初期回転にランダム回転を加算する");

		ImGui::Separator();
		ImGui::Text("時間経過による変化");

		// 時間経過回転
		bool rotationOverTime = settings_->GetRotationOverTime();
		if (ImGuiControlsHelper::CheckboxWithReset("回転時間変化", &rotationOverTime, false)) {
			settings_->SetRotationOverTime(rotationOverTime);
			OnSettingChanged();
		}
		ImGuiControlsHelper::ShowTooltip("時間経過と共に回転速度を変化させる");

		if (rotationOverTime) {
			// 回転加速度
			Vector3 rotationAcceleration = settings_->GetRotationAcceleration();
			if (ImGuiControlsHelper::DragFloat3WithReset("回転加速度", &rotationAcceleration, 0.01f, -5.0f, 5.0f, Vector3{ 0.0f, 0.0f, 0.0f })) {
				settings_->SetRotationAcceleration(rotationAcceleration);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("各軸の回転加速度（正の値で加速、負の値で減速）");

			// 回転減衰
			float rotationDamping = settings_->GetRotationDamping();
			if (ImGuiControlsHelper::PercentageSlider("回転減衰率", &rotationDamping, 0.0f)) {
				settings_->SetRotationDamping(rotationDamping);
				OnSettingChanged();
			}
			ImGuiControlsHelper::ShowTooltip("回転速度の減衰率（0%=減衰なし、100%=即座に停止）");
		}

		ImGui::Separator();
		ImGui::Text("プレビュー情報");

		// 現在の設定の概要表示
		Vector3 currentRange = settings_->GetRandomRotationRange();
		Vector3 currentSpeed = settings_->GetRandomRotationSpeed();

		ImGui::Text("回転範囲: X=±%.1f° Y=±%.1f° Z=±%.1f°",
			currentRange.x * 180.0f / 3.14159f,
			currentRange.y * 180.0f / 3.14159f,
			currentRange.z * 180.0f / 3.14159f);

		ImGui::Text("回転速度: X=%.2f Y=%.2f Z=%.2f rad/s",
			currentSpeed.x, currentSpeed.y, currentSpeed.z);

		// 効果の説明
		if (currentRange.x > 0.0f || currentRange.y > 0.0f || currentRange.z > 0.0f) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
			ImGui::Text("ランダム初期回転: 有効");
			ImGui::PopStyleColor();
		}

		if (currentSpeed.x > 0.0f || currentSpeed.y > 0.0f || currentSpeed.z > 0.0f) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
			ImGui::Text("継続回転: 有効");
			ImGui::PopStyleColor();
		}
	}
#endif
}

void ParticleImGuiHelper::ShowCollisionSettings()
{
	//if (!ImGui::CollapsingHeader("衝突設定")) return;

	//// 衝突有効
	//bool collisionEnabled = settings_->GetCollisionEnabled();
	//if (ImGuiControlsHelper::CheckboxWithReset("衝突有効", &collisionEnabled, false)) {
	//    settings_->SetCollisionEnabled(collisionEnabled);
	//    OnSettingChanged();
	//}
	//ImGuiControlsHelper::ShowTooltip("パーティクルの衝突判定を有効にする");

	//if (collisionEnabled) {
	//    ImGui::Separator();

	//    // 基本衝突設定
	//    ImGui::Text("基本設定");

	//    float collisionRadius = settings_->GetCollisionRadius();
	//    if (ImGuiControlsHelper::DragFloatWithReset("衝突半径", &collisionRadius, 0.01f, 0.001f, 10.0f, 0.1f)) {
	//        settings_->SetCollisionRadius(collisionRadius);
	//        OnSettingChanged();
	//    }
	//    ImGuiControlsHelper::ShowTooltip("パーティクルの衝突判定半径");

	//    float restitution = settings_->GetCollisionRestitution();
	//    if (ImGuiControlsHelper::PercentageSlider("反発係数", &restitution, 0.5f)) {
	//        settings_->SetCollisionRestitution(restitution);
	//        OnSettingChanged();
	//    }
	//    ImGuiControlsHelper::ShowTooltip("跳ね返りの強さ（0%=跳ねない、100%=完全反発）");

	//    float friction = settings_->GetCollisionFriction();
	//    if (ImGuiControlsHelper::PercentageSlider("摩擦係数", &friction, 0.1f)) {
	//        settings_->SetCollisionFriction(friction);
	//        OnSettingChanged();
	//    }
	//    ImGuiControlsHelper::ShowTooltip("衝突時の速度減衰（0%=摩擦なし、100%=即座に停止）");

	//    ImGui::Separator();
	//    ImGui::Text("球体コライダー");

	//    // 球体コライダーリスト表示
	//    auto colliders = settings_->GetSphereColliders();
	//    for (size_t i = 0; i < colliders.size(); ++i) {
	//        ImGui::PushID(static_cast<int>(i));

	//        if (ImGui::TreeNode(("コライダー " + std::to_string(i + 1)).c_str())) {
	//            SphereCollider collider = colliders[i];
	//            bool changed = false;

	//            // 中心座標
	//            if (ImGuiControlsHelper::DragFloat3WithReset("中心座標", &collider.center, 0.1f, -100.0f, 100.0f, Vector3{ 0, 0, 0 })) {
	//                changed = true;
	//            }

	//            // 半径
	//            if (ImGuiControlsHelper::DragFloatWithReset("半径", &collider.radius, 0.1f, 0.1f, 50.0f, 1.0f)) {
	//                changed = true;
	//            }

	//            // 反発係数
	//            if (ImGuiControlsHelper::PercentageSlider("反発係数", &collider.restitution, 0.5f)) {
	//                changed = true;
	//            }

	//            // 摩擦係数
	//            if (ImGuiControlsHelper::PercentageSlider("摩擦係数", &collider.friction, 0.1f)) {
	//                changed = true;
	//            }

	//            // アクティブフラグ
	//            if (ImGuiControlsHelper::CheckboxWithReset("有効", &collider.isActive, true)) {
	//                changed = true;
	//            }

	//            // 削除ボタン
	//            if (ImGui::Button("削除")) {
	//                settings_->RemoveSphereCollider(i);
	//                OnSettingChanged();
	//            }

	//            if (changed) {
	//                // 変更をリストに反映
	//                auto newColliders = settings_->GetSphereColliders();
	//                newColliders[i] = collider;
	//                settings_->SetSphereColliders(newColliders);
	//                OnSettingChanged();
	//            }

	//            ImGui::TreePop();
	//        }

	//        ImGui::PopID();
	//    }

	//    // 新しいコライダー追加
	//    if (ImGui::Button("コライダー追加")) {
	//        SphereCollider newCollider(Vector3{ 0, 0, 0 }, 1.0f);
	//        settings_->AddSphereCollider(newCollider);
	//        OnSettingChanged();
	//    }

	//    ImGui::SameLine();
	//    if (ImGui::Button("全削除")) {
	//        settings_->ClearSphereColliders();
	//        OnSettingChanged();
	//    }

	//    // プリセットコライダー
	//    ImGui::Separator();
	//    ImGui::Text("プリセット:");

	//    if (ImGui::Button("地面")) {
	//        SphereCollider ground(Vector3{ 0, -10, 0 }, 10.0f, 0.3f, 0.5f);
	//        settings_->AddSphereCollider(ground);
	//        OnSettingChanged();
	//    }
	//    ImGui::SameLine();
	//    if (ImGui::Button("壁")) {
	//        SphereCollider wall(Vector3{ 5, 0, 0 }, 1.0f, 0.8f, 0.1f);
	//        settings_->AddSphereCollider(wall);
	//        OnSettingChanged();
	//    }
	//    ImGui::SameLine();
	//    if (ImGui::Button("天井")) {
	//        SphereCollider ceiling(Vector3{ 0, 10, 0 }, 10.0f, 0.5f, 0.2f);
	//        settings_->AddSphereCollider(ceiling);
	//        OnSettingChanged();
	//    }
	//}
}

void ParticleImGuiHelper::ShowMassSettings()
{

}
/// <summary>
/// ライティングの設定
/// </summary>
void ParticleImGuiHelper::ShowLightingSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Lighting Settings", ImGuiTreeNodeFlags_DefaultOpen)) {

		// ライティング有効/無効
		bool enableLighting = settings_->GetEnableLighting();
		if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
			settings_->SetEnableLighting(enableLighting);
			OnSettingChanged();
		}

		if (enableLighting) {
			ImGui::Indent();
			ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Light affects from LightManager");
			ImGui::Text("- Directional Light");
			ImGui::Text("- Point Light");
			ImGui::Text("- Spot Light");
			ImGui::Text("- Specular Reflection");
			ImGui::Unindent();

			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
				"Note: Configure lights in LightManager");
		}
	}
#endif
}

void ParticleImGuiHelper::ShowAllSettings() {
	ShowBasicSettings();
	ShowPhysicsSettings();
	ShowColorSettings();
	ShowRandomColorSettings();
	ShowVelocitySettings();
	ShowTransformSettings();
	ShowRotationSettings();
	ShowEmissionSettings();
	ShowRenderSettings();
	ShowLightingSettings();
	ShowTrailSettings();
	ShowForceSettings();
	ShowAdvancedSettings();
	ShowCollisionSettings();
	ShowMassSettings();
}

// プリセット適用メソッド（既存のまま）
void ParticleImGuiHelper::ApplyFirePreset() {
	settings_->SetStartColor(Vector4{ 1.0f, 0.3f, 0.0f, 1.0f });
	settings_->SetEndColor(Vector4{ 1.0f, 1.0f, 0.0f, 0.0f });
	settings_->SetColorType(ParticleManagerEnums::ColorChangeType::Fire);
	settings_->SetBaseVelocity(Vector3{ 0, 2, 0 });
	settings_->SetVelocityVariation(Vector3{ 1, 0.5f, 1 });
	settings_->SetGravity(Vector3{ 0, 1, 0 });
	settings_->SetScaleMin(Vector3{ 0.5f, 0.5f, 0.5f });
	settings_->SetScaleMax(Vector3{ 1.5f, 1.5f, 1.5f });
	settings_->SetLifeTimeRange(Vector2{ 0.5f, 2.0f });
	settings_->SetEmissionRate(50.0f);
	settings_->SetBlendMode(BlendMode::kBlendModeAdd);
	OnSettingChanged();
}

void ParticleImGuiHelper::ApplySmokePreset() {
	settings_->SetStartColor(Vector4{ 0.8f, 0.8f, 0.8f, 0.8f });
	settings_->SetEndColor(Vector4{ 0.5f, 0.5f, 0.5f, 0.0f });
	settings_->SetColorType(ParticleManagerEnums::ColorChangeType::Fade);
	settings_->SetBaseVelocity(Vector3{ 0, 1.5f, 0 });
	settings_->SetVelocityVariation(Vector3{ 2, 0.5f, 2 });
	settings_->SetGravity(Vector3{ 0, -0.5f, 0 });
	settings_->SetScaleMin(Vector3{ 1.0f, 1.0f, 1.0f });
	settings_->SetScaleMax(Vector3{ 3.0f, 3.0f, 3.0f });
	settings_->SetLifeTimeRange(Vector2{ 2.0f, 5.0f });
	settings_->SetEmissionRate(20.0f);
	settings_->SetBlendMode(BlendMode::kBlendModeNormal);
	OnSettingChanged();
}

void ParticleImGuiHelper::ApplyMagicPreset() {
	settings_->SetStartColor(Vector4{ 0.5f, 0.0f, 1.0f, 1.0f });
	settings_->SetEndColor(Vector4{ 1.0f, 0.5f, 1.0f, 0.0f });
	settings_->SetColorType(ParticleManagerEnums::ColorChangeType::Rainbow);
	settings_->SetBaseVelocity(Vector3{ 0, 0, 0 });
	settings_->SetVelocityVariation(Vector3{ 3, 3, 3 });
	settings_->SetRandomDirection(true);
	settings_->SetGravity(Vector3{ 0, 0, 0 });
	settings_->SetScaleMin(Vector3{ 0.2f, 0.2f, 0.2f });
	settings_->SetScaleMax(Vector3{ 0.8f, 0.8f, 0.8f });
	settings_->SetLifeTimeRange(Vector2{ 1.0f, 3.0f });
	settings_->SetEmissionRate(100.0f);
	settings_->SetBlendMode(BlendMode::kBlendModeAdd);

	// 魔法効果のためのトレイル追加
	settings_->SetTrailEnabled(true);
	settings_->SetTrailLength(15);
	settings_->SetTrailWidth(0.05f);
	settings_->SetTrailColor(Vector4{ 0.8f, 0.3f, 1.0f, 0.7f });

	OnSettingChanged();
}

void ParticleImGuiHelper::ApplyExplosionPreset() {
	settings_->SetStartColor(Vector4{ 1.0f, 0.8f, 0.0f, 1.0f });
	settings_->SetEndColor(Vector4{ 1.0f, 0.0f, 0.0f, 0.0f });
	settings_->SetColorType(ParticleManagerEnums::ColorChangeType::Fire);
	settings_->SetBaseVelocity(Vector3{ 0, 0, 0 });
	settings_->SetVelocityVariation(Vector3{ 10, 10, 10 });
	settings_->SetRandomDirection(true);
	settings_->SetSpeed(15.0f);
	settings_->SetGravity(Vector3{ 0, -5, 0 });
	settings_->SetScaleMin(Vector3{ 0.5f, 0.5f, 0.5f });
	settings_->SetScaleMax(Vector3{ 2.0f, 2.0f, 2.0f });
	settings_->SetLifeTimeRange(Vector2{ 0.5f, 2.0f });
	settings_->SetEmissionType(ParticleManagerEnums::EmissionType::Sphere);
	settings_->SetBurstEnabled(true);
	settings_->SetBurstCount(200);
	settings_->SetBlendMode(BlendMode::kBlendModeAdd);
	OnSettingChanged();
}

void ParticleImGuiHelper::ApplyRainPreset() {
	settings_->SetStartColor(Vector4{ 0.7f, 0.8f, 1.0f, 0.8f });
	settings_->SetEndColor(Vector4{ 0.7f, 0.8f, 1.0f, 0.8f });
	settings_->SetColorType(ParticleManagerEnums::ColorChangeType::None);
	settings_->SetBaseVelocity(Vector3{ 0, -20, 0 });
	settings_->SetVelocityVariation(Vector3{ 2, 2, 2 });
	settings_->SetGravity(Vector3{ 0, -30, 0 });
	settings_->SetScaleMin(Vector3{ 0.1f, 1.0f, 0.1f });
	settings_->SetScaleMax(Vector3{ 0.2f, 2.0f, 0.2f });
	settings_->SetLifeTimeRange(Vector2{ 2.0f, 4.0f });
	settings_->SetEmissionType(ParticleManagerEnums::EmissionType::Box);
	settings_->SetEmissionSize(Vector3{ 20, 1, 20 });
	settings_->SetEmissionRate(200.0f);
	settings_->SetBlendMode(BlendMode::kBlendModeNormal);

	// 雨のトレイル
	settings_->SetTrailEnabled(true);
	settings_->SetTrailLength(5);
	settings_->SetTrailWidth(0.02f);
	settings_->SetTrailColor(Vector4{ 0.7f, 0.8f, 1.0f, 0.6f });

	OnSettingChanged();
}

void ParticleImGuiHelper::ApplySnowPreset() {
	settings_->SetStartColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
	settings_->SetEndColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
	settings_->SetColorType(ParticleManagerEnums::ColorChangeType::None);
	settings_->SetBaseVelocity(Vector3{ 0, -2, 0 });
	settings_->SetVelocityVariation(Vector3{ 1, 0.5f, 1 });
	settings_->SetGravity(Vector3{ 0, -1, 0 });
	settings_->SetScaleMin(Vector3{ 0.2f, 0.2f, 0.2f });
	settings_->SetScaleMax(Vector3{ 0.5f, 0.5f, 0.5f });
	settings_->SetLifeTimeRange(Vector2{ 5.0f, 10.0f });
	settings_->SetEmissionType(ParticleManagerEnums::EmissionType::Box);
	settings_->SetEmissionSize(Vector3{ 15, 1, 15 });
	settings_->SetEmissionRate(50.0f);
	settings_->SetBlendMode(BlendMode::kBlendModeNormal);

	// 風の影響を追加
	settings_->SetTurbulenceEnabled(true);
	settings_->SetTurbulenceStrength(0.5f);
	settings_->SetTurbulenceFrequency(0.3f);

	OnSettingChanged();
}

void ParticleImGuiHelper::OnSettingChanged() {
	if (changeCallback_) {
		changeCallback_();
	}
}

const char* ParticleImGuiHelper::GetEnumName(ParticleManagerEnums::ColorChangeType type) {
	const char* names[] = { "なし", "フェード", "炎", "虹色", "点滅", "グラデーション", "電気" };
	int index = static_cast<int>(type);
	return (index >= 0 && index < 7) ? names[index] : "不明";
}

const char* ParticleImGuiHelper::GetEnumName(ParticleManagerEnums::EmissionType type) {
	const char* names[] = { "点", "球", "ボックス", "円", "リング", "コーン", "ライン", "半球" };
	int index = static_cast<int>(type);
	return (index >= 0 && index < 8) ? names[index] : "不明";
}

const char* ParticleImGuiHelper::GetEnumName(BlendMode mode) {
	const char* names[] = { "なし", "通常", "加算", "減算", "乗算", "スクリーン" };
	int index = static_cast<int>(mode);
	return (index >= 0 && index < 6) ? names[index] : "不明";
}