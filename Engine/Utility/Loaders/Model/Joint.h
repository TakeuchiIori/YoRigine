#pragma once

// C++
#include <optional>
#include <map>
#include<vector>

// Engine

// Math
#include "Quaternion.h"
#include "Vector3.h"
#include "Node.h"


class Joint
{

public:


	/// <summary>
	/// Jointの更新
	/// </summary>
	void Update(std::vector<Joint>& joints);

	/// <summary>
	/// ジョイント作成
	/// </summary>
	static int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);

	/// <summary>
	/// 
	/// </summary>
	static Vector3 ExtractJointPosition(const Joint& joint);


public:

	void SetTransform(const QuaternionTransform& _transform) { transform_ = _transform; }
	Matrix4x4 GetSkeletonSpaceMatrix() const { return skeletonSpaceMatrix_; }
	std::string& GetName() { return name_; }
	int32_t& GetIndex() { return index_; }
	std::optional<int32_t>& GetParent() { return parent_; }

private:

	QuaternionTransform transform_; 
	Matrix4x4 localMatrix_; 
	Matrix4x4 skeletonSpaceMatrix_;	// skeletonSpaceでの変換行列
	std::string name_;
	std::vector<int32_t> children_;	// 子JointのIndexのリスト。いなければ空
	int32_t index_;					// 自身のIndex
	std::optional<int32_t> parent_;	// 親JointのIndex。いなければnull

};

