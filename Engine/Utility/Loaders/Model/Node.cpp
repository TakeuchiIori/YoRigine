#include "Node.h"

Node Node::ReadNode(aiNode* node)
{
	Node result;
	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate); // assimpの行列からSRTを抽出する関数を利用
	result.transform_.scale = { scale.x,scale.y,scale.z }; // scaleはそのまま
	result.transform_.rotate = { rotate.x,-rotate.y,-rotate.z,rotate.w }; // x軸を反転。さらに回転方向が逆なので軸を反転させる
	result.transform_.translate = { -translate.x,translate.y,translate.z }; // x軸を反転
	result.localMatrix_ = MakeAffineMatrix(result.transform_.scale, result.transform_.rotate, result.transform_.translate);

	result.name_ = node->mName.C_Str();
	result.children_.resize(node->mNumChildren);
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		result.children_[childIndex] = ReadNode(node->mChildren[childIndex]);
	}

	return result;
}
