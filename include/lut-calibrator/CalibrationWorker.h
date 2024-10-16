#pragma once

/* CalibrationWorker.h
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

#include <QRunnable>
#include <linalg.h>
#include <lut-calibrator/BestResult.h>

using namespace linalg;
using namespace aliases;
using namespace ColorSpaceMath;
using namespace BoardUtils;

class CalibrationWorker : public QObject, public QRunnable
{
	Q_OBJECT

	BestResult bestResult;
	YuvConverter* yuvConverter;
	const int id;
	const int krIndexStart;
	const int krIndexEnd;
	const int halfKDelta;
	const bool precise;
	const int coef;
	const std::vector<std::pair<double3, byte2>>& sampleColors;
	const int gamma;
	const double gammaHLG;
	const double NITS;
	const double3x3& bt2020_to_sRgb;
	std::list<std::pair<CapturedColor*, double3>> vertex;
	std::atomic<long long int>& weakBestScore;
	const bool lchCorrection;
	std::atomic<bool>& forcedExit;
	std::atomic<int>& progress;
public:
	CalibrationWorker(BestResult* _bestResult, std::atomic<long long int>& _weakBestScore, YuvConverter* _yuvConverter, const int _id, const int _krIndexStart, const int _krIndexEnd, const int _halfKDelta, const bool _precise, const int _coef,
		const std::vector<std::pair<double3, byte2>>& _sampleColors, const int _gamma, const double _gammaHLG, const double _NITS, const double3x3& _bt2020_to_sRgb,
		const std::list<CapturedColor*>& _vertex, const bool _lchCorrection, std::atomic<bool>& _forcedExit, std::atomic<int>& _progress) :
		yuvConverter(_yuvConverter),
		id(_id),
		krIndexStart(_krIndexStart),
		krIndexEnd(_krIndexEnd),
		halfKDelta(_halfKDelta),
		precise(_precise),
		coef(_coef),
		sampleColors(_sampleColors),
		gamma(_gamma),
		gammaHLG(_gammaHLG),
		NITS(_NITS),
		bt2020_to_sRgb(_bt2020_to_sRgb),
		weakBestScore(_weakBestScore),
		lchCorrection(_lchCorrection),
		forcedExit(_forcedExit),
		progress(_progress)
	{
		bestResult = *_bestResult;
		this->setAutoDelete(false);
		for (auto& v : _vertex)
		{
			vertex.push_back(std::pair<CapturedColor*, double3>(v, double3{}));
		}
	};

	void run() override;
	void getBestResult(BestResult* otherScore) const
	{
		if (otherScore->minError > bestResult.minError)
		{
			(*otherScore) = bestResult;
		}
	};
signals:
	void notifyCalibrationMessage(QString message, bool started);
};
