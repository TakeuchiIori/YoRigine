#include "Skeleton.h"
#include "Drawer/LineManager/Line.h"
#include <queue> // ★追加

void Skeleton::Create(const Node& rootNode)
{
	root_ = Joint::CreateJoint(rootNode, {}, joints_);

	// 名前とindexのマッピングを行いアクセスしやすくなる
	for (Joint& joint : joints_) {
		jointMap_.emplace(joint.GetName(), joint.GetIndex());

		joint.Initialize();

		if (joint.GetParent().has_value()) {
			connections_.emplace_back(joint.GetParent().value(), joint.GetIndex());
		}
	}

}

void Skeleton::Update()
{
	// すべてのJointを更新。親が若いので通常ループで処理が可能になっている
	for (Joint& joint : joints_) {

		joint.Update(joints_);
	}
}

void Skeleton::Draw(Line& line, const Matrix4x4& worldMatrix)
{
	if (joints_.empty()) {
		return;
	}

	// スケルトン内の全ての接続をBlenderの八面体ボーン（ワイヤーフレーム）として描画
	for (const auto& connection : connections_) {
		int32_t parentIndex = connection.first;
		int32_t childIndex = connection.second;

		// 親(Head)と子(Tail)の座標を取得
		const Vector3& parentLocal = Joint::ExtractJointPosition(joints_[parentIndex]);
		const Vector3& childLocal = Joint::ExtractJointPosition(joints_[childIndex]);

		Vector3 pWorld = Transform(parentLocal, worldMatrix);
		Vector3 cWorld = Transform(childLocal, worldMatrix);

		// 方向と長さを計算
		Vector3 dir = cWorld - pWorld;
		float length = Length(dir);

		// 長さが短すぎる場合は描画をスキップ
		if (length < 0.0001f) continue;

		// 進行方向を正規化
		Vector3 forward = Normalize(dir);

		// 進行方向に直交する2つの軸（right と up）を求める
		Vector3 upGuide = { 0.0f, 1.0f, 0.0f };
		// もしボーンが真上を向いていて外積が計算できない場合の回避処理
		if (std::abs(Dot(forward, upGuide)) > 0.99f) {
			upGuide = { 1.0f, 0.0f, 0.0f };
		}
		Vector3 right = Normalize(Cross(upGuide, forward)); // ※外積関数
		Vector3 up = Normalize(Cross(forward, right));

		// ボーンの太さと、一番太くなる位置の割合（根元から10%の位置を太くする）
		float headRatio = 0.1f;
		float radius = length * 0.08f; // ボーンの長さの8%を太さ（半径）とする
		Vector3 mid = pWorld + (forward * (length * headRatio));

		// 中間（一番太い部分）の4つの頂点を計算
		Vector3 vRight = mid + (right * radius);
		Vector3 vLeft = mid - (right * radius);
		Vector3 vUp = mid + (up * radius);
		Vector3 vDown = mid - (up * radius);

		// 根元(Head) から 中間の4頂点へ（4本）
		line.RegisterLine(pWorld, vRight);
		line.RegisterLine(pWorld, vLeft);
		line.RegisterLine(pWorld, vUp);
		line.RegisterLine(pWorld, vDown);

		// 中間の4頂点 から 先端(Tail)へ（4本）
		line.RegisterLine(vRight, cWorld);
		line.RegisterLine(vLeft, cWorld);
		line.RegisterLine(vUp, cWorld);
		line.RegisterLine(vDown, cWorld);

		// 中間の4頂点同士を結ぶ（断面のひし形・4本）
		line.RegisterLine(vRight, vUp);
		line.RegisterLine(vUp, vLeft);
		line.RegisterLine(vLeft, vDown);
		line.RegisterLine(vDown, vRight);
	}
	line.DrawLine();
}

// ============================================================
//  指定したボーンとその全ての子ボーン（子孫）の名前リストを取得
// （幅優先探索 / BFS を使用）
// ============================================================
std::unordered_set<std::string> Skeleton::GetDescendantBones(const std::string& rootBoneName) const
{
	std::unordered_set<std::string> descendants;

	auto it = jointMap_.find(rootBoneName);
	if (it == jointMap_.end()) {
		return descendants; // 見つからない場合は空のセットを返す
	}

	// 幅優先探索 (BFS) で起点ボーンとその子孫を全て収集
	std::queue<int32_t> queue;
	queue.push(it->second);

	while (!queue.empty()) {
		int32_t currentIndex = queue.front();
		queue.pop();

		const Joint& currentJoint = joints_[currentIndex];

		// 自身の名前をマスク用リストに追加
		descendants.insert(currentJoint.GetName());

		// 子ジョイントをキューに追加して探索を続ける
		for (int32_t childIndex : currentJoint.GetChildren()) {
			queue.push(childIndex);
		}
	}

	return descendants;
}