/* ColorSpaceCalibration.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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

#ifndef PCH_ENABLED
	#include <QtCore/qmath.h>
	#include <cmath>
#endif

#include <QtMath>

#include <led-strip/ColorSpaceCalibration.h>

namespace
{
	inline uint8_t clamp(int x)
	{
		return (x < 0) ? 0 : ((x > 255) ? 255 : static_cast<uint8_t>(x));
	}
}

ColorSpaceCalibration::ColorSpaceCalibration(uint8_t instance, const QJsonObject& colorConfig) :
	_log(Logger::getInstance(QString("COLORSPACE_CALIBRATION%1").arg(instance)))
{
	const int backlightThreshold = colorConfig["backlightThreshold"].toInt(0);
	const bool   backlightColored = colorConfig["backlightColored"].toBool(false);
	const int brightness = colorConfig["brightness"].toInt(100);
	const int brightnessComp = colorConfig["brightnessCompensation"].toInt(100);
	const double gammaR = colorConfig["gammaRed"].toDouble(1.0);
	const double gammaG = colorConfig["gammaGreen"].toDouble(1.0);
	const double gammaB = colorConfig["gammaBlue"].toDouble(1.0);

	const bool   classic_config = colorConfig["classic_config"].toBool(false);
	const double saturationGain = colorConfig["saturationGain"].toDouble(1.0000);
	const double luminanceGain = colorConfig["luminanceGain"].toDouble(1.0000);

	init(classic_config,
		saturationGain, luminanceGain,
		gammaR, gammaG, gammaB,
		backlightThreshold, backlightColored, static_cast<uint8_t>(brightness), static_cast<uint8_t>(brightnessComp));
}

void ColorSpaceCalibration::init(
	bool classic_config,
	double saturationGain, double luminanceGain,
	double gammaR, double gammaG, double gammaB,
	int backlightThreshold, bool backlightColored, uint8_t brightness, uint8_t brightnessCompensation)
{
	std::fill(std::begin(_mappingR), std::end(_mappingR), 0);
	std::fill(std::begin(_mappingG), std::end(_mappingG), 0);
	std::fill(std::begin(_mappingB), std::end(_mappingB), 0);

	_brightness_rgb = 0;
	_brightness_cmy = 0;
	_brightness_w = 0;

	_gammaR = 0;
	_gammaG = 0;
	_gammaB = 0;
	_backlightThreshold = 0;
	_sumBrightnessRGBLow = 0;
	_classic_config = classic_config;
	_saturationGain = saturationGain;
	_luminanceGain = luminanceGain;
	_luminanceMinimum = 0;
	_brightness = brightness;
	_brightnessCompensation = brightnessCompensation;
	_backLightEnabled = true;
	_backlightColored = true;

	_log->setInstanceEnable(false);
	setGamma(gammaR, gammaG, gammaB);	
	setBacklightThreshold(backlightThreshold);
	setBacklightColored(backlightColored);
	setBrightness(brightness);
	setBrightnessCompensation(brightnessCompensation);
	initializeMapping();
	_log->setInstanceEnable(true);

	Info(_log, "classicMode: %s, gammas:[%0.2f, %0.2f, %0.2f], saturation: %0.2f, luminance: %0.2f, backLight: [%s, threshold: %i, colored: %s]",
		(_classic_config ? "yes" : "no"), gammaR, gammaG, gammaB, _saturationGain, _luminanceGain, (_backLightEnabled ? "enabled" : "disabled"),_backlightThreshold, (_backlightColored) ? "yes" : "no");

}

double ColorSpaceCalibration::getGammaR() const
{
	return _gammaR;
}

double ColorSpaceCalibration::getGammaG() const
{
	return _gammaG;
}

double ColorSpaceCalibration::getGammaB() const
{
	return _gammaB;
}

void ColorSpaceCalibration::setGamma(double gammaR, double gammaG, double gammaB)
{
	_gammaR = std::max(gammaR, 0.05);
	_gammaG = std::max(gammaG, 0.05);
	_gammaB = std::max(gammaB, 0.05);
	initializeMapping();
	Debug(_log, "setGamma to [%f, %f, %f]", _gammaR, _gammaG, _gammaB);
}

void ColorSpaceCalibration::setSaturationGain(double saturationGain)
{
	if (saturationGain < 0)
		return;

	if (_saturationGain != saturationGain)
		Debug(_log, "set SaturationGain to %f", saturationGain);

	_saturationGain = saturationGain;
}

void ColorSpaceCalibration::setLuminanceGain(double luminanceGain)
{
	if (luminanceGain < 0)
		return;

	if (_luminanceGain != luminanceGain)
		Debug(_log, "set LuminanceGain to %f", luminanceGain);

	_luminanceGain = luminanceGain;
}

double ColorSpaceCalibration::getSaturationGain() const
{
	return _saturationGain;
}

double ColorSpaceCalibration::getLuminanceGain() const
{
	return _luminanceGain;
}

bool ColorSpaceCalibration::getClassicConfig() const
{
	return _classic_config;
}

void ColorSpaceCalibration::setClassicConfig(bool classic_config)
{
	_classic_config = classic_config;
}

void ColorSpaceCalibration::initializeMapping()
{
	for (int i = 0; i < 256; ++i)
	{
		_mappingR[i] = qMin(qMax((int)(qPow(i / 255.0, _gammaR) * 255), 0), 255);
		_mappingG[i] = qMin(qMax((int)(qPow(i / 255.0, _gammaG) * 255), 0), 255);
		_mappingB[i] = qMin(qMax((int)(qPow(i / 255.0, _gammaB) * 255), 0), 255);
	}
}


int ColorSpaceCalibration::getBacklightThreshold() const
{
	return _backlightThreshold;
}

void ColorSpaceCalibration::setBacklightThreshold(int backlightThreshold)
{
	if (backlightThreshold < 0)
		return;

	_backlightThreshold = backlightThreshold;

	uint8_t rgb = clamp(_backlightThreshold);

	if (_sumBrightnessRGBLow != rgb)
		Debug(_log, "setBacklightThreshold: %i", rgb);

	_sumBrightnessRGBLow = rgb;
}

bool ColorSpaceCalibration::getBacklightColored() const
{
	return _backlightColored;
}

void ColorSpaceCalibration::setBacklightColored(bool backlightColored)
{
	_backlightColored = backlightColored;
	Debug(_log, "setBacklightColored: %i", _backlightColored);
}

bool ColorSpaceCalibration::getBackLightEnabled() const
{
	return _backLightEnabled;
}

void ColorSpaceCalibration::setBackLightEnabled(bool enable)
{	
	_backLightEnabled = enable;
	Debug(_log, "setBackLightEnabled: %i", _backLightEnabled);
}

uint8_t ColorSpaceCalibration::getBrightness() const
{
	return _brightness;
}

void ColorSpaceCalibration::setBrightness(int brightness)
{
	if (brightness < 0)
		return;	

	_brightness = clamp(brightness);
	updateBrightnessComponents();

	Debug(_log, "setBrightness: %i", _brightness);
}

void ColorSpaceCalibration::setBrightnessCompensation(int brightnessCompensation)
{
	if (brightnessCompensation < 0)
		return;	

	_brightnessCompensation = clamp(brightnessCompensation);
	updateBrightnessComponents();

	Debug(_log, "setBrightnessCompensation: %i", _brightnessCompensation);
}

uint8_t ColorSpaceCalibration::getBrightnessCompensation() const
{
	return _brightnessCompensation;
}

void ColorSpaceCalibration::updateBrightnessComponents()
{
	double Fw = _brightnessCompensation * 2.0 / 100.0 + 1.0;
	double Fcmy = _brightnessCompensation / 100.0 + 1.0;

	double B_in = 0;
	_brightness_rgb = 0;
	_brightness_cmy = 0;
	_brightness_w = 0;

	if (_brightness > 0)
	{
		B_in = (_brightness < 50) ? -0.09 * _brightness + 7.5 : -0.04 * _brightness + 5.0;

		_brightness_rgb = std::ceil(qMin(255.0, 255.0 / B_in));
		_brightness_cmy = std::ceil(qMin(255.0, 255.0 / (B_in * Fcmy)));
		_brightness_w = std::ceil(qMin(255.0, 255.0 / (B_in * Fw)));
	}
}

bool ColorSpaceCalibration::isBrightnessCorrectionEnabled() const
{
	return _brightnessCompensation != 0 || _brightness != 100;
}

void ColorSpaceCalibration::getBrightnessComponents(uint8_t& rgb, uint8_t& cmy, uint8_t& w) const
{
	rgb = _brightness_rgb;
	cmy = _brightness_cmy;
	w = _brightness_w;
}

void ColorSpaceCalibration::applyGamma(uint8_t& red, uint8_t& green, uint8_t& blue) const
{
	// apply gamma
	red = _mappingR[red];
	green = _mappingG[green];
	blue = _mappingB[blue];
}

void ColorSpaceCalibration::applyBacklight(uint8_t& red, uint8_t& green, uint8_t& blue) const
{
	if (_backLightEnabled && _sumBrightnessRGBLow > 0)
	{
		if (_backlightColored)
		{
			red = std::max(red, _sumBrightnessRGBLow);
			green = std::max(green, _sumBrightnessRGBLow);
			blue = std::max(blue, _sumBrightnessRGBLow);
		}
		else
		{
			int avVal = (std::min(int(red), std::min(int(green), int(blue))) +
				std::max(int(red), std::max(int(green), int(blue)))) / 2;
			if (avVal < int(_sumBrightnessRGBLow))
			{
				red = _sumBrightnessRGBLow;
				green = _sumBrightnessRGBLow;
				blue = _sumBrightnessRGBLow;
			}
		}
	}
}

void ColorSpaceCalibration::applySaturationLuminance(uint8_t& red, uint8_t& green, uint8_t& blue) const
{
	if (_saturationGain != 1.0 || _luminanceGain != 1.0 || _luminanceMinimum != 0.0)
	{
		uint16_t hue;
		float saturation, luminance;
		ColorRgb::rgb2hsl(red, green, blue, hue, saturation, luminance);

		float s = saturation * _saturationGain;
		if (s > 1.0f)
			saturation = 1.0f;
		else
			saturation = s;

		float l = luminance * _luminanceGain;
		if (l < _luminanceMinimum)
		{
			saturation = 0;
			l = _luminanceMinimum;
		}
		if (l > 1.0f)
			luminance = 1.0f;
		else
			luminance = l;

		ColorRgb::hsl2rgb(hue, saturation, luminance, red, green, blue);
	}
}



void ColorSpaceCalibration::rgb2hsl_d(double r, double g, double b, double& hue, double& saturation, double& luminance)
{
	double min, max, chroma;

	// HSV Calculations -- formulas sourced from https://en.wikipedia.org/wiki/HSL_and_HSV
	// Compute constants
	min = (r < g) ? r : g;
	min = (min < b) ? min : b;

	max = (r > g) ? r : g;
	max = (max > b) ? max : b;

	chroma = max - min;

	// Compute L
	luminance = 0.5 * (max + min);

	if (chroma < 0.0001 || max < 0.0001) {
		hue = saturation = 0;
	}

	// Compute S
	saturation = chroma / (1 - fabs((2 * luminance) - 1));

	// Compute H
	if (max == r) { hue = fmod((g - b) / chroma, 6); }
	else if (max == g) { hue = ((b - r) / chroma) + 2; }
	else { hue = ((r - g) / chroma) + 4; }

	hue *= 60;
	if (hue < 0) { hue += 360; }
}

void ColorSpaceCalibration::hsl2rgb_d(double hue, double saturation, double luminance, double& R, double& G, double& B)
{
	// HSV Calculations -- formulas sourced from https://en.wikipedia.org/wiki/HSL_and_HSV
	if (saturation <= 0.001) {
		R = G = B = luminance;
	}
	else {
		double c = (1 - fabs((2 * luminance) - 1)) * saturation;
		double hh = hue / 60;
		double x = c * (1 - fabs(fmod(hh, 2) - 1));
		double r, g, b;

		if (hh <= 1) { r = c; g = x; b = 0; }
		else if (hh <= 2) { r = x; g = c; b = 0; }
		else if (hh <= 3) { r = 0; g = c; b = x; }
		else if (hh <= 4) { r = 0; g = x; b = c; }
		else if (hh <= 5) { r = x; g = 0; b = c; }
		else { r = c; g = 0; b = x; }

		double m = luminance - (0.5 * c);
		R = (r + m);
		G = (g + m);
		B = (b + m);
	}
}

