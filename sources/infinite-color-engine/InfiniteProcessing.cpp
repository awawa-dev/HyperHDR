/* InfiniteProcessing.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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
	#include <QTimer>
	#include <QThread>	

	#include <cmath>
	#include <algorithm>
	#include <chrono>
	#include <iomanip>
	#include <iostream>
	#include <limits>
	#include <tuple>
	#include <iomanip>
	#include <vector>
	#include <iostream>
	#include <iomanip>
	#include <tuple>
	#include <cassert>
	#include <cmath>
	#include <cstdint>
#endif


#include <QMutexLocker>

#include <infinite-color-engine/InfiniteSmoothing.h>
#include <infinite-color-engine/InfiniteProcessing.h>
#include <base/HyperHdrInstance.h>
#include <infinite-color-engine/ColorSpace.h>
#include <image/ColorRgb.h>

using namespace linalg::aliases;

namespace
{	
	float3 to_float3(const ColorRgb& color)
	{
		return {
			color.red / 255.0f,
			color.green / 255.0f,
			color.blue / 255.0f
		};
	}

	ColorRgb toColorRgb(const QJsonArray& array)
	{
		return ColorRgb(
			array[0].toInt(0),
			array[1].toInt(0),
			array[2].toInt(0)
		);
	}
}

InfiniteProcessing::InfiniteProcessing() :
	_enabled(true),
	_colorOrder(LedString::ColorOrder::ORDER_RGB),
	_temperature_enabled(false),
	_temperature_tint{},
	_brightness(1.0f),
	_saturation(1.0f),
	_scaleOutput(1.0f),
	_powerLimit(1.0f),
	_minimalBacklight(1.0f / 255.0f),
	_coloredBacklight(true),	
	_log(nullptr),
	_user_gamma_lut{}
{
	generateUserGamma(1.0f);
}

InfiniteProcessing::InfiniteProcessing(const QJsonDocument& config, Logger* log) :
	InfiniteProcessing()
{
	_log = log;
	handleSignalInstanceSettingsChanged(settings::type::COLOR, config);
}

void InfiniteProcessing::setProcessingEnabled(bool enabled)
{
	if (_enabled != enabled)
	{
		_enabled = enabled;
		if (_log)
		{
			Info(_log, "The InfiniteProcessing is set to: %", ((_enabled) ? "enabled" : "disabled"));
		}
	}
}

void InfiniteProcessing::updateCurrentConfig(const QJsonObject& config)
{
	TemperaturePreset tempPreset = TemperaturePreset::Disabled;
	auto configTemp = config["temperatureSetting"].toString("disabled");
	if (configTemp == "cold")
		tempPreset = TemperaturePreset::Cold;
	else if (configTemp == "neutral")
		tempPreset = TemperaturePreset::Neutral;
	else if (configTemp == "warm")
		tempPreset = TemperaturePreset::Warm;
	else if (configTemp == "custom")
		tempPreset = TemperaturePreset::Custom;
	float redTemp = config["temperatureRed"].toDouble(1.0);
	float greenTemp = config["temperatureGreen"].toDouble(1.0);
	float blueTemp = config["temperatureBlue"].toDouble(1.0);
	setTemperature(tempPreset, { redTemp , greenTemp, blueTemp });

	bool usePrimariesOnly = config["classic_config"].toBool(true);
	auto black = toColorRgb(config["black"].toArray());
	auto red= toColorRgb(config["red"].toArray());
	auto green = toColorRgb(config["green"].toArray());
	auto blue = toColorRgb(config["blue"].toArray());
	auto cyan = toColorRgb(config["cyan"].toArray());
	auto magenta = toColorRgb(config["magenta"].toArray());
	auto yellow = toColorRgb(config["yellow"].toArray());
	auto white = toColorRgb(config["white"].toArray());
	generateColorspace(
		usePrimariesOnly, red, green, blue,
		cyan, magenta, yellow,
		white, black);	
	
	float brightness = config["luminanceGain"].toDouble(1);
	float saturation = config["saturationGain"].toDouble(1);	
	setBrightnessAndSaturation(brightness, saturation);

	float gamma = config["gamma"].toDouble(1.5);
	generateUserGamma(gamma);

	float scaleOutput = config["scaleOutput"].toDouble(1);
	setScaleOutput(scaleOutput);

	float minimalBacklight = config["backlightThreshold"].toDouble(0);
	auto coloredBacklight = config["backlightColored"].toBool(true);
	setMinimalBacklight(minimalBacklight, coloredBacklight);

	float powerLimit = config["powerLimit"].toDouble(1);
	setPowerLimit(powerLimit);

	_currentConfig = config;
}

QJsonArray InfiniteProcessing::getCurrentConfig()
{
	QJsonArray array;
	array.push_back(_currentConfig);
	return array;
}

void InfiniteProcessing::handleSignalInstanceSettingsChanged(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::DEVICE)
	{
		_colorOrder = LedString::stringToColorOrder(config["colorOrder"].toString("rgb"));
	}
	else if (type == settings::type::COLOR)
	{
		updateCurrentConfig(config["channelAdjustment"].toArray().first().toObject());
	}
}

void InfiniteProcessing::applyyAllProcessingSteps(std::vector<linalg::vec<float, 3>>& linearRgbColors)
{
	if (_enabled)
	{
		auto colorCalibration = getColorspaceCalibrationSnapshot();
		for (auto it = linearRgbColors.begin(); it != linearRgbColors.end(); ++it)
		{
			auto& color = *it;
			applyTemperature(color);
			calibrateColorInColorspace(colorCalibration, color);			
			color = srgbLinearToNonlinear(color);			
			applyUserGamma(color);
			applyBrightnessAndSaturation(color);
			applyScaleOutput(color);
			applyMinimalBacklight(color);
		}

		applyPowerLimit(linearRgbColors);
	}
	else
	{
		for (auto it = linearRgbColors.begin(); it != linearRgbColors.end(); ++it)
		{
			auto& color = *it;
			color = srgbLinearToNonlinear(color);
		}
	}

	if (_colorOrder != LedString::ColorOrder::ORDER_RGB)
	{
		for (auto it = linearRgbColors.begin(); it != linearRgbColors.end(); ++it)
		{
			auto& color = *it;
			switch (_colorOrder)
			{
				case LedString::ColorOrder::ORDER_RGB:
					break;
				case LedString::ColorOrder::ORDER_BGR:
					std::swap(color.x, color.z);
					break;
				case LedString::ColorOrder::ORDER_RBG:
					std::swap(color.y, color.z);
					break;
				case LedString::ColorOrder::ORDER_GRB:
					std::swap(color.x, color.y);
					break;
				case LedString::ColorOrder::ORDER_GBR:
					std::swap(color.x, color.y);
					std::swap(color.y, color.z);
					break;
				case LedString::ColorOrder::ORDER_BRG:
					std::swap(color.x, color.z);
					std::swap(color.y, color.z);
					break;
			}
		}
	}
}

void InfiniteProcessing::setMinimalBacklight(float minimalLevel, bool coloreBacklight)
{
	if (minimalLevel >= 1.0f)
	{
		if (_log)
		{
			Error(_log, "Minimal backlight level is way to high: %f, resetting to 0.0039", minimalLevel);
		}
		minimalLevel = 0.0039f;
	}
	_minimalBacklight = minimalLevel;
	_coloredBacklight = coloreBacklight;
	if (_log)
	{
		Info(_log, "--- MINIMAL BACKLIGHT  (ENABLED: %s) ---", ((_minimalBacklight > 0.0f) ? "true" : "false"));
		Info(_log, "MINIMAL LEVEL: %f", _minimalBacklight);
		Info(_log, "COLORED:       %s", ((_coloredBacklight)?"true" : "false"));
	}
}

void InfiniteProcessing::applyMinimalBacklight(linalg::vec<float, 3>& color) const
{
	if (_minimalBacklight > 0.0f)
	{
		if (_coloredBacklight)
		{
			color = {
				std::max(color.x, _minimalBacklight),
				std::max(color.y, _minimalBacklight),
				std::max(color.z, _minimalBacklight)
			};
		}
		else
		{
			float avVal = (color.x + color.y + color.z) / 3.0f;
			if (avVal < _minimalBacklight)
			{
				color = float3{ _minimalBacklight };
			}
		}
	}
}

void InfiniteProcessing::generateColorspace(
	bool use_primaries_only,
	ColorRgb target_red, ColorRgb target_green, ColorRgb target_blue,
	ColorRgb target_cyan, ColorRgb target_magenta, ColorRgb target_yellow,
	ColorRgb target_white, ColorRgb target_black)
{
	std::vector<linalg::vec<float, 3>> lut;
	linalg::mat<float, 3, 3> primary_calib_matrix;
	CalibrationMode mode = CalibrationMode::None;

	auto printfInfo = [this](bool is_identity, bool use_primaries_only,
		ColorRgb target_red, ColorRgb target_green, ColorRgb target_blue,
		ColorRgb target_cyan, ColorRgb target_magenta, ColorRgb target_yellow,
		ColorRgb target_white, ColorRgb target_black)
		{
			if (_log)
			{
				Info(_log, "--- LED CALIBRATION (ENABLED: %s) ---", ((!is_identity) ? "true" : "false"));
				Info(_log, "CLASSIC: %s", ((use_primaries_only) ? "true" : "false"));
				Info(_log, "RED:     %s", std::string(target_red).c_str());
				Info(_log, "GREEN:   %s", std::string(target_green).c_str());
				Info(_log, "BLUE:    %s", std::string(target_blue).c_str());
				Info(_log, "CYAN:    %s", std::string(target_cyan).c_str());
				Info(_log, "MAGENTA: %s", std::string(target_magenta).c_str());
				Info(_log, "YELLOW:  %s", std::string(target_yellow).c_str());
				Info(_log, "WHITE:   %s", std::string(target_white).c_str());
				Info(_log, "BLACK:   %s", std::string(target_black).c_str());
			}
		};	
	if (use_primaries_only)
	{
		bool is_identity = (target_red == ColorRgb{ 255,0,0 } &&
			target_green == ColorRgb{ 0,255,0 } &&
			target_blue == ColorRgb{ 0,0,255 });

		if (_log)
		{
			printfInfo(is_identity, use_primaries_only, target_red, target_green, target_blue, target_cyan, target_magenta, target_yellow, target_white, target_black);
		}

		if (is_identity) return;

		mode = CalibrationMode::Matrix;
		primary_calib_matrix = {
			to_float3(target_red),
			to_float3(target_green),
			to_float3(target_blue)
		};
		primary_calib_matrix = linalg::transpose(primary_calib_matrix);
	}
	else
	{
		const std::array<ColorRgb, 8> ideal_colors = {
			ColorRgb{255,0,0}, ColorRgb{0,255,0}, ColorRgb{0,0,255},
			ColorRgb{0,255,255}, ColorRgb{255,0,255}, ColorRgb{255,255,0},
			ColorRgb{255,255,255}, ColorRgb{0,0,0}
		};

		const std::array<ColorRgb, 8> target_colors = {
			target_red, target_green, target_blue,
			target_cyan, target_magenta, target_yellow,
			target_white, target_black
		};

		bool is_identity = true;
		for (size_t i = 0; i < ideal_colors.size(); ++i)
		{
			if (!(target_colors[i] == ideal_colors[i])) {
				is_identity = false;
				break;
			}
		}

		if (_log)
		{
			printfInfo(is_identity, use_primaries_only, target_red, target_green, target_blue, target_cyan, target_magenta, target_yellow, target_white, target_black);
		}

		if (is_identity) return;

		mode = CalibrationMode::Lut;
		lut.resize(LUT_DIMENSION * LUT_DIMENSION * LUT_DIMENSION);

		const auto r = to_float3(target_red);
		const auto g = to_float3(target_green);
		const auto b = to_float3(target_blue);
		const auto c = to_float3(target_cyan);
		const auto m = to_float3(target_magenta);
		const auto y = to_float3(target_yellow);
		const auto w = to_float3(target_white);
		const auto k = to_float3(target_black);

		for (int iz = 0; iz < LUT_DIMENSION; ++iz)
		{
			float tz = static_cast<float>(iz) / (LUT_DIMENSION - 1);
			auto k_r = lerp(k, r, tz);
			auto g_y = lerp(g, y, tz);
			auto b_m = lerp(b, m, tz);
			auto c_w = lerp(c, w, tz);

			for (int iy = 0; iy < LUT_DIMENSION; ++iy)
			{
				float ty = static_cast<float>(iy) / (LUT_DIMENSION - 1);
				auto kr_gy = lerp(k_r, g_y, ty);
				auto bm_cw = lerp(b_m, c_w, ty);

				for (int ix = 0; ix < LUT_DIMENSION; ++ix)
				{
					float tx = static_cast<float>(ix) / (LUT_DIMENSION - 1);
					size_t index = ix + iy * LUT_DIMENSION + iz * LUT_DIMENSION * LUT_DIMENSION;
					lut[index] = lerp(kr_gy, bm_cw, tx);
				}
			}
		}
	}

	_colorCalibrationData.update(std::move(lut), std::move(primary_calib_matrix), mode);
}

void InfiniteProcessing::calibrateColorInColorspace(const std::shared_ptr<const CalibrationSnapshot>& calibrationSnapshot, linalg::vec<float, 3>& color) const
{
	switch (calibrationSnapshot->mode)
	{
		case CalibrationMode::None:
			return;

		case CalibrationMode::Matrix:
		{
			color = linalg::mul(calibrationSnapshot->primary_calib_matrix, color);
			color.x = std::max(0.0f, std::min(color.x, 1.0f));
			color.y = std::max(0.0f, std::min(color.y, 1.0f));
			color.z = std::max(0.0f, std::min(color.z, 1.0f));
			return;
		}

		case CalibrationMode::Lut:
		{
			float r = color.x * (LUT_DIMENSION - 1);
			float g = color.y * (LUT_DIMENSION - 1);
			float b = color.z * (LUT_DIMENSION - 1);

			int r0 = static_cast<int>(r);
			int g0 = static_cast<int>(g);
			int b0 = static_cast<int>(b);

			int r1 = std::min(r0 + 1, LUT_DIMENSION - 1);
			int g1 = std::min(g0 + 1, LUT_DIMENSION - 1);
			int b1 = std::min(b0 + 1, LUT_DIMENSION - 1);

			float r_frac = r - r0;
			float g_frac = g - g0;
			float b_frac = b - b0;

			const size_t i000 = b0 + g0 * LUT_DIMENSION + r0 * LUT_DIMENSION * LUT_DIMENSION;
			const size_t i100 = b1 + g0 * LUT_DIMENSION + r0 * LUT_DIMENSION * LUT_DIMENSION;
			const size_t i010 = b0 + g1 * LUT_DIMENSION + r0 * LUT_DIMENSION * LUT_DIMENSION;
			const size_t i110 = b1 + g1 * LUT_DIMENSION + r0 * LUT_DIMENSION * LUT_DIMENSION;
			const size_t i001 = b0 + g0 * LUT_DIMENSION + r1 * LUT_DIMENSION * LUT_DIMENSION;
			const size_t i101 = b1 + g0 * LUT_DIMENSION + r1 * LUT_DIMENSION * LUT_DIMENSION;
			const size_t i011 = b0 + g1 * LUT_DIMENSION + r1 * LUT_DIMENSION * LUT_DIMENSION;
			const size_t i111 = b1 + g1 * LUT_DIMENSION + r1 * LUT_DIMENSION * LUT_DIMENSION;

			auto c00 = linalg::lerp(calibrationSnapshot->lut[i000], calibrationSnapshot->lut[i100], r_frac);
			auto c10 = linalg::lerp(calibrationSnapshot->lut[i010], calibrationSnapshot->lut[i110], r_frac);
			auto c01 = linalg::lerp(calibrationSnapshot->lut[i001], calibrationSnapshot->lut[i101], r_frac);
			auto c11 = linalg::lerp(calibrationSnapshot->lut[i011], calibrationSnapshot->lut[i111], r_frac);

			auto c0 = linalg::lerp(c00, c10, g_frac);
			auto c1 = linalg::lerp(c01, c11, g_frac);

			color = linalg::lerp(c0, c1, b_frac);
			return;
		}
	}
}

void InfiniteProcessing::generateUserGamma(float gamma)
{
	if (_log)
	{
		Info(_log, "--- USER GAMMA ---");
		Info(_log, "GAMMA:   %.2f", gamma);
	}

	for (int i = 0; i < LUT_SIZE; ++i)
	{
		_user_gamma_lut[i] = std::pow(static_cast<float>(i) / (LUT_SIZE - 1), gamma);
	}
}

void InfiniteProcessing::applyUserGamma(linalg::vec<float, 3>& color) const
{
	for (int i = 0; i < 3; ++i)
	{		
		float pos = color[i] * (LUT_SIZE - 1);
		int index0 = static_cast<int>(pos);
		int index1 = std::min(index0 + 1, LUT_SIZE - 1);

		float fraction = pos - index0;

		color[i] = linalg::lerp(_user_gamma_lut[index0], _user_gamma_lut[index1], fraction);
	}
}

void InfiniteProcessing::setTemperature(TemperaturePreset preset, linalg::vec<float, 3>  custom_tint)
{
	_temperature_enabled = true;
	switch (preset)
	{
		case TemperaturePreset::Warm:
			_temperature_tint = { 1.0f, 0.93f, 0.85f };
			break;
		case TemperaturePreset::Neutral:
			_temperature_tint = { 1.0f, 1.0f, 1.0f };
			break;
		case TemperaturePreset::Cold:
			_temperature_tint = { 0.9f, 0.95f, 1.0f };
			break;
		case TemperaturePreset::Custom:
			_temperature_tint = custom_tint;
			break;
		case TemperaturePreset::Disabled:
		default:
			_temperature_enabled = false;
			break;
	}

	if (_log)
	{
		Info(_log, "--- TEMPERATURE (ENABLED: %s) ---", ((_temperature_enabled) ? "true" : "false"));
		Info(_log, "RED:     %0.3f", _temperature_tint.x);
		Info(_log, "GREEN:   %0.3f", _temperature_tint.y);
		Info(_log, "BLUE:    %0.3f", _temperature_tint.z);
	}
}

void InfiniteProcessing::applyTemperature(linalg::vec<float, 3>& color) const
{
	if (!_temperature_enabled)
	{
		return;
	}
	color = color * _temperature_tint;
}

void InfiniteProcessing::setBrightnessAndSaturation(float brightness, float saturation)
{
	_brightness = std::max(0.0f, brightness);
	_saturation = std::max(0.0f, saturation);

	if (_log)
	{
		Info(_log, "--- HSL CORRECTION (ENABLED: %s) ---", ((std::abs(_brightness - 1.0f) < 1e-5 && std::abs(_saturation - 1.0f) < 1e-5) ? "false" : "true"));
		Info(_log, "BRIGHTNESS: %0.3f", _brightness);
		Info(_log, "SATURATION: %0.3f", _saturation);
	}
}

void InfiniteProcessing::applyBrightnessAndSaturation(linalg::vec<float, 3>& color) const
{
	if (std::abs(_brightness - 1.0f) < 1e-5 && std::abs(_saturation - 1.0f) < 1e-5)
	{
		return;
	}

	linalg::vec<float, 3> hsv = ColorSpaceMath::rgb2hsv(color);

	hsv.z *= _brightness;
	hsv.y *= _saturation;

	hsv.z = std::min(hsv.z, 1.0f);
	hsv.y = std::min(hsv.y, 1.0f);

	color = ColorSpaceMath::hsv2rgb(hsv);

	color.x = std::max(0.0f, std::min(color.x, 1.0f));
	color.y = std::max(0.0f, std::min(color.y, 1.0f));
	color.z = std::max(0.0f, std::min(color.z, 1.0f));
}


void InfiniteProcessing::setScaleOutput(float scaleOutput)
{
	_scaleOutput = std::clamp(scaleOutput, 0.0f, 1.0f);

	if (_log)
	{
		Info(_log, "--- SCALE OUTPUT (ENABLED: %s) ---", ((_scaleOutput >= 1.f) ? "false" : "true"));
		Info(_log, "SCALE:   %0.3f", _scaleOutput);
	}
}

void InfiniteProcessing::applyScaleOutput(linalg::vec<float, 3>& color) const
{
	if (_scaleOutput >= 1.0f)
		return;

	color *= _scaleOutput;
}


void InfiniteProcessing::setPowerLimit(float powerLimit)
{
	_powerLimit = std::clamp(powerLimit, 0.0f, 1.0f);

	if (_log)
	{
		Info(_log, "--- POWER LIMIT (ENABLED: %s) ---", ((_powerLimit >= 1.f) ? "false" : "true"));
		Info(_log, "LIMIT:   %0.3f", _powerLimit);
	}
}

void InfiniteProcessing::applyPowerLimit(std::vector<linalg::vec<float, 3>>& nonlinearRgbColors) const
{
	if (_powerLimit >= 1.0f)
		return;

	float totalPower = 0.0f;
	for (auto it = nonlinearRgbColors.begin(); it != nonlinearRgbColors.end(); ++it)
		totalPower += linalg::sum(*it);

	const float allowedPower = (nonlinearRgbColors.size() * 3.f) * _powerLimit;

	if (totalPower > 0.0f && allowedPower < totalPower)
	{
		const float scale = allowedPower / totalPower;
		for (auto it = nonlinearRgbColors.begin(); it != nonlinearRgbColors.end(); ++it)
			*it *= scale;
	}
}

std::shared_ptr<const CalibrationSnapshot> InfiniteProcessing::getColorspaceCalibrationSnapshot()
{
	return _colorCalibrationData.getSnapshot();
}

void InfiniteProcessing::test()
{
	// --- Funkcje pomocnicze dla testów ---
	auto print_vec = [](const linalg::vec<float, 3>& vec) -> std::string {
		std::stringstream ss;
		ss << std::fixed << std::setprecision(3)
			<< "{ R:" << vec.x << " G:" << vec.y << " B:" << vec.z << " }";
		return ss.str();
		};

	auto check_equal = [](const linalg::vec<float, 3>& a, const linalg::vec<float, 3>& b) -> bool {
		constexpr float epsilon = 0.008f;
		return (std::abs(a.x - b.x) < epsilon) &&
			(std::abs(a.y - b.y) < epsilon) &&
			(std::abs(a.z - b.z) < epsilon);
		};

	auto run_test = [&](const std::string& test_name,
		const linalg::vec<float, 3>& input,
		const std::function<void(InfiniteProcessing&)>& setup_func,
		const std::function<linalg::vec<float, 3>(InfiniteProcessing&, linalg::vec<float, 3>)>& run_func,
		const linalg::vec<float, 3>& expected)
		{
			std::cout << "--- TEST: " << test_name << " ---\n";
			InfiniteProcessing processor;
			setup_func(processor);
			linalg::vec<float, 3> actual = run_func(processor, input);
			bool passed = check_equal(expected, actual);

			std::cout << "  Wejscie:    " << print_vec(input) << "\n";
			std::cout << "  Oczekiwano: " << print_vec(expected) << "\n";
			std::cout << "  Otrzymano:  " << print_vec(actual) << "\n";
			std::cout << "  Wynik:      " << (passed ? "PASS v" : "FAIL x") << "\n\n";
		};

	std::cout << "========================================\n";
	std::cout << "   Uruchamiam testy InfiniteProcessing \n";
	std::cout << "========================================\n\n";


	// ####################################################################
	// ###                    TESTY JEDNOSTKOWE                         ###
	// ####################################################################
	std::cout << "----------------------------------------\n";
	std::cout << "         TESTY JEDNOSTKOWE\n";
	std::cout << "----------------------------------------\n\n";

	run_test("sRGB -> Linear", { 0.5f, 0.5f, 0.5f },
		[](auto& ) {},
		[](auto& p, auto c) { return p.srgbNonlinearToLinear(c); },
		{ 0.214f, 0.214f, 0.214f }
	);

	run_test("Linear -> sRGB", { 0.214f, 0.214f, 0.214f },
		[](auto& ) {},
		[](auto& p, auto c) { return p.srgbLinearToNonlinear(c); },
		{ 0.5f, 0.5f, 0.5f }
	);

	run_test("User Gamma (1.5)", { 0.708f, 0.332f, 0.1f },
		[](auto& p) { p.generateUserGamma(1.5f); },
		[](auto& p, auto c) { p.applyUserGamma(c); return c; },
		{ 0.596f, 0.191f, 0.032f }
	);

	run_test("Apply Temperature (Cold)", { 0.8f, 0.5f, 0.2f },
		[](auto& p) { p.setTemperature(TemperaturePreset::Cold, {}); },
		[](auto& p, auto c) { p.applyTemperature(c); return c; },
		{ 0.720f, 0.475f, 0.200f }
	);

	run_test("Brightness & Saturation", { 1.0f, 0.5f, 0.0f },
		[](auto& p) { p.setBrightnessAndSaturation(0.8f, 1.2f); },
		[](auto& p, auto c) { p.applyBrightnessAndSaturation(c); return c; },
		{ 0.800f, 0.400f, 0.000f }
	);

	run_test("Calibrate (Matrix)", { 1.0f, 0.5f, 0.0f },
		[](auto& p) { p.generateColorspace(true, { 0,0,255 }, { 0,255,0 }, { 255,0,0 }); },
		[](auto& p, auto c) { p.calibrateColorInColorspace(p.getColorspaceCalibrationSnapshot(), c); return c; },
		{ 0.0f, 0.5f, 1.0f }
	);

	run_test("Calibrate (LUT)", { 1.0f, 0.0f, 0.0f },
		[](auto& p) { p.generateColorspace(false, { 250, 5, 5 }); },
		[](auto& p, auto c) { p.calibrateColorInColorspace(p.getColorspaceCalibrationSnapshot(), c); return c; },
		{ 0.980f, 0.020f, 0.020f }
	);

	// ####################################################################
	// ###                    TESTY INTEGRACYJNE                        ###
	// ####################################################################
	std::cout << "----------------------------------------\n";
	std::cout << "         TESTY INTEGRACYJNE (PIPELINE)\n";
	std::cout << "----------------------------------------\n\n";

	auto run_full_pipeline = [](InfiniteProcessing& p, linalg::vec<float, 3> color) {
		color = p.srgbNonlinearToLinear(color);
		p.calibrateColorInColorspace(p.getColorspaceCalibrationSnapshot(), color);
		p.applyTemperature(color);
		p.applyBrightnessAndSaturation(color);
		color = p.srgbLinearToNonlinear(color);
		p.applyUserGamma(color);
		return color;
		};

	run_test("Pipeline - Oryginalny", { 0.8f, 0.4f, 0.1f },
		[](auto& p) {
			p.generateColorspace(true, { 240, 10, 10 }, { 10, 240, 10 }, { 10, 10, 240 });
			p.setTemperature(TemperaturePreset::Warm, {});
			p.setBrightnessAndSaturation(0.8f, 1.2f);
			p.generateUserGamma(1.5f);
		},
		run_full_pipeline,
		{ 0.595f, 0.191f, 0.000f }
	);

	run_test("Pipeline - Zimny Obraz", { 0.6f, 0.6f, 0.6f },
		[](auto& p) {
			p.setTemperature(TemperaturePreset::Cold, {});
			p.setBrightnessAndSaturation(0.7f, 1.0f);
			p.generateUserGamma(1.1f);
		},
		run_full_pipeline,
		{ 0.451f, 0.464f, 0.476f }
	);

	run_test("Pipeline - Wysoki Kontrast", { 0.9f, 0.2f, 0.2f },
		[](auto& p) {
			p.generateColorspace(true, { 255, 10, 10 }, { 10, 255, 10 }, { 10, 10, 255 });
			p.setBrightnessAndSaturation(1.1f, 1.4f);
		},
		run_full_pipeline,
		{ 0.940f, 0.000f, 0.000f }
	);

	run_test("Pipeline - Brak Operacji", { 0.7f, 0.5f, 0.3f },
		[](auto& ) {},
		run_full_pipeline,
		{ 0.7f, 0.5f, 0.3f }
	);

	// ####################################################################
	// ###              DODATKOWE TESTY INTEGRACYJNE                    ###
	// ####################################################################
	std::cout << "----------------------------------------\n";
	std::cout << "         DODATKOWE TESTY (PIPELINE)\n";
	std::cout << "----------------------------------------\n\n";

	run_test("Pipeline - Inwersja Matrix", { 0.8f, 0.5f, 0.2f },
		[](auto& p) {
			p.generateColorspace(true, { 0, 255, 0 }, { 0, 0, 255 }, { 255, 0, 0 });
		},
		run_full_pipeline,
		{ 0.500f, 0.200f, 0.800f }
	);

	run_test("Pipeline - Efekt Sepii (LUT)", { 0.7f, 0.7f, 0.7f },
		[](auto& p) {
			p.generateColorspace(false,
				{ 112, 66, 20 }, { 112, 66, 20 }, { 112, 66, 20 }, {}, {}, {},
				{ 255, 240, 192 }
				);
			p.setBrightnessAndSaturation(1.0f, 0.8f);
		},
		run_full_pipeline,
		{ 0.556f, 0.492f, 0.401f }
	);

	run_test("Pipeline - Wzmocnienie Zieleni", { 0.4f, 0.8f, 0.4f },
		[](auto& p) {
			p.generateColorspace(true, { 235, 0, 0 }, { 0, 255, 0 }, { 0, 0, 235 });
			p.setTemperature(TemperaturePreset::Warm, {});
		},
		run_full_pipeline,
		{ 0.385f, 0.775f, 0.356f }
	);

	run_test("Pipeline - Pastelowe Kolory (LUT)", { 1.0f, 0.0f, 1.0f },
		[](auto& p) {
			p.generateColorspace(false,
				{ 255, 128, 128 }, { 128, 255, 128 }, { 128, 128, 255 }, {},
				{ 255, 128, 255 }
				);
			p.generateUserGamma(0.9f);
		},
		run_full_pipeline,
		{ 1.0f, 0.760f, 1.0f }
	);

	
	run_test("Pipeline - Niezalezna Weryfikacja LUT", { 0.5f, 0.5f, 0.5f },
		[](auto& p) {
			p.generateColorspace(false,
				{ 240, 50, 30 },   // target_red
				{ 10, 250, 11 },   // target_green
				{ 23, 45, 255 },   // target_blue
				{ 0,255,255 },
				{ 255,0,255 },
				{ 250, 240, 0 },   // target_yellow
				{ 255,255,255 },
				{ 1, 2, 2 }        // target_black
				);
		},
		run_full_pipeline,
		{ 0.511f, 0.549f, 0.526f }
	);

	std::cout << "========================================\n";
	std::cout << "              Koniec testów\n";
	std::cout << "========================================\n";
}

/*
LedCalibration::LedCalibration(quint8 instance, int ledNumber, const QJsonObject& colorConfig)
	: _perLedConfig(ledNumber, nullptr)
	, _log(Logger::getInstance("LED_CALIBRATION" + QString::number(instance)))
{
	int index = 0;
	const QRegularExpression overallExp("^([0-9]+(\\-[0-9]+)?)(,[ ]*([0-9]+(\\-[0-9]+)?))*$");
	const QJsonArray& adjustmentConfigArray = colorConfig["channelAdjustment"].toArray();

	for (auto item = adjustmentConfigArray.begin(); item != adjustmentConfigArray.end(); ++item, ++index)
	{
		const QJsonObject& config = (*item).toObject();
		const QString range = config["leds"].toString("").trimmed();
		std::shared_ptr<ColorCalibration> colorAdjustment = std::make_shared<ColorCalibration>(instance, config);

		_calibrationConfig.push_back(colorAdjustment);

		if (range.compare("*") == 0)
		{
			setAdjustmentForLed(index, colorAdjustment, 0, ledNumber - 1);
			continue;
		}

		if (!overallExp.match(range).hasMatch())
		{
			Warning(_log, "Unrecognized segment range configuration: %s", QSTRING_CSTR(range));
			continue;
		}

		for (const auto& segment : range.split(","))
		{
			int first = 0, second = 0;
			bool ok = false;
			if (segment.contains("-"))
			{
				QStringList ledIndices = segment.split("-");
				if (ledIndices.size() == 2)
				{
					first = ledIndices[0].toInt(&ok);
					second = (ok) ? ledIndices[1].toInt(&ok) : 0;
				}
			}
			else
			{
				first = second = segment.toInt(&ok);
			}

			if (ok)
				setAdjustmentForLed(index, colorAdjustment, first, second);
			else
				Warning(_log, "Unrecognized segment range configuration: %s", QSTRING_CSTR(segment));
		}
	}
}
*/
