/* InfiniteRgbInterpolator.cpp
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

#include <infinite-color-engine/InfiniteRgbInterpolator.h>
#include <infinite-color-engine/ColorSpace.h>

using namespace linalg::aliases;

InfiniteRgbInterpolator::InfiniteRgbInterpolator() = default;

void InfiniteRgbInterpolator::setTransitionDuration(float durationMs)
{
	_initialDuration = std::max(1.0f, durationMs);
	_deltaMs = _initialDuration;
}

void InfiniteRgbInterpolator::resetToColors(std::vector<float3> colors)
{
	_currentColorsRGB = std::move(colors);
	_targetColorsRGB = _currentColorsRGB;
	_isAnimationComplete = true;
}

void InfiniteRgbInterpolator::setTargetColors(std::vector<float3> new_rgb_targets, float startTimeMs)
{
	if (!_isAnimationComplete)
	{
		updateCurrentColors(startTimeMs);
	}

	_targetColorsRGB = std::move(new_rgb_targets);
	size_t new_size = _targetColorsRGB.size();

	if (_currentColorsRGB.size() != new_size)
	{
		_currentColorsRGB.resize(new_size, { 0.0f, 0.0f, 0.0f });
	}

	_startColorsRGB = _currentColorsRGB;

	float new_distance = 0.0f;
	for (size_t i = 0; i < new_size; ++i)
	{
		new_distance += linalg::distance(_startColorsRGB[i], _targetColorsRGB[i]);
	}

	if (_isAnimationComplete)
	{
		_deltaMs = _initialDuration;
		_initialDistance = new_distance;
	}
	else
	{
		if (_initialDistance > 1e-5f)
		{
			float duration_scale = new_distance / _initialDistance;
			_deltaMs = _initialDuration * duration_scale;
		}
		else
		{
			_deltaMs = _initialDuration;
		}
	}

	if (_smoothingFactor > 0.0f)
	{
		float durationMultiplier = 1.0f + 2.0f * _smoothingFactor;

		_deltaMs *= durationMultiplier;
	}

	_deltaMs = std::max(10.0f, _deltaMs);

	_startTimeMs = startTimeMs;
	_isAnimationComplete = false;

}

void InfiniteRgbInterpolator::setSmoothingFactor(float factor)
{
	_smoothingFactor = std::max(0.0f, std::min(factor, 1.0f));
}

void InfiniteRgbInterpolator::updateCurrentColors(float currentTimeMs)
{
	if (!_isAnimationComplete)
	{		
		float elapsed = currentTimeMs - _startTimeMs;
		float t = (_deltaMs > 0.0f) ? elapsed / _deltaMs : 1.0f;
		size_t num_colors = _targetColorsRGB.size();

		_isAnimationComplete = (t >= 1.0f);

		if (_smoothingFactor <= 0.0f)
		{
			if (t >= 1.0f)
			{
				_currentColorsRGB = _targetColorsRGB;
			}
			else
			{
				t = std::max(0.0f, t);
				for (size_t i = 0; i < num_colors; ++i) {
					_currentColorsRGB[i] = linalg::lerp(_startColorsRGB[i], _targetColorsRGB[i], t);
				}
			}
		}
		else
		{

			float t_clamped = std::max(0.0f, std::min(t, 1.0f));
			float effective_smoothing = linalg::lerp(_smoothingFactor, 1.0f, t_clamped);

			for (size_t i = 0; i < num_colors; ++i)
			{
				float3 idealPosition = linalg::lerp(_startColorsRGB[i], _targetColorsRGB[i], t_clamped);
				_currentColorsRGB[i] = linalg::lerp(_currentColorsRGB[i], idealPosition, effective_smoothing);
			}

			if (_isAnimationComplete)
			{
				_currentColorsRGB = _targetColorsRGB;
			}
		}
	}
}

SharedOutputColors InfiniteRgbInterpolator::getCurrentColors() const
{
	return std::make_shared<std::vector<linalg::vec<float, 3>>>(_currentColorsRGB);
}

void InfiniteRgbInterpolator::test()
{
	using TestTuple = std::tuple<std::string, float3, float3, float3, float3>;

	auto run_test_lambda = [](InfiniteRgbInterpolator& interpolator, const TestTuple& test, bool use_smoothing) {
		const auto& name = std::get<0>(test);
		const auto& start_A = std::get<1>(test);
		const auto& interrupt_C = std::get<2>(test);
		const auto& interrupt_D = std::get<3>(test);
		const auto& final_B = std::get<4>(test);

		std::string test_title = name + (use_smoothing ? " (z wygładzaniem)" : " (liniowo)");
		std::cout << "\n--- TEST: " << test_title << " ---\n";
		std::cout << "--------------------------------------------------\n";

		if (use_smoothing)
		{
			interpolator.setSmoothingFactor(0.20f);
		}
		else
		{
			interpolator.setSmoothingFactor(0.0f);
		}

		interpolator.resetToColors({ start_A });

		float base_duration = 150.0f;
		float time_offset = 0.0f;

		interpolator.setTransitionDuration(base_duration);
		interpolator.setTargetColors({ interrupt_C }, time_offset);

		bool retargeted_to_D = false;
		bool retargeted_to_B = false;

		for (float time_ms = 0; time_ms <= 1000; time_ms += 25) {
			if (time_ms >= 150 && !retargeted_to_D) {
				std::cout << "--- ZMIANA CELU 1 @ " << time_ms << "ms (-> D) ---\n";
				interpolator.setTargetColors({ interrupt_D }, time_ms);
				retargeted_to_D = true;
			}
			if (time_ms >= 300 && !retargeted_to_B) {
				std::cout << "--- ZMIANA CELU 2 @ " << time_ms << "ms (-> B) ---\n";
				interpolator.setTargetColors({ final_B }, time_ms);
				retargeted_to_B = true;
			}

			interpolator.updateCurrentColors(time_ms);

			auto temp_color = interpolator.getCurrentColors();
			const auto& current_color = *(temp_color);

			if (current_color.empty()) continue;

			std::cout << std::setw(4) << static_cast<int>(time_ms) << "ms: { R: "
				<< std::fixed << std::setprecision(3) << current_color[0].x
				<< ", G: " << current_color[0].y
				<< ", B: " << current_color[0].z << " }\n";
		}
		std::cout << "--------------------------------------------------\n";
		};

	InfiniteRgbInterpolator interpolator;

	std::vector<TestTuple> test_cases = {
		{"Czarny -> Ciemny Nieb. -> Jasny Żółty -> Biały", {0,0,0}, {0.1f, 0.1f, 0.4f}, {0.9f, 0.9f, 0.2f}, {1,1,1}},
		{"Czerwony -> Żółty -> Cyjan -> Niebieski", {1,0,0}, {1,1,0}, {0,1,1}, {0,0,1}},
		{"Niebieski -> Zielony -> Czerwony -> Złoty", {0,0,1}, {0,1,0}, {1,0,0}, {1,1,0}},
		{"Magenta -> Cyjan -> Żółty -> Zielony", {1,0,1}, {0,1,1}, {1,1,0}, {0,1,0}},
		{"Cyjan -> Żółty -> Magenta -> Czerwony", {0,1,1}, {1,1,0}, {1,0,1}, {1,0,0}},
		{"Niebieski (50%) -> Czerwony -> Zielony -> Złoty (50%)", {0.25f, 0.25f, 0.75f}, {1,0,0}, {0,1,0}, {0.75f, 0.75f, 0.25f}},
		{"Magenta (50%) -> Niebieski -> Czerwony -> Zielony (50%)", {0.75f, 0.25f, 0.75f}, {0,0,1}, {1,0,0}, {0.25f, 0.75f, 0.25f}},
		{"Niebieski (25%) -> Szary -> Jasny Szary -> Złoty (25%)", {0.0f, 0.0f, 0.25f}, {0.2f,0.2f,0.2f}, {0.8f,0.8f,0.8f}, {0.25f, 0.25f, 0.0f}},
		{"Magenta (25%) -> Ciemny Cyjan -> Jasny Żółty -> Zielony (25%)", {0.25f, 0.0f, 0.25f}, {0.0f,0.2f,0.2f}, {0.8f,0.8f,0.0f}, {0.0f, 0.25f, 0.0f}},
		{"Cyjan (25%) -> Fiolet -> Pomarańcz -> Czerwony (25%)", {0.0f, 0.25f, 0.25f}, {0.5f,0.0f,0.5f}, {1.0f,0.5f,0.0f}, {0.25f, 0.0f, 0.0f}},
		{"Ciemny Nieb. -> Biały -> Czarny -> Jasny Żółty", {0,0,0.5f}, {1,1,1}, {0,0,0}, {1,1,0.8f}},
		{"Jasny Czerw. -> Ciemny Nieb. -> Jasny Zielony -> Ciemny Cyjan", {1,0,0}, {0,0,0.2f}, {0.5f,1.0f,0.5f}, {0.1f, 0.4f, 0.4f}},
		{"Zielony -> Fiolet -> Cyjan -> Czerwony", {0,1,0}, {0.5f,0,1.0f}, {0,1,1}, {1,0,0}},
		{"Zielony -> Żółty -> Niebieski -> Magenta", {0,1,0}, {1,1,0}, {0,0,1}, {1,0,1}},
		{"Biały -> Czerwony -> Zielony -> Niebieski", {1,1,1}, {1,0,0}, {0,1,0}, {0,0,1}},
		{"Zielony -> Cyjan -> Czerwony -> Niebieski", {0,1,0}, {0,1,1}, {1,0,0}, {0,0,1}}
	};

	for (size_t i = 0; i < test_cases.size(); ++i)
	{
		bool use_smoothing = (i % 2 == 1);
		run_test_lambda(interpolator, test_cases[i], use_smoothing);
	}
}
