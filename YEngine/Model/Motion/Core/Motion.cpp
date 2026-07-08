#include "Motion.h"
#include "MathFunc.h"
#include <assert.h>
#include <fstream>
#include <filesystem>
#include <json.hpp>
#include "Quaternion.h"
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <Debugger/Logger.h>

namespace {
	std::vector<std::string> LoadGLTFInterpolations(
		const std::string& gltfFilePath,
		uint32_t animationIndex,
		uint32_t channelCount)
	{
		std::vector<std::string> interpolations(channelCount, "LINEAR");

		if (!std::filesystem::path(gltfFilePath).extension().string().ends_with("gltf")) {
			return interpolations;
		}

		std::ifstream file(gltfFilePath);
		if (!file.is_open()) {
			return interpolations;
		}

		nlohmann::json gltfJson;
		file >> gltfJson;

		if (!gltfJson.contains("animations") || animationIndex >= gltfJson["animations"].size()) {
			return interpolations;
		}

		const auto& animation = gltfJson["animations"][animationIndex];
		if (!animation.contains("samplers") || !animation.contains("channels")) {
			return interpolations;
		}

		const auto& samplers = animation["samplers"];
		const auto& channels = animation["channels"];
		for (uint32_t i = 0; i < channelCount && i < channels.size(); ++i) {
			uint32_t samplerIndex = channels[i].value("sampler", i);
			if (samplerIndex < samplers.size() && samplers[samplerIndex].contains("interpolation")) {
				interpolations[i] = samplers[samplerIndex]["interpolation"].get<std::string>();
			}
		}

		return interpolations;
	}

	void EnsureCompleteNodeAnimation(Motion::NodeAnimation& nodeAnim)
	{
		if (nodeAnim.translate.keyframes.empty()) {
			nodeAnim.translate.keyframes.push_back({ 0.0f, Vector3{ 0.0f, 0.0f, 0.0f } });
		}
		if (nodeAnim.rotate.keyframes.empty()) {
			nodeAnim.rotate.keyframes.push_back({ 0.0f, Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } });
		}
		if (nodeAnim.scale.keyframes.empty()) {
			nodeAnim.scale.keyframes.push_back({ 0.0f, Vector3{ 1.0f, 1.0f, 1.0f } });
		}
	}

	void EnsureCompleteMotion(Motion& motion)
	{
		for (auto& [_, nodeAnim] : motion.animation_.nodeAnimations_) {
			EnsureCompleteNodeAnimation(nodeAnim);
		}
	}
}

