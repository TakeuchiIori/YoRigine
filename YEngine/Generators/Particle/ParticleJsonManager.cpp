#include "ParticleJsonManager.h"
#include "Loaders/Json/JsonConverters.h"
#include <iostream>

/// <summary>
/// 指定したパーティクルシステムの設定を JSON として保存
/// </summary>
bool ParticleJsonManager::SaveSettings(const std::string& systemName, const ParticleSetting& settings) {
	std::string filePath = GetSettingsPath(systemName);
	return SaveToFile(filePath, settings);
}

/// <summary>
/// 指定したパーティクルシステム名の JSON 設定を読み込む
/// </summary>
bool ParticleJsonManager::LoadSettings(const std::string& systemName, ParticleSetting& settings) {
	std::string filePath = GetSettingsPath(systemName);
	return LoadFromFile(filePath, settings);
}

/// <summary>
/// プリセットを JSON として保存
/// </summary>
bool ParticleJsonManager::SavePreset(const std::string& presetName, const ParticleSetting& settings) {
	std::string filePath = GetPresetPath(presetName);
	return SaveToFile(filePath, settings);
}

/// <summary>
/// プリセットを JSON から読み込む
/// </summary>
bool ParticleJsonManager::LoadPreset(const std::string& presetName, ParticleSetting& settings) {
	std::string filePath = GetPresetPath(presetName);
	return LoadFromFile(filePath, settings);
}

/// <summary>
/// Settings フォルダ内の JSON ファイル一覧を取得
/// </summary>
std::vector<std::string> ParticleJsonManager::GetAvailableSettings() const {
	return GetFilesInDirectory(baseDirectory_ + "Settings/");
}

/// <summary>
/// Presets フォルダ内の JSON ファイル一覧を取得
/// </summary>
std::vector<std::string> ParticleJsonManager::GetAvailablePresets() const {
	return GetFilesInDirectory(baseDirectory_ + "Presets/");
}

/// <summary>
/// システム設定ファイルを削除
/// </summary>
bool ParticleJsonManager::DeleteSettings(const std::string& systemName) {
	try {
		std::string filePath = GetSettingsPath(systemName);
		return std::filesystem::remove(filePath);
	}
	catch (const std::exception& e) {
		std::cerr << "設定ファイル削除失敗: " << e.what() << std::endl;
		return false;
	}
}

/// <summary>
/// プリセットファイルを削除
/// </summary>
bool ParticleJsonManager::DeletePreset(const std::string& presetName) {
	try {
		std::string filePath = GetPresetPath(presetName);
		return std::filesystem::remove(filePath);
	}
	catch (const std::exception& e) {
		std::cerr << "プリセット削除失敗: " << e.what() << std::endl;
		return false;
	}
}

/// <summary>
/// Settings フォルダ内の対象システム名の JSON ファイルパスを生成
/// </summary>
std::string ParticleJsonManager::GetSettingsPath(const std::string& systemName) const {
	return baseDirectory_ + "Settings/" + systemName + ".json";
}

/// <summary>
/// Presets フォルダ内のプリセットファイルのパスを生成
/// </summary>
std::string ParticleJsonManager::GetPresetPath(const std::string& presetName) const {
	return baseDirectory_ + "Presets/" + presetName + ".json";
}

