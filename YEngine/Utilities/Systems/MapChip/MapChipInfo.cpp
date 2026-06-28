#include "MapChipInfo.h"

// Math
#include "Matrix4x4.h"
#include "MathFunc.h"

MapChipInfo::~MapChipInfo()
{
	mpField_ = nullptr;
}

void MapChipInfo::Initialize()
{
	mpField_ = std::make_unique<MapChipField>();
	mpField_->LoadMapChipCsv("Resources/images/MapChip.csv");

	GenerateBlocks();
}

void MapChipInfo::Update()
{

	for (std::vector<std::unique_ptr<WorldTransform>>& row : wt_) {
		for (auto& wt : row) {
			if (wt) {
				Matrix4x4 scaleMatrix = MakeScaleMatrix(wt->scale_);
				// 各軸の回転行列
				Matrix4x4 rotX = MakeRotateMatrixX(wt->rotate_.x);
				Matrix4x4 rotY = MakeRotateMatrixY(wt->rotate_.y);
				Matrix4x4 rotZ = MakeRotateMatrixZ(wt->rotate_.z);
				Matrix4x4 rotXYZ = Multiply(rotX, Multiply(rotY, rotZ));
				// 平行移動行列
				Matrix4x4 translateMatrix = MakeTranslateMatrix(wt->translate_);
				wt->UpdateMatrix();
			}
		}
	}


}

void MapChipInfo::Draw() {
	for (uint32_t i = 0; i < wt_.size(); ++i) {
		for (uint32_t j = 0; j < wt_[i].size(); ++j) {
			if (wt_[i][j] && objects_[i][j]) {
				objects_[i][j]->Draw(camera_, *wt_[i][j]);
			}
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
	objects_.resize(numBlockVirtical);
	// キューブの生成
	for (uint32_t i = 0; i < numBlockVirtical; ++i) {
		// 1列の要素数を設定 (横方向のブロック数)
		wt_[i].resize(numBlockHorizotal);
		objects_[i].resize(numBlockHorizotal);
	}
	// ブロックの生成
	for (uint32_t i = 0; i < numBlockVirtical; ++i) {
		for (uint32_t j = 0; j < numBlockHorizotal; ++j) {
			// どちらも2で割り切れる時またはどちらも割り切れない時
			//i % 2 == 0 && j % 2 == 0 || i % 2 != 0 && j % 2 != 0 02_02の穴あき
			if (mpField_->GetMapChipTypeByIndex(j, i) == MapChipType::kBlock) {
				std::unique_ptr<WorldTransform> worldTransform = std::make_unique<WorldTransform>();
				worldTransform->Initialize();
				wt_[i][j] = std::move(worldTransform);
				wt_[i][j]->translate_ = mpField_->GetMapChipPositionByIndex(j, i);

				auto obj = std::make_unique<Object3d>();
				obj->Initialize();
				obj->SetModel("cube.obj");
				objects_[i][j] = std::move(obj);
			}
		}
	}
}
