/* ImageToLedManager.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
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

#include <base/HyperHdrInstance.h>
#include <base/ImageToLedManager.h>
#include <base/ImageColorAveraging.h>
#include <blackborder/BlackBorderProcessor.h>
#include <image/GaussianBlur.h>

using namespace hyperhdr;
using namespace linalg::aliases;

void ImageToLedManager::registerProcessingUnit(
	const unsigned width,
	const unsigned height,
	const unsigned horizontalBorder,
	const unsigned verticalBorder)
{
	if (width > 0 && height > 0)
		_colorAveraging = std::make_unique<ImageColorAveraging>(
			_log,
			_mappingType,
			_sparseProcessing,
			width,
			height,
			horizontalBorder,
			verticalBorder,
			_instanceIndex,
			_ledString.leds());
	else
		_colorAveraging = nullptr;
}


// global transform method
int ImageToLedManager::mappingTypeToInt(const QString& mappingType)
{
	if (mappingType == "unicolor_mean")
		return 1;

	return 0;
}
// global transform method
QString ImageToLedManager::mappingTypeToStr(int mappingType)
{

	if (mappingType == 1)
		return "unicolor_mean";

	return "advanced";
}

ImageToLedManager::ImageToLedManager(const LedString& ledString, HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr)
	, _instanceIndex(hyperhdr->getInstanceIndex())
	, _log(QString("IMAGETOLED_MNG%1").arg(_instanceIndex))
	, _ledString(ledString)
	, _borderProcessor(new BlackBorderProcessor(hyperhdr, this))
	, _colorAveraging(nullptr)
	, _mappingType(0)
	, _sparseProcessing(false)
{
	// init
	handleSettingsUpdate(settings::type::COLOR, hyperhdr->getSetting(settings::type::COLOR));
	// listen for changes in color - ledmapping
	connect(hyperhdr, &HyperHdrInstance::SignalInstanceSettingsChanged, this, &ImageToLedManager::handleSettingsUpdate);
	connect(this, &ImageToLedManager::SignalImageToLedsMappingChanged, hyperhdr, &HyperHdrInstance::SignalImageToLedsMappingChanged);

	Debug(_log, "ImageToLedManager initialized");
}

void ImageToLedManager::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
    if (type == settings::type::COLOR)
    {
        const QJsonObject& obj = config.object();
        int newType = mappingTypeToInt(obj["imageToLedMappingType"].toString());
        setLedMappingType(newType);

        bool newSparse = obj["sparse_processing"].toBool(false);
        setSparseProcessing(newSparse);

		std::vector<LedBrightnessRange> ranges;
		const QJsonArray rangesArray = obj["ledBrightnessRanges"].toArray();
		for (const QJsonValue& v : rangesArray)
		{
			const QJsonObject o = v.toObject();
			LedBrightnessRange r;
			r.from       = o["from"].toInt(0);
			r.to         = o["to"].toInt(0);
			r.brightness = static_cast<float>(o["brightness"].toDouble(1.0));
			ranges.push_back(r);
		}
		setLedBrightnessRanges(ranges);

        // AJOUTER CES LIGNES :
        int newBlurRadius = obj["gaussianBlurRadius"].toInt(0);
        setGaussianBlurRadius(newBlurRadius);
	int newBlackThreshold = obj["blackThreshold"].toInt(0);
	setBlackThreshold(newBlackThreshold);
    }
}

void ImageToLedManager::setSize(unsigned width, unsigned height)
{
	// Check if the existing buffer-image is already the correct dimensions
	if (_colorAveraging != nullptr && _colorAveraging->width() == width && _colorAveraging->height() == height)
	{
		return;
	}

	// Construct a new buffer and mapping
	registerProcessingUnit(width, height, 0, 0);
}

void ImageToLedManager::setLedString(const LedString& ledString)
{
	if (_colorAveraging != nullptr)
	{
		_ledString = ledString;

		// get current width/height
		unsigned width = _colorAveraging->width();
		unsigned height = _colorAveraging->height();

		// Construct a new buffer and mapping
		registerProcessingUnit(width, height, 0, 0);
	}
}

void ImageToLedManager::setBlackbarDetectDisable(bool enable)
{
	_borderProcessor->setHardDisable(enable);
}

bool ImageToLedManager::blackBorderDetectorEnabled() const
{
	return _borderProcessor->enabled();
}

void ImageToLedManager::processFrame(std::vector<float3>& ledColors, const Image<ColorRgb>& frameBuffer, Image<ColorRgb>& outImage)
{
    if (_gaussianBlurRadius > 0)
    {
        outImage = Image<ColorRgb>(frameBuffer.width(), frameBuffer.height());
        memcpy(outImage.rawMem(), frameBuffer.rawMem(), frameBuffer.size());
        GaussianBlur::apply(outImage, _gaussianBlurRadius);
        setSize(outImage);
        verifyBorder(outImage);
        if (_colorAveraging != nullptr && _colorAveraging->width() == outImage.width() && _colorAveraging->height() == outImage.height())
            _colorAveraging->process(ledColors, outImage);
    }
    else
    {
    // radius == 0 : outImage reste vide (width==0), l'appelant utilisera l'image originale
    setSize(frameBuffer);
    verifyBorder(frameBuffer);
    if (_colorAveraging != nullptr && _colorAveraging->width() == frameBuffer.width() && _colorAveraging->height() == frameBuffer.height())
        _colorAveraging->process(ledColors, frameBuffer);
    }
    // Appliquer la luminosité par plage
    for (const auto& range : _brightnessRanges)
    {
        int end = std::min(range.to, static_cast<int>(ledColors.size()) - 1);
        for (int i = range.from; i <= end; ++i)
            ledColors[i] = linalg::clamp(ledColors[i] * range.brightness, 0.0f, 1.0f);
    }

    // Forcer le noir absolu sous le seuil
    if (_blackThreshold > 0.0f)
    {
        for (auto& color : ledColors)
        {
            if (color.x < _blackThreshold && color.y < _blackThreshold && color.z < _blackThreshold)
                color = float3{ 0.0f, 0.0f, 0.0f };
        }
    }
}

void ImageToLedManager::setSparseProcessing(bool sparseProcessing)
{
	bool _orgmappingType = _sparseProcessing;

	_sparseProcessing = sparseProcessing;

	Debug(_log, "setSparseProcessing to {:d}", _sparseProcessing);
	if (_orgmappingType != _sparseProcessing && _colorAveraging != nullptr)
	{
		unsigned width = _colorAveraging->width();
		unsigned height = _colorAveraging->height();

		registerProcessingUnit(width, height, 0, 0);
	}
}

void ImageToLedManager::setLedMappingType(int mapType)
{
	int _orgmappingType = _mappingType;

	_mappingType = mapType;

	Debug(_log, "Set LED mapping type to {:s}", (mappingTypeToStr(mapType)));

	if (_orgmappingType != _mappingType && _colorAveraging != nullptr)
	{
		unsigned width = _colorAveraging->width();
		unsigned height = _colorAveraging->height();

		registerProcessingUnit(width, height, 0, 0);
	}

	if (_orgmappingType != _mappingType)
	{
		emit SignalImageToLedsMappingChanged(_mappingType);
	}
}

bool ImageToLedManager::getScanParameters(size_t led, double& hscanBegin, double& hscanEnd, double& vscanBegin, double& vscanEnd) const
{
	if (led < _ledString.leds().size())
	{
		const LedString::Led& l = _ledString.leds()[led];
		hscanBegin = l.minX_frac;
		hscanEnd = l.maxX_frac;
		vscanBegin = l.minY_frac;
		vscanEnd = l.maxY_frac;
	}

	return false;
}

void ImageToLedManager::verifyBorder(const Image<ColorRgb>& image)
{
	if (!_borderProcessor->enabled() && (_colorAveraging->horizontalBorder() != 0 || _colorAveraging->verticalBorder() != 0))
	{
		Debug(_log, "Reset border");
		_borderProcessor->process(image);

		registerProcessingUnit(image.width(), image.height(), 0, 0);
	}

	if (_borderProcessor->enabled() && _borderProcessor->process(image))
	{
		const hyperhdr::BlackBorder border = _borderProcessor->getCurrentBorder();

		if (border.unknown)
		{
			// Construct a new buffer and mapping
			registerProcessingUnit(image.width(), image.height(), 0, 0);
		}
		else
		{
			// Construct a new buffer and mapping
			registerProcessingUnit(image.width(), image.height(), border.horizontalSize, border.verticalSize);
		}
	}
}

void ImageToLedManager::setSize(const Image<ColorRgb>& image)
{
	setSize(image.width(), image.height());
}

int ImageToLedManager::getLedMappingType() const
{
	return _mappingType;
}

void ImageToLedManager::setGaussianBlurRadius(int radius)
{
    _gaussianBlurRadius = std::max(0, std::min(radius, 30));
    Debug(_log, "Gaussian blur radius set to: {:d}", _gaussianBlurRadius);
}

void ImageToLedManager::setLedBrightnessRanges(const std::vector<LedBrightnessRange>& ranges)
{
    _brightnessRanges = ranges;
    Debug(_log, "LED brightness ranges updated: {:d} range(s)", static_cast<int>(ranges.size()));
}

void ImageToLedManager::setBlackThreshold(int threshold)
{
    _blackThreshold = std::clamp(threshold, 0, 30) / 255.0f;
    Debug(_log, "Black threshold set to: {:d}/255", threshold);
}