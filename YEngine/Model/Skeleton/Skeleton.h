#pragma once

// C++
#include <optional>
#include <map>
#include <vector>

// Engine
#include "Joint.h"
#include "../Node/Node.h"

// Math
#include "Quaternion.h"
#include <Matrix4x4.h>
#include "Vector3.h"

// スケルトンクラス
class Line;
class Skeleton
{
public:
	///************************* 基本関数 *************************///

	// スケルトン作成
	void Create(const Node& rootNode);

	// スケルトン更新
	void Update();

	// スケルトン描画
	void Draw(Line& line, const Matrix4x4& worldMatrix);

public:
	///************************* アクセッサ *************************///

	// ジョイントリスト取得
	std::vector<Joint>& GetJoints() { return joints_; }

	// ジョイントマップ取得
	std::map<std::string, int32_t>& GetJointMap() { return jointMap_; }

	// 名前からジョイント取得
	Joint* GetJointByName(const std::string& name) {
		auto it = jointMap_.find(name);
		if (it == jointMap_.end()) {
			return nullptr;
		}
		return &joints_[it->second];
	}

	// ジョイント名一覧取得
	std::vector<std::string> GetAllJointNames() const {
		std::vector<std::string> names;
		names.reserve(joints_.size());
		for (const auto& joint : joints_) {
			names.push_back(joint.GetName());
		}
		return names;
	}

	// ルートの親設定
	void SetRootParent(WorldTransform* parent) {
		joints_[root_].GetWorldTransform().parent_ = parent;
	}

	// 接続情報取得
	std::vector<std::pair<int32_t, int32_t>>& GetConnections() { return connections_; }

private:
	///************************* メンバ変数 *************************///

	// ルートジョイントのインデックス
	int32_t root_;

	// ジョイントマップ
	std::map<std::string, int32_t> jointMap_;

	// ジョイントリスト
	std::vector<Joint> joints_;

	// 接続情報
	std::vector<std::pair<int32_t, int32_t>> connections_;
};
