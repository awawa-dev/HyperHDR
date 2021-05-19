#pragma once

#include <QObject>
#include <cstdint>

#include <utils/ColorRgb.h>
#include <utils/Image.h>
#include <utils/ImageResampler.h>
#include <utils/Logger.h>
#include <utils/Components.h>

#include <QMultiMap>

///
/// @brief The Grabber class is responsible to apply image resizes (with or without ImageResampler)
/// Overwrite the videoMode with setVideoMode()
/// Overwrite setCropping()
class Grabber : public QObject
{
	Q_OBJECT

public:
	Grabber(const QString& grabberName = "", int width = 0, int height = 0,
			int cropLeft = 0, int cropRight = 0, int cropTop = 0, int cropBottom = 0);

	///
	/// Set the video mode (HDR)
	/// @param[in] mode The new video mode
	///
	virtual void setHdrToneMappingEnabled(int mode) = 0;

	///
	/// @brief Apply new crop values, on errors reject the values
	///
	void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom);

	///
	/// @brief Apply new video input (used from v4l)
	/// @param input device input
	///
	virtual bool setInput(int input);

	///
	/// @brief Apply new width/height values, on errors (collide with cropping) reject the values
	/// @return True on success else false
	///
	virtual bool setWidthHeight(int width, int height);

	///
	/// @brief Apply new framerate (used from v4l)
	/// @param fps framesPerSecond
	///
	virtual bool setFramerate(int fps);

	///
	/// @brief Apply new signalThreshold (used from v4l)
	///
	virtual void setSignalThreshold(
		double redSignalThreshold,
		double greenSignalThreshold,
		double blueSignalThreshold,
		int noSignalCounterThreshold = 50) = 0;

	///
	/// @brief Apply new SignalDetectionOffset  (used from v4l)
	///
	virtual void setSignalDetectionOffset(
		double verticalMin,
		double horizontalMin,
		double verticalMax,
		double horizontalMax) = 0;

	///
	/// @brief Apply SignalDetectionEnable
	///
	virtual void setSignalDetectionEnable(bool enable) = 0;

	///
	/// @brief Apply device and videoStanded
	///
	virtual void setDeviceVideoStandard(QString device) = 0;

	///
	/// @brief get current resulting height of image (after crop)
	///
	int getImageWidth();

	///
	/// @brief get current resulting width of image (after crop)
	///
	int getImageHeight();

	///
	/// @brief Prevent the real capture implementation from capturing if disabled
	///
	void setEnabled(bool enable);

	///
	/// @brief Get a list of all available V4L devices
	/// @return List of all available V4L devices on success else empty List
	///
	virtual QStringList getV4L2devices() const = 0;

	///
	/// @brief Get the V4L device name
	/// @param devicePath The device path
	/// @return The name of the V4L device on success else empty String
	///
	virtual QString getV4L2deviceName(const QString& devicePath) const = 0;

	///
	/// @brief Get a name/index pair of supported device inputs
	/// @param devicePath The device path
	/// @return multi pair of name/index on success else empty pair
	///
	virtual QMultiMap<QString, int> getV4L2deviceInputs(const QString& devicePath) const = 0;

	///
	/// @brief Get a list of supported device resolutions
	/// @param devicePath The device path
	/// @return List of resolutions on success else empty List
	///
	virtual QStringList getResolutions(const QString& devicePath) const = 0;

	///
	/// @brief Get a list of supported device framerates
	/// @param devicePath The device path
	/// @return List of framerates on success else empty List
	///
	virtual QStringList getFramerates(const QString& devicePath) const = 0;

	virtual QStringList getVideoCodecs(const QString& devicePath) const = 0;

protected:	
	/// the selected HDR mode
	int _hdr;

	/// With of the captured snapshot [pixels]
	int _width;

	/// Height of the captured snapshot [pixels]
	int _height;

	/// frame per second
	int _fps;

	/// device input
	int _input;

	/// number of pixels to crop after capturing
	int _cropLeft, _cropRight, _cropTop, _cropBottom;

	bool _enabled;

	// enable/disable HDR tone mapping
	uint8_t _hdrToneMappingEnabled;
	
	/// logger instance
	Logger* _log;
};
