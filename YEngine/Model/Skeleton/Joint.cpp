#include "Joint.h"
#include "MathFunc.h"
#include <unordered_set>

void Joint::Initialize()
{
	wt_.Initialize();
}

void Joint::Update(std::vector<Joint>& joints)
{
	localMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	if (parent_) {
		skeletonSpaceMatrix_ = localMatrix_ * joints[*parent_].skeletonSpaceMatrix_;
		wt_.parent_ = &joints[*parent_].wt_;
	}
	else {
		skeletonSpaceMatrix_ = localMatrix_;
	}

	wt_.translate_ = ExtractTranslation(localMatrix_);
	wt_.rotate_ = MatrixToEuler(localMatrix_);

	wt_.UpdateMatrix();
}

// イラン奴は除く
static bool IsIgnoredNode(const std::string& name) {
	static const std::unordered_set<std::string> ignored = {
		"Armature", "Retopology_hp_Plane.002"
	};
	return ignored.count(name) > 0;
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
	joints.push_back(std::move(joint)); // SkeletonのJoint列に追加

	for (const Node& child : node.children_) {
		// 子Jointを作成し、そのIndex
		int32_t childIndex = CreateJoint(child, joint.index_, joints);
		joints[joint.index_].children_.push_back(std::move(childIndex));
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
