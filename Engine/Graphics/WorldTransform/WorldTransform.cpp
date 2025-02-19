#include "WorldTransform.h"
// Engine
#include "DX./DirectXCommon.h"
#include "Object3D/Object3dCommon.h"

// Math
#include "MathFunc.h"

void WorldTransform::Initialize()
{
    // ワールド行列の初期化
    matWorld_ = MakeAffineMatrix(scale_, rotation_, translation_);

    // 定数バッファ生成
    CreateConstBuffer();
}

void WorldTransform::CreateConstBuffer()
{
    // MVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
    constBuffer_ = Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
    // 書き込むためのアドレスを取得
    constBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
    // 単位行列を書き込んでおく
    transformData_->WVP = MakeIdentity4x4();
    transformData_->World = MakeIdentity4x4();
    transformData_->WorldInverse = TransPose(Inverse(transformData_->World));
}

void WorldTransform::UpdateMatrix()
{
    // スケール、回転、平行移動を合成して行列を計算する
    matWorld_ = MakeAffineMatrix(scale_, rotation_, translation_);

    // ワールド行列を定数バッファに転送
    if (transformData_ != nullptr) {
        // 親が存在する場合、親のワールド行列を掛け合わせる
        if (parent_) {
            Matrix4x4 parentMatrix = parent_->matWorld_;
            matWorld_ = matWorld_ * parentMatrix; // 親の行列と自身の行列を合成
        }

        transformData_->World = matWorld_; // 定数バッファに行列をコピー
        transformData_->WorldInverse = Inverse(matWorld_);
    }
}
