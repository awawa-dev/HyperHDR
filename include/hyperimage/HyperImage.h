#pragma once

#ifndef PCH_ENABLED
	#include <vector>
	#include <cstdint>
#endif

#include <image/ColorRgb.h>
#include <image/Image.h>

class HyperImage
{
public:
	HyperImage();
	HyperImage(int width, int height);

	HyperImage(const HyperImage& t) = delete;
	HyperImage& operator=(const HyperImage& x) = delete;

	int width() const;
	int height() const;

	Image<ColorRgb> renderImage() const;

	void resize(int sizeX, int sizeY);
	void setPen(const ColorRgb& color);
	void drawVerticalLine(int ax, int ay, int by);
	void drawHorizontalLine(int ax, int bx, int by);
	void drawPoint(int x, int y);
	void fill(const ColorRgb& color);
	void fillRect(int ax, int ay, int bx, int by, const ColorRgb& color);
	void conicalFill(double angle, const std::vector<uint8_t>& points, bool reset = false);
	void radialFill(int center_x, int center_y, double diag, const std::vector<uint8_t>& points);

private:
	Image<ColorRgb>	_surface;
	ColorRgb		_pen;
};
