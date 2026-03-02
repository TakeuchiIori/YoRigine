#pragma once

// C++
#include <optional>
#include <map>
#include <vector>

// Engine
#include "../Node/Node.h"
#include <WorldTransform/WorldTransform.h>

// Math
#include "Quaternion.h"
#include "Vector3.h"

// ジョイントクラス
class Joint
{
public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// ジョイント更新
	void Update(std::vector<Joint>& joints);

	// ジョイント作成
	static int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);

	// ジョイント位置抽出
	static Vector3 ExtractJointPosition(const Joint& joint);

public:
	///************************* アクセッサ *************************///

	// トランスフォーム設定
	void SetTransform(const QuaternionTransform& _transform) { transform_ = _transform; }

	// トランスフォーム取得
	const QuaternionTransform& GetTransform() const { return transform_; }

	// スケルトンスペース行列取得
	Matrix4x4 GetSkeletonSpaceMatrix() const { return skeletonSpaceMatrix_; }

	// 名前取得
	const std::string& GetName() const { return name_; }

	// インデックス取得
	int32_t& GetIndex() { return index_; }

	// 親ジョイント取得
	std::optional<int32_t>& GetParent() { return parent_; }

	// ワールドトランスフォーム取得
	WorldTransform& GetWorldTransform() { return wt_; }

private:
	///************************* メンバ変数 *************************///

	// ワールドトランスフォーム
	WorldTransform wt_;

	// トランスフォーム
	QuaternionTransform transform_;

	// ローカル行列
	Matrix4x4 localMatrix_;

	// スケルトンスペース行列
	Matrix4x4 skeletonSpaceMatrix_;

	// ジョイント名
	std::string name_;

	// 子ジョイントのインデックス
	std::vector<int32_t> children_;

	// 自身のインデックス
	int32_t index_;

	// 親ジョイントのインデックス
	std::optional<int32_t> parent_;
};