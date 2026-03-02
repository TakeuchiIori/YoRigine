#include "Easing.h"


float Easing::Ease(Function func, float x) {
	switch (func) {
	case Function::Linear: return linear(x);

	case Function::EaseInSine: return EaseInSine(x);
	case Function::EaseOutSine: return EaseOutSine(x);
	case Function::EaseInOutSine: return EaseInOutSine(x);

	case Function::EaseInQuad: return EaseInQuad(x);
	case Function::EaseOutQuad: return EaseOutQuad(x);
	case Function::EaseInOutQuad: return EaseInOutQuad(x);

	case Function::EaseInCubic: return EaseInCubic(x);
	case Function::EaseOutCubic: return EaseOutCubic(x);
	case Function::EaseInOutCubic: return EaseInOutCubic(x);

	case Function::EaseInQuart: return EaseInQuart(x);
	case Function::EaseOutQuart: return EaseOutQuart(x);
	case Function::EaseInOutQuart: return EaseInOutQuart(x);

	case Function::EaseInQuint: return EaseInQuint(x);
	case Function::EaseOutQuint: return EaseOutQuint(x);
	case Function::EaseInOutQuint: return EaseInOutQuint(x);

	case Function::EaseInExpo: return EaseInExpo(x);
	case Function::EaseOutExpo: return EaseOutExpo(x);
	case Function::EaseInOutExpo: return EaseInOutExpo(x);

	case Function::EaseInCirc: return EaseInCirc(x);
	case Function::EaseOutCirc: return EaseOutCirc(x);
	case Function::EaseInOutCirc: return EaseInOutCirc(x);

	case Function::EaseInBack: return EaseInBack(x);
	case Function::EaseOutBack: return EaseOutBack(x);
	case Function::EaseInOutBack: return EaseInOutBack(x);

	case Function::EaseInElastic: return EaseInElastic(x);
	case Function::EaseOutElastic: return EaseOutElastic(x);
	case Function::EaseInOutElastic: return EaseInOutElastic(x);

	case Function::EaseInBounce: return EaseInBounce(x);
	case Function::EaseOutBounce: return EaseOutBounce(x);
	case Function::EaseInOutBounce: return EaseInOutBounce(x);

	case Function::EaseOutGrowBounce: return EaseOutGrowBounce(x);

	default: return x; // デフォルトは線形
	}
}

// 文字列から列挙型への変換
Easing::Function Easing::functionFromString(const std::string& name) {
	static const std::unordered_map<std::string, Function> functionMap = {
		{"linear", Function::Linear},

		{"EaseInSine", Function::EaseInSine},
		{"EaseOutSine", Function::EaseOutSine},
		{"EaseInOutSine", Function::EaseInOutSine},

		{"EaseInQuad", Function::EaseInQuad},
		{"EaseOutQuad", Function::EaseOutQuad},
		{"EaseInOutQuad", Function::EaseInOutQuad},

		{"EaseInCubic", Function::EaseInCubic},
		{"EaseOutCubic", Function::EaseOutCubic},
		{"EaseInOutCubic", Function::EaseInOutCubic},

		{"EaseInQuart", Function::EaseInQuart},
		{"EaseOutQuart", Function::EaseOutQuart},
		{"EaseInOutQuart", Function::EaseInOutQuart},

		{"EaseInQuint", Function::EaseInQuint},
		{"EaseOutQuint", Function::EaseOutQuint},
		{"EaseInOutQuint", Function::EaseInOutQuint},

		{"EaseInExpo", Function::EaseInExpo},
		{"EaseOutExpo", Function::EaseOutExpo},
		{"EaseInOutExpo", Function::EaseInOutExpo},

		{"EaseInCirc", Function::EaseInCirc},
		{"EaseOutCirc", Function::EaseOutCirc},
		{"EaseInOutCirc", Function::EaseInOutCirc},

		{"EaseInBack", Function::EaseInBack},
		{"EaseOutBack", Function::EaseOutBack},
		{"EaseInOutBack", Function::EaseInOutBack},

		{"EaseInElastic", Function::EaseInElastic},
		{"EaseOutElastic", Function::EaseOutElastic},
		{"EaseInOutElastic", Function::EaseInOutElastic},

		{"EaseInBounce", Function::EaseInBounce},
		{"EaseOutBounce", Function::EaseOutBounce},
		{"EaseInOutBounce", Function::EaseInOutBounce},

		{"EaseOutGrowBounce", Function::EaseOutGrowBounce}


	};

	auto it = functionMap.find(name);
	return it != functionMap.end() ? it->second : Function::Linear;
}

float Easing::linear(float x) {
	return x;
}

float Easing::EaseInSine(float x) {
	return 1.0f - cos((x * PI) / 2.0f);
}

float Easing::EaseOutSine(float x) {
	return sin((x * PI) / 2.0f);
}

float Easing::EaseInOutSine(float x) {
	return -(cos(PI * x) - 1.0f) / 2.0f;
}

float Easing::EaseInQuad(float x) {
	return x * x;
}

float Easing::EaseOutQuad(float x) {
	return 1.0f - (1.0f - x) * (1.0f - x);
}

float Easing::EaseInOutQuad(float x) {
	return x < 0.5f ? 2.0f * x * x : 1.0f - pow(-2.0f * x + 2.0f, 2.0f) / 2.0f;
}

float Easing::EaseInCubic(float x) {
	return x * x * x;
}

float Easing::EaseOutCubic(float x) {
	return 1.0f - pow(1.0f - x, 3.0f);
}

