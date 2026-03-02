#include "ModelCommon.h"

ModelCommon* ModelCommon::GetInstance() {
    static ModelCommon instance;
    return &instance;
}

void ModelCommon::Initialize(YoRigine::DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
}
