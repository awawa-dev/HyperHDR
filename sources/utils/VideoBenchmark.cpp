/* VideoBenchmark.cpp
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

#include <utils/VideoBenchmark.h>
#include <utils/GlobalSignals.h>

VideoBenchmark::VideoBenchmark(QObject *parent): QObject(parent),
	_benchmarkStatus(-1),
	_benchmarkMessage(""),
	_connected(false)
{}

void VideoBenchmark::signalSetGlobalImageHandler(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin, QString clientDescription)
{
	newFrame(image);
}


void VideoBenchmark::signalNewVideoImageHandler(const QString& name, const Image<ColorRgb>& image)
{
	newFrame(image);
}

void VideoBenchmark::benchmarkCapture(int status, QString message)
{
	if (message == "ping")
	{
		if (!_connected)
		{
			_connected = true;
			connect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &VideoBenchmark::signalNewVideoImageHandler, Qt::ConnectionType::UniqueConnection);
			connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &VideoBenchmark::signalSetGlobalImageHandler, Qt::ConnectionType::UniqueConnection);
		}
		emit SignalBenchmarkUpdate(status, "pong");
	}
	else
	{
		_benchmarkStatus = status;
		_benchmarkMessage = message;

		if (_benchmarkMessage == "stop" || _benchmarkStatus < 0)
		{
			_connected = false;
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalNewVideoImage, this, &VideoBenchmark::signalNewVideoImageHandler);
			disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetGlobalImage, this, &VideoBenchmark::signalSetGlobalImageHandler);

		}
	}
}

void VideoBenchmark::newFrame(const Image<ColorRgb>& image)
{
	if (_benchmarkStatus >= 0)
	{
		ColorRgb pixel = image(image.width() / 2, image.height() / 2);
		if ((_benchmarkMessage == "white" && pixel.red > 120 && pixel.green > 120 && pixel.blue > 120) ||
			(_benchmarkMessage == "red" && pixel.red > 120 && pixel.green < 30 && pixel.blue < 30) ||
			(_benchmarkMessage == "green" && pixel.red < 30 && pixel.green > 120 && pixel.blue < 30) ||
			(_benchmarkMessage == "blue" && pixel.red < 30 && pixel.green < 40 && pixel.blue > 120) ||
			(_benchmarkMessage == "black" && pixel.red < 30 && pixel.green < 30 && pixel.blue < 30))

		{
			emit SignalBenchmarkUpdate(_benchmarkStatus, _benchmarkMessage);
			_benchmarkStatus = -1;
			_benchmarkMessage = "";
		}
	}
}
