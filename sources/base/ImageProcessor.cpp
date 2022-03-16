

#include <base/HyperHdrInstance.h>
#include <base/ImageProcessor.h>
#include <base/ImageToLedsMap.h>

// Blacborder includes
#include <blackborder/BlackBorderProcessor.h>
#include <QDateTime>

using namespace hyperhdr;

void ImageProcessor::registerProcessingUnit(
	const unsigned width,
	const unsigned height,
	const unsigned horizontalBorder,
	const unsigned verticalBorder)
{
	if (width > 0 && height > 0)
		_imageToLedColors = std::make_shared<ImageToLedsMap>(
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
		_imageToLedColors = nullptr;
}


// global transform method
int ImageProcessor::mappingTypeToInt(const QString& mappingType)
{
	if (mappingType == "weighted")
		return 3;

	if (mappingType == "advanced")
		return 2;

	if (mappingType == "unicolor_mean")
		return 1;

	return 0;
}
// global transform method
QString ImageProcessor::mappingTypeToStr(int mappingType)
{
	if (mappingType == 3)
		return "weighted";

	if (mappingType == 2)
		return "advanced";

	if (mappingType == 1)
		return "unicolor_mean";

	return "multicolor_mean";
}

ImageProcessor::ImageProcessor(const LedString& ledString, HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr)
	, _log(Logger::getInstance(QString("IMAGETOLED%1").arg(hyperhdr->getInstanceIndex())))
	, _ledString(ledString)
	, _borderProcessor(new BlackBorderProcessor(hyperhdr, this))
	, _imageToLedColors(nullptr)
	, _mappingType(0)
	, _sparseProcessing(false)
	, _hyperhdr(hyperhdr)
	, _instanceIndex(hyperhdr->getInstanceIndex())
{
	// init
	handleSettingsUpdate(settings::type::COLOR, _hyperhdr->getSetting(settings::type::COLOR));
	// listen for changes in color - ledmapping
	connect(_hyperhdr, &HyperHdrInstance::settingsChanged, this, &ImageProcessor::handleSettingsUpdate);

	for (int i = 0; i < 256; i++)
		advanced[i] = i * i;
}

ImageProcessor::~ImageProcessor()
{	
}

void ImageProcessor::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::COLOR)
	{
		const QJsonObject& obj = config.object();
		int newType = mappingTypeToInt(obj["imageToLedMappingType"].toString());
		if (_mappingType != newType)
		{
			setLedMappingType(newType);
		}

		bool newSparse = obj["sparse_processing"].toBool(false);
		setSparseProcessing(newSparse);
	}
}

void ImageProcessor::setSize(unsigned width, unsigned height)
{
	// Check if the existing buffer-image is already the correct dimensions
	if (_imageToLedColors !=nullptr && _imageToLedColors->width() == width && _imageToLedColors->height() == height)
	{
		return;
	}

	// Construct a new buffer and mapping
	registerProcessingUnit(width, height, 0, 0);
}

void ImageProcessor::setLedString(const LedString& ledString)
{
	if (_imageToLedColors != nullptr)
	{
		_ledString = ledString;

		// get current width/height
		unsigned width = _imageToLedColors->width();
		unsigned height = _imageToLedColors->height();

		// Construct a new buffer and mapping
		registerProcessingUnit(width, height, 0, 0);
	}
}

void ImageProcessor::setBlackbarDetectDisable(bool enable)
{
	_borderProcessor->setHardDisable(enable);
}

bool ImageProcessor::blackBorderDetectorEnabled() const
{
	return _borderProcessor->enabled();
}

void ImageProcessor::setSparseProcessing(bool sparseProcessing)
{
	bool _orgmappingType = _sparseProcessing;

	_sparseProcessing = sparseProcessing;

	Debug(_log, "setSparseProcessing to %d", _sparseProcessing);
	if (_orgmappingType != _sparseProcessing && _imageToLedColors != nullptr)
	{
		unsigned width = _imageToLedColors->width();
		unsigned height = _imageToLedColors->height();

		registerProcessingUnit(width, height, 0, 0);
	}
}

void ImageProcessor::setLedMappingType(int mapType)
{
	int _orgmappingType = _mappingType;

	_mappingType = mapType;

	Debug(_log, "Set LED mapping type to %s", QSTRING_CSTR(mappingTypeToStr(mapType)));	

	if (_orgmappingType != _mappingType && _imageToLedColors != nullptr)
	{
		unsigned width = _imageToLedColors->width();
		unsigned height = _imageToLedColors->height();

		registerProcessingUnit(width, height, 0, 0);
	}
}

bool ImageProcessor::getScanParameters(size_t led, double& hscanBegin, double& hscanEnd, double& vscanBegin, double& vscanEnd) const
{
	if (led < _ledString.leds().size())
	{
		const Led& l = _ledString.leds()[led];
		hscanBegin = l.minX_frac;
		hscanEnd = l.maxX_frac;
		vscanBegin = l.minY_frac;
		vscanEnd = l.maxY_frac;
	}

	return false;
}

void ImageProcessor::verifyBorder(const Image<ColorRgb>& image)
{
	if (!_borderProcessor->enabled() && (_imageToLedColors->horizontalBorder() != 0 || _imageToLedColors->verticalBorder() != 0))
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

void ImageProcessor::setSize(const Image<ColorRgb>& image)
{
	setSize(image.width(), image.height());
}

int ImageProcessor::getLedMappingType() const
{
	return _mappingType;
}
