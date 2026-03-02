#pragma once
#include <json.hpp>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

//----------------------------------------
// Vector2 → JSON
//----------------------------------------
static inline nlohmann::json Vector2ToJson(const Vector2& v)
{
	return {
		{"x", v.x},
		{"y", v.y}
	};
}
//----------------------------------------
// Vector2 → JSON
//----------------------------------------
static inline Vector2 JsonToVector2(const nlohmann::json& j)
{
	return Vector2{
		j.value("x", 0.0f),
		j.value("y", 0.0f),
	};
}

//----------------------------------------
// Vector3 → JSON
//----------------------------------------
static inline nlohmann::json Vector3ToJson(const Vector3& v)
{
	return {
		{"x", v.x},
		{"y", v.y},
		{"z", v.z}
	};
}

//----------------------------------------
// JSON → Vector3
//----------------------------------------
static inline Vector3 JsonToVector3(const nlohmann::json& j)
{
	return Vector3{
		j.value("x", 0.0f),
		j.value("y", 0.0f),
		j.value("z", 0.0f)
	};
}

//----------------------------------------
// Vector4 → JSON
//----------------------------------------
static inline nlohmann::json Vector4ToJson(const Vector4& v)
{
	return {
		{"x", v.x},
		{"y", v.y},
		{"z", v.z},
		{"w", v.w}
	};
}

//----------------------------------------
// JSON → Vector4
//----------------------------------------
static inline Vector4 JsonToVector4(const nlohmann::json& j)
{
	return Vector4{
		j.value("x", 0.0f),
		j.value("y", 0.0f),
		j.value("z", 0.0f),
		j.value("w", 0.0f)
	};
}

//----------------------------------------
// float → JSON
//----------------------------------------
static inline nlohmann::json FloatToJson(float v)
{
	return v;
}

//----------------------------------------
// JSON → float
//----------------------------------------
static inline float JsonToFloat(const nlohmann::json& j)
{
	return j.get<float>();
}

//----------------------------------------
// bool → JSON
//----------------------------------------
static inline nlohmann::json BoolToJson(bool v)
{
	return v;
}

//----------------------------------------
// JSON → bool
//----------------------------------------
static inline bool JsonToBool(const nlohmann::json& j)
{
	return j.get<bool>();
}

