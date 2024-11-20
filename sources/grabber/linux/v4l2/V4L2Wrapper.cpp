/* V4L2Wrapper.cpp
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

#include <QMetaType>
#include <grabber/linux/v4l2/V4L2Wrapper.h>
#include <base/HyperHdrManager.h>
#include <utils/GlobalSignals.h>

V4L2Wrapper::V4L2Wrapper(const QString& device,
	const QString& configurationPath)
	: GrabberWrapper("V4L2:" + device.left(14))
{
	_grabber = std::unique_ptr<Grabber>(new V4L2Grabber(device, configurationPath));
    connect(_grabber.get(), &Grabber::SignalCapturingException, this, &GrabberWrapper::capturingExceptionHandler);
	connect(_grabber.get(), &Grabber::SignalSetNewComponentStateToAllInstances, this, &GrabberWrapper::SignalSetNewComponentStateToAllInstances);
	connect(_grabber.get(), &Grabber::SignalSaveCalibration, this, &GrabberWrapper::SignalSaveCalibration);
}
