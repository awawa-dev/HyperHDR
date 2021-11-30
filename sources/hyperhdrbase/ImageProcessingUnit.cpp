/* ImageProcessingUnit.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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

#include <utils/Image.h>
#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/ImageProcessingUnit.h>
#include <hyperhdrbase/ImageProcessor.h>
#include <hyperhdrbase/ImageToLedsMap.h>

// Blacborder includes
#include <blackborder/BlackBorderProcessor.h>
#include <QDateTime>

using namespace hyperhdr;


ImageProcessingUnit::ImageProcessingUnit(HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr)
{
	_hyperhdr = hyperhdr;
	connect(this, &ImageProcessingUnit::processImageSignal, this, &ImageProcessingUnit::processImage, Qt::ConnectionType::QueuedConnection);
	connect(this, &ImageProcessingUnit::clearQueueImageSignal, this, &ImageProcessingUnit::clearQueueImage, Qt::ConnectionType::QueuedConnection);
	connect(this, &ImageProcessingUnit::queueImageSignal, this, &ImageProcessingUnit::queueImage, Qt::ConnectionType::QueuedConnection);
}

ImageProcessingUnit::~ImageProcessingUnit()
{
	_buffer.clear();
}

void ImageProcessingUnit::queueImage(const Image<ColorRgb>& image)
{
	while (_buffer.length() >= 2)
		_buffer.dequeue();

	_buffer.enqueue(image);

	emit processImageSignal();
}

void ImageProcessingUnit::clearQueueImage()
{
	_buffer.clear();
}


void ImageProcessingUnit::processImage()
{
	if (_buffer.length() < 1)
		return;

	const Image<ColorRgb>& image = _buffer.dequeue();

	ImageProcessor* imageProcessor = _hyperhdr->getImageProcessor();

	if (imageProcessor == nullptr)
		return;

	std::shared_ptr<hyperhdr::ImageToLedsMap> image2leds = imageProcessor->_imageToLedColors;

	if (image2leds == nullptr || image2leds->width() != image.width() || image2leds->height() != image.height())
		return;

	std::vector<ColorRgb> colors = image2leds->Process(image, imageProcessor->advanced);

	emit dataReadySignal(colors);
	emit _hyperhdr->currentImage(image);
}
