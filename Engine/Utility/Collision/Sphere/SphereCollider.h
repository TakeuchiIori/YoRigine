#pragma once
#include "../Core/BaseCollider.h"
#include "MathFunc.h"

class SphereCollider : public BaseCollider
{
public:
	/*===============================================================//
	

							  ポリモーフィズム


	//===============================================================*/

	~SphereCollider() = default;
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
	void InitJson();
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

	void SetRadius(float radius) { radius_ = radius; }
private:
	Sphere sphere_;
	Sphere sphereOffset_;
	float radius_;
};

