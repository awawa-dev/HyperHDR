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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <cmath>

#include <turbojpeg.h>
#include <lunasvg.h>
#include <plutovg.h>
#include <stb_image_write.h>

#include <QFile>
#include <QByteArray>
#include <hyperimage/HyperImage.h>
#include <utility>	
#include <list>



ColorRgb HyperImage::ColorRgbfromString(QString colorName)
{
	std::list<std::pair<QString, ColorRgb>> colors = {
		{ "white", ColorRgb(255,255,255)},
		{ "red", ColorRgb(255,0,0)},
		{ "green",ColorRgb(0,255,0)},
		{ "blue", ColorRgb(0,0,255)},
		{ "yellow", ColorRgb(255,255,0)},
		{ "magenta", ColorRgb(255,0,255)},
		{ "cyan", ColorRgb(0,255,255)}
	};

	std::list<std::pair<QString, ColorRgb>>::iterator findIter = std::find_if(colors.begin(), colors.end(),
		[&colorName](const std::pair<QString, ColorRgb>& a)
		{
			if (a.first == colorName)
				return true;
			else
				return false;
		}
	);

	if (findIter != colors.end())
		return (*findIter).second;

	return ColorRgb(255, 255, 255);
}

void HyperImage::svg2png(QString filename, int width, int height, QBuffer& buffer)
{
	if (filename.indexOf(":/") == 0)
	{
		QFile stream(filename);
		if (!stream.open(QIODevice::ReadOnly))
			return;
		QByteArray ar = stream.readAll();
		stream.close();

		filename = QString(ar);
	}

	const std::uint32_t bgColor = 0x00000000;

	std::string svgFile = filename.toStdString();
	auto document = lunasvg::Document::loadFromData(svgFile);

	auto bitmap = document->renderToBitmap(width, height, bgColor);
	if (!bitmap.valid())
		return;
	bitmap.convertToRGBA();

	int len;
	unsigned char* png = stbi_write_png_to_mem(bitmap.data(), 0, bitmap.width(), bitmap.height(), 4, &len);
	if (png == NULL)
		return;

	buffer.write(reinterpret_cast<const char*>(png), len);
	
	STBIW_FREE(png);
}



HyperImage::HyperImage() : HyperImage(QSize(1,1))
{
}

HyperImage::HyperImage(QSize size)
	: _pen(ColorRgb::BLACK)
{
	_surface = plutovg_surface_create(size.width(), size.height());
	_pluto = plutovg_create(_surface);
}

HyperImage::~HyperImage()
{
	plutovg_surface_destroy(_surface);
	plutovg_destroy(_pluto);
}

int HyperImage::width() const
{	
	return plutovg_surface_get_width(_surface);
}

int HyperImage::height() const
{
	return plutovg_surface_get_height(_surface);
}

Image<ColorRgb> HyperImage::renderImage() const
{
	Image<ColorRgb> ret(this->width(), this->height());
	auto dest = ret.rawMem();
	auto data = plutovg_surface_get_data(_surface);

	for (int y = 0; y < this->height(); y++)
	{
		auto source = data;
		for (int x = 0; x < this->width(); x++)
		{
			dest[0] = source[2];
			dest[1] = source[1];
			dest[2] = source[0];
			dest += 3;
			source += 4;
		}
		data += plutovg_surface_get_stride(_surface);
	}
	return ret;
}

void HyperImage::resize(int sizeX, int sizeY)
{
	plutovg_surface_destroy(_surface);
	plutovg_destroy(_pluto);
	_surface = plutovg_surface_create(sizeX, sizeY);
	_pluto = plutovg_create(_surface);
}

void HyperImage::setPen(const ColorRgb& color)
{
	plutovg_set_rgb(_pluto, color.red / 255.0, color.green / 255.0, color.blue / 255.0);
	_pen = color;
}

void HyperImage::drawLine(int ax, int ay, int bx, int by)
{
	plutovg_save(_pluto);
	plutovg_set_line_width(_pluto, 1);
	plutovg_move_to(_pluto, bx, by);
	plutovg_line_to(_pluto, ax, ay);
	plutovg_stroke(_pluto);
	plutovg_restore(_pluto);
}

