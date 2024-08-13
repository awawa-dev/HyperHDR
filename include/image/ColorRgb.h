#pragma once

#ifndef PCH_ENABLED
	#include <sstream>
	#include <cstdint>
	#include <iostream>
#endif

struct ColorRgb
{
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;

	static const ColorRgb BLACK;
	static const ColorRgb RED;
	static const ColorRgb GREEN;
	static const ColorRgb BLUE;
	static const ColorRgb YELLOW;
	static const ColorRgb WHITE;

	ColorRgb() = default;

	ColorRgb(uint8_t _red, uint8_t _green, uint8_t _blue) :
		red(_red),
		green(_green),
		blue(_blue)
	{

	}

	void getHsv(int& hue, int& sat, int& bri)
	{
		uint16_t h;
		uint8_t s,v;
		rgb2hsv(red, green, blue, h, s, v);
		hue = h;
		sat = s;
		bri = v;
	}

	void fromHsv(int hue, int sat, int bri)
	{
		hsv2rgb(hue, sat, bri, red, green, blue);
	}

	int Red()
	{
		return red;
	}

	int Green()
	{
		return green;
	}

	int Blue()
	{
		return blue;
	}

	void getRGB(int& r, int& g, int& b)
	{
		r = red;
		g = green;
		b = blue;
	}

	void setRed(uint8_t r)
	{
		red = r;
	}

	void setGreen(uint8_t g)
	{
		green = g;
	}

	void setBlue(uint8_t b)
	{
		blue = b;
	}



	inline bool hasColor() const
	{
		return red || green || blue;
	}

	operator std::string()
	{
		std::stringstream ss;
		ss << "(" << std::to_string(red) << ", " << std::to_string(green) << ", " << std::to_string(blue) << ")";
		return ss.str();
	}

	inline static uint8_t clamp(int x)
	{
		return (x < 0) ? 0 : ((x > 255) ? 255 : uint8_t(x));
	}

	static void rgb2hsv(uint8_t red, uint8_t green, uint8_t blue, uint16_t& _hue, uint8_t& _saturation, uint8_t& _value);
	static void hsv2rgb(uint16_t hue, uint8_t saturation, uint8_t value, uint8_t& red, uint8_t& green, uint8_t& blue);
	static void rgb2hsl(uint8_t red, uint8_t green, uint8_t blue, uint16_t& hue, float& saturation, float& luminance);
	static void hsl2rgb(uint16_t hue, float saturation, float luminance, uint8_t& red, uint8_t& green, uint8_t& blue);

	static void yuv2rgb(uint8_t y, uint8_t u, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b);
	static void rgb2yuv(int r, int g, int b, uint8_t& y, uint8_t& u, uint8_t& v);
};

static_assert(sizeof(ColorRgb) == 3, "Incorrect size of ColorRgb");

inline std::ostream& operator<<(std::ostream& os, const ColorRgb& color)
{
	os << "{"
		<< static_cast<unsigned>(color.red) << ","
		<< static_cast<unsigned>(color.green) << ","
		<< static_cast<unsigned>(color.blue)
		<< "}";

	return os;
}

inline bool operator==(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red == rhs.red &&
		lhs.green == rhs.green &&
		lhs.blue == rhs.blue;
}

inline bool operator<(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red < rhs.red&&
		lhs.green < rhs.green&&
		lhs.blue < rhs.blue;
}

inline bool operator!=(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return !(lhs == rhs);
}

inline bool operator<=(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red <= rhs.red &&
		lhs.green <= rhs.green &&
		lhs.blue <= rhs.blue;
}

inline bool operator>(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red > rhs.red &&
		lhs.green > rhs.green &&
		lhs.blue > rhs.blue;
}

inline bool operator>=(const ColorRgb& lhs, const ColorRgb& rhs)
{
	return	lhs.red >= rhs.red &&
		lhs.green >= rhs.green &&
		lhs.blue >= rhs.blue;
}
