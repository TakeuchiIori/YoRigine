#pragma once

// C++
#include <optional>
#include <map>

// Math
#include "Quaternion.h"
#include "Vector3.h"

// assimp
#include <assimp/scene.h>


class Node
{
public:
	static Node ReadNode(aiNode* node);
	Matrix4x4 GetLocalMatrix() const { return localMatrix_; }

	std::string name_;
	std::vector<Node> children_;
	QuaternionTransform transform_;
private:
	Matrix4x4 localMatrix_;

};