void HyperImage::drawPoint(int x, int y)
{
	if (x < width() && y < height())
	{
		auto data = plutovg_surface_get_data(_surface);
		int index = 4 * x + y * plutovg_surface_get_stride(_surface);
		data[index] = _pen.blue;
		data[index + 1] = _pen.green;
		data[index + 2] = _pen.red;
		data[index + 3] = 1.0;
	}
}

void HyperImage::fill(const ColorRgb& color)
{
	plutovg_save(_pluto);
	plutovg_set_rgb(_pluto, color.red / 255.0, color.green / 255.0, color.blue / 255.0);
	plutovg_set_line_width(_pluto, 1);
	plutovg_rect(_pluto, 0, 0, width(), height());
	plutovg_fill(_pluto);
	plutovg_restore(_pluto);
}

void HyperImage::fillRect(int ax, int ay, int bx, int by, const ColorRgb& color)
{
	plutovg_save(_pluto);
	plutovg_set_rgb(_pluto, color.red / 255.0, color.green / 255.0, color.blue / 255.0);
	plutovg_set_line_width(_pluto, 1);
	plutovg_rect(_pluto, 0, 0, width(), height());
	plutovg_fill(_pluto);
	plutovg_restore(_pluto);
}

void HyperImage::radialFill(int center_x, int center_y, double diag, const std::vector<uint8_t>& points)
{
	auto dest = plutovg_surface_get_data(_surface);
	auto h = this->height();
	auto w = this->width();

	if (points.size() < 5 || points.size() % 5)
		return;

	for (int y = 0; y < h; y++)
	{
		auto data = dest;
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
				data[2] = c.red;
				data[1] = c.green;
				data[0] = c.blue;
				data[3] = 255;
			}
			else
			{
				float asp1 = lastAlfa / 255.0;
				float asp2 = 1 - asp1;
				data[2] = ColorRgb::clamp(c.red * asp1 + data[2] * asp2);
				data[1] = ColorRgb::clamp(c.green * asp1 + data[1] * asp2);
				data[0] = ColorRgb::clamp(c.blue * asp1 + data[0] * asp2);
			}

			data += 4;
		}
		dest += plutovg_surface_get_stride(_surface);
	}
}

void HyperImage::conicalFill(double angle, const std::vector<uint8_t>& points, bool reset)
{
	auto dest = plutovg_surface_get_data(_surface);
	auto h = this->height();
	auto w = this->width();
	float pi = std::atan(1) * 4;

	if (reset)
		memset(dest, 0, static_cast<size_t>(h) * plutovg_surface_get_stride(_surface));

	if (points.size() < 5 || points.size() % 5)
		return;

	for (int y = 0; y < h; y++)
	{
		auto data = dest;
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
				data[2] = c.red;
				data[1] = c.green;
				data[0] = c.blue;
				data[3] = 255;
			}
			else 
			{
				float asp1 = lastAlfa / 255.0;
				float asp2 = 1 - asp1;
				data[2] = ColorRgb::clamp(c.red * asp1 + data[2] * asp2);
				data[1] = ColorRgb::clamp(c.green * asp1 + data[1] * asp2);
				data[0] = ColorRgb::clamp(c.blue * asp1 + data[0] * asp2);
			}

			data += 4;
		}
		dest += plutovg_surface_get_stride(_surface);
	}
}

void HyperImage::encodeJpeg(MemoryBuffer<uint8_t>& buffer, Image<ColorRgb>& inputImage, bool scaleDown)
{
	const int aspect = (scaleDown) ? 2 : 1;
	const int width = inputImage.width();
	const int height = inputImage.height() / aspect;
	int pitch = width * sizeof(ColorRgb) * aspect;
	int subSample = (scaleDown) ? TJSAMP_422 : TJSAMP_444;
	int quality = 75;

	unsigned long compressedImageSize = 0;
	unsigned char* compressedImage = NULL;

	tjhandle _jpegCompressor = tjInitCompress();

	tjCompress2(_jpegCompressor, inputImage.rawMem(), width, pitch, height, TJPF_RGB,
		&compressedImage, &compressedImageSize, subSample, quality, TJFLAG_FASTDCT);

	buffer.resize(compressedImageSize);
	memcpy(buffer.data(), compressedImage, compressedImageSize);

	tjDestroy(_jpegCompressor);
	tjFree(compressedImage);
}
