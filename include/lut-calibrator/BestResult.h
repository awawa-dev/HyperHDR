#pragma once

/* BestResult.h
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

#ifndef PCH_ENABLED
	#include <QJsonArray>
	#include <QJsonObject>
	#include <QFile>
	#include <QDateTime>
	#include <QThread>

	#include <cmath>
	#include <cfloat>
	#include <climits>	
#endif

#include <linalg.h>
#include <lut-calibrator/ColorSpace.h>
#include <lut-calibrator/BoardUtils.h>
#include <lut-calibrator/YuvConverter.h>

using namespace linalg;
using namespace aliases;
using namespace ColorSpaceMath;
using namespace BoardUtils;

struct LchLists
{
	std::list<double4> low;
	std::list<double4> mid;
	std::list<double4> high;
};

struct BestResult
{
	YuvConverter::YUV_COEFS coef = YuvConverter::YUV_COEFS::BT601;
	double4x4	coefMatrix;
	double2		coefDelta;
	int			coloredAspectMode = 0;
	std::pair<double3, double3> colorAspect;
	double3		aspect;
	int			bt2020Range = 0;
	int			altConvert = 0;
	double3x3	altPrimariesToSrgb;
	ColorSpaceMath::HDR_GAMMA gamma = ColorSpaceMath::HDR_GAMMA::PQ;
	double		gammaHLG = 0;
	double		nits = 0;
	bool		lchEnabled = false;
	LchLists	lchPrimaries;

	struct Signal
	{
		YuvConverter::COLOR_RANGE range = YuvConverter::COLOR_RANGE::FULL;
		double yRange = 0;
		double upYLimit = 0;
		double downYLimit = 0;
		double yShift = 0;
		byte3  yuvRange = {};
		bool isSourceP010 = false;
	} signal;

	long long int minError = MAX_CALIBRATION_ERROR;

	void serializePrimaries(std::stringstream& out) const
	{
		for (const auto& p : { lchPrimaries.low, lchPrimaries.mid, lchPrimaries.high })
		{
			out << std::endl << "\t\t\tstd::list<double4>{" << std::endl << "\t\t\t\t";
			for (const auto& v : p)
			{
				out << "double4";  ColorSpaceMath::serialize(out, v); out << ", ";
			}
			out << std::endl << "\t\t\t}," << std::endl;
		}
	}

	void serialize(std::stringstream& out) const
	{
		out.precision(12);
		out << "/*" << std::endl;
		out << "BestResult bestResult;" << std::endl;
		out << "bestResult.coef = YuvConverter::YUV_COEFS(" << std::to_string(coef) << ");" << std::endl;
		out << "bestResult.coefMatrix = double4x4"; ColorSpaceMath::serialize(out, coefMatrix); out << ";" << std::endl;
		out << "bestResult.coefDelta = double2"; ColorSpaceMath::serialize(out, coefDelta); out << ";" << std::endl;
		out << "bestResult.coloredAspectMode = " << std::to_string(coloredAspectMode) << ";" << std::endl;
		out << "bestResult.colorAspect = std::pair<double3, double3>(double3";  ColorSpaceMath::serialize(out, colorAspect.first); out << ", double3";  ColorSpaceMath::serialize(out, colorAspect.second);  out << ");" << std::endl;
		out << "bestResult.aspect = double3";  ColorSpaceMath::serialize(out, aspect); out << ";" << std::endl;
		out << "bestResult.bt2020Range = " << std::to_string(bt2020Range) << ";" << std::endl;
		out << "bestResult.altConvert = " << std::to_string(altConvert) << ";" << std::endl;
		out << "bestResult.altPrimariesToSrgb = double3x3"; ColorSpaceMath::serialize(out, altPrimariesToSrgb); out << ";" << std::endl;
		out << "bestResult.gamma = ColorSpaceMath::HDR_GAMMA(" << std::to_string(gamma) << ");" << std::endl;
		out << "bestResult.gammaHLG = " << std::to_string(gammaHLG) << ";" << std::endl;
		out << "bestResult.lchEnabled = " << std::to_string(lchEnabled) << ";" << std::endl;
		out << "bestResult.lchPrimaries = LchLists{"; serializePrimaries(out); out << "\t\t};" << std::endl;
		out << "bestResult.nits = " << std::to_string(nits) << ";" << std::endl;
		out << "bestResult.signal.range = YuvConverter::COLOR_RANGE(" << std::to_string(signal.range) << ");" << std::endl;
		out << "bestResult.signal.yRange = " << std::to_string(signal.yRange) << ";" << std::endl;
		out << "bestResult.signal.upYLimit = " << std::to_string(signal.upYLimit) << ";" << std::endl;
		out << "bestResult.signal.downYLimit = " << std::to_string(signal.downYLimit) << ";" << std::endl;
		out << "bestResult.signal.yShift = " << std::to_string(signal.yShift) << ";" << std::endl;
		out << "bestResult.signal.isSourceP010 = " << std::to_string(signal.isSourceP010) << ";" << std::endl;
		out << "bestResult.minError = " << std::to_string(std::round(minError * 100.0) / 30000.0) << ";" << std::endl;
		out << "bestResult.signal.yuvRange = byte3{ " << std::to_string(signal.yuvRange[0]) << ", " << std::to_string(signal.yuvRange[1]) << ", " << std::to_string(signal.yuvRange[2]) << "};" << std::endl;
		out << "*/" << std::endl;
	}
};
