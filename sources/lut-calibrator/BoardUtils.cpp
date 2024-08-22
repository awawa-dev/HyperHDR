/* BoardUtils.cpp
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

#include <lut-calibrator/BoardUtils.h>
#include <lut-calibrator/YuvConverter.h>
#include <utils-image/utils-image.h>

// YUV444
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,colorspace=space=bt709:primaries=bt709:range=pc:trc=iec61966-2-1:ispace=bt709:iprimaries=bt709:irange=pc:itrc=iec61966-2-1:format=yuv444p10:fast=0:dither=none -c:v libx265 -pix_fmt yuv444p10le -profile:v main444-10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:lossless=1:colorprim=bt709:transfer=iec61966-2-1:colormatrix=bt709:range=full -f mp4 test_SDR_yuv444_high_quality.mp4
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,zscale=m=bt2020nc:p=bt2020:t=smpte2084:r=full:min=709:pin=709:tin=iec61966-2-1:rin=full:c=topleft,format=yuv444p10 -c:v libx265 -vtag hvc1 -pix_fmt yuv444p10le -profile:v main444-10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:hdr10=1:lossless=1:colorprim=bt2020:transfer=bt2020-10:colormatrix=bt2020nc:range=full -f mp4 test_HDR_yuv444_high_quality.mp4
// YUV420
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,colorspace=space=bt709:primaries=bt709:range=pc:trc=iec61966-2-1:ispace=bt709:iprimaries=bt709:irange=pc:itrc=iec61966-2-1:format=yuv444p12:fast=0:dither=none -c:v libx265 -pix_fmt yuv420p10le -profile:v main10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:lossless=1:colorprim=bt709:transfer=iec61966-2-1:colormatrix=bt709:range=full -f mp4 test_SDR_yuv420_low_quality.mp4
// ffmpeg -loop 1 -t 90 -framerate 1/3 -i table_%d.png -stream_loop -1 -i audio.ogg -shortest -map 0:v:0 -map 1:a:0 -vf fps=25,zscale=m=bt2020nc:p=bt2020:t=smpte2084:r=full:min=709:pin=709:tin=iec61966-2-1:rin=full:c=topleft,format=yuv444p12 -c:v libx265 -vtag hvc1 -pix_fmt yuv420p10le -profile:v main10 -preset veryslow -x265-params keyint=75:min-keyint=75:bframes=0:scenecut=0:psy-rd=0:psy-rdoq=0:rdoq=0:sao=false:cutree=false:deblock=false:strong-intra-smoothing=0:hdr10=1:lossless=1:colorprim=bt2020:transfer=bt2020-10:colormatrix=bt2020nc:range=full -f mp4 test_HDR_yuv420_low_quality.mp4

using namespace linalg;
using namespace aliases;

namespace BoardUtils
{

	int indexToColorAndPos(int index, ColorRgb& color, int2& position)
	{
		const int crcLines = 2;
		const int totalBoard = (SCREEN_BLOCKS_X / 2) * (SCREEN_BLOCKS_Y - crcLines);
		int currentIndex = index % totalBoard;
		int boardIndex = index / totalBoard;

		position = int2(0, 1);		
		position.y += currentIndex / (SCREEN_BLOCKS_X / 2);
		position.x += (currentIndex % (SCREEN_BLOCKS_X / 2)) * 2 + ((position.y  + boardIndex  )% 2);

		int B = (index % SCREEN_COLOR_DIMENSION) * SCREEN_COLOR_STEP;
		int G = ((index / (SCREEN_COLOR_DIMENSION)) % SCREEN_COLOR_DIMENSION) * SCREEN_COLOR_STEP;
		int R = (index / (SCREEN_COLOR_DIMENSION * SCREEN_COLOR_DIMENSION)) * SCREEN_COLOR_STEP;

		color.red = std::min(R, 255);
		color.green = std::min(G, 255);
		color.blue = std::min(B, 255);

		return boardIndex;
	}

	CapturedColor readBlock(const Image<ColorRgb>& yuvImage, int2 position)
	{
		const int2 delta(yuvImage.width() / SCREEN_BLOCKS_X, yuvImage.height() / SCREEN_BLOCKS_Y);
		CapturedColor color;

		const int2 start = position * delta;
		const int2 end = ((position + int2(1, 1)) * delta) - int2(1, 1);
		const int2 middle = (start + end) / 2;

		for (int x = -1; x <= 1; x++)
			for (int y = -1; y <= 1; y++)
			{
				auto pos = middle + int2(x, y);
				color.addColor(yuvImage(pos.x, pos.y));
			}
		if (!color.calculateFinalColor())
			throw new std::exception("Too much noice while reading the color");
		return color;
	}

	void getWhiteBlackColorLevels(const Image<ColorRgb>& yuvImage, CapturedColor& white, CapturedColor& black, int& line)
	{
		auto top = readBlock(yuvImage, int2(0, 0));
		auto bottom = readBlock(yuvImage, int2(0, SCREEN_BLOCKS_Y - 1));
		if (top.y() > bottom.y())
		{
			white = top;
			black = bottom;
			line = 0;
		}
		else
		{
			white = bottom;
			black = top;
			line = SCREEN_BLOCKS_Y - 1;
		}
	}

	bool verifyBlackColorPattern(const Image<ColorRgb>& yuvImage, bool isFirstWhite, CapturedColor& black)
	{
		try
		{
			for (int x = 0; x < SCREEN_BLOCKS_X; x++)
				for (int y = 0; y < SCREEN_BLOCKS_Y; y++)
					if ((x + isFirstWhite + y) % 2)
					{
						auto test = readBlock(yuvImage, int2(x, y));
						if (test.Y() > black.Y())
							return false;
					}
		}
		catch (std::exception& ex)
		{
			// too much noice or too low resolution
			return false;
		}

		return true;
	}

	bool parseBoard(const Image<ColorRgb>& yuvImage)
	{
		int line;
		CapturedColor white, black;

		try
		{
			getWhiteBlackColorLevels(yuvImage, white, black, line);
		}
		catch (std::exception& ex)
		{
			// too much noice or too low resolution
			return false;
		}

		if (!verifyBlackColorPattern(yuvImage, (line == 0), black))
		{
			// black pattern failed, too much noice or too low resolution
			return false;
		}

		return true;
	}

	Image<ColorRgb> loadTestBoardAsYuv(const std::string& filename)
	{
		YuvConverter yuvConverter;
		auto image = utils_image::load2image(filename);
		for (int x = 0; x < image.width(); x++)
			for (int y = 0; y < image.height(); y++)
			{
				const ColorRgb& inputRgb = image(x, y);
				const double3 scaledRgb = double3(inputRgb.red, inputRgb.green, inputRgb.blue) / 255.0;
				const double3 outputYuv = yuvConverter.toYuvBT709(YuvConverter::COLOR_RANGE::FULL, scaledRgb) * 255.0;
				const ColorRgb outputRgb = ColorRgb(ColorRgb::round(outputYuv.x), ColorRgb::round(outputYuv.y), ColorRgb::round(outputYuv.z));
				image(x, y) = outputRgb;
			}
		return image;
	}

	void createTestBoards(const char* pattern)
	{
		constexpr int2 margin(3, 2);
		int maxIndex = std::pow(SCREEN_COLOR_DIMENSION, 3);
		int boardIndex = 0;
		Image<ColorRgb> image(1920, 1080);
		const int2 delta(image.width() / SCREEN_BLOCKS_X, image.height() / SCREEN_BLOCKS_Y);

		auto saveImage = [](Image<ColorRgb>& image, const int2& delta, int boardIndex, const char* pattern)
			{
				for (int line = 0; line < SCREEN_BLOCKS_Y; line += SCREEN_BLOCKS_Y - 1)
				{
					int2 position = int2((line + boardIndex) % 2, line);

					for (int x = 0; x < boardIndex + 5; x++, position.x += 2)
					{
						const int2 start = position * delta;
						const int2 end = ((position + int2(1, 1)) * delta) - int2(1, 1);
						image.fastBox(start.x + margin.x, start.y + margin.y, end.x - margin.x, end.y - margin.y, 255, 255, 255);
					}
				}
				utils_image::savePng(QString(pattern).arg(QString::number(boardIndex)).toStdString(), image);
			};


		image.clear();

		if (256 % SCREEN_COLOR_STEP > 0 || image.width() % SCREEN_BLOCKS_X || image.height() % SCREEN_BLOCKS_Y)
			return;

		for (int index = 0; index < maxIndex; index++)
		{
			ColorRgb color;
			int2 position;
			int currentBoard = indexToColorAndPos(index, color, position);

			if (currentBoard < 0)
				return;

			if (boardIndex != currentBoard)
			{
				saveImage(image, delta, boardIndex, pattern);
				image.clear();
				boardIndex = currentBoard;
			}

			const int2 start = position * delta;
			const int2 end = ((position + int2(1, 1)) * delta) - int2(1, 1);

			image.fastBox(start.x + margin.x, start.y + margin.y, end.x - margin.x, end.y - margin.y, color.red, color.green, color.blue);
		}
		saveImage(image, delta, boardIndex, pattern);
	}
}
