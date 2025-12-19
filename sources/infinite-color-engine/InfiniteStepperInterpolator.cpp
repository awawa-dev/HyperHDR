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
	#include <vector>
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

void InfiniteStepperInterpolator::setTargetColors(std::vector<float3>&& new_rgb_targets, float startTimeMs, bool debug)
{
	if (new_rgb_targets.empty())
		return;

	const float delta = (!_isAnimationComplete) ? std::max(startTimeMs - _lastUpdate, 0.f) : 0.f;

	if (debug)
	{
		printf(" | Δ%.0f | time: %.0f\n", delta, _initialDuration);
	}

	startTimeMs -= delta;

	if (_currentColorsRGB.size() != new_rgb_targets.size() || _targetColorsRGB.size() != new_rgb_targets.size())
	{
		_lastUpdate = startTimeMs;
		_currentColorsRGB = new_rgb_targets;
		_targetColorsRGB = std::move(new_rgb_targets);
		_isAnimationComplete = true;
	}
	else
	{
		_targetColorsRGB = std::move(new_rgb_targets);
		_isAnimationComplete = false;
	}

	_startAnimationTimeMs = startTimeMs;
	_targetTime = startTimeMs + _initialDuration;
}

void InfiniteStepperInterpolator::updateCurrentColors(float currentTimeMs)
{
	if (_isAnimationComplete)
		return;

	// obliczenie czasu, analog setupAdvColor
	float deltaTime = _targetTime - currentTimeMs;
	float totalTime = _targetTime - _startAnimationTimeMs;
	float kOrg = std::min(std::max(1.0f - deltaTime / totalTime, 0.0001f), 1.0f);
	_lastUpdate = currentTimeMs;

	float4 aspectK = float4{
		std::min(std::pow(kOrg, 1.0f),   1.0f), // aspectK[0] = kMin
		std::min(std::pow(kOrg, 0.9f),   1.0f), // aspectK[1] = kMid
		std::min(std::pow(kOrg, 0.75f),  1.0f), // aspectK[2] = kAbove
		std::min(std::pow(kOrg, 0.6f),   1.0f)  // aspectK[3] = kMax
	};

	float3 limits = float3(16.0f, 32.0f, 60.0f) / 255.0f;
	// limits[0] = 16/255  => stary limitMin
	// limits[1] = 32/255  => stary limitMid
	// limits[2] = 60/255  => stary limitMax

	auto computeChannelVec = [&](float3& cur, const float3& diff) -> bool {
		const float FINISH_COMPONENT_THRESHOLD = 0.2f / 255.0f;

		float val = linalg::maxelem(linalg::abs(diff));

		if (val < FINISH_COMPONENT_THRESHOLD)
		{
			cur += diff; // cur + diff = target
			return false; // kolor już osiągnął target
		}
		else
		{
			int idx = (val < limits[0]) ? 3 :   // limitMin (16)     => używa kMax
				(val < limits[1]) ? 2 :         // limitMid (32)     => używa kAbove
				(val < limits[2]) ? 1 :         // limitMax (60)     => używa kMid
				0;                              // powyżej limitMax  => używa kMin

			for (int i = 0; i < 3; ++i)
			{
				cur[i] = std::clamp(cur[i] + aspectK[idx] * diff[i], 0.f, 1.0f);
			}
			return true;
		}
		};

	_isAnimationComplete = true;

	for (auto cur = _currentColorsRGB.begin(), tgt = _targetColorsRGB.begin();
		cur != _currentColorsRGB.end() && tgt != _targetColorsRGB.end();
		++cur, ++tgt)
	{
		if (computeChannelVec(*cur, *tgt - *cur)) // jeśli true => kolor jeszcze nie doszedł do target
		{
			_isAnimationComplete = false;
		}
	}
}


SharedOutputColors InfiniteStepperInterpolator::getCurrentColors()
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