/// <summary>
/// ParticleSetting を JSON に変換してファイルに保存
/// </summary>
bool ParticleJsonManager::SaveToFile(const std::string& filePath, const ParticleSetting& settings) {
	try {
		// JSON保存用フォルダ作成
		std::filesystem::path path(filePath);
		EnsureDirectoryExists(path.parent_path().string());

		nlohmann::json json;

		// -----------------------
		// 基本設定
		// -----------------------
		auto& basic = json["基本設定"];
		basic["最大パーティクル数"] = settings.GetMaxParticles();
		basic["エミッション率"] = settings.GetEmissionRate();
		basic["寿命範囲"] = Vector2ToJson(settings.GetLifeTimeRange());
		basic["ループ"] = settings.GetLooping();
		basic["持続時間"] = settings.GetDuration();
		basic["開始遅延"] = settings.GetStartDelay();

		// -----------------------
		// 物理設定
		// -----------------------
		auto& physics = json["物理設定"];
		physics["物理有効"] = settings.GetIsPhysicsEnabled();
		physics["重力"] = Vector3ToJson(settings.GetGravity());
		physics["空気抵抗"] = settings.GetDrag();
		physics["質量"] = settings.GetMass();
		physics["反発力"] = settings.GetBounciness();
		physics["摩擦"] = settings.GetFriction();
		physics["衝突判定"] = settings.GetCollisionEnabled();
		physics["衝突半径"] = settings.GetCollisionRadius();
		physics["衝突反発係数"] = settings.GetCollisionRestitution();
		physics["衝突摩擦係数"] = settings.GetCollisionFriction();
		physics["質量範囲"] = Vector2ToJson(settings.GetMassRange());

		// -----------------------
		// 乱流・ノイズ
		// -----------------------
		auto& turbulence = json["乱流設定"];
		turbulence["乱流有効"] = settings.GetTurbulenceEnabled();
		turbulence["乱流強度"] = settings.GetTurbulenceStrength();
		turbulence["乱流周波数"] = settings.GetTurbulenceFrequency();
		turbulence["ノイズスケール"] = Vector3ToJson(settings.GetNoiseScale());
		turbulence["ノイズ速度"] = settings.GetNoiseSpeed();

		// -----------------------
		// 色設定
		// -----------------------
		auto& color = json["色設定"];
		color["開始色"] = Vector4ToJson(settings.GetStartColor());
		color["終了色"] = Vector4ToJson(settings.GetEndColor());
		color["色変化タイプ"] = static_cast<int>(settings.GetColorType());
		color["アルファフェードイン時間"] = settings.GetAlphaFadeInTime();
		color["アルファフェードアウト時間"] = settings.GetAlphaFadeOutTime();
		color["ランダム開始色"] = settings.GetRandomStartColor();

		// -----------------------
		// 速度設定
		// -----------------------
		auto& velocity = json["速度設定"];
		velocity["基本速度"] = Vector3ToJson(settings.GetBaseVelocity());
		velocity["速度バリエーション"] = Vector3ToJson(settings.GetVelocityVariation());
		velocity["ランダム方向"] = settings.GetRandomDirection();
		velocity["速度"] = settings.GetSpeed();
		velocity["速度バリエーション値"] = settings.GetSpeedVariation();
		velocity["時間経過速度変化"] = settings.GetVelocityOverTime();
		velocity["速度倍率"] = Vector3ToJson(settings.GetVelocityOverTimeMultiplier());

		// -----------------------
		// 変形設定
		// -----------------------
		auto& transform = json["変形設定"];
		transform["スケール最小"] = Vector3ToJson(settings.GetScaleMin());
		transform["スケール最大"] = Vector3ToJson(settings.GetScaleMax());
		transform["サイズアニメーション"] = settings.GetSizeOverTime();
		transform["サイズ倍率開始"] = settings.GetSizeMultiplierStart();
		transform["サイズ倍率終了"] = settings.GetSizeMultiplierEnd();
		transform["回転最小"] = Vector3ToJson(settings.GetRotateMin());
		transform["回転最大"] = Vector3ToJson(settings.GetRotateMax());
		transform["角速度最小"] = settings.GetAngularVelocityMin();
		transform["角速度最大"] = settings.GetAngularVelocityMax();

		// -----------------------
		// ランダム回転設定
		// -----------------------
		auto& randomRotation = json["ランダム回転設定"];
		randomRotation["ランダム回転有効"] = settings.GetRandomRotationEnabled();
		randomRotation["ランダム回転範囲"] = Vector3ToJson(settings.GetRandomRotationRange());
		randomRotation["ランダム回転速度"] = Vector3ToJson(settings.GetRandomRotationSpeed());
		randomRotation["初期回転継承"] = settings.GetInheritInitialRotation();
		randomRotation["軸ごとに独立"] = settings.GetRandomRotationPerAxis();
		randomRotation["回転時間変化"] = settings.GetRotationOverTime();
		randomRotation["回転加速度"] = Vector3ToJson(settings.GetRotationAcceleration());
		randomRotation["回転減衰率"] = settings.GetRotationDamping();

		// -----------------------
		// 発生設定
		// -----------------------
		auto& emission = json["発生設定"];
		emission["発生形状"] = static_cast<int>(settings.GetEmissionType());
		emission["発生半径"] = settings.GetEmissionRadius();
		emission["発生サイズ"] = Vector3ToJson(settings.GetEmissionSize());
		emission["発生角度"] = settings.GetEmissionAngle();
		emission["発生高さ"] = settings.GetEmissionHeight();
		emission["バースト有効"] = settings.GetBurstEnabled();
		emission["バースト数"] = settings.GetBurstCount();
		emission["バースト間隔"] = settings.GetBurstInterval();
		emission["コーン角度"] = settings.GetConeAngle();

		// -----------------------
		// 描画設定
		// -----------------------
		auto& render = json["描画設定"];
		render["ブレンドモード"] = static_cast<int>(settings.GetBlendMode());
		render["ビルボード有効"] = settings.GetEnableBillboard();
		render["オフセット"] = Vector3ToJson(settings.GetOffset());
		render["UVスケール"] = Vector2ToJson(settings.GetUVScale());
		render["UV移動"] = Vector2ToJson(settings.GetUVTranslate());
		render["UV回転"] = settings.GetUVRotate();
		render["UVアニメーション"] = settings.GetUVAnimationEnabled();
		render["UVアニメーション速度"] = Vector2ToJson(settings.GetUVAnimationSpeed());
		render["テクスチャシート有効"] = settings.GetTextureSheetEnabled();
		render["テクスチャシートタイル"] = Vector2ToJson(settings.GetTextureSheetTiles());
		render["テクスチャシートフレームレート"] = settings.GetTextureSheetFrameRate();

		// -----------------------
		// トレイル設定
		// -----------------------
		auto& trail = json["トレイル設定"];
		trail["トレイル有効"] = settings.GetTrailEnabled();
		trail["トレイル長"] = settings.GetTrailLength();
		trail["トレイル幅"] = settings.GetTrailWidth();
		trail["トレイル色"] = Vector4ToJson(settings.GetTrailColor());

		// -----------------------
		// 力設定
		// -----------------------
		auto& forces = json["力設定"];
		forces["時間経過力"] = settings.GetForceOverTime();
		forces["力ベクトル"] = Vector3ToJson(settings.GetForceVector());
		forces["渦力有効"] = settings.GetVortexEnabled();
		forces["渦中心"] = Vector3ToJson(settings.GetVortexCenter());
		forces["渦強度"] = settings.GetVortexStrength();
		forces["渦半径"] = settings.GetVortexRadius();

		// -----------------------
		// 高度な設定
		// -----------------------
		auto& advanced = json["高度な設定"];
		advanced["変換速度継承"] = settings.GetInheritTransformVelocity();
		advanced["速度継承倍率"] = settings.GetInheritVelocityMultiplier();
		advanced["カリング有効"] = settings.GetCullingEnabled();
		advanced["カリング距離"] = settings.GetCullingDistance();
		advanced["LOD有効"] = settings.GetLODEnabled();
		advanced["LOD距離1"] = settings.GetLODDistance1();
		advanced["LOD距離2"] = settings.GetLODDistance2();

		// JSON をファイル書き込み
		std::ofstream file(filePath);
		if (!file.is_open()) {
			std::cerr << "ファイルが開けません: " << filePath << std::endl;
			return false;
		}

		file << json.dump(4); // 見やすいように整形して保存
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "設定保存エラー: " << e.what() << std::endl;
		return false;
	}
}

