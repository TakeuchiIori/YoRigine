#include "TopDownCamera.h"
#include "MathFunc.h"
#include "Matrix4x4.h"
#include <DirectXMath.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // _DEBUG

/// <summary>
/// 初期化処理
/// </summary>
void TopDownCamera::Initialize()
{
}

/// <summary>
/// 更新処理
/// </summary>
void TopDownCamera::Update()
{
	TopDownProsess();
}

/// <summary>
/// 真上視点の処理
/// </summary>
void TopDownCamera::TopDownProsess()
{
	// ターゲットがない場合は処理しない
	if (target_ == nullptr) {
		return;
	}

	// 真上から見下ろす角度に設定
	rotate_ = { DirectX::XMConvertToRadians(90), 0.0f, 0.0f };

	// ターゲット位置にオフセットを加算
	translate_ = target_->translate_ + offset_;

	// ビュー行列を更新
	matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
}
