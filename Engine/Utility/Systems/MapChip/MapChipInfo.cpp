#include "MapChipInfo.h"

// Math
#include "Matrix4x4.h"
#include "MathFunc.h"

MapChipInfo::~MapChipInfo()
{

	for (auto& row : wt_) {
		for (auto& ptr : row) {
			delete ptr;
			ptr = nullptr;
		}
	}

	delete obj_;
	delete mpField_;
}

void MapChipInfo::Initialize()
{
	mpField_ = new MapChipField();
	mpField_->LoadMapChipCsv("Resources/images/MapChip.csv");

	obj_ = new Object3d();
	obj_->Initialize();
	obj_->SetModel("cube.obj");

	GenerateBlocks();
}

void MapChipInfo::Update()
{

	for (std::vector<WorldTransform*>& row : wt_) {
		for (WorldTransform* wt : row) {
			if (wt) {
				Matrix4x4 scaleMatrix = MakeScaleMatrix(wt->scale_);
				// 各軸の回転行列
				Matrix4x4 rotX = MakeRotateMatrixX(wt->rotation_.x);
				Matrix4x4 rotY = MakeRotateMatrixY(wt->rotation_.y);
				Matrix4x4 rotZ = MakeRotateMatrixZ(wt->rotation_.z);
				Matrix4x4 rotXYZ = Multiply(rotX, Multiply(rotY, rotZ));
				// 平行移動行列
				Matrix4x4 translateMatrix = MakeTranslateMatrix(wt->translation_);
				wt->UpdateMatrix();
			}
		}
	}


}

void MapChipInfo::Draw()
{
	for (std::vector<WorldTransform*>& wt : wt_) {
		for (WorldTransform* worldTransformBlock : wt) {
			if (!worldTransformBlock)
				continue;
			obj_->Draw(camera_, *worldTransformBlock);

		}
	}


}

void MapChipInfo::GenerateBlocks()
{
	// 要素数
	uint32_t numBlockVirtical = mpField_->GetNumBlockVertical();
	uint32_t numBlockHorizotal = mpField_->GetNumBlockHorizontal();
	// 列数を設定 (縦方向のブロック数)
	wt_.resize(numBlockVirtical);
	// キューブの生成
	for (uint32_t i = 0; i < numBlockVirtical; ++i) {
		// 1列の要素数を設定 (横方向のブロック数)
		wt_[i].resize(numBlockHorizotal);
	}
	// ブロックの生成
	for (uint32_t i = 0; i < numBlockVirtical; ++i) {
		for (uint32_t j = 0; j < numBlockHorizotal; ++j) {
			// どちらも2で割り切れる時またはどちらも割り切れない時
			//i % 2 == 0 && j % 2 == 0 || i % 2 != 0 && j % 2 != 0 02_02の穴あき
			if (mpField_->GetMapChipTypeByIndex(j, i) == MapChipType::kBlock) {
				WorldTransform* worldTransform = new WorldTransform();
				worldTransform->Initialize();
				wt_[i][j] = worldTransform;
				wt_[i][j]->translation_ = mpField_->GetMapChipPositionByIndex(j, i);
			}
		}
	}
}
