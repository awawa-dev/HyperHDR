#pragma once

#include <QString>

#include <memory>

// Utils includes
#include <utils/Image.h>

#include <hyperhdrbase/LedString.h>
#include <hyperhdrbase/ImageToLedsMap.h>
#include <utils/Logger.h>

// settings
#include <utils/settings.h>

// Black border includes
#include <blackborder/BlackBorderProcessor.h>

class HyperHdrInstance;
class ImageProcessingUnit;

///
/// The ImageProcessor translates an RGB-image to RGB-values for the LEDs. The processing is
/// performed in two steps. First the average color per led-region is computed. Second a
/// color-transform is applied based on a gamma-correction.
///
class ImageProcessor : public QObject
{
	Q_OBJECT

public:
	friend class ImageProcessingUnit;

	///
	/// Constructs an image-processor for translating an image to led-color values based on the
	/// given led-string specification
	///	@param[in] ledString    LedString data
	/// @param[in] hyperhdr     HyperHDR instance pointer
	///
	ImageProcessor(const LedString& ledString, HyperHdrInstance* hyperhdr);

	~ImageProcessor() override;

	///
	/// Specifies the width and height of 'incoming' images. This will resize the buffer-image to
	/// match the given size.
	/// NB All earlier obtained references will be invalid.
	///
	/// @param[in] width   The new width of the buffer-image
	/// @param[in] height  The new height of the buffer-image
	///
	void setSize(unsigned width, unsigned height);

	///
	/// @brief Update the led string (eg on settings change)
	///
	void setLedString(const LedString& ledString);

	/// Returns state of black border detector
	bool blackBorderDetectorEnabled() const;

	/// Returns the current _mappingType
	int getLedMappingType() const;

	static int mappingTypeToInt(const QString& mappingType);
	static QString mappingTypeToStr(int mappingType);

	void setSparseProcessing(bool sparseProcessing);

public slots:

	void setBlackbarDetectDisable(bool enable);

	void setLedMappingType(int mapType);

public:
	///
	/// Specifies the width and height of 'incoming' images. This will resize the buffer-image to
	/// match the given size.
	/// NB All earlier obtained references will be invalid.
	///
	/// @param[in] image   The dimensions taken from image
	///
	void setSize(const Image<ColorRgb>& image);

	void verifyBorder(const Image<ColorRgb>& image);

	///
	/// Processes the image to a list of led colors. This will update the size of the buffer-image
	/// if required and call the image-to-leds mapping to determine the mean color per led.
	///
	/// @param[in] image  The image to translate to led values
	///
	/// @return The color value per led
	///	
	std::vector<ColorRgb> process(const Image<ColorRgb>& image);

	///
	/// Get the hscan and vscan parameters for a single led
	///
	/// @param[in] led Index of the led
	/// @param[out] hscanBegin begin of the hscan
	/// @param[out] hscanEnd end of the hscan
	/// @param[out] vscanBegin begin of the hscan
	/// @param[out] vscanEnd end of the hscan
	/// @return true if the parameters could be retrieved
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
	Logger* _log;
	/// The Led-string specification
	LedString _ledString;

	/// The processor for black border detection
	hyperhdr::BlackBorderProcessor* _borderProcessor;

	/// The mapping of image-pixels to LEDs
	std::shared_ptr<hyperhdr::ImageToLedsMap> _imageToLedColors;

	/// Type of image 2 led mapping
	int _mappingType;

	bool _sparseProcessing;

	/// HyperHDR instance pointer
	HyperHdrInstance* _hyperhdr;

	// lut advanced operator
	uint16_t advanced[256];

	quint8 _instanceIndex;
};
