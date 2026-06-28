#pragma once
#include <string>
#include <vector>

// Engine
#include "Loaders/Json/EnumRegisterMacro.h"

// ---- BlendMode定義 ----
enum class BlendMode {
	kBlendModeNone,
	kBlendModeNormal,
	kBlendModeAdd,
	kBlendModeSubtract,
	kBlendModeMultiply,
	kBlendModeScreen,
	kCount0fBlendMode,
};
REGISTER_ENUM(BlendMode,
	"None", "Normal", "Add", "Subtract", "Multiply", "Screen"
);

// ---- ParticleManager関連のenum定義 ----
namespace ParticleManagerEnums {

	// 🌪️ 力場タイプ
	enum class ForceType {
		None = 0,
		Gravity = 1,
		Wind = 2,
		Vortex = 3,
		Radial = 4,
		Turbulence = 5,
		Spring = 6,
		Damping = 7,
		Magnet = 8
	};

	REGISTER_ENUM(ForceType,
		"None", "Gravity", "Wind", "Vortex", "Radial",
		"Turbulence", "Spring", "Damping", "Magnet"
	);

	// 📐 発生形状タイプ
	enum class EmissionType {
		Point = 0,
		Sphere = 1,
		Box = 2,
		Circle = 3,
		Ring = 4,
		Cone = 5,
		Line = 6,
		Hemisphere = 7
	};

	REGISTER_ENUM(EmissionType,
		"Point", "Sphere", "Box", "Circle", "Ring",
		"Cone", "Line", "Hemisphere"
	);

	// 🎨 カラー変化タイプ
	enum class ColorChangeType {
		None = 0,
		Fade = 1,
		Gradient = 2,
		Flash = 3,
		Rainbow = 4,
		Fire = 5,
		Electric = 6
	};

	REGISTER_ENUM(ColorChangeType,
		"None", "Fade", "Gradient", "Flash", "Rainbow",
		"Fire", "Electric"
	);

	// 📏 スケール変化タイプ
	enum class ScaleChangeType {
		None = 0,
		Shrink = 1,
		Grow = 2,
		Pulse = 3,
		Stretch = 4
	};

	REGISTER_ENUM(ScaleChangeType,
		"None", "Shrink", "Grow", "Pulse", "Stretch"
	);

	// 🔄 回転タイプ
	enum class RotationType {
		None = 0,
		ConstantX = 1,
		ConstantY = 2,
		ConstantZ = 3,
		Random = 4,
		Velocity = 5,
		Tumble = 6
	};

	REGISTER_ENUM(RotationType,
		"None", "ConstantX", "ConstantY", "ConstantZ",
		"Random", "Velocity", "Tumble"
	);

	// 🏃 移動パターン
	enum class MovementType {
		Linear = 0,
		Curve = 1,
		Spiral = 2,
		Wave = 3,
		Bounce = 4,
		Orbit = 5,
		Zigzag = 6
	};

	REGISTER_ENUM(MovementType,
		"Linear", "Curve", "Spiral", "Wave", "Bounce",
		"Orbit", "Zigzag"
	);

}