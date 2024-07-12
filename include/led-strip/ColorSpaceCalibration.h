#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QJsonObject>

	#include <cstdint>
#endif

#include <image/ColorRgb.h>
#include <utils/Logger.h>

class ColorSpaceCalibration
{
public:
	ColorSpaceCalibration(uint8_t instance, const QJsonObject& colorConfig);

	double getGammaR() const;
	double getGammaG() const;
	double getGammaB() const;
	void setGamma(double gammaR, double gammaG = -1, double gammaB = -1);
	void setSaturationGain(double saturationGain);
	void setLuminanceGain(double luminanceGain);
	double getSaturationGain() const;
	double getLuminanceGain() const;
	bool getClassicConfig() const;
	void setClassicConfig(bool classic_config);

	int getBacklightThreshold() const;
	void setBacklightThreshold(int backlightThreshold);
	bool getBacklightColored() const;
	void setBacklightColored(bool backlightColored);
	bool getBackLightEnabled() const;
	void setBackLightEnabled(bool enable);
	uint8_t getBrightness() const;
	void setBrightness(int brightness);
	uint8_t getBrightnessCompensation() const;
	void setBrightnessCompensation(int brightnessCompensation);
	void getBrightnessComponents(uint8_t& rgb, uint8_t& cmy, uint8_t& w) const;
	bool isBrightnessCorrectionEnabled() const;
	void applyGamma(uint8_t& red, uint8_t& green, uint8_t& blue) const;
	void applyBacklight(uint8_t& red, uint8_t& green, uint8_t& blue) const;
	void applySaturationLuminance(uint8_t& red, uint8_t& green, uint8_t& blue) const;

	static void rgb2hsl_d(double r, double g, double b, double& hue, double& saturation, double& luminance);
	static void hsl2rgb_d(double hue, double saturation, double luminance, double& r, double& g, double& b);

private:
	void init(
		bool classic_config,
		double saturationGain,
		double luminanceGain,
		double gammaR, double gammaG, double gammaB,
		int backlightThreshold, bool backlightColored, uint8_t brightness, uint8_t brightnessCompensation);
	
	void initializeMapping();

	void updateBrightnessComponents();

	bool      _backLightEnabled, _backlightColored;
	int		  _backlightThreshold;

	uint8_t   _sumBrightnessRGBLow;

	double    _gammaR, _gammaG, _gammaB;

	uint8_t   _mappingR[256], _mappingG[256], _mappingB[256];

	uint8_t   _brightness, _brightnessCompensation, _brightness_rgb, _brightness_cmy, _brightness_w;

	Logger* _log;

public:

	bool   _classic_config;
	double _saturationGain;
	double _luminanceGain;
	double _luminanceMinimum;
};
