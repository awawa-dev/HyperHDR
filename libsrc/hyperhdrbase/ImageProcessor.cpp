

#include <hyperhdrbase/HyperHdrInstance.h>
#include <hyperhdrbase/ImageProcessor.h>
#include <hyperhdrbase/ImageToLedsMap.h>

// Blacborder includes
#include <blackborder/BlackBorderProcessor.h>
#include <QDateTime>

using namespace hyperhdr;

// global transform method
int ImageProcessor::mappingTypeToInt(const QString& mappingType)
{
	if (mappingType == "weighted" )
		return 3;
		
	if (mappingType == "advanced" )
		return 2;
		
	if (mappingType == "unicolor_mean" )
		return 1;

	return 0;
}
// global transform method
QString ImageProcessor::mappingTypeToStr(int mappingType)
{
	if (mappingType == 3 )
		return "weighted";
		
	if (mappingType == 2 )
		return "advanced";
		
	if (mappingType == 1 )
		return "unicolor_mean";

	return "multicolor_mean";
}

ImageProcessor::ImageProcessor(const LedString& ledString, HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr)
	, _log(Logger::getInstance("IMAGETOLED"))
	, _ledString(ledString)
	, _borderProcessor(new BlackBorderProcessor(hyperhdr, this))
	, _imageToLeds(nullptr)
	, _mappingType(0)
	, _userMappingType(0)
	, _hardMappingType(0)
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

	ledFrameStat.ledStatBegin = 0;
}

ImageProcessor::~ImageProcessor()
{
	delete _imageToLeds;
}

void ImageProcessor::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::type::COLOR)
	{
		const QJsonObject& obj = config.object();
		int newType = mappingTypeToInt(obj["imageToLedMappingType"].toString());
		if(_userMappingType != newType)
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
	if (_imageToLeds && _imageToLeds->width() == width && _imageToLeds->height() == height)
	{
		return;
	}

	// Clean up the old buffer and mapping
	delete _imageToLeds;

	// Construct a new buffer and mapping
	_imageToLeds = (width>0 && height>0) ? (new ImageToLedsMap(_log, _mappingType, _sparseProcessing, width, height, 0, 0, _instanceIndex, _ledString.leds())) : nullptr;
}

