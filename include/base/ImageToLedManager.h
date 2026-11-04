#pragma once

#ifndef PCH_ENABLED
	#include <QString>

	#include <memory>
	#include <vector>
#endif

#include <image/Image.h>
#include <image/GaussianBlur.h>
#include <utils/Logger.h>
#include <base/LedString.h>
#include <base/ImageColorAveraging.h>
#include <blackborder/BlackBorderProcessor.h>

#include <linalg.h>
#include <vector>

struct LedBrightnessRange
{
    int from;
    int to;
    float brightness;
};

class HyperHdrInstance;

class ImageToLedManager : public QObject
{
	Q_OBJECT

public:
	ImageToLedManager(const LedString& ledString, HyperHdrInstance* hyperhdr);

	void setSize(unsigned width, unsigned height);
	void setLedString(const LedString& ledString);
	bool blackBorderDetectorEnabled() const;
	int getLedMappingType() const;

	static int mappingTypeToInt(const QString& mappingType);
	static QString mappingTypeToStr(int mappingType);

	void setSparseProcessing(bool sparseProcessing);
	void setGaussianBlurRadius(int radius);
	void setLedBrightnessRanges(const std::vector<LedBrightnessRange>& ranges);
	void setBlackThreshold(int threshold);
	void processFrame(std::vector<linalg::aliases::float3>& ledColors, const Image<ColorRgb>& frameBuffer, Image<ColorRgb>& outImage);

signals:
	void SignalImageToLedsMappingChanged(int mappingType);

public slots:
	void setBlackbarDetectDisable(bool enable);
	void setLedMappingType(int mapType);

public:
	void setSize(const Image<ColorRgb>& image);
	void verifyBorder(const Image<ColorRgb>& image);
	bool getScanParameters(size_t led, double& hscanBegin, double& hscanEnd, double& vscanBegin, double& vscanEnd) const;

private:
	void registerProcessingUnit(
		const unsigned width,
		const unsigned height,
		const unsigned horizontalBorder,
		const unsigned verticalBorder);

private slots:
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

private:
	quint8		_instanceIndex;
	LoggerName	_log;	
	LedString	_ledString;
	hyperhdr::BlackBorderProcessor* _borderProcessor;
	std::unique_ptr<hyperhdr::ImageColorAveraging> _colorAveraging;
	int		_mappingType;
	bool	_sparseProcessing;
	int _gaussianBlurRadius = 0;
	std::vector<LedBrightnessRange> _brightnessRanges;
	float _blackThreshold = 0.0f;
};
