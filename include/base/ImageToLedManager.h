#pragma once

#ifndef PCH_ENABLED
	#include <QString>

	#include <memory>
	#include <vector>
#endif

#include <image/Image.h>
#include <utils/Logger.h>
#include <led-strip/LedString.h>
#include <base/ImageColorAveraging.h>
#include <blackborder/BlackBorderProcessor.h>

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
	void processFrame(std::vector<ColorRgb>& colors, const Image<ColorRgb>& frameBuffer);

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
	Logger*		_log;	
	LedString	_ledString;
	hyperhdr::BlackBorderProcessor* _borderProcessor;
	std::unique_ptr<hyperhdr::ImageColorAveraging> _colorAveraging;
	int		_mappingType;
	bool	_sparseProcessing;
	uint16_t _advanced[256];	
};