void ImageProcessor::setLedString(const LedString& ledString)
{
	if ( _imageToLeds != nullptr)
	{
		_ledString = ledString;

		// get current width/height
		unsigned width = _imageToLeds->width();
		unsigned height = _imageToLeds->height();

		// Clean up the old buffer and mapping
		delete _imageToLeds;

		// Construct a new buffer and mapping
		_imageToLeds = new ImageToLedsMap(_log, _mappingType, _sparseProcessing, width, height, 0, 0, _instanceIndex, _ledString.leds());
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
	if (_orgmappingType != _sparseProcessing && _imageToLeds != nullptr)
	{
		unsigned width = _imageToLeds->width();
		unsigned height = _imageToLeds->height();
		delete _imageToLeds;
		_imageToLeds = new ImageToLedsMap(_log, _mappingType, _sparseProcessing, width, height, 0, 0, _instanceIndex, _ledString.leds());
	}
}

void ImageProcessor::setLedMappingType(int mapType)
{
	int _orgmappingType = _mappingType;
	
	// if the _hardMappingType is >-1 we aren't allowed to overwrite it
	_userMappingType = mapType;
	
	Debug(_log, "set user led mapping to %s", QSTRING_CSTR(mappingTypeToStr(mapType)));
	
	if(_hardMappingType == -1)
	{
		_mappingType = mapType;
	}
	
	if (_orgmappingType != _mappingType && _imageToLeds != nullptr)
	{		
		unsigned width = _imageToLeds->width();
		unsigned height = _imageToLeds->height();
		delete _imageToLeds;
		_imageToLeds = new ImageToLedsMap(_log, _mappingType, _sparseProcessing, width, height, 0, 0, _instanceIndex, _ledString.leds());
	}
}

void ImageProcessor::setHardLedMappingType(int mapType)
{
	int _orgmappingType = _mappingType;
	
	// force the maptype, if set to -1 we use the last requested _userMappingType
	_hardMappingType = mapType;
	if(mapType == -1)
		_mappingType = _userMappingType;
	else
		_mappingType = mapType;
		
	Debug(_log, "set hard led mapping to %s", QSTRING_CSTR(mappingTypeToStr(mapType)));		
		
	if (_orgmappingType != _mappingType && _imageToLeds != nullptr)
	{
		unsigned width = _imageToLeds->width();
		unsigned height = _imageToLeds->height();
		delete _imageToLeds;
		_imageToLeds = new ImageToLedsMap(_log, _mappingType, _sparseProcessing, width, height, 0, 0, _instanceIndex, _ledString.leds());
	}
}

bool ImageProcessor::getScanParameters(size_t led, double &hscanBegin, double &hscanEnd, double &vscanBegin, double &vscanEnd) const
{
	if (led < _ledString.leds().size())
	{
		const Led & l = _ledString.leds()[led];
		hscanBegin = l.minX_frac;
		hscanEnd = l.maxX_frac;
		vscanBegin = l.minY_frac;
		vscanEnd = l.maxY_frac;
	}

	return false;
}

std::vector<ColorRgb> ImageProcessor::process(const Image<ColorRgb>& image)
{
	std::vector<ColorRgb> colors;
	uint64_t currentTime = QDateTime::currentMSecsSinceEpoch();

	if (ledFrameStat.ledStatBegin == 0 || ledFrameStat.ledStatBegin > currentTime)
	{
		ledFrameStat.ledStatBegin = currentTime;
		ledFrameStat.averageFrame = 0;
		ledFrameStat.total = 0;
	}

	long	 diff = currentTime - ledFrameStat.ledStatBegin;
	
	if (image.width()>0 && image.height()>0)
	{
		// Ensure that the buffer-image is the proper size
		setSize(image);

		// Check black border detection
		verifyBorder(image);

		// Create a result vector and call the 'in place' function
		colors = _imageToLeds->Process(image, advanced);			
	}
	else
	{
		Warning(_log, "ImageProcessor::process called with image size 0");
	}

	ledFrameStat.averageFrame += QDateTime::currentMSecsSinceEpoch() - currentTime;
	ledFrameStat.total++;

	if ( diff >= 1000 * 60)
	{
		if (diff < 2 * 1000 * 60)
		{
			int av = (ledFrameStat.total > 0) ? ledFrameStat.averageFrame / ledFrameStat.total : 0;

			if (av > 10)
			{
				Info(_log, "Image to led instance %i processing => FPS: %.2f, av. delay: %dms, total time: %.2fs, refreshes count: %d",
					_instanceIndex,
					ledFrameStat.total / 60.0,
					av,
					ledFrameStat.averageFrame / 1000.0,
					ledFrameStat.total);
			}
		}

		ledFrameStat.ledStatBegin = currentTime;
		ledFrameStat.averageFrame = 0;
		ledFrameStat.total = 0;
	}	
	// return the computed colors
	return colors;
}

void ImageProcessor::verifyBorder(const Image<ColorRgb> & image)
{
	if (!_borderProcessor->enabled() && ( _imageToLeds->horizontalBorder()!=0 || _imageToLeds->verticalBorder()!=0 ))
	{
		Debug(_log, "Reset border");
		_borderProcessor->process(image);
		delete _imageToLeds;
		_imageToLeds = new hyperhdr::ImageToLedsMap(_log, _mappingType, _sparseProcessing, image.width(), image.height(), 0, 0, _instanceIndex, _ledString.leds());
	}

	if(_borderProcessor->enabled() && _borderProcessor->process(image))
	{
		const hyperhdr::BlackBorder border = _borderProcessor->getCurrentBorder();

		// Clean up the old mapping
		delete _imageToLeds;

		if (border.unknown)
		{
			// Construct a new buffer and mapping
			_imageToLeds = new hyperhdr::ImageToLedsMap(_log, _mappingType, _sparseProcessing, image.width(), image.height(), 0, 0, _instanceIndex, _ledString.leds());
		}
		else
		{
			// Construct a new buffer and mapping
			_imageToLeds = new hyperhdr::ImageToLedsMap(_log, _mappingType, _sparseProcessing, image.width(), image.height(), border.horizontalSize, border.verticalSize, _instanceIndex, _ledString.leds());
		}

		//Debug(Logger::getInstance("BLACKBORDER"),  "CURRENT BORDER TYPE: unknown=%d hor.size=%d vert.size=%d",
		//	border.unknown, border.horizontalSize, border.verticalSize );
	}
}

void ImageProcessor::setSize(const Image<ColorRgb> &image)
{
	setSize(image.width(), image.height());
}


int ImageProcessor::getUserLedMappingType() const
{
	return _userMappingType;
}


int ImageProcessor::ledMappingType() const
{
	return _mappingType;
}
