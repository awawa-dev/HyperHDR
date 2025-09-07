/* InfiniteStepperInterpolator.cpp
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

#include <infinite-color-engine/InfiniteStepperInterpolator.h>
#include <infinite-color-engine/ColorSpace.h>

using namespace linalg::aliases;

InfiniteStepperInterpolator::InfiniteStepperInterpolator() = default;

void InfiniteStepperInterpolator::setTransitionDuration(float durationMs)
{
	_initialDuration = std::max(1.0f, durationMs);
}

void InfiniteStepperInterpolator::resetToColors(std::vector<float3>&& colors, float startTimeMs)
{
	_currentColorsRGB.clear();
	setTargetColors(std::move(colors), startTimeMs);
}

void InfiniteStepperInterpolator::setTargetColors(std::vector<float3>&& new_rgb_targets, float startTimeMs)
{
	if (_currentColorsRGB.size() != new_rgb_targets.size())
	{
		_previousFrameTimeMs = startTimeMs;
		_currentColorsRGB = std::move(new_rgb_targets);
		_targetColorsRGB = _currentColorsRGB;
		_isAnimationComplete = true;
	}
	else
	{
		_targetColorsRGB = std::move(new_rgb_targets);
		_isAnimationComplete = false;
	}
}

void InfiniteStepperInterpolator::updateCurrentColors(float currentTimeMs)
{
	if (_isAnimationComplete)
	{
		return;
	}

	float dt = currentTimeMs - _previousFrameTimeMs;
	if (dt <= 0.0f)
	{
		return;
	}
	_previousFrameTimeMs = currentTimeMs;

	// prosty filtr dolnoprzepustowy (stała czasowa = _initialDuration)
	float alpha = dt / _initialDuration;
	if (alpha > 1.0f) alpha = 1.0f;

	for (auto current_it = _currentColorsRGB.begin(), target_it = _targetColorsRGB.begin();
		current_it != _currentColorsRGB.end() && target_it != _targetColorsRGB.end();
		++current_it, ++target_it)
	{
		*current_it = linalg::lerp(*current_it, *target_it, alpha);
	}

	const float FINISH_COMPONENT_THRESHOLD = 1.0f / 255.0f;
	if (std::ranges::equal(
		_currentColorsRGB,
		_targetColorsRGB,
		[&](const float3& cur, const float3& target) {
			return linalg::maxelem(linalg::abs(target - cur)) <= FINISH_COMPONENT_THRESHOLD;
		}
	))
	{
		_currentColorsRGB = _targetColorsRGB;
		_isAnimationComplete = true;
	}
}

SharedOutputColors InfiniteStepperInterpolator::getCurrentColors() const
{
	return std::make_shared<std::vector<linalg::vec<float, 3>>>(_currentColorsRGB);
}

void InfiniteStepperInterpolator::test()
{
	using TestTuple = std::tuple<std::string, float3, float3, float3, float3>;

	auto run_test_lambda = [](InfiniteStepperInterpolator& interpolator, const TestTuple& test) {
		const auto& name = std::get<0>(test);
		const auto& start_A = std::get<1>(test);
		const auto& interrupt_C = std::get<2>(test);
		const auto& interrupt_D = std::get<3>(test);
		const auto& final_B = std::get<4>(test);

		std::cout << "\n--- TEST: " << name << " ---\n";
		std::cout << "--------------------------------------------------\n";		

		float base_duration = 150.0f;
		float time_offset = 0.0f;

		interpolator.resetToColors({ start_A }, 0);
		interpolator.setTransitionDuration(base_duration);
		interpolator.setTargetColors({ interrupt_C }, time_offset);

		bool retargeted_to_D = false;
		bool retargeted_to_B = false;

		for (float time_ms = 0; time_ms <= 300; time_ms += 10)
		{
			if (time_ms >= 80 && !retargeted_to_D)
			{
				std::cout << "--- ZMIANA CELU 1 @ " << time_ms << "ms (-> D) ---\n";
				interpolator.setTargetColors({ interrupt_D }, time_ms);
				retargeted_to_D = true;
			}
			if (time_ms >= 130 && !retargeted_to_B)
			{
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

	InfiniteStepperInterpolator interpolator;

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
		run_test_lambda(interpolator, test_cases[i]);
	}
}
