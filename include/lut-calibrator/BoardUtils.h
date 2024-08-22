#pragma once

/* BoardUtils.h
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

#ifndef PCH_ENABLED
	#include <QString>
	#include <cmath>
	#include <cstring>
#endif

#include <linalg.h>
#include <lut-calibrator/ColorSpace.h>
#include <image/ColorRgb.h>
#include <image/Image.h>
#include <lut-calibrator/CapturedColor.h>
#include <utils/Logger.h>

namespace BoardUtils
{
	const int SCREEN_BLOCKS_X = 48;
	const int SCREEN_BLOCKS_Y = 30;
	const int SCREEN_COLOR_STEP = 16;
	const int SCREEN_COLOR_DIMENSION = (256 / SCREEN_COLOR_STEP) + 1;

	const int SCREEN_CRC_LINES = 2;
	const int SCREEN_CRC_COUNT = 5;
	const int SCREEN_SAMPLES_PER_BOARD = (SCREEN_BLOCKS_X / 2) * (SCREEN_BLOCKS_Y - SCREEN_CRC_LINES);

	int indexToColorAndPos(int index, ColorRgb& color, int2& position);
	CapturedColor readBlock(const Image<ColorRgb>& yuvImage, int2 position);
	void getWhiteBlackColorLevels(const Image<ColorRgb>& yuvImage, CapturedColor& white, CapturedColor& black, int& line);
	bool verifyBlackColorPattern(const Image<ColorRgb>& yuvImage, bool isFirstWhite, CapturedColor& black);
	bool parseBoard(Logger* _log, const Image<ColorRgb>& yuvImage, int& boardIndex);
	Image<ColorRgb> loadTestBoardAsYuv(const std::string& filename);
	void createTestBoards(const char* pattern = "D:/table_%1.png");
	bool verifyTestBoards(Logger* _log, const char* pattern = "D:/table_%1.png");
};
