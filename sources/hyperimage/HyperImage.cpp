/* HyperImage.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <cmath>
#include <cstring>

#include <hyperimage/HyperImage.h>
#include <utility>	
#include <list>


HyperImage::HyperImage() : HyperImage(1,1)
{
}

HyperImage::HyperImage(int width, int height)
	: _pen(ColorRgb::BLACK)
{
	_surface = Image<ColorRgb>(width, height);
}

int HyperImage::width() const
{	
	return _surface.width();
}

int HyperImage::height() const
{
	return _surface.height();
}

Image<ColorRgb> HyperImage::renderImage() const
{
	Image<ColorRgb> ret(this->width(), this->height());
	memcpy(ret.rawMem(), _surface.rawMem(), 3ll * width() * height() );
	return ret;
}

void HyperImage::resize(int sizeX, int sizeY)
{
	_surface.resize(sizeX, sizeY);
}

void HyperImage::setPen(const ColorRgb& color)
{
	_pen = color;
}

void HyperImage::drawVerticalLine(int ax, int ay, int by)
{
	_surface.fastBox(ax, ay, ax, by, _pen.red, _pen.green, _pen.blue);
}

void HyperImage::drawHorizontalLine(int ax, int bx, int by)
{
	_surface.fastBox(ax, by, bx, by, _pen.red, _pen.green, _pen.blue);
}

void HyperImage::drawPoint(int x, int y)
{	
	if (x < width() && y < height())
	{
		_surface(x, y) = _pen;
	}
}

void HyperImage::fill(const ColorRgb& color)
{
	_surface.fastBox(0, 0, width() - 1, height() - 1, color.red, color.green, color.blue);
}

void HyperImage::fillRect(int ax, int ay, int bx, int by, const ColorRgb& color)
{
	_surface.fastBox(ax, ay, bx, by, color.red, color.green, color.blue);
}

void HyperImage::radialFill(int center_x, int center_y, double diag, const std::vector<uint8_t>& points)
{
	auto data = _surface.rawMem();
	auto h = this->height();
	auto w = this->width();

	if (points.size() < 5 || points.size() % 5)
		return;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			ColorRgb c;
			float dx = (center_x - x);
			float dy = (center_y - y);
			float len = dx * dx + dy * dy;
			int currentAngle = 0;

			if (len > 0.0)
			{
				currentAngle = ColorRgb::clamp(std::round(std::sqrt(len) * 255.0 / diag ));
			}


			int lastAngle = 0, lastAlfa = 0;
			ColorRgb lastColor;

			for (size_t index = 0; index < points.size(); index += 5)
			{
				int nextAngle = points[index];
				ColorRgb nextColor = ColorRgb(points[index + 1], points[index + 2], points[index + 3]);
				int nextAlfa = points[index + 4];

				if (index > 0)
				{
					if (lastAngle <= currentAngle && currentAngle <= nextAngle)
					{
						float distance = nextAngle - lastAngle;
						float aspect2 = (currentAngle - lastAngle) / distance;
						float aspect1 = 1.0 - aspect2;

						if (nextAlfa > 0 && lastAlfa > 0)
						{
							c.red = ColorRgb::clamp(lastColor.red * aspect1 + nextColor.red * aspect2);
							c.green = ColorRgb::clamp(lastColor.green * aspect1 + nextColor.green * aspect2);
							c.blue = ColorRgb::clamp(lastColor.blue * aspect1 + nextColor.blue * aspect2);
							if (nextAlfa != lastAlfa)
								lastAlfa = ColorRgb::clamp(std::round(nextAlfa * aspect2 + lastAlfa * aspect1));
						}
						else if (nextAlfa > 0)
						{
							c.red = ColorRgb::clamp(nextColor.red * aspect2 * nextAlfa / 255.0);
							c.green = ColorRgb::clamp(nextColor.green * aspect2 * nextAlfa / 255.0);
							c.blue = ColorRgb::clamp(nextColor.blue * aspect2 * nextAlfa / 255.0);
							lastAlfa = ColorRgb::clamp(std::round(nextAlfa * aspect2));
						}
						else if (lastAlfa > 0)
						{
							c.red = ColorRgb::clamp(lastColor.red * aspect1 * lastAlfa / 255.0);
							c.green = ColorRgb::clamp(lastColor.green * aspect1 * lastAlfa / 255.0);
							c.blue = ColorRgb::clamp(lastColor.blue * aspect1 * lastAlfa / 255.0);
							lastAlfa = ColorRgb::clamp(std::round(lastAlfa * aspect1));
						}


						break;
					}
				}


				lastAngle = nextAngle;
				lastColor = nextColor;
				lastAlfa = nextAlfa;
			}

			if (lastAlfa == 255)
			{
				data[0] = c.red;
				data[1] = c.green;
				data[2] = c.blue;
			}
			else
			{
				float asp1 = lastAlfa / 255.0;
				float asp2 = 1 - asp1;
				data[0] = ColorRgb::clamp(c.red * asp1 + data[0] * asp2);
				data[1] = ColorRgb::clamp(c.green * asp1 + data[1] * asp2);
				data[2] = ColorRgb::clamp(c.blue * asp1 + data[2] * asp2);
			}

			data += 3;
		}
	}
}

void HyperImage::conicalFill(double angle, const std::vector<uint8_t>& points, bool reset)
{
	auto data = _surface.rawMem();
	auto h = this->height();
	auto w = this->width();
	float pi = std::atan(1) * 4;

	if (reset)
		memset(data, 0, 3ll * w * h);

	if (points.size() < 5 || points.size() % 5)
		return;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			ColorRgb c;			

			float dx = (x - w/2);
			float dy = (h/2 - y);
			float len = dx * dx + dy * dy;
			float deltaAngle = angle;

			if (len > 0.0)
			{				
				double newAngle = atan2(1.0, 0.0) - atan2(dy, dx);
				newAngle = newAngle * 360.0 / (2 * pi);
				deltaAngle += newAngle;
			}


			while (deltaAngle > 360.0)
				deltaAngle -= 360.0;
			while (deltaAngle < 0.0)
				deltaAngle += 360.0;

			int currentAngle = ColorRgb::clamp(std::round(deltaAngle * 255 / 360.0));


			int lastAngle = 0, lastAlfa = 0;
			ColorRgb lastColor;

			for (size_t index = 0; index < points.size(); index += 5)
			{
				int nextAngle = points[index];
				ColorRgb nextColor = ColorRgb(points[index + 1], points[index + 2], points[index + 3]);
				int nextAlfa = points[index + 4];

				if (index > 0)
				{
					if (lastAngle <= currentAngle && currentAngle <= nextAngle)
					{
						float distance = nextAngle - lastAngle;
						float aspect2 = (currentAngle - lastAngle) / distance;
						float aspect1 = 1.0 - aspect2;

						if (nextAlfa > 0 && lastAlfa > 0)
						{
							c.red = ColorRgb::clamp(lastColor.red * aspect1 + nextColor.red * aspect2);
							c.green = ColorRgb::clamp(lastColor.green * aspect1 + nextColor.green * aspect2);
							c.blue = ColorRgb::clamp(lastColor.blue * aspect1 + nextColor.blue * aspect2);
							if (nextAlfa != lastAlfa)
								lastAlfa = ColorRgb::clamp(std::round(nextAlfa * aspect2 + lastAlfa * aspect1));
						}
						else if (nextAlfa > 0)
						{
							c.red = ColorRgb::clamp(nextColor.red * aspect2 * nextAlfa / 255.0);
							c.green = ColorRgb::clamp(nextColor.green * aspect2 * nextAlfa / 255.0);
							c.blue = ColorRgb::clamp(nextColor.blue * aspect2 * nextAlfa / 255.0);
							lastAlfa = ColorRgb::clamp(std::round(nextAlfa * aspect2));
						}
						else if (lastAlfa > 0)
						{
							c.red = ColorRgb::clamp(lastColor.red * aspect1 * lastAlfa / 255.0);
							c.green = ColorRgb::clamp(lastColor.green * aspect1 * lastAlfa / 255.0);
							c.blue = ColorRgb::clamp(lastColor.blue * aspect1 * lastAlfa / 255.0);
							lastAlfa = ColorRgb::clamp(std::round(lastAlfa * aspect1));
						}


						break;
					}
				}
				

				lastAngle = nextAngle;
				lastColor = nextColor;
				lastAlfa = nextAlfa;
			}

			if (lastAlfa == 255)
			{
				data[0] = c.red;
				data[1] = c.green;
				data[2] = c.blue;
			}
			else 
			{
				float asp1 = lastAlfa / 255.0;
				float asp2 = 1 - asp1;
				data[0] = ColorRgb::clamp(c.red * asp1 + data[0] * asp2);
				data[1] = ColorRgb::clamp(c.green * asp1 + data[1] * asp2);
				data[2] = ColorRgb::clamp(c.blue * asp1 + data[2] * asp2);
			}

			data += 3;
		}
	}
}
