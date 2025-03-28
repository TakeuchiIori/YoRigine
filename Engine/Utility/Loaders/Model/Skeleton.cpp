#include "Skeleton.h"
#include "Drawer/LineManager/Line.h"

void Skeleton::CreateSkeleton(const Node& rootNode)
{
	root_ = Joint::CreateJoint(rootNode, {}, joints_);

	// 名前とindexのマッピングを行いアクセスしやすくなる
	for ( Joint& joint : joints_) {
		jointMap_.emplace(joint.GetName() , joint.GetIndex());

		if (joint.GetParent().has_value()) {
			connections_.emplace_back(joint.GetParent().value(), joint.GetIndex());
		}
	}

}

void Skeleton::UpdateSkeleton()
{
	// すべてのJointを更新。親が若いので通常ループで処理が可能になっている
	for (Joint& joint : joints_) {
	
		joint.Update(joints_);
	}
}

void Skeleton::DrawSkeleton(Line& line)
{
	if (joints_.empty()) {
		return;
	}

	// スケルトン内の全ての接続を描画
	for (const auto& connection : connections_) {
		int32_t parentIndex = connection.first;
		int32_t childIndex = connection.second;

		// 親ジョイントと子ジョイントのワールド座標を取得
		const Vector3& parentPosition = Joint::ExtractJointPosition(joints_[parentIndex]);
		const Vector3& childPosition = Joint::ExtractJointPosition(joints_[childIndex]);
		line.RegsterLine(parentPosition, childPosition);
	}
}
