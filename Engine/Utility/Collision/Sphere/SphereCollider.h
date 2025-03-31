#pragma once
#include "../BaseCollider.h"
#include "MathFunc.h"

class SphereCollider : public BaseCollider
{
public:
	/*===============================================================//
	

							  ポリモーフィズム


	//===============================================================*/

	~SphereCollider();
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
	///  球を取得
	/// </summary>
	Sphere GetSphere() const { return sphere_; }
	/// <summary>
	///  球を設定
	/// </summary>
	void SetSphere(Sphere sphere) { sphere_ = sphere; }

	float GetRadius() const { return sphere_.radius; }
	float GetRadius() { return sphere_.radius; }
private:
	Sphere sphere_;
	Sphere sphereOffset_;
	float radius_;
};