// ============================================================
// Assimp Scene からのロード
// ============================================================
Motion Motion::LoadFromScene(const aiScene* scene, const std::string& gltfFilePath, const std::string& animationName)
{
	assert(scene && scene->mNumAnimations > 0);
	Motion anim;
	std::string resolvedName = animationName;
	aiAnimation* animationAssimp = nullptr;
	uint32_t animationIndex = 0;

	// ------------------------------------------------------------
	// アニメーション名の検索
	// ------------------------------------------------------------
	if (!animationName.empty()) {
		for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
			if (scene->mAnimations[i]->mName.C_Str() == animationName) {
				animationAssimp = scene->mAnimations[i];
				animationIndex = i;
				break;
			}
		}
		if (!animationAssimp) {
			throw std::runtime_error("アニメーション名 : \"" + animationName + "\" が見つかりませんでした");
		}
	}
	else {
		animationAssimp = scene->mAnimations[0];
	}

	// ------------------------------------------------------------
	// タイム設定の適用
	// ------------------------------------------------------------
	float tps = static_cast<float>(animationAssimp->mTicksPerSecond);

	if (tps < 1e-3f) {
		Logger("アニメーションの ticksPerSecond が小さすぎます。代わりに 30.0 を使用します。");
		tps = 30.0f;
	}

	float duration = float(animationAssimp->mDuration / tps);
	anim.animation_.duration_ = duration;

	if (duration > 60.0f) {
		Logger("アニメーションの時間が長すぎます: " + animationName);
	}

	const std::vector<std::string> interpolations =
		LoadGLTFInterpolations(gltfFilePath, animationIndex, animationAssimp->mNumChannels);

	// ------------------------------------------------------------
	// チャンネル（ノード）ごとのキーフレーム読み込み
	// ------------------------------------------------------------
	for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
		NodeAnimation& nodeAnimation = anim.animation_.nodeAnimations_[nodeAnimationAssimp->mNodeName.C_Str()];

		const std::string& interpolation = interpolations[channelIndex];
		if (interpolation == "LINEAR") nodeAnimation.interpolationType = InterpolationType::Linear;
		else if (interpolation == "STEP") nodeAnimation.interpolationType = InterpolationType::Step;
		else if (interpolation == "CUBICSPLINE") nodeAnimation.interpolationType = InterpolationType::CubicSpline;
		else nodeAnimation.interpolationType = InterpolationType::Linear;

		for (uint32_t i = 0; i < nodeAnimationAssimp->mNumPositionKeys; ++i) {
			KeyframeVector3 kf;
			kf.time = float(nodeAnimationAssimp->mPositionKeys[i].mTime / tps);
			const auto& val = nodeAnimationAssimp->mPositionKeys[i].mValue;
			kf.value = { -val.x, val.y, val.z };
			nodeAnimation.translate.keyframes.push_back(kf);
		}

		for (uint32_t i = 0; i < nodeAnimationAssimp->mNumScalingKeys; ++i) {
			KeyframeVector3 kf;
			kf.time = float(nodeAnimationAssimp->mScalingKeys[i].mTime / tps);
			const auto& val = nodeAnimationAssimp->mScalingKeys[i].mValue;
			kf.value = { val.x, val.y, val.z };
			nodeAnimation.scale.keyframes.push_back(kf);
		}

		for (uint32_t i = 0; i < nodeAnimationAssimp->mNumRotationKeys; ++i) {
			KeyframeQuaternion kf;
			kf.time = float(nodeAnimationAssimp->mRotationKeys[i].mTime / tps);
			const auto& val = nodeAnimationAssimp->mRotationKeys[i].mValue;
			kf.value = { val.x, -val.y, -val.z, val.w };
			nodeAnimation.rotate.keyframes.push_back(kf);
		}

		EnsureCompleteNodeAnimation(nodeAnimation);
	}

	EnsureCompleteMotion(anim);
	return anim;
}

// ============================================================
// GLTFからの補間方法の解析
// ============================================================
std::string Motion::ParseGLTFInterpolation(const std::string& gltfFilePath, uint32_t samplerIndex) {
	std::ifstream file(gltfFilePath);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open GLTF file: " + gltfFilePath);
	}

	nlohmann::json gltfJson;
	file >> gltfJson;

	const auto& samplers = gltfJson["animations"][0]["samplers"];
	if (samplerIndex >= samplers.size()) {
		return "LINEAR";
	}

	if (samplers[samplerIndex].contains("interpolation")) {
		return samplers[samplerIndex]["interpolation"].get<std::string>();
	}

	return "LINEAR";
}