float Easing::EaseInOutCubic(float x) {
	return x < 0.5f ? 4.0f * x * x * x : 1.0f - pow(-2.0f * x + 2.0f, 3.0f) / 2.0f;
}

float Easing::EaseInQuart(float x) {
	return x * x * x * x;
}

float Easing::EaseOutQuart(float x) {
	return 1.0f - pow(1.0f - x, 4.0f);
}

float Easing::EaseInOutQuart(float x) {
	return x < 0.5f ? 8.0f * x * x * x * x : 1.0f - pow(-2.0f * x + 2.0f, 4.0f) / 2.0f;
}

float Easing::EaseInQuint(float x) {
	return x * x * x * x * x;
}

float Easing::EaseOutQuint(float x) {
	return 1.0f - pow(1.0f - x, 5.0f);
}

float Easing::EaseInOutQuint(float x) {
	return x < 0.5f ? 16.0f * x * x * x * x * x : 1.0f - pow(-2.0f * x + 2.0f, 5.0f) / 2.0f;
}

float Easing::EaseInExpo(float x) {
	return x == 0.0f ? 0.0f : pow(2.0f, 10.0f * x - 10.0f);
}

float Easing::EaseOutExpo(float x) {
	return x == 1.0f ? 1.0f : 1.0f - pow(2.0f, -10.0f * x);
}

float Easing::EaseInOutExpo(float x) {
	return x == 0.0f ? 0.0f : x == 1.0f ? 1.0f : x < 0.5f
		? pow(2.0f, 20.0f * x - 10.0f) / 2.0f
		: (2.0f - pow(2.0f, -20.0f * x + 10.0f)) / 2.0f;
}

float Easing::EaseInCirc(float x) {
	return 1.0f - sqrt(1.0f - pow(x, 2.0f));
}

float Easing::EaseOutCirc(float x) {
	return sqrt(1.0f - pow(x - 1.0f, 2.0f));
}

float Easing::EaseInOutCirc(float x) {
	return x < 0.5f
		? (1.0f - sqrt(1.0f - pow(2.0f * x, 2.0f))) / 2.0f
		: (sqrt(1.0f - pow(-2.0f * x + 2.0f, 2.0f)) + 1.0f) / 2.0f;
}

float Easing::EaseInBack(float x) {
	return c3 * x * x * x - c1 * x * x;
}

float Easing::EaseOutBack(float x) {
	return 1.0f + c3 * pow(x - 1.0f, 3.0f) + c1 * pow(x - 1.0f, 2.0f);
}

float Easing::EaseInOutBack(float x) {
	return x < 0.5f
		? (pow(2.0f * x, 2.0f) * ((c2 + 1.0f) * 2.0f * x - c2)) / 2.0f
		: (pow(2.0f * x - 2.0f, 2.0f) * ((c2 + 1.0f) * (x * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
}

float Easing::EaseInElastic(float x) {
	if (x == 0.0f) return 0.0f;
	if (x == 1.0f) return 1.0f;
	return -pow(2.0f, 10.0f * x - 10.0f) * sin((x * 10.0f - 10.75f) * c4);
}

float Easing::EaseOutElastic(float x) {
	if (x == 0.0f) return 0.0f;
	if (x == 1.0f) return 1.0f;
	return pow(2.0f, -10.0f * x) * sin((x * 10.0f - 0.75f) * c4) + 1.0f;
}

float Easing::EaseInOutElastic(float x) {
	if (x == 0.0f) return 0.0f;
	if (x == 1.0f) return 1.0f;
	return x < 0.5f
		? -(pow(2.0f, 20.0f * x - 10.0f) * sin((20.0f * x - 11.125f) * c5)) / 2.0f
		: (pow(2.0f, -20.0f * x + 10.0f) * sin((20.0f * x - 11.125f) * c5)) / 2.0f + 1.0f;
}

float Easing::EaseInBounce(float x) {
	return 1.0f - EaseOutBounce(1.0f - x);
}

float Easing::EaseOutBounce(float x) {
	const float n1 = 7.5625f;
	const float d1 = 2.75f;

	if (x < 1.0f / d1) {
		return n1 * x * x;
	} else if (x < 2.0f / d1) {
		x -= 1.5f / d1;
		return n1 * x * x + 0.75f;
	} else if (x < 2.5f / d1) {
		x -= 2.25f / d1;
		return n1 * x * x + 0.9375f;
	} else {
		x -= 2.625f / d1;
		return n1 * x * x + 0.984375f;
	}
}

float Easing::EaseInOutBounce(float x) {
	return x < 0.5f
		? (1.0f - EaseOutBounce(1.0f - 2.0f * x)) / 2.0f
		: (1.0f + EaseOutBounce(2.0f * x - 1.0f)) / 2.0f;
}

float Easing::EaseOutGrowBounce(float t)
{
	// クランプして安全に（念のため）
	t = std::clamp(t, 0.0f, 1.0f);

	if (t < 0.2f) {
		// 前半：急激に成長（1.6倍まで）
		float f = t / 0.2f;
		float grow = (1.0f - powf(1.0f - f, 2.0f)) * 0.6f; // 0〜0.6の範囲で増加
		return 1.0f + grow;
	} else {
		// 後半：バウンドしながら1.0に収束
		float f = (t - 0.2f) / 0.8f; // 0〜1
		float damping = powf(1.0f - f, 2.0f); // 減衰（指数は安定的な2.0）
		float bounce = sinf(f * 8.0f * PI) * damping * 0.3f; // 周期と振幅を調整
		return 1.0f + bounce;
	}
}