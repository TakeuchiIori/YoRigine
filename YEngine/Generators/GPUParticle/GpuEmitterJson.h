#pragma once
///=============================================================================
/// GPUエミッタ用の nlohmann::json 変換定義。
///
/// AutoJson は「登録した型を j = *ptr / j.get<T>() でシリアライズ」するため、
/// 構造体型（GpuForceFieldParams など）を扱うには to_json/from_json が必要。
/// ここに1箇所だけ定義しておけば、YGpuEmitManager 側は AutoJson.Add() 1行で
/// フォースフィールド配列ごと保存・読込できる。
///
/// スカラ・Vector・enum は ConversionJson.h / nlohmann 標準で変換されるため
/// 個別定義は不要。新しいフォースフィールド項目を足すときだけ、この2関数へ
/// 1行ずつ追記する。
///=============================================================================
#include <json.hpp>
#include "GpuParticleParams.h"
#include "Loaders/Json/ConversionJson.h"   // Vector3 等の to_json/from_json

//-----------------------------------------------------------------------------
// GpuForceFieldParams
//   enum(shape/mode) は int として保存し、旧JSON互換を保つ。
//   from_json は value() で欠損キーを既定値にフォールバック（後方互換）。
//-----------------------------------------------------------------------------
inline void to_json(nlohmann::json& j, const GpuForceFieldParams& p)
{
	j = nlohmann::json{
		{"isEnable",          p.isEnable},
		{"shape",             static_cast<int>(p.shape)},
		{"center",            p.center},
		{"halfExtents",       p.halfExtents},
		{"radius",            p.radius},
		{"mode",              static_cast<int>(p.mode)},
		{"direction",         p.direction},
		{"strength",          p.strength},
		{"falloff",           p.falloff},
		{"spiralStrengthMin", p.spiralStrengthMin},
		{"spiralStrengthMax", p.spiralStrengthMax},
		{"randomAxisBlend",   p.randomAxisBlend},
		{"orbitHoldRatio",    p.orbitHoldRatio},
		{"approachVariance",  p.approachVariance},
		{"maxSpeed",          p.maxSpeed},
		{"killRadius",        p.killRadius},
	};
}

inline void from_json(const nlohmann::json& j, GpuForceFieldParams& p)
{
	const GpuForceFieldParams d{}; // 既定値ソース
	p.isEnable          = j.value("isEnable", d.isEnable);
	p.shape             = static_cast<GpuFieldShape>(j.value("shape", static_cast<int>(d.shape)));
	p.center            = j.value("center", d.center);
	p.halfExtents       = j.value("halfExtents", d.halfExtents);
	p.radius            = j.value("radius", d.radius);
	p.mode              = static_cast<GpuFieldMode>(j.value("mode", static_cast<int>(d.mode)));
	p.direction         = j.value("direction", d.direction);
	p.strength          = j.value("strength", d.strength);
	p.falloff           = j.value("falloff", d.falloff);
	p.spiralStrengthMin = j.value("spiralStrengthMin", d.spiralStrengthMin);
	p.spiralStrengthMax = j.value("spiralStrengthMax", d.spiralStrengthMax);
	p.randomAxisBlend   = j.value("randomAxisBlend", d.randomAxisBlend);
	p.orbitHoldRatio    = j.value("orbitHoldRatio", d.orbitHoldRatio);
	p.approachVariance  = j.value("approachVariance", d.approachVariance);
	p.maxSpeed          = j.value("maxSpeed", d.maxSpeed);
	p.killRadius        = j.value("killRadius", d.killRadius);
}

//-----------------------------------------------------------------------------
// GpuAccelerationParams
//   ForceFieldParams と同じ方針: enum は int 保存、from_json は value() でフォールバック。
//-----------------------------------------------------------------------------
inline void to_json(nlohmann::json& j, const GpuAccelerationParams& p)
{
	j = nlohmann::json{
		{"isEnable",    p.isEnable},
		{"shape",       static_cast<int>(p.shape)},
		{"center",      p.center},
		{"halfExtents", p.halfExtents},
		{"radius",      p.radius},
		{"direction",   p.direction},
		{"strength",    p.strength},
		{"falloff",     p.falloff},
	};
}

inline void from_json(const nlohmann::json& j, GpuAccelerationParams& p)
{
	const GpuAccelerationParams d{}; // 既定値ソース
	p.isEnable    = j.value("isEnable", d.isEnable);
	p.shape       = static_cast<GpuFieldShape>(j.value("shape", static_cast<int>(d.shape)));
	p.center      = j.value("center", d.center);
	p.halfExtents = j.value("halfExtents", d.halfExtents);
	p.radius      = j.value("radius", d.radius);
	p.direction   = j.value("direction", d.direction);
	p.strength    = j.value("strength", d.strength);
	p.falloff     = j.value("falloff", d.falloff);
}

//-----------------------------------------------------------------------------
// GpuNoiseParams (Curl / Turbulence / Vortex)
//   ForceFieldParams と同じ方針: enum は int 保存、from_json は value() でフォールバック。
//-----------------------------------------------------------------------------
inline void to_json(nlohmann::json& j, const GpuNoiseParams& p)
{
	j = nlohmann::json{
		{"isEnable",    p.isEnable},
		{"type",        static_cast<int>(p.type)},
		{"frequency",   p.frequency},
		{"amplitude",   p.amplitude},
		{"octaves",     p.octaves},
		{"lacunarity",  p.lacunarity},
		{"gain",        p.gain},
		{"scrollSpeed", p.scrollSpeed},
		{"axis",        p.axis},
		{"center",      p.center},
		{"radius",      p.radius},
	};
}

inline void from_json(const nlohmann::json& j, GpuNoiseParams& p)
{
	const GpuNoiseParams d{}; // 既定値ソース
	p.isEnable    = j.value("isEnable", d.isEnable);
	p.type        = static_cast<GpuNoiseType>(j.value("type", static_cast<int>(d.type)));
	p.frequency   = j.value("frequency", d.frequency);
	p.amplitude   = j.value("amplitude", d.amplitude);
	p.octaves     = j.value("octaves", d.octaves);
	p.lacunarity  = j.value("lacunarity", d.lacunarity);
	p.gain        = j.value("gain", d.gain);
	p.scrollSpeed = j.value("scrollSpeed", d.scrollSpeed);
	p.axis        = j.value("axis", d.axis);
	p.center      = j.value("center", d.center);
	p.radius      = j.value("radius", d.radius);
}