/// <summary>
/// JSON ファイルから ParticleSetting を復元する
/// </summary>
bool ParticleJsonManager::LoadFromFile(const std::string& filePath, ParticleSetting& settings) {
	try {
		std::ifstream file(filePath);
		if (!file.is_open()) {
			std::cerr << "ファイルが見つかりません: " << filePath << std::endl;
			return false;
		}

		nlohmann::json json;
		file >> json;

		// ---------------------------------------------------------
		// 基本設定
		// ---------------------------------------------------------
		if (json.contains("基本設定")) {
			auto& basic = json["基本設定"];

			if (basic.contains("最大パーティクル数")) settings.SetMaxParticles(basic["最大パーティクル数"]);
			if (basic.contains("エミッション率")) settings.SetEmissionRate(basic["エミッション率"]);
			if (basic.contains("寿命範囲")) settings.SetLifeTimeRange(JsonToVector2(basic["寿命範囲"]));
			if (basic.contains("ループ")) settings.SetLooping(basic["ループ"]);
			if (basic.contains("持続時間")) settings.SetDuration(basic["持続時間"]);
			if (basic.contains("開始遅延")) settings.SetStartDelay(basic["開始遅延"]);
		}

		// ---------------------------------------------------------
		// 物理設定
		// ---------------------------------------------------------
		if (json.contains("物理設定")) {
			auto& physics = json["物理設定"];

			if (physics.contains("物理有効")) settings.SetIsPhysicsEnabled(physics["物理有効"]);
			if (physics.contains("重力")) settings.SetGravity(JsonToVector3(physics["重力"]));
			if (physics.contains("空気抵抗")) settings.SetDrag(physics["空気抵抗"]);
			if (physics.contains("質量")) settings.SetMass(physics["質量"]);
			if (physics.contains("反発力")) settings.SetBounciness(physics["反発力"]);
			if (physics.contains("摩擦")) settings.SetFriction(physics["摩擦"]);
			if (physics.contains("衝突判定")) settings.SetCollisionEnabled(physics["衝突判定"]);
			if (physics.contains("衝突半径")) settings.SetCollisionRadius(physics["衝突半径"]);
			if (physics.contains("衝突反発係数")) settings.SetCollisionRestitution(physics["衝突反発係数"]);
			if (physics.contains("衝突摩擦係数")) settings.SetCollisionFriction(physics["衝突摩擦係数"]);
			if (physics.contains("質量範囲")) settings.SetMassRange(JsonToVector2(physics["質量範囲"]));
		}

		// ---------------------------------------------------------
		// 乱流・ノイズ設定
		// ---------------------------------------------------------
		if (json.contains("乱流設定")) {
			auto& turbulence = json["乱流設定"];

			if (turbulence.contains("乱流有効")) settings.SetTurbulenceEnabled(turbulence["乱流有効"]);
			if (turbulence.contains("乱流強度")) settings.SetTurbulenceStrength(turbulence["乱流強度"]);
			if (turbulence.contains("乱流周波数")) settings.SetTurbulenceFrequency(turbulence["乱流周波数"]);
			if (turbulence.contains("ノイズスケール")) settings.SetNoiseScale(JsonToVector3(turbulence["ノイズスケール"]));
			if (turbulence.contains("ノイズ速度")) settings.SetNoiseSpeed(turbulence["ノイズ速度"]);
		}

		// ---------------------------------------------------------
		// 色設定
		// ---------------------------------------------------------
		if (json.contains("色設定")) {
			auto& color = json["色設定"];

			if (color.contains("開始色")) settings.SetStartColor(JsonToVector4(color["開始色"]));
			if (color.contains("終了色")) settings.SetEndColor(JsonToVector4(color["終了色"]));
			if (color.contains("色変化タイプ")) settings.SetColorType(static_cast<ParticleManagerEnums::ColorChangeType>(color["色変化タイプ"]));
			if (color.contains("アルファフェードイン時間")) settings.SetAlphaFadeInTime(color["アルファフェードイン時間"]);
			if (color.contains("アルファフェードアウト時間")) settings.SetAlphaFadeOutTime(color["アルファフェードアウト時間"]);
			if (color.contains("ランダム開始色")) settings.SetRandomStartColor(color["ランダム開始色"]);
		}

		// ---------------------------------------------------------
		// 速度設定
		// ---------------------------------------------------------
		if (json.contains("速度設定")) {
			auto& velocity = json["速度設定"];

			if (velocity.contains("基本速度")) settings.SetBaseVelocity(JsonToVector3(velocity["基本速度"]));
			if (velocity.contains("速度バリエーション")) settings.SetVelocityVariation(JsonToVector3(velocity["速度バリエーション"]));
			if (velocity.contains("ランダム方向")) settings.SetRandomDirection(velocity["ランダム方向"]);
			if (velocity.contains("速度")) settings.SetSpeed(velocity["速度"]);
			if (velocity.contains("速度バリエーション値")) settings.SetSpeedVariation(velocity["速度バリエーション値"]);
			if (velocity.contains("時間経過速度変化")) settings.SetVelocityOverTime(velocity["時間経過速度変化"]);
			if (velocity.contains("速度倍率")) settings.SetVelocityOverTimeMultiplier(JsonToVector3(velocity["速度倍率"]));
		}

		// ---------------------------------------------------------
		// 変形設定
		// ---------------------------------------------------------
		if (json.contains("変形設定")) {
			auto& transform = json["変形設定"];

			if (transform.contains("スケール最小")) settings.SetScaleMin(JsonToVector3(transform["スケール最小"]));
			if (transform.contains("スケール最大")) settings.SetScaleMax(JsonToVector3(transform["スケール最大"]));
			if (transform.contains("サイズアニメーション")) settings.SetSizeOverTime(transform["サイズアニメーション"]);
			if (transform.contains("サイズ倍率開始")) settings.SetSizeMultiplierStart(transform["サイズ倍率開始"]);
			if (transform.contains("サイズ倍率終了")) settings.SetSizeMultiplierEnd(transform["サイズ倍率終了"]);
			if (transform.contains("回転最小")) settings.SetRotateMin(JsonToVector3(transform["回転最小"]));
			if (transform.contains("回転最大")) settings.SetRotateMax(JsonToVector3(transform["回転最大"]));
			if (transform.contains("角速度最小")) settings.SetAngularVelocityMin(transform["角速度最小"]);
			if (transform.contains("角速度最大")) settings.SetAngularVelocityMax(transform["角速度最大"]);
		}

		// ---------------------------------------------------------
		// ランダム回転設定
		// ---------------------------------------------------------
		if (json.contains("ランダム回転設定")) {
			auto& randomRotation = json["ランダム回転設定"];

			if (randomRotation.contains("ランダム回転有効")) settings.SetRandomRotationEnabled(randomRotation["ランダム回転有効"]);
			if (randomRotation.contains("ランダム回転範囲")) settings.SetRandomRotationRange(JsonToVector3(randomRotation["ランダム回転範囲"]));
			if (randomRotation.contains("ランダム回転速度")) settings.SetRandomRotationSpeed(JsonToVector3(randomRotation["ランダム回転速度"]));
			if (randomRotation.contains("初期回転継承")) settings.SetInheritInitialRotation(randomRotation["初期回転継承"]);
			if (randomRotation.contains("軸ごとに独立")) settings.SetRandomRotationPerAxis(randomRotation["軸ごとに独立"]);
			if (randomRotation.contains("回転時間変化")) settings.SetRotationOverTime(randomRotation["回転時間変化"]);
			if (randomRotation.contains("回転加速度")) settings.SetRotationAcceleration(JsonToVector3(randomRotation["回転加速度"]));
			if (randomRotation.contains("回転減衰率")) settings.SetRotationDamping(randomRotation["回転減衰率"]);
		}

		// ---------------------------------------------------------
		// 発生設定
		// ---------------------------------------------------------
		if (json.contains("発生設定")) {
			auto& emission = json["発生設定"];

			if (emission.contains("発生形状")) settings.SetEmissionType(static_cast<ParticleManagerEnums::EmissionType>(emission["発生形状"]));
			if (emission.contains("発生半径")) settings.SetEmissionRadius(emission["発生半径"]);
			if (emission.contains("発生サイズ")) settings.SetEmissionSize(JsonToVector3(emission["発生サイズ"]));
			if (emission.contains("発生角度")) settings.SetEmissionAngle(emission["発生角度"]);
			if (emission.contains("発生高さ")) settings.SetEmissionHeight(emission["発生高さ"]);
			if (emission.contains("バースト有効")) settings.SetBurstEnabled(emission["バースト有効"]);
			if (emission.contains("バースト数")) settings.SetBurstCount(emission["バースト数"]);
			if (emission.contains("バースト間隔")) settings.SetBurstInterval(emission["バースト間隔"]);
			if (emission.contains("コーン角度")) settings.SetConeAngle(emission["コーン角度"]);
		}

		// ---------------------------------------------------------
		// 描画設定
		// ---------------------------------------------------------
		if (json.contains("描画設定")) {
			auto& render = json["描画設定"];

			if (render.contains("ブレンドモード"))
				settings.SetBlendMode(static_cast<BlendMode>(render["ブレンドモード"]));

			if (render.contains("ビルボード有効")) settings.SetEnableBillboard(render["ビルボード有効"]);
			if (render.contains("オフセット")) settings.SetOffset(JsonToVector3(render["オフセット"]));
			if (render.contains("UVスケール")) settings.SetUVScale(JsonToVector2(render["UVスケール"]));
			if (render.contains("UV移動")) settings.SetUVTranslate(JsonToVector2(render["UV移動"]));
			if (render.contains("UV回転")) settings.SetUVRotate(render["UV回転"]);
			if (render.contains("UVアニメーション")) settings.SetUVAnimationEnabled(render["UVアニメーション"]);
			if (render.contains("UVアニメーション速度")) settings.SetUVAnimationSpeed(JsonToVector2(render["UVアニメーション速度"]));
			if (render.contains("テクスチャシート有効")) settings.SetTextureSheetEnabled(render["テクスチャシート有効"]);
			if (render.contains("テクスチャシートタイル")) settings.SetTextureSheetTiles(JsonToVector2(render["テクスチャシートタイル"]));
			if (render.contains("テクスチャシートフレームレート")) settings.SetTextureSheetFrameRate(render["テクスチャシートフレームレート"]);
		}

		// ---------------------------------------------------------
		// トレイル設定
		// ---------------------------------------------------------
		if (json.contains("トレイル設定")) {
			auto& trail = json["トレイル設定"];

			if (trail.contains("トレイル有効")) settings.SetTrailEnabled(trail["トレイル有効"]);
			if (trail.contains("トレイル長")) settings.SetTrailLength(trail["トレイル長"]);
			if (trail.contains("トレイル幅")) settings.SetTrailWidth(trail["トレイル幅"]);
			if (trail.contains("トレイル色")) settings.SetTrailColor(JsonToVector4(trail["トレイル色"]));
		}

		// ---------------------------------------------------------
		// 力設定
		// ---------------------------------------------------------
		if (json.contains("力設定")) {
			auto& forces = json["力設定"];

			if (forces.contains("時間経過力")) settings.SetForceOverTime(forces["時間経過力"]);
			if (forces.contains("力ベクトル")) settings.SetForceVector(JsonToVector3(forces["力ベクトル"]));
			if (forces.contains("渦力有効")) settings.SetVortexEnabled(forces["渦力有効"]);
			if (forces.contains("渦中心")) settings.SetVortexCenter(JsonToVector3(forces["渦中心"]));
			if (forces.contains("渦強度")) settings.SetVortexStrength(forces["渦強度"]);
			if (forces.contains("渦半径")) settings.SetVortexRadius(forces["渦半径"]);
		}

		// ---------------------------------------------------------
		// 高度な設定
		// ---------------------------------------------------------
		if (json.contains("高度な設定")) {
			auto& advanced = json["高度な設定"];

			if (advanced.contains("変換速度継承")) settings.SetInheritTransformVelocity(advanced["変換速度継承"]);
			if (advanced.contains("速度継承倍率")) settings.SetInheritVelocityMultiplier(advanced["速度継承倍率"]);
			if (advanced.contains("カリング有効")) settings.SetCullingEnabled(advanced["カリング有効"]);
			if (advanced.contains("カリング距離")) settings.SetCullingDistance(advanced["カリング距離"]);
			if (advanced.contains("LOD有効")) settings.SetLODEnabled(advanced["LOD有効"]);
			if (advanced.contains("LOD距離1")) settings.SetLODDistance1(advanced["LOD距離1"]);
			if (advanced.contains("LOD距離2")) settings.SetLODDistance2(advanced["LOD距離2"]);
		}

		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "設定読み込みエラー: " << e.what() << std::endl;
		return false;
	}
}


/// <summary>
/// 指定ディレクトリが存在しない場合は作成する
/// </summary>
void ParticleJsonManager::EnsureDirectoryExists(const std::string& directory) {
	try {
		if (!std::filesystem::exists(directory)) {
			std::filesystem::create_directories(directory);
		}
	}
	catch (const std::exception& e) {
		std::cerr << "ディレクトリ作成失敗: " << e.what() << std::endl;
	}
}

/// <summary>
/// フォルダ内の JSON ファイル名のみを一覧取得（拡張子除く）
/// </summary>
std::vector<std::string> ParticleJsonManager::GetFilesInDirectory(const std::string& directory) const {
	std::vector<std::string> files;
	try {
		if (std::filesystem::exists(directory)) {
			for (const auto& entry : std::filesystem::directory_iterator(directory)) {
				if (entry.is_regular_file() && entry.path().extension() == ".json") {
					files.push_back(entry.path().stem().string());
				}
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "ファイル一覧取得失敗: " << e.what() << std::endl;
	}
	return files;
}