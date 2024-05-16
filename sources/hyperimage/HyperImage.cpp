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
