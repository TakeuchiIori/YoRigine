#include "Motion.h"
#include "MathFunc.h"
#include <assert.h>
#include <fstream>
#include <filesystem>
#include <json.hpp>
#include "Quaternion.h"
#include <iostream>
#include <assimp/Importer.hpp>
#include <Debugger/Logger.h>

Motion Motion::LoadFromScene(const aiScene* scene, const std::string& gltfFilePath, const std::string& animationName)
{

	assert(scene && scene->mNumAnimations > 0);
	Motion anim;
	std::string resolvedName = animationName;
	aiAnimation* animationAssimp = nullptr;

	if (!animationName.empty()) {
		for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
			if (scene->mAnimations[i]->mName.C_Str() == animationName) {
				animationAssimp = scene->mAnimations[i];
				break;
			}
		}
		if (!animationAssimp) {
			throw std::runtime_error("アニメーション名 : \"" + animationName + "\" が見つかりませんでした");
		}
	} else {
		// 名前が指定されていなければ先頭を使用
		animationAssimp = scene->mAnimations[0];
	}

	float tps = static_cast<float>(animationAssimp->mTicksPerSecond);

	// 安全チェック：小さすぎる or 0 の場合は補正
	if (tps < 1e-3f) {
		Logger("アニメーションの ticksPerSecond が小さすぎます（" + std::to_string(tps) + "）。代わりに 30.0 を使用します。");
		tps = 30.0f; // Blender等の標準値が30
	}

	float duration = float(animationAssimp->mDuration / tps);
	anim.animation_.duration_ = duration;

	if (duration > 60.0f) {
		Logger("アニメーションの時間が長すぎます（" + std::to_string(duration) + " 秒）：アニメーション名 = " + animationName);
	}


	for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
		NodeAnimation& nodeAnimation = anim.animation_.nodeAnimations_[nodeAnimationAssimp->mNodeName.C_Str()];

		std::string interpolation = ParseGLTFInterpolation(gltfFilePath, channelIndex);
		if (interpolation == "LINEAR") nodeAnimation.interpolationType = InterpolationType::Linear;
		else if (interpolation == "STEP") nodeAnimation.interpolationType = InterpolationType::Step;
		else if (interpolation == "CUBICSPLINE") nodeAnimation.interpolationType = InterpolationType::CubicSpline;
		else nodeAnimation.interpolationType = InterpolationType::Linear;

		for (uint32_t i = 0; i < nodeAnimationAssimp->mNumPositionKeys; ++i) {
			KeyframeVector3 kf;
			kf.time = float(nodeAnimationAssimp->mPositionKeys[i].mTime / animationAssimp->mTicksPerSecond);
			const auto& val = nodeAnimationAssimp->mPositionKeys[i].mValue;
			kf.value = { -val.x, val.y, val.z };
			nodeAnimation.translate.keyframes.push_back(kf);
		}

		for (uint32_t i = 0; i < nodeAnimationAssimp->mNumScalingKeys; ++i) {
			KeyframeVector3 kf;
			kf.time = float(nodeAnimationAssimp->mScalingKeys[i].mTime / animationAssimp->mTicksPerSecond);
			const auto& val = nodeAnimationAssimp->mScalingKeys[i].mValue;
			kf.value = { val.x, val.y, val.z };
			nodeAnimation.scale.keyframes.push_back(kf);
		}

		for (uint32_t i = 0; i < nodeAnimationAssimp->mNumRotationKeys; ++i) {
			KeyframeQuaternion kf;
			kf.time = float(nodeAnimationAssimp->mRotationKeys[i].mTime / animationAssimp->mTicksPerSecond);
			const auto& val = nodeAnimationAssimp->mRotationKeys[i].mValue;
			kf.value = { val.x, -val.y, -val.z, val.w };
			nodeAnimation.rotate.keyframes.push_back(kf);
		}
	}

	return anim;
}

std::string Motion::ParseGLTFInterpolation(const std::string& gltfFilePath, uint32_t samplerIndex) {
	// GLTFファイルを開く
	std::ifstream file(gltfFilePath);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open GLTF file: " + gltfFilePath);
	}

	// JSONを読み込む
	nlohmann::json gltfJson;
	file >> gltfJson;

	// サンプラー情報を取得
	const auto& samplers = gltfJson["animations"][0]["samplers"];
	if (samplerIndex >= samplers.size()) {
		return "LINEAR"; // デフォルト値
	}

	// 補間方法を取得
	if (samplers[samplerIndex].contains("interpolation")) {
		return samplers[samplerIndex]["interpolation"].get<std::string>();
	}

	return "LINEAR"; // デフォルト値
}


