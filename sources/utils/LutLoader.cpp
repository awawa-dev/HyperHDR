/* LutLoader.cpp
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

#include <utils/LutLoader.h>
#include <image/ColorRgb.h>
#include <utils/FrameDecoder.h>
#include <QFile>

namespace {
	const int LUT_FILE_SIZE = 256 * 256 * 256 *3;
	const int LUT_MEMORY_ALIGN = 64;
}

void LutLoader::loadLutFile(Logger* _log, PixelFormat color, const QList<QString>& files)
{
	bool is_yuv = (color == PixelFormat::YUYV);

	_lutBufferInit = false;

	if (color != PixelFormat::NO_CHANGE && color != PixelFormat::RGB24 && color != PixelFormat::YUYV)
	{
		Error(_log, "Unsupported mode for loading LUT table: %s", QSTRING_CSTR(pixelFormatToString(color)));
		return;
	}

	if (color == PixelFormat::NO_CHANGE)
	{
		_lut.resize(LUT_FILE_SIZE + LUT_MEMORY_ALIGN);

		if (_lut.data() != nullptr)
		{
			for (int y = 0; y < 256; y++)
				for (int u = 0; u < 256; u++)
					for (int v = 0; v < 256; v++)
					{
						uint32_t ind_lutd = LUT_INDEX(y, u, v);
						ColorRgb::yuv2rgb(y, u, v,
							_lut.data()[ind_lutd],
							_lut.data()[ind_lutd + 1],
							_lut.data()[ind_lutd + 2]);
					}
			_lutBufferInit = true;
		}

		Error(_log, "You have forgotten to put lut_lin_tables.3d file in the HyperHDR configuration folder. Internal LUT table for YUV conversion has been created instead.");
		return;
	}

	if (_hdrToneMappingEnabled || is_yuv)
	{
		for (QString fileName3d : files)
		{
			QFile file(fileName3d);

			if (file.open(QIODevice::ReadOnly))
			{
				int length;
				Debug(_log, "LUT file found: %s", QSTRING_CSTR(fileName3d));

				length = file.size();

				if ((length == LUT_FILE_SIZE * 3) || (length == LUT_FILE_SIZE && !is_yuv))
				{
					qint64 index = 0;

					if (is_yuv && _hdrToneMappingEnabled)
					{
						Debug(_log, "Index 1 for HDR YUV");
						index = LUT_FILE_SIZE;
					}
					else if (is_yuv)
					{
						Debug(_log, "Index 2 for YUV");
						index = LUT_FILE_SIZE * 2;
					}
					else
						Debug(_log, "Index 0 for HDR RGB");

					file.seek(index);

					_lut.resize(LUT_FILE_SIZE + LUT_MEMORY_ALIGN);

					if (file.read((char*)_lut.data(), LUT_FILE_SIZE) != LUT_FILE_SIZE)
					{
						Error(_log, "Error reading LUT file %s", QSTRING_CSTR(fileName3d));
					}
					else
					{
						_lutBufferInit = true;
						Info(_log, "Found and loaded LUT: '%s'", QSTRING_CSTR(fileName3d));
					}
				}
				else
					Error(_log, "LUT file has invalid length: %i vs %i => %s", length, (LUT_FILE_SIZE * 3), QSTRING_CSTR(fileName3d));

				file.close();

				return;
			}
			else
				Warning(_log, "LUT file is not found here: %s", QSTRING_CSTR(fileName3d));
		}

		Error(_log, "Could not find any required LUT file");
	}
}
