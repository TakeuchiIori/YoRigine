#include "Joint.h"
#include "MathFunc.h"

void Joint::Update(std::vector<Joint>& joints)
{
	localMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	if (parent_) {
		skeletonSpaceMatrix_ = localMatrix_ * joints[*parent_].skeletonSpaceMatrix_;
	}
	else {
		skeletonSpaceMatrix_ = localMatrix_;
	}
}

int32_t Joint::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints)
{
	Joint joint;
	joint.name_ = node.name_;
	joint.localMatrix_= node.GetLocalMatrix();
	joint.skeletonSpaceMatrix_ = MakeIdentity4x4();
	joint.transform_ = node.transform_;
	joint.index_ = int32_t(joints.size()); // 現在登録されているIndexに
	joint.parent_ = parent;
	joints.push_back(joint); // SkeletonのJoint列に追加

	for (const Node& child : node.children_) {
		// 子Jointを作成し、そのIndex
		int32_t childIndex = CreateJoint(child, joint.index_, joints);
		joints[joint.index_].children_.push_back(childIndex);
	}
	// 自身のIndexを返す
	return joint.index_;
}

Vector3 Joint::ExtractJointPosition(const Joint& joint)
{
	return {
		joint.skeletonSpaceMatrix_.m[3][0],
		joint.skeletonSpaceMatrix_.m[3][1],
		joint.skeletonSpaceMatrix_.m[3][2]
	};
}
