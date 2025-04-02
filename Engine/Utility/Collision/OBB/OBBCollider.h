#pragma once
#include "../Core/BaseCollider.h"
#include "MathFunc.h"

class OBBCollider : public BaseCollider
{
public:
	/*===============================================================//


							 ポリモーフィズム


	//===============================================================*/

	~OBBCollider() = default;
	void InitJson(JsonManager* jsonManager) override;
	Vector3 GetCenterPosition() const override;
	const WorldTransform& GetWorldTransform() override;
	Vector3 GetEulerRotation() const override;

	/*===============================================================//


								基本的な関数


	//===============================================================*/	
	
	
	void Initialize();
	void Update();
	void Draw();

public:

	/// <summary>
	///  OBBを取得
	/// </summary>
	OBB GetOBB() const { return obb_; }
	/// <summary>
	///  OBBを設定
	/// </summary>
	void SetOBB(OBB obb) { obb_ = obb; }

private:
	OBB obb_;
	OBB obbOffset_;
	Vector3 obbEulerOffset_;

};