// ============================================================
// バイナリへの保存
// ============================================================
void Motion::SaveBinary(const Motion& motion, const std::string& animationName, const std::string& path)
{
	Motion safeMotion = motion;
	EnsureCompleteMotion(safeMotion);

	std::string safeName = animationName;
	std::replace(safeName.begin(), safeName.end(), ' ', '_');
	std::string fullPath = path;
	if (fullPath.size() < 5 || fullPath.substr(fullPath.size() - 5) != ".anim") {
		// 拡張子がない場合のみ animName を付加
		std::string safeName2 = animationName;
		std::replace(safeName2.begin(), safeName2.end(), ' ', '_');
		fullPath += "_" + safeName2 + ".anim";
	}

	// ディレクトリが存在しなければ作成
	std::filesystem::path filePath(fullPath);
	if (filePath.has_parent_path()) {
		std::filesystem::create_directories(filePath.parent_path());
	}

	std::ofstream ofs(fullPath, std::ios::binary);
	if (!ofs) {
		throw std::runtime_error("書き込みできない: " + fullPath);
	}

	// ------------------------------------------------------------
	// ヘッダー情報書き出し
	// ------------------------------------------------------------
	ofs.write("ANIM", 4);
	uint32_t animCount = 1;
	ofs.write(reinterpret_cast<const char*>(&animCount), sizeof(uint32_t));

	uint32_t nameLen = static_cast<uint32_t>(animationName.size());
	ofs.write(reinterpret_cast<const char*>(&nameLen), sizeof(uint32_t));
	ofs.write(animationName.c_str(), nameLen);

	ofs.write(reinterpret_cast<const char*>(&safeMotion.animation_.duration_), sizeof(float));
	size_t nodeCount = safeMotion.animation_.nodeAnimations_.size();
	ofs.write(reinterpret_cast<const char*>(&nodeCount), sizeof(size_t));

	// ------------------------------------------------------------
	// ノードごとのアニメーションデータ書き出し
	// ------------------------------------------------------------
	for (const auto& [jointName, nodeAnim] : safeMotion.animation_.nodeAnimations_) {
		size_t jointNameLen = jointName.size();
		ofs.write(reinterpret_cast<const char*>(&jointNameLen), sizeof(size_t));
		ofs.write(jointName.c_str(), jointNameLen);

		auto writeVector3 = [&](const auto& keyframes) {
			size_t count = keyframes.size();
			ofs.write(reinterpret_cast<const char*>(&count), sizeof(size_t));

			for (const auto& kf : keyframes) {
				ofs.write(reinterpret_cast<const char*>(&kf.time), sizeof(float));
				ofs.write(reinterpret_cast<const char*>(&kf.value), sizeof(Vector3));
			}
			};

		auto writeQuaternion = [&](const auto& keyframes) {
			size_t count = keyframes.size();
			ofs.write(reinterpret_cast<const char*>(&count), sizeof(size_t));
			for (const auto& kf : keyframes) {
				ofs.write(reinterpret_cast<const char*>(&kf.time), sizeof(float));
				ofs.write(reinterpret_cast<const char*>(&kf.value), sizeof(Quaternion));
			}
			};

		writeVector3(nodeAnim.translate.keyframes);
		writeQuaternion(nodeAnim.rotate.keyframes);
		writeVector3(nodeAnim.scale.keyframes);

		int interp = static_cast<int>(nodeAnim.interpolationType);
		ofs.write(reinterpret_cast<const char*>(&interp), sizeof(int));
	}
}

// ============================================================
// バイナリからの読み込み
// ============================================================
Motion Motion::LoadBinary(const std::string& path)
{
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) {
		throw std::runtime_error("バイナリファイルが開けません: " + path);
	}

	char header[4];
	ifs.read(header, 4);
	if (std::strncmp(header, "ANIM", 4) != 0) {
		throw std::runtime_error("バイナリファイル形式が不正です: " + path);
	}

	uint32_t animCount;
	ifs.read(reinterpret_cast<char*>(&animCount), sizeof(uint32_t));
	if (animCount != 1) {
		throw std::runtime_error("複数のアニメーションが含まれています: " + path);
	}

	uint32_t nameLen;
	ifs.read(reinterpret_cast<char*>(&nameLen), sizeof(uint32_t));
	std::string animName(nameLen, '\0');
	ifs.read(animName.data(), nameLen);

	Motion motion;

	ifs.read(reinterpret_cast<char*>(&motion.animation_.duration_), sizeof(float));

	size_t nodeCount;
	ifs.read(reinterpret_cast<char*>(&nodeCount), sizeof(size_t));

	// ------------------------------------------------------------
	// 各ノードデータの読み込み
	// ------------------------------------------------------------
	for (size_t i = 0; i < nodeCount; ++i) {
		size_t jointNameLen;
		ifs.read(reinterpret_cast<char*>(&jointNameLen), sizeof(size_t));
		std::string jointName(jointNameLen, '\0');
		ifs.read(jointName.data(), jointNameLen);

		Motion::NodeAnimation nodeAnim;

		auto readVec3 = [&](auto& keyframes) {
			size_t count;
			ifs.read(reinterpret_cast<char*>(&count), sizeof(size_t));
			keyframes.resize(count);
			for (size_t i = 0; i < count; ++i) {
				ifs.read(reinterpret_cast<char*>(&keyframes[i].time), sizeof(float));
				ifs.read(reinterpret_cast<char*>(&keyframes[i].value), sizeof(Vector3));
			}
			};

		auto readQuat = [&](auto& keyframes) {
			size_t count;
			ifs.read(reinterpret_cast<char*>(&count), sizeof(size_t));
			keyframes.resize(count);
			for (size_t i = 0; i < count; ++i) {
				ifs.read(reinterpret_cast<char*>(&keyframes[i].time), sizeof(float));
				ifs.read(reinterpret_cast<char*>(&keyframes[i].value), sizeof(Quaternion));
			}
			};

		readVec3(nodeAnim.translate.keyframes);
		readQuat(nodeAnim.rotate.keyframes);
		readVec3(nodeAnim.scale.keyframes);

		int interp;
		ifs.read(reinterpret_cast<char*>(&interp), sizeof(int));
		nodeAnim.interpolationType = static_cast<InterpolationType>(interp);

		EnsureCompleteNodeAnimation(nodeAnim);
		motion.animation_.nodeAnimations_[jointName] = std::move(nodeAnim);
	}

	EnsureCompleteMotion(motion);
	return motion;
}

