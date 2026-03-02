#pragma once

// Engine
#include "../Core/BaseCollider.h"

// Math
#include "MathFunc.h"

// OBBコライダーを扱うクラス
class OBBCollider : public BaseCollider
{
public:
	///************************* ポリモーフィズム *************************///

	~OBBCollider() = default;
	void InitJson(YoRigine::JsonManager* jsonManager) override;
	Vector3 GetCenterPosition() const override;
	const WorldTransform& GetWorldTransform() override;
	Vector3 GetEulerRotation() const override;

public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 描画
	void Draw();

public:
	///************************* アクセッサ *************************///

	// OBB取得
	OBB GetOBB() const { return obb_; }

	// OBB設定
	void SetOBB(OBB obb) { obb_ = obb; }

public:
	///************************* 調整用 *************************///

	OBB obbOffset_;

private:
	///************************* メンバ変数 *************************///

	OBB obb_;
	Vector3 obbEulerOffset_;
};
