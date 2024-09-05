/* ColorCalibration.cpp
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

#include <led-strip/ColorCalibration.h>

ColorCalibration::ColorCalibration(uint8_t instance, const QJsonObject& adjustmentConfig)
{
	_id = adjustmentConfig["id"].toString("default");
	_blackCalibration = std::unique_ptr<ChannelCalibration>(new ChannelCalibration(instance, "black", adjustmentConfig, 0, 0, 0));
	_whiteCalibration = std::unique_ptr<ChannelCalibration>(new ChannelCalibration(instance, "white", adjustmentConfig, 255, 255, 255));
	_redCalibration = std::unique_ptr<ChannelCalibration>(new ChannelCalibration(instance, "red", adjustmentConfig, 255, 0, 0));
	_greenCalibration = std::unique_ptr<ChannelCalibration>(new ChannelCalibration(instance, "green", adjustmentConfig, 0, 255, 0));
	_blueCalibration = std::unique_ptr<ChannelCalibration>(new ChannelCalibration(instance, "blue", adjustmentConfig, 0, 0, 255));
	_cyanCalibration = std::unique_ptr<ChannelCalibration>(new ChannelCalibration(instance, "cyan", adjustmentConfig, 0, 255, 255));
	_magentaCalibration = std::unique_ptr<ChannelCalibration>(new ChannelCalibration(instance, "magenta", adjustmentConfig, 255, 0, 255));
	_yellowCalibration = std::unique_ptr<ChannelCalibration>(new ChannelCalibration(instance, "yellow", adjustmentConfig, 255, 255, 0));
	_colorspaceCalibration = std::unique_ptr<ColorSpaceCalibration>(new ColorSpaceCalibration(instance, adjustmentConfig));
}

QString ColorCalibration::getId()
{
	return _id;
}

void ColorCalibration::setBackLightEnabled(bool enabled)
{
	_colorspaceCalibration->setBackLightEnabled(enabled);
}

void ColorCalibration::calibrate(ColorRgb& color)
{
	uint8_t& ored = color.red;
	uint8_t& ogreen = color.green;
	uint8_t& oblue = color.blue;

	if (_colorspaceCalibration->_classic_config)
	{
		_colorspaceCalibration->applySaturationLuminance(ored, ogreen, oblue);
		_colorspaceCalibration->applyGamma(ored, ogreen, oblue);

		if (_redCalibration->isEnabled() ||
			_greenCalibration->isEnabled() ||
			_blueCalibration->isEnabled())
		{
			int RR = _redCalibration->adjustmentR(ored);
			int RG = ored > ogreen ? _redCalibration->adjustmentG(ored - ogreen) : 0;
			int RB = ored > oblue ? _redCalibration->adjustmentB(ored - oblue) : 0;

			int GR = ogreen > ored ? _greenCalibration->adjustmentR(ogreen - ored) : 0;
			int GG = _greenCalibration->adjustmentG(ogreen);
			int GB = ogreen > oblue ? _greenCalibration->adjustmentB(ogreen - oblue) : 0;

			int BR = oblue > ored ? _blueCalibration->adjustmentR(oblue - ored) : 0;
			int BG = oblue > ogreen ? _blueCalibration->adjustmentG(oblue - ogreen) : 0;
			int BB = _blueCalibration->adjustmentB(oblue);

			int ledR = RR + GR + BR;
			int maxR = _redCalibration->getAdjustment().red;
			int ledG = RG + GG + BG;
			int maxG = _greenCalibration->getAdjustment().green;
			int ledB = RB + GB + BB;
			int maxB = _blueCalibration->getAdjustment().blue;

			color.red = static_cast<uint8_t>(std::min(ledR, maxR));
			color.green = static_cast<uint8_t>(std::min(ledG, maxG));
			color.blue = static_cast<uint8_t>(std::min(maxB, ledB));
		}

		// temperature
		color.red = _redCalibration->correction(color.red);
		color.green = _greenCalibration->correction(color.green);
		color.blue = _blueCalibration->correction(color.blue);		
	}
	else
	{
		uint8_t B_RGB = 0, B_CMY = 0, B_W = 0;

		_colorspaceCalibration->getBrightnessComponents(B_RGB, B_CMY, B_W);
		_colorspaceCalibration->applyGamma(ored, ogreen, oblue);
		if (!_blackCalibration->isEnabled() && !_redCalibration->isEnabled() &&
			!_greenCalibration->isEnabled() && !_blueCalibration->isEnabled() &&
			!_cyanCalibration->isEnabled() && !_magentaCalibration->isEnabled() &&
			!_yellowCalibration->isEnabled() && !_whiteCalibration->isEnabled() &&
			!_colorspaceCalibration->isBrightnessCorrectionEnabled())
		{
			if (B_RGB != 255)
			{
				color.red = ((uint32_t)color.red * B_RGB) / 255;
				color.green = ((uint32_t)color.green * B_RGB) / 255;
				color.blue = ((uint32_t)color.blue * B_RGB) / 255;
			}
		}
		else
		{
			uint32_t nrng = (uint32_t)(255 - ored) * (255 - ogreen);
			uint32_t rng = (uint32_t)(ored) * (255 - ogreen);
			uint32_t nrg = (uint32_t)(255 - ored) * (ogreen);
			uint32_t rg = (uint32_t)(ored) * (ogreen);

			uint8_t black = nrng * (255 - oblue) / 65025;
			uint8_t red = rng * (255 - oblue) / 65025;
			uint8_t green = nrg * (255 - oblue) / 65025;
			uint8_t blue = nrng * (oblue) / 65025;
			uint8_t cyan = nrg * (oblue) / 65025;
			uint8_t magenta = rng * (oblue) / 65025;
			uint8_t yellow = rg * (255 - oblue) / 65025;
			uint8_t white = rg * (oblue) / 65025;

			uint8_t OR, OG, OB, RR, RG, RB, GR, GG, GB, BR, BG, BB;
			uint8_t CR, CG, CB, MR, MG, MB, YR, YG, YB, WR, WG, WB;

			_blackCalibration->apply(black, 255, OR, OG, OB);
			_redCalibration->apply(red, B_RGB, RR, RG, RB);
			_greenCalibration->apply(green, B_RGB, GR, GG, GB);
			_blueCalibration->apply(blue, B_RGB, BR, BG, BB);
			_cyanCalibration->apply(cyan, B_CMY, CR, CG, CB);
			_magentaCalibration->apply(magenta, B_CMY, MR, MG, MB);
			_yellowCalibration->apply(yellow, B_CMY, YR, YG, YB);
			_whiteCalibration->apply(white, B_W, WR, WG, WB);

			color.red = OR + RR + GR + BR + CR + MR + YR + WR;
			color.green = OG + RG + GG + BG + CG + MG + YG + WG;
			color.blue = OB + RB + GB + BB + CB + MB + YB + WB;
		}
	}
	_colorspaceCalibration->applyBacklight(ored, ogreen, oblue);
}

void ColorCalibration::putCurrentConfig(QJsonObject& adjustment)
{
	ChannelCalibration* calibColors[] = { _blackCalibration.get(), _whiteCalibration.get(), _redCalibration.get(),
				_greenCalibration.get(), _blueCalibration.get(), _cyanCalibration.get(),
				_magentaCalibration.get(), _yellowCalibration.get() };

	adjustment["id"] = _id;

	for (const auto& calibChannel : calibColors)
	{
		QJsonArray adj;
		adj.append(calibChannel->getAdjustment().red);
		adj.append(calibChannel->getAdjustment().green);
		adj.append(calibChannel->getAdjustment().blue);
		adjustment.insert(calibChannel->getChannelName(), adj);
	}

	adjustment["backlightThreshold"] = _colorspaceCalibration->getBacklightThreshold();
	adjustment["backlightColored"] = _colorspaceCalibration->getBacklightColored();
	adjustment["brightness"] = _colorspaceCalibration->getBrightness();
	adjustment["brightnessCompensation"] = _colorspaceCalibration->getBrightnessCompensation();
	adjustment["gammaRed"] = _colorspaceCalibration->getGammaR();
	adjustment["gammaGreen"] = _colorspaceCalibration->getGammaG();
	adjustment["gammaBlue"] = _colorspaceCalibration->getGammaB();
	adjustment["temperatureRed"] = _redCalibration->getCorrection();
	adjustment["temperatureGreen"] = _greenCalibration->getCorrection();
	adjustment["temperatureBlue"] = _blueCalibration->getCorrection();
	adjustment["saturationGain"] = _colorspaceCalibration->getSaturationGain();
	adjustment["luminanceGain"] = _colorspaceCalibration->getLuminanceGain();
	adjustment["classic_config"] = _colorspaceCalibration->getClassicConfig();
}

void ColorCalibration::updateConfig(const QJsonObject& adjustment)
{
	if (adjustment.contains("classic_config"))
		_colorspaceCalibration->setClassicConfig(adjustment["classic_config"].toBool(false));

	_redCalibration->setCorrection(adjustment["temperatureRed"].toInt(-1));
	_greenCalibration->setCorrection(adjustment["temperatureGreen"].toInt(-1));
	_blueCalibration->setCorrection(adjustment["temperatureBlue"].toInt(-1));

	_blackCalibration->setAdjustment(adjustment["black"].toArray());
	_redCalibration->setAdjustment(adjustment["red"].toArray());
	_greenCalibration->setAdjustment(adjustment["green"].toArray());
	_blueCalibration->setAdjustment(adjustment["blue"].toArray());
	_cyanCalibration->setAdjustment(adjustment["cyan"].toArray());
	_magentaCalibration->setAdjustment(adjustment["magenta"].toArray());
	_yellowCalibration->setAdjustment(adjustment["yellow"].toArray());
	_whiteCalibration->setAdjustment(adjustment["white"].toArray());

	if (adjustment.contains("gammaRed") || adjustment.contains("gammaGreen") || adjustment.contains("gammaBlue"))
		_colorspaceCalibration->setGamma(adjustment["gammaRed"].toDouble(_colorspaceCalibration->getGammaR()),
			adjustment["gammaGreen"].toDouble(_colorspaceCalibration->getGammaG()),
			adjustment["gammaBlue"].toDouble(_colorspaceCalibration->getGammaB()));

	_colorspaceCalibration->setBacklightThreshold(adjustment["backlightThreshold"].toDouble(-1));
	if (adjustment.contains("backlightColored"))
		_colorspaceCalibration->setBacklightColored(adjustment["backlightColored"].toBool(_colorspaceCalibration->getBacklightColored()));
	_colorspaceCalibration->setBrightness(adjustment["brightness"].toInt(-1));
	_colorspaceCalibration->setBrightnessCompensation(adjustment["brightnessCompensation"].toInt(-1));
	_colorspaceCalibration->setSaturationGain(adjustment["saturationGain"].toDouble(-1));
	_colorspaceCalibration->setLuminanceGain(adjustment["luminanceGain"].toDouble(-1));
}