// ============================================================
// アニメーションの適用（複数ジョイント）
// ============================================================
void Motion::ApplyAnimation(std::vector<Joint>& joints, float animationtime)
{
	for (Joint& joint : joints) {
		if (auto it = animation_.nodeAnimations_.find(joint.GetName()); it != animation_.nodeAnimations_.end()) {
			const NodeAnimation& rootNodeAnimation = (*it).second;
			QuaternionTransform transform;

			transform.translate = CalculateValue(rootNodeAnimation.translate.keyframes, animationtime, rootNodeAnimation.interpolationType);
			transform.rotate = CalculateValue(rootNodeAnimation.rotate.keyframes, animationtime, rootNodeAnimation.interpolationType);
			transform.scale = CalculateValue(rootNodeAnimation.scale.keyframes, animationtime, rootNodeAnimation.interpolationType);

			joint.SetTransform(transform);
		}
	}
}

// ============================================================
// アニメーションの適用（単一ノード）
// ============================================================
void Motion::PlayerAnimation(float animationTime, Node& node)
{
	NodeAnimation& rootNodeAnimation = animation_.nodeAnimations_[node.name_];

	Vector3 translate = CalculateValue(rootNodeAnimation.translate.keyframes, animationTime, rootNodeAnimation.interpolationType);
	Quaternion rotate = CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime, rootNodeAnimation.interpolationType);
	Vector3 scale = CalculateValue(rootNodeAnimation.scale.keyframes, animationTime, rootNodeAnimation.interpolationType);

	node.localMatrix_ = MakeAffineMatrix(scale, rotate, translate);
}

// ============================================================
// キーフレーム計算（Vector3）
// ============================================================
Vector3 Motion::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time, InterpolationType interpolationType) {
	assert(!keyframes.empty());

	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;

		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);

			switch (interpolationType) {
			case InterpolationType::Linear:
				return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);

			case InterpolationType::Step:
				return keyframes[index].value;

			case InterpolationType::CubicSpline: {
				size_t prevIndex = (index == 0) ? index : index - 1;
				size_t nextNextIndex = (nextIndex + 1 < keyframes.size()) ? nextIndex + 1 : nextIndex;

				return CatmullRomInterpolation(
					keyframes[prevIndex].value,
					keyframes[index].value,
					keyframes[nextIndex].value,
					keyframes[nextNextIndex].value,
					t
				);
			}

			default:
				return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
			}
		}
	}

	return (*keyframes.rbegin()).value;
}

// ============================================================
// キーフレーム計算（float）
// ============================================================
float Motion::CalculateValue(const std::vector<KeyframeFloat>& keyframes, float time, InterpolationType interpolationType)
{
	assert(!keyframes.empty());

	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;

		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);

			switch (interpolationType) {
			case InterpolationType::Linear:
				return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);

			case InterpolationType::Step:
				return keyframes[index].value;

			case InterpolationType::CubicSpline: {
				size_t prevIndex = (index == 0) ? index : index - 1;
				size_t nextNextIndex = (nextIndex + 1 < keyframes.size()) ? nextIndex + 1 : nextIndex;

				return CubicSplineInterpolate(
					keyframes[prevIndex].value,
					keyframes[index].value,
					keyframes[nextIndex].value,
					keyframes[nextNextIndex].value,
					t
				);
			}

			default:
				return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
			}
		}
	}

	return (*keyframes.rbegin()).value;
}

// ============================================================
// キーフレーム計算（Quaternion）
// ============================================================
Quaternion Motion::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time, InterpolationType interpolationType) {
	assert(!keyframes.empty());

	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;

		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);

			switch (interpolationType) {
			case InterpolationType::Linear:
				return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);

			case InterpolationType::Step:
				return keyframes[index].value;

			case InterpolationType::CubicSpline: {
				// クォータニオンのSpline補間が必要な場合はここに記述
			}

			default:
				return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
			}
		}
	}

	return (*keyframes.rbegin()).value;
}
