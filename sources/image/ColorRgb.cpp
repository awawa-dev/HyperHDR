#include <image/ColorRgb.h>
#include <limits>
#include <cmath>

const ColorRgb ColorRgb::BLACK  = {   0,   0,   0 };
const ColorRgb ColorRgb::RED    = { 255,   0,   0 };
const ColorRgb ColorRgb::GREEN  = {   0, 255,   0 };
const ColorRgb ColorRgb::BLUE   = {   0,   0, 255 };
const ColorRgb ColorRgb::YELLOW = { 255, 255,   0 };
const ColorRgb ColorRgb::WHITE  = { 255, 255, 255 };

void ColorRgb::hsl2rgb(uint16_t hue, float saturation, float luminance, uint8_t& red, uint8_t& green, uint8_t& blue)
{
	if (saturation == 0.0f) {
		red = (uint8_t)(luminance * 255.0f);
		green = (uint8_t)(luminance * 255.0f);
		blue = (uint8_t)(luminance * 255.0f);
		return;
	}

	float q;

	if (luminance < 0.5f)
		q = luminance * (1.0f + saturation);
	else
		q = (luminance + saturation) - (luminance * saturation);

	float p = (2.0f * luminance) - q;
	float h = hue / 360.0f;

	float t[3]{};

	t[0] = h + (1.0f / 3.0f);
	t[1] = h;
	t[2] = h - (1.0f / 3.0f);

	for (int i = 0; i < 3; i++) {
		if (t[i] < 0.0f)
			t[i] += 1.0f;
		if (t[i] > 1.0f)
			t[i] -= 1.0f;
	}

	float out[3]{};

	for (int i = 0; i < 3; i++) {
		if (t[i] * 6.0f < 1.0f)
			out[i] = p + (q - p) * 6.0f * t[i];
		else if (t[i] * 2.0f < 1.0f)
			out[i] = q;
		else if (t[i] * 3.0f < 2.0f)
			out[i] = p + (q - p) * ((2.0f / 3.0f) - t[i]) * 6.0f;
		else out[i] = p;
	}

	//convert back to 0...255 range
	red = (uint8_t)(out[0] * 255.0f);
	green = (uint8_t)(out[1] * 255.0f);
	blue = (uint8_t)(out[2] * 255.0f);

}

void ColorRgb::rgb2hsl(uint8_t red, uint8_t green, uint8_t blue, uint16_t& hue, float& saturation, float& luminance)
{
	float r = red / 255.0f;
	float g = green / 255.0f;
	float b = blue / 255.0f;

	float rgbMin = r < g ? (r < b ? r : b) : (g < b ? g : b);
	float rgbMax = r > g ? (r > b ? r : b) : (g > b ? g : b);
	float diff = rgbMax - rgbMin;

	//luminance
	luminance = (rgbMin + rgbMax) / 2.0f;

	if (diff == 0.0f) {
		saturation = 0.0f;
		hue = 0;
		return;
	}

	//saturation
	if (luminance < 0.5f)
		saturation = diff / (rgbMin + rgbMax);
	else
		saturation = diff / (2.0f - rgbMin - rgbMax);

	if (rgbMax == r)
	{
		// start from 360 to be sure that we won't assign a negative number to the unsigned hue value
		hue = 360 + 60 * (g - b) / (rgbMax - rgbMin);

		if (hue > 359)
			hue -= 360;
	}
	else if (rgbMax == g)
	{
		hue = 120 + 60 * (b - r) / (rgbMax - rgbMin);
	}
	else
	{
		hue = 240 + 60 * (r - g) / (rgbMax - rgbMin);
	}

}


void ColorRgb::rgb2hsv(uint8_t red, uint8_t green, uint8_t blue, uint16_t& _hue, uint8_t& _saturation, uint8_t& _value)
{
	float r = red / 255.0;
	float g = green / 255.0;
	float b = blue / 255.0;
	float max = std::max(std::max(r, g), b);
	float min = std::min(std::min(r, g), b);

	float delta = max - min, h, s;


	_value = clamp(std::round(max * 255.0));

	if (delta < std::numeric_limits<float>::epsilon())
	{
		_saturation = 0;
		_hue = 0;
		return;
	}
	if (max > std::numeric_limits<float>::epsilon())
	{
		s = (delta / max);
	}
	else
	{
		_saturation = 0;
		_hue = 0;
		return;
	}

	if (r >= max)
		h = (g - b) / delta;
	else
		if (g >= max)
			h = 2.0 + (b - r) / delta;
		else
			h = 4.0 + (r - g) / delta;

	h *= 60.0;

	while (h < 0.0)
		h += 360.0;
	while (h > 360.0)
		h -= 360.0;

	_hue = std::round(h);
	_saturation = clamp(std::round(s * 255));

}

void ColorRgb::hsv2rgb(uint16_t hue, uint8_t saturation, uint8_t value, uint8_t& red, uint8_t& green, uint8_t& blue)
{
	float hh = hue, p, q, t, ff;
	long  i;
	float outr, outg, outb;
	float ins = saturation / 255.0;
	float inv = value / 255.0;

	if (ins == 0)
	{
		outr = inv;
		outg = inv;
		outb = inv;
	}
	else
	{

		while (hh < 0.0)
			hh += 360.0;
		while (hh > 360.0)
			hh -= 360.0;

		if (hh >= 360.0)
			hh = 0.0;

		hh /= 60.0;

		i = static_cast<long>(std::trunc(hh));
		ff = hh - i;
		p = inv * (1.0 - ins);
		q = inv * (1.0 - (ins * ff));
		t = inv * (1.0 - (ins * (1.0 - ff)));

		switch (i) {
		case 0:
			outr = inv;
			outg = t;
			outb = p;
			break;
		case 1:
			outr = q;
			outg = inv;
			outb = p;
			break;
		case 2:
			outr = p;
			outg = inv;
			outb = t;
			break;
		case 3:
			outr = p;
			outg = q;
			outb = inv;
			break;
		case 4:
			outr = t;
			outg = p;
			outb = inv;
			break;
		case 5:
		default:
			outr = inv;
			outg = p;
			outb = q;
			break;
		}
	}

	red = clamp(std::round(outr * 255.0));
	green = clamp(std::round(outg * 255.0));
	blue = clamp(std::round(outb * 255.0));
}

void ColorRgb::yuv2rgb(uint8_t y, uint8_t u, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b)
{
	// see: http://en.wikipedia.org/wiki/YUV#Y.27UV444_to_RGB888_conversion
	int c = ColorRgb::clamp(y - 16);
	int d = u - 128;
	int e = v - 128;

	r = ColorRgb::clamp((298 * c + 409 * e + 128) >> 8);
	g = ColorRgb::clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
	b = ColorRgb::clamp((298 * c + 516 * d + 128) >> 8);
}

void ColorRgb::rgb2yuv(int r, int g, int b, uint8_t& y, uint8_t& u, uint8_t& v)
{
	y = ColorRgb::clamp(((66 * (r)+129 * (g)+25 * (b)+128) >> 8) + 16);
	u = ColorRgb::clamp(((-38 * (r)-74 * (g)+112 * (b)+128) >> 8) + 128);
	v = ColorRgb::clamp(((112 * (r)-94 * (g)-18 * (b)+128) >> 8) + 128);
}

