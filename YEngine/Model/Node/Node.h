#pragma once

// C++
#include <optional>
#include <map>

// Math
#include "Quaternion.h"
#include "Vector3.h"

// assimp
#include <assimp/scene.h>

// ノードクラス
class Node
{
public:
	///************************* 基本関数 *************************///

	// Assimpのノード読み込み
	static Node ReadNode(aiNode* node);

	// ローカル行列取得
	Matrix4x4 GetLocalMatrix() const { return localMatrix_; }

public:
	///************************* メンバ変数 *************************///

	// ノード名
	std::string name_;

	// 子ノード
	std::vector<Node> children_;

	// トランスフォーム
	QuaternionTransform transform_;

	// ローカル行列
	Matrix4x4 localMatrix_;

};
