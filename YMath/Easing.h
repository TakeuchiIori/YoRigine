#ifndef EASING_H
#define EASING_H

#include <string>
#include <unordered_map>
#include <numbers>
#include <algorithm>

class Easing {
public:
	enum class Function {
		Linear,

		EaseInSine, EaseOutSine, EaseInOutSine,
		EaseInQuad, EaseOutQuad, EaseInOutQuad,
		EaseInCubic, EaseOutCubic, EaseInOutCubic,
		EaseInQuart, EaseOutQuart, EaseInOutQuart,
		EaseInQuint, EaseOutQuint, EaseInOutQuint,
		EaseInExpo, EaseOutExpo, EaseInOutExpo,
		EaseInCirc, EaseOutCirc, EaseInOutCirc,
		EaseInBack, EaseOutBack, EaseInOutBack,
		EaseInElastic, EaseOutElastic, EaseInOutElastic,
		EaseInBounce, EaseOutBounce, EaseInOutBounce,
		EaseOutGrowBounce
	};

	// 指定されたイージング関数で値を計算
	static float Ease(Function func, float x);

	// 文字列から列挙型への変換
	static Function functionFromString(const std::string& name);

	// 個別の関数を直接利用する場合のAPI
	static float linear(float x);

	static float EaseInSine(float x);
	static float EaseOutSine(float x);
	static float EaseInOutSine(float x);

	static float EaseInQuad(float x);
	static float EaseOutQuad(float x);
	static float EaseInOutQuad(float x);

	static float EaseInCubic(float x);
	static float EaseOutCubic(float x);
	static float EaseInOutCubic(float x);

	static float EaseInQuart(float x);
	static float EaseOutQuart(float x);
	static float EaseInOutQuart(float x);

	static float EaseInQuint(float x);
	static float EaseOutQuint(float x);
	static float EaseInOutQuint(float x);

	static float EaseInExpo(float x);
	static float EaseOutExpo(float x);
	static float EaseInOutExpo(float x);

	static float EaseInCirc(float x);
	static float EaseOutCirc(float x);
	static float EaseInOutCirc(float x);

	static float EaseInBack(float x);
	static float EaseOutBack(float x);
	static float EaseInOutBack(float x);

	static float EaseInElastic(float x);
	static float EaseOutElastic(float x);
	static float EaseInOutElastic(float x);

	static float EaseInBounce(float x);
	static float EaseOutBounce(float x);
	static float EaseInOutBounce(float x);


	//  徐々に大きくなり、最後にバウンドするイージング
	static float EaseOutGrowBounce(float t);

private:
	static constexpr float PI = std::numbers::pi_v<float>;
	static constexpr float c1 = 1.70158f;
	static constexpr float c2 = c1 * 1.525f;
	static constexpr float c3 = c1 + 1.0f;
	static constexpr float c4 = (2.0f * PI) / 3.0f;
	static constexpr float c5 = (2.0f * PI) / 4.5f;
};

#endif // EASING_H