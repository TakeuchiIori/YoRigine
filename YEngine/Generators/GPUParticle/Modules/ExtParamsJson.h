#pragma once
// ===========================================================================
// 拡張Paramモジュール(UVScroll/ScalePulse/ColorFlicker)の nlohmann::json 変換定義。
// GpuForceFieldParams (GpuEmitterJson.h) と同じ方針: from_json は value() で
// 欠損キーを既定値にフォールバックする。
// ===========================================================================
#include <json.hpp>
#include "GpuExtModules.h"
#include "Loaders/Json/ConversionJson.h" // Vector2 等の to_json/from_json

inline void to_json(nlohmann::json& j, const UVScrollParams& p)
{
	j = nlohmann::json{
		{"isEnable",    p.isEnable},
		{"scrollSpeed", p.scrollSpeed},
	};
}
inline void from_json(const nlohmann::json& j, UVScrollParams& p)
{
	const UVScrollParams d{};
	p.isEnable    = j.value("isEnable", d.isEnable);
	p.scrollSpeed = j.value("scrollSpeed", d.scrollSpeed);
}

inline void to_json(nlohmann::json& j, const ScalePulseParams& p)
{
	j = nlohmann::json{
		{"isEnable",  p.isEnable},
		{"amplitude", p.amplitude},
		{"frequency", p.frequency},
	};
}
inline void from_json(const nlohmann::json& j, ScalePulseParams& p)
{
	const ScalePulseParams d{};
	p.isEnable  = j.value("isEnable", d.isEnable);
	p.amplitude = j.value("amplitude", d.amplitude);
	p.frequency = j.value("frequency", d.frequency);
}

inline void to_json(nlohmann::json& j, const ColorFlickerParams& p)
{
	j = nlohmann::json{
		{"isEnable",  p.isEnable},
		{"speed",     p.speed},
		{"intensity", p.intensity},
	};
}
inline void from_json(const nlohmann::json& j, ColorFlickerParams& p)
{
	const ColorFlickerParams d{};
	p.isEnable  = j.value("isEnable", d.isEnable);
	p.speed     = j.value("speed", d.speed);
	p.intensity = j.value("intensity", d.intensity);
}

inline void to_json(nlohmann::json& j, const DragParams& p)
{
	j = nlohmann::json{
		{"isEnable",    p.isEnable},
		{"coefficient", p.coefficient},
	};
}
inline void from_json(const nlohmann::json& j, DragParams& p)
{
	const DragParams d{};
	p.isEnable    = j.value("isEnable", d.isEnable);
	p.coefficient = j.value("coefficient", d.coefficient);
}

inline void to_json(nlohmann::json& j, const StretchByVelocityParams& p)
{
	j = nlohmann::json{
		{"isEnable",   p.isEnable},
		{"scale",      p.scale},
		{"maxStretch", p.maxStretch},
	};
}
inline void from_json(const nlohmann::json& j, StretchByVelocityParams& p)
{
	const StretchByVelocityParams d{};
	p.isEnable   = j.value("isEnable", d.isEnable);
	p.scale      = j.value("scale", d.scale);
	p.maxStretch = j.value("maxStretch", d.maxStretch);
}

inline void to_json(nlohmann::json& j, const BounceParams& p)
{
	j = nlohmann::json{
		{"isEnable",     p.isEnable},
		{"groundY",      p.groundY},
		{"restitution",  p.restitution},
		{"friction",     p.friction},
	};
}
inline void from_json(const nlohmann::json& j, BounceParams& p)
{
	const BounceParams d{};
	p.isEnable    = j.value("isEnable", d.isEnable);
	p.groundY     = j.value("groundY", d.groundY);
	p.restitution = j.value("restitution", d.restitution);
	p.friction    = j.value("friction", d.friction);
}

inline void to_json(nlohmann::json& j, const EmissiveParams& p)
{
	j = nlohmann::json{
		{"isEnable",  p.isEnable},
		{"intensity", p.intensity},
	};
}
inline void from_json(const nlohmann::json& j, EmissiveParams& p)
{
	const EmissiveParams d{};
	p.isEnable  = j.value("isEnable", d.isEnable);
	p.intensity = j.value("intensity", d.intensity);
}

inline void to_json(nlohmann::json& j, const FlipbookParams& p)
{
	j = nlohmann::json{
		{"isEnable", p.isEnable},
		{"cols",     p.cols},
		{"rows",     p.rows},
		{"fps",      p.fps},
	};
}
inline void from_json(const nlohmann::json& j, FlipbookParams& p)
{
	const FlipbookParams d{};
	p.isEnable = j.value("isEnable", d.isEnable);
	p.cols     = j.value("cols", d.cols);
	p.rows     = j.value("rows", d.rows);
	p.fps      = j.value("fps", d.fps);
}
