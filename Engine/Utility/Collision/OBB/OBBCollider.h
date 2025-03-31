#pragma once
#include "../BaseCollider.h"
#include "MathFunc.h"

class OBBCollider : public BaseCollider
{
public:
	/*===============================================================//


							 ポリモーフィズム


	//===============================================================*/

	~OBBCollider();
	void InitJson(JsonManager* jsonManager);
	Vector3 GetCenterPosition() const override;
	Vector3 GetScale() const override;
	Vector3 GetAnchorPoint() const override;
	Matrix4x4 GetWorldMatrix() const override;
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

