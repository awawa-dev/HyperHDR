/* YuvConverter.cpp
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

#include <lut-calibrator/YuvConverter.h>
#include <mutex>

namespace {
	std::mutex yuvConverterMutex;
}

double3 YuvConverter::toRgb(COLOR_RANGE range, YUV_COEFS coef, const double3& input) const
{
	double4 ret(input, 1);
	ret = mul(yuv2rgb.at(range).at(coef), ret);
	return double3(ret.x, ret.y, ret.z);
}

double3 YuvConverter::toYuvBT709(COLOR_RANGE range, const double3& input) const
{
	double4 ret(input, 1);
	ret = mul(rgb2yuvBT709.at(range), ret);
	return double3(ret.x, ret.y, ret.z);
}

QString YuvConverter::coefToString(YUV_COEFS cf)
{
	switch (cf)
	{
		case(FCC):		return "FCC";		break;
		case(BT601):	return "BT601";		break;
		case(BT709):	return "BT709";		break;
		case(BT2020):	return "BT2020";	break;
		default:		return "?";
	}
}

YuvConverter::YuvConverter()
{
	for (const auto& coeff : knownCoeffs)
		for (const COLOR_RANGE& range : { COLOR_RANGE::FULL,  COLOR_RANGE::LIMITED })
		{
			const double Kr = coeff.second.x;
			const double Kb = coeff.second.y;
			const double Kg = 1.0 - Kr - Kb;
			const double Cr = 0.5 / (1.0 - Kb);
			const double Cb = 0.5 / (1.0 - Kr);

			double scaleY = 1.0, addY = 0.0, scaleUV = 1.0, addUV = 128 / 255.0;

			if (range == COLOR_RANGE::LIMITED)
			{
				scaleY = 219 / 255.0;
				addY = 16 / 255.0;
				scaleUV = 224 / 255.0;
			}

			double4 c1(Kr * scaleY, -Kr * Cr * scaleUV, (1 - Kr) * Cb * scaleUV, 0);
			double4 c2(Kg * scaleY, -Kg * Cr * scaleUV, -Kg * Cb * scaleUV, 0);
			double4 c3(Kb * scaleY, (1 - Kb) * Cr * scaleUV, -Kb * Cb * scaleUV, 0);
			double4 c4(addY, addUV, addUV, 1);

			double4x4 rgb2yuvMatrix(c1, c2, c3, c4);

			double4x4 yuv2rgbMatrix = inverse(rgb2yuvMatrix);

			yuv2rgb[range][coeff.first] = yuv2rgbMatrix;

			if (coeff.first == YUV_COEFS::BT709)
			{
				rgb2yuvBT709[range] = rgb2yuvMatrix;
			}
		}
}

QString YuvConverter::toString()
{
	QStringList ret, report;

	for (const auto& coeff : knownCoeffs)
		for (const COLOR_RANGE& range : { COLOR_RANGE::FULL,  COLOR_RANGE::LIMITED })
		{
			double4x4 matrix = yuv2rgb[range][coeff.first];
			ret.append(QString("YUV to RGB %1 (%2):").arg(coefToString(coeff.first), 6).arg((range == COLOR_RANGE::LIMITED) ? "Limited" : "Full"));
			ret.append(ColorSpaceMath::matToString(matrix).split("\r\n"));
		}

	for (const COLOR_RANGE& range : { COLOR_RANGE::FULL,  COLOR_RANGE::LIMITED })
	{
		ret.append(QString("RGB to YUV %1 (%2):").arg(coefToString(YUV_COEFS::BT709), 6).arg((range == COLOR_RANGE::LIMITED) ? "Limited" : "Full"));
		ret.append(ColorSpaceMath::matToString(rgb2yuvBT709[range]).split("\r\n"));
	}

	for (int i = 0; i + 5 < ret.size(); i += 5)
		for (int j = 0; j < 5; j++)
			report.append(QString("%1 %2").arg(ret[i], -32).arg(ret[(i++) + 5], -32));

	return "Supported YUV/RGB matrix transformation:\r\n\r\n" + report.join("\r\n");
}