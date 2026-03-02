#pragma once
class YoRigineMath
{
public:

	template<typename T>

	static T Clamp(const T& value, const T& min, const T& max)
	{
		if (value < min) return min;
		if (value > max) return max;
		return value;
	}



};

