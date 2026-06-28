#pragma once
#include "Mesh.h"
#include <memory>

// メッシュプリミティブの種類
enum class MeshPrimitiveType
{
	Plane,
	Box,
	Ring,
	Cylinder,
	Sphere,
	Cone,
	FanShape,
};
// メッシュプリミティブ生成クラス
class MeshPrimitive
{
public:
	static std::shared_ptr<Mesh> CreatePlane(float w, float h);
	static std::shared_ptr<Mesh> CreateBox(float w, float h, float d);
	static std::shared_ptr<Mesh> CreateRing(float outerRadius, float innerRadius, uint32_t divide);
	static std::shared_ptr<Mesh> CreateCylinder(float outerRadius, float innerRadius, uint32_t divide, float height);
	static std::shared_ptr<Mesh> CreateSphere(float radius, uint32_t subdivisions);
	static std::shared_ptr<Mesh> CreateCone(float radius, float height, uint32_t divide);
	static std::shared_ptr<Mesh> CreateFanShape(float radius, float angleDegree, uint32_t divide);
};

