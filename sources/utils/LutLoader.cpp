/* LutLoader.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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
#include <HyperhdrConfig.h>
#include <utils/LutLoader.h>
#include <image/ColorRgb.h>
#include <utils/FrameDecoder.h>
#include <utils/InternalClock.h>
#include <QFile>

#ifdef ENABLE_ZSTD
	#include <utils-zstd/utils-zstd.h>
#endif


namespace {
	const int LUT_FILE_SIZE = 256 * 256 * 256 *3;
	const int LUT_MEMORY_ALIGN = 64;
}

void LutLoader::loadLutFile(const LoggerName& _log, PixelFormat color, const QList<QString>& files)
{
	bool is_yuv = (color == PixelFormat::YUYV);

	_lutBufferInit = false;

	if (color != PixelFormat::RGB24 && color != PixelFormat::YUYV)
	{
		if (_log.size()) Error(_log, "Unsupported mode for loading LUT table: {:s}", (pixelFormatToString(color)));
		return;
	}

	if (_hdrToneMappingEnabled || is_yuv)
	{
		for (QString fileName3d : files)
		{
			QFile file(fileName3d);
			bool compressed = false;

			if (file.open(QIODevice::ReadOnly)
				#ifdef ENABLE_ZSTD
					|| [&]() {file.setFileName(fileName3d + ".zst"); compressed = true; return file.open(QIODevice::ReadOnly); } ()
				#endif
				)
			{
				int length;

				if (_log.size())
				{
					Debug(_log, "LUT file found: {:s} ({:s})", (file.fileName()), (compressed) ? "compressed" : "uncompressed");
				}

				length = file.size();

				if ((length == LUT_FILE_SIZE * 3) || compressed)
				{
					int index = 0;

					if (is_yuv && _hdrToneMappingEnabled)
					{
						if (_log.size()) Debug(_log, "Index 1 for HDR YUV");
						index = LUT_FILE_SIZE;
					}
					else if (is_yuv)
					{
						if (_log.size()) Debug(_log, "Index 2 for YUV");
						index = LUT_FILE_SIZE * 2;
					}
					else
					{
						if (_log.size()) Debug(_log, "Index 0 for HDR RGB");
					}

					_lut.resize(LUT_FILE_SIZE + LUT_MEMORY_ALIGN);

					if (!compressed)
					{
						file.seek(index);

						if (file.read((char*)_lut.data(), LUT_FILE_SIZE) != LUT_FILE_SIZE)
						{
							if (_log.size()) Error(_log, "Error reading LUT file {:s}", (fileName3d));
						}
						else
						{
							_lutBufferInit = true;
							if (_log.size()) Info(_log, "Found and loaded LUT: '{:s}'", (fileName3d));
						}
					}
					else
					{
						_lutBufferInit = decompressLut(_log, file, index);
						if (_log.size())
						{
							if (_lutBufferInit) Info(_log, "Found and loaded LUT: '{:s}'", (fileName3d));
							else Error(_log, "Error reading LUT file {:s}", (fileName3d));
						}
					}

					// hasher(index / LUT_FILE_SIZE, _log);
				}
				else
				{
					if (_log.size()) Error(_log, "LUT file has invalid length: {:d} vs {:d} => {:s}", length, (LUT_FILE_SIZE * 3), (fileName3d));
				}

				file.close();

				return;
			}
			else
			{
				if (_log.size()) Warning(_log, "LUT file is not found here: {:s}", (fileName3d));
			}
		}

		if (_log.size()) Error(_log, "Could not find any required LUT file");
	}
}

bool LutLoader::decompressLut(const LoggerName& _log, QFile& file, int index)
{
	auto now = InternalClock::nowPrecise();
	[[maybe_unused]] const char* retVal = "HyperHDR was built without a support for ZSTD decoder";
	QByteArray compressedFile = file.readAll();
	#ifdef ENABLE_ZSTD
		retVal = DecompressZSTD(compressedFile.size(), reinterpret_cast<uint8_t*>(compressedFile.data()), _lut.data(), index, LUT_FILE_SIZE);
	#endif

	if (retVal != nullptr && _log.size())
	{
		Error(_log, "Error while decompressing LUT: {:s}", retVal);
	}

	if (_log.size()) Info(_log, "Decompression took {:f} seconds", (InternalClock::nowPrecise() - now) / 1000.0);

	return retVal == nullptr;
}

void LutLoader::hasher(int index, const LoggerName& _log)
{
	if (_log.size())
	{
		auto start = _lut.data();
		auto end = start + _lut.size();
		uint8_t position = 0;
		uint16_t fletcher1 = 0;
		uint16_t fletcher2 = 0;
		uint16_t fletcherExt = 0;
		while (start < end)
		{
			fletcher1 = (fletcher1 + (uint16_t)(*(start))) % 255;
			fletcher2 = (fletcher2 + fletcher1) % 255;
			fletcherExt = (fletcherExt + (*(start++) ^ (position++))) % 255;
		}
		Info(_log, "CRC for {:d} segment: 0x{:02X} 0x{:02X} 0x{:02X}", index, fletcher1, fletcher2, fletcherExt);
	}
}
