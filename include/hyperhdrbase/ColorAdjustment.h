#pragma once

// STL includes
#include <QString>

// Utils includes
#include <utils/RgbChannelAdjustment.h>
#include <utils/RgbTransform.h>

class ColorAdjustment
{
public:
	/// Unique identifier for this color transform
	QString _id;

	/// The BLACK (RGB-Channel) adjustment
	RgbChannelAdjustment _rgbBlackAdjustment;
	/// The WHITE (RGB-Channel) adjustment
	RgbChannelAdjustment _rgbWhiteAdjustment;
	/// The RED (RGB-Channel) adjustment
	RgbChannelAdjustment _rgbRedAdjustment;
	/// The GREEN (RGB-Channel) adjustment
	RgbChannelAdjustment _rgbGreenAdjustment;
	/// The BLUE (RGB-Channel) adjustment
	RgbChannelAdjustment _rgbBlueAdjustment;
	/// The CYAN (RGB-Channel) adjustment
	RgbChannelAdjustment _rgbCyanAdjustment;
	/// The MAGENTA (RGB-Channel) adjustment
	RgbChannelAdjustment _rgbMagentaAdjustment;
	/// The YELLOW (RGB-Channel) adjustment
	RgbChannelAdjustment _rgbYellowAdjustment;

	RgbTransform _rgbTransform;

	static ColorAdjustment* createColorAdjustment(quint8 instance, const QJsonObject& adjustmentConfig)
	{
		const QString id = adjustmentConfig["id"].toString("default");

		ColorAdjustment* adjustment = new ColorAdjustment();
		adjustment->_id = id;
		adjustment->_rgbBlackAdjustment = RgbChannelAdjustment::createRgbChannelAdjustment(instance, adjustmentConfig, "black", 0, 0, 0);
		adjustment->_rgbWhiteAdjustment = RgbChannelAdjustment::createRgbChannelAdjustment(instance, adjustmentConfig, "white", 255, 255, 255);
		adjustment->_rgbRedAdjustment = RgbChannelAdjustment::createRgbChannelAdjustment(instance, adjustmentConfig, "red", 255, 0, 0);
		adjustment->_rgbGreenAdjustment = RgbChannelAdjustment::createRgbChannelAdjustment(instance, adjustmentConfig, "green", 0, 255, 0);
		adjustment->_rgbBlueAdjustment = RgbChannelAdjustment::createRgbChannelAdjustment(instance, adjustmentConfig, "blue", 0, 0, 255);
		adjustment->_rgbCyanAdjustment = RgbChannelAdjustment::createRgbChannelAdjustment(instance, adjustmentConfig, "cyan", 0, 255, 255);
		adjustment->_rgbMagentaAdjustment = RgbChannelAdjustment::createRgbChannelAdjustment(instance, adjustmentConfig, "magenta", 255, 0, 255);
		adjustment->_rgbYellowAdjustment = RgbChannelAdjustment::createRgbChannelAdjustment(instance, adjustmentConfig, "yellow", 255, 255, 0);
		adjustment->_rgbTransform = RgbTransform::createRgbTransform(instance, adjustmentConfig);

		adjustment->_rgbRedAdjustment.setCorrection(adjustmentConfig["temperatureRed"].toInt(255));
		adjustment->_rgbBlueAdjustment.setCorrection(adjustmentConfig["temperatureBlue"].toInt(255));
		adjustment->_rgbGreenAdjustment.setCorrection(adjustmentConfig["temperatureGreen"].toInt(255));

		return adjustment;
	}
};
