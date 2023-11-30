#pragma once

#ifndef PCH_ENABLED
	#include <cstdint>
#endif

class ColorSys
{
public:
	static void rgb2hsl(uint8_t red, uint8_t green, uint8_t blue, uint16_t& hue, float& saturation, float& luminance);
	static void hsl2rgb(uint16_t hue, float saturation, float luminance, uint8_t& red, uint8_t& green, uint8_t& blue);
	static void rgb2hsv(uint8_t red, uint8_t green, uint8_t blue, uint16_t& hue, uint8_t& saturation, uint8_t& value);
	static void hsv2rgb(uint16_t hue, uint8_t saturation, uint8_t value, uint8_t& red, uint8_t& green, uint8_t& blue);
	static void yuv2rgb(uint8_t y, uint8_t u, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b);
	static void rgb2yuv(int r, int g, int b, uint8_t& y, uint8_t& u, uint8_t& v);
};