void Motion::SaveBinary(const Motion& motion, const std::string& animationName, const std::string& path)
{
	std::string safeName = animationName;
	std::replace(safeName.begin(), safeName.end(), ' ', '_');
	std::string fullPath = path + "_" + safeName + ".anim";

	std::ofstream ofs(fullPath, std::ios::binary);
	if (!ofs) {
		std::cerr << "[ERROR]  書き込みできない" << fullPath << std::endl;
		return;
	}

	// ヘッダー
	ofs.write("ANIM", 4);
	uint32_t animCount = 1;
	ofs.write(reinterpret_cast<const char*>(&animCount), sizeof(uint32_t));

	// アニメーション名
	uint32_t nameLen = static_cast<uint32_t>(animationName.size());
	ofs.write(reinterpret_cast<const char*>(&nameLen), sizeof(uint32_t));
	ofs.write(animationName.c_str(), nameLen);

	// アニメーション全体の長さ・ノード数
	ofs.write(reinterpret_cast<const char*>(&motion.animation_.duration_), sizeof(float));
	size_t nodeCount = motion.animation_.nodeAnimations_.size();
	ofs.write(reinterpret_cast<const char*>(&nodeCount), sizeof(size_t));


	// 各ノードの保存
	for (const auto& [jointName, nodeAnim] : motion.animation_.nodeAnimations_) {
		size_t jointNameLen = jointName.size();
		ofs.write(reinterpret_cast<const char*>(&jointNameLen), sizeof(size_t));
		ofs.write(jointName.c_str(), jointNameLen);

		/// Vector3の書き出し用
		auto writeVector3 = [&](const auto& keyframes) {
			size_t count = keyframes.size();
			ofs.write(reinterpret_cast<const char*>(&count), sizeof(size_t));

			for (const auto& kf : keyframes) {
				ofs.write(reinterpret_cast<const char*>(&kf.time), sizeof(float));
				ofs.write(reinterpret_cast<const char*>(&kf.value), sizeof(Vector3));
			}
			};
		/// Quaternionの書き出し用
		auto writeQuaternion = [&](const auto& keyframes) {
			size_t count = keyframes.size();
			ofs.write(reinterpret_cast<const char*>(&count), sizeof(size_t));
			for (const auto& kf : keyframes) {
				ofs.write(reinterpret_cast<const char*>(&kf.time), sizeof(float));
				ofs.write(reinterpret_cast<const char*>(&kf.value), sizeof(Quaternion));
			}
			};

		// SRTの書き出し
		writeVector3(nodeAnim.translate.keyframes);
		writeQuaternion(nodeAnim.rotate.keyframes);
		writeVector3(nodeAnim.scale.keyframes);

		// 補完方法の書き出し
		int interp = static_cast<int>(nodeAnim.interpolationType);
		ofs.write(reinterpret_cast<const char*>(&interp), sizeof(int));
	}

}


Motion Motion::LoadBinary(const std::string& path)
{
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) {
		throw std::runtime_error("バイナリファイルが開けません" + path);
	}

	char header[4];
	ifs.read(header, 4);
	if (std::strncmp(header, "ANIM", 4) != 0) {
		throw std::runtime_error("バイナリファイル形式が不正です" + path);
	}

	uint32_t animCount;
	ifs.read(reinterpret_cast<char*>(&animCount), sizeof(uint32_t));
	if (animCount != 1) {
		throw std::runtime_error("このファイルには複数のアニメーションが含まれています: " + path);
	}

	// アニメーション名（読み捨て）
	uint32_t nameLen;
	ifs.read(reinterpret_cast<char*>(&nameLen), sizeof(uint32_t));
	std::string animName(nameLen, '\0');
	ifs.read(animName.data(), nameLen);

	Motion motion;

	// duration
	ifs.read(reinterpret_cast<char*>(&motion.animation_.duration_), sizeof(float));

	// ノード数
	size_t nodeCount;
	ifs.read(reinterpret_cast<char*>(&nodeCount), sizeof(size_t));

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

		motion.animation_.nodeAnimations_[jointName] = std::move(nodeAnim);
	}

	return motion;

}


void Motion::ApplyAnimation(std::vector<Joint>& joints, float animationtime)
{
	for (Joint& joint : joints) {
		// 対象のJointのMotionがあれば、値の適用を行う。
		if (auto it = animation_.nodeAnimations_.find(joint.GetName()); it != animation_.nodeAnimations_.end()) {
			const NodeAnimation& rootNodeAnimation = (*it).second;
			QuaternionTransform transform;
			transform.translate = CalculateValue(rootNodeAnimation.translate.keyframes, animationtime, rootNodeAnimation.interpolationType); // 指定時刻の値を取得
			transform.rotate = CalculateValue(rootNodeAnimation.rotate.keyframes, animationtime, rootNodeAnimation.interpolationType);
			transform.scale = CalculateValue(rootNodeAnimation.scale.keyframes, animationtime, rootNodeAnimation.interpolationType);
			joint.SetTransform(transform);

		}
	}
}

void Motion::PlayerAnimation(float animationTime, Node& node)
{
	NodeAnimation& rootNodeAnimation = animation_.nodeAnimations_[node.name_]; // rootNodeのMotionを取得
	Vector3 translate = CalculateValue(rootNodeAnimation.translate.keyframes, animationTime, rootNodeAnimation.interpolationType); // 指定時刻の値を取得
	Quaternion rotate = CalculateValue(rootNodeAnimation.rotate.keyframes, animationTime, rootNodeAnimation.interpolationType);
	Vector3 scale = CalculateValue(rootNodeAnimation.scale.keyframes, animationTime, rootNodeAnimation.interpolationType);

	node.localMatrix_ = MakeAffineMatrix(scale, rotate, translate);
}

Vector3 Motion::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time, InterpolationType interpolationType) {
	assert(!keyframes.empty());

	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value; // 最初のキー値を返す
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

float Motion::CalculateValue(const std::vector<KeyframeFloat>& keyframes, float time, InterpolationType interpolationType)
{
	assert(!keyframes.empty());

	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value; // 最初のキー値を返す
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

Quaternion Motion::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time, InterpolationType interpolationType) {
	assert(!keyframes.empty());

	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value; // 最初のキー値を返す
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

			}

			default:
				return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
			}
		}
	}

	return (*keyframes.rbegin()).value;
}
