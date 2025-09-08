/* InfiniteHybridInterpolator.cpp
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

#include <infinite-color-engine/InfiniteHybridInterpolator.h>
#include <infinite-color-engine/ColorSpace.h>

using namespace linalg::aliases;

InfiniteHybridInterpolator::InfiniteHybridInterpolator() = default;

void InfiniteHybridInterpolator::setTransitionDuration(float durationMs)
{
	_initialDuration = std::max(1.0f, durationMs);
}

void InfiniteHybridInterpolator::setSpringiness(float stiffness, float damping)
{
	_stiffness = std::max(0.1f, stiffness);
	_damping = std::max(0.1f, damping);
}

void InfiniteHybridInterpolator::setMaxLuminanceChangePerFrame(float maxYChangePerFrame)
{
	_maxLuminanceChangePerFrame = maxYChangePerFrame;
}

void InfiniteHybridInterpolator::resetToColors(std::vector<float3> colors)
{
	size_t new_size = colors.size();
	_currentColorsRGB = std::move(colors);
	_finalTargetRGB = _currentColorsRGB;

	_currentColorsYUV.resize(new_size);
	_pacer_startColorsYUV.resize(new_size);
	_pacer_targetColorsYUV.resize(new_size);
	_velocityYUV.assign(new_size, { 0.0f, 0.0f, 0.0f });

	for (size_t i = 0; i < new_size; ++i)
	{
		_currentColorsYUV[i] = ColorSpaceMath::rgb_to_bt709(_currentColorsRGB[i]);
	}
	_pacer_startColorsYUV = _currentColorsYUV;
	_pacer_targetColorsYUV = _currentColorsYUV;

	_isAnimationComplete = true;
}

void InfiniteHybridInterpolator::setTargetColors(std::vector<float3>&& new_rgb_targets, float startTimeMs, bool debug) {
	size_t new_size = new_rgb_targets.size();
	if (_currentColorsRGB.size() != new_size)
	{
		_currentColorsRGB.resize(new_size);
		_currentColorsYUV.resize(new_size);
		_velocityYUV.resize(new_size);
		_pacer_startColorsYUV.resize(new_size);
	}

	_finalTargetRGB = new_rgb_targets;

	float elapsed = startTimeMs - _startTimeMs;
	float t = _isAnimationComplete ? 0.0f : std::max(0.0f, std::min(1.0f, (_deltaMs > 0.0f) ? elapsed / _deltaMs : 1.0f));

	_pacer_targetColorsYUV.resize(new_size);
	float new_distance = 0.0f;
	for (size_t i = 0; i < new_size; ++i)
	{
		float3 pacerCurrentPos = linalg::lerp(_pacer_startColorsYUV[i], _pacer_targetColorsYUV[i], t);
		_pacer_startColorsYUV[i] = pacerCurrentPos;
		_pacer_targetColorsYUV[i] = ColorSpaceMath::rgb_to_bt709(new_rgb_targets[i]);
		new_distance += linalg::distance(_pacer_startColorsYUV[i], _pacer_targetColorsYUV[i]);
	}

	if (_isAnimationComplete || _initialDistance < 1e-5f)
	{
		_lastTimeMs = startTimeMs;
		_deltaMs = _initialDuration;
		_initialDistance = new_distance > 1e-5f ? new_distance : 1.0f;
	}
	else
	{
		float duration_scale = new_distance / _initialDistance;
		_deltaMs = _initialDuration * duration_scale;
	}
	_deltaMs = std::max(10.0f, _deltaMs);

	_startTimeMs = startTimeMs;	
	_isAnimationComplete = false;
}

void InfiniteHybridInterpolator::updateCurrentColors(float currentTimeMs) {
	if (_isAnimationComplete)
	{
		return;
	}

	float dt = (currentTimeMs - _lastTimeMs )/ 1000.0f;
	if (dt <= 0.0f) return;

	_lastTimeMs = currentTimeMs;

	float elapsed = currentTimeMs - _startTimeMs;
	float t = std::max(0.0f, std::min(1.0f, (_deltaMs > 0.0f) ? elapsed / _deltaMs : 1.0f));

	bool all_finished = true;
	for (size_t i = 0; i < _pacer_targetColorsYUV.size(); ++i)
	{
		float3 pacerPosition = linalg::lerp(_pacer_startColorsYUV[i], _pacer_targetColorsYUV[i], t);
		float3 currentPos = _currentColorsYUV[i];
		float3 currentVel = _velocityYUV[i];

		float3 diffEnd = pacerPosition - currentPos;
		if (t >= 1.0f && linalg::dot(diffEnd, diffEnd) < FINISH_THRESHOLD_SQ && linalg::dot(currentVel, currentVel) < FINISH_THRESHOLD_SQ)
		{
			_currentColorsRGB[i] = _finalTargetRGB[i];
			_currentColorsYUV[i] = pacerPosition;
			_velocityYUV[i] = { 0.0f, 0.0f, 0.0f };
			continue;
		}
		all_finished = false;

		float3 springForce = diffEnd * _stiffness;
		float3 dampingForce = currentVel * -_damping;
		float3 acceleration = springForce + dampingForce;

		float3 nextVel = currentVel + acceleration * dt;
		float3 nextPos = currentPos + nextVel * dt;

		float y_change = nextPos.x - currentPos.x;
		
		if (_maxLuminanceChangePerFrame > 0 && std::abs(y_change) > _maxLuminanceChangePerFrame)
		{
			float clamped_y_change = std::max(-_maxLuminanceChangePerFrame, std::min(_maxLuminanceChangePerFrame, y_change));
			nextPos.x = currentPos.x + clamped_y_change;
			nextVel.x = clamped_y_change / dt;
		}

		_currentColorsYUV[i] = nextPos;
		_velocityYUV[i] = nextVel;
		_currentColorsRGB[i] = ColorSpaceMath::bt709_to_rgb(nextPos);
	}

	if (all_finished)
	{
		_isAnimationComplete = true;
	}
}

SharedOutputColors InfiniteHybridInterpolator::getCurrentColors() const
{
	return std::make_shared<std::vector<linalg::vec<float, 3>>>(_currentColorsRGB);
}

void InfiniteHybridInterpolator::test() {
	using TestTuple = std::tuple<std::string, float3, float3, float3, float3>;

	auto run_test_lambda = [](InfiniteHybridInterpolator& interpolator, const TestTuple& test) {
		const auto& name = std::get<0>(test);
		const auto& start_A = std::get<1>(test);
		const auto& interrupt_C = std::get<2>(test);
		const auto& interrupt_D = std::get<3>(test);
		const auto& final_B = std::get<4>(test);

		std::cout << "\n--- TEST: " << name << " ---\n";
		std::cout << "--------------------------------------------------\n";

		interpolator.resetToColors({ start_A });
		interpolator.setTransitionDuration(150.0f);

		bool retargeted_to_D = false;
		bool retargeted_to_B = false;

		for (float time_ms = 0; time_ms <= 1000; time_ms += 25) {
			if (time_ms == 0) {
				interpolator.setTargetColors({ interrupt_C }, time_ms);
			}
			if (time_ms >= 200 && !retargeted_to_D) {
				std::cout << "--- ZMIANA CELU 1 @ " << time_ms << "ms (-> D) ---\n";
				interpolator.setTargetColors({ interrupt_D }, time_ms);
				retargeted_to_D = true;
			}
			if (time_ms >= 350 && !retargeted_to_B) {
				std::cout << "--- ZMIANA CELU 2 @ " << time_ms << "ms (-> B) ---\n";
				interpolator.setTargetColors({ final_B }, time_ms);
				retargeted_to_B = true;
			}

			interpolator.updateCurrentColors(time_ms);

			auto temp_color = interpolator.getCurrentColors();
			const auto& current_color = *(temp_color);
			if (current_color.empty()) continue;

			auto yuv = ColorSpaceMath::rgb_to_bt709(current_color[0]);
			std::cout << std::setw(4) << static_cast<int>(time_ms) << "ms: { R: "
				<< std::fixed << std::setprecision(3) << current_color[0].x
				<< ", G: " << current_color[0].y
				<< ", B: " << current_color[0].z
				<< " } (Y: " << yuv.x << ")\n";
		}
		std::cout << "--------------------------------------------------\n";
		};

	InfiniteHybridInterpolator interpolator;
	interpolator.setSpringiness(200.0f, 26.0f);

	std::cout << "\n##################################################\n";
	std::cout << "### URUCHAMIANIE TESTÓW Z LIMITEM LUMINANCJI (0.05/klatkę) ###\n";
	std::cout << "##################################################\n";
	interpolator.setMaxLuminanceChangePerFrame(0.05f);

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
			{"Zielony -> Cyjan -> Czerwony -> Niebieski", {0,1,0}, {0,1,1}, {1,0,0}, {0,0,1}},
			{"Test Odwrócenia (Biały -> Czarny -> Biały -> Czarny)", {1,1,1}, {0,0,0}, {1,1,1}, {0,0,0}},
			{"Test Oscylacji (Żółty -> Niebieski -> Żółty -> Niebieski)", {1,1,0}, {0,0,1}, {1,1,0}, {0,0,1}},
			{"Test Oscylacji (Magenta -> Zielony -> Magenta -> Zielony)", {1,0,1}, {0,1,0}, {1,0,1}, {0,1,0}}
	};

	for (const auto& test : test_cases) {
		run_test_lambda(interpolator, test);
	}

	std::cout << "\n##################################################\n";
	std::cout << "### URUCHAMIANIE TESTÓW BEZ LIMITU LUMINANCJI ###\n";
	std::cout << "##################################################\n";
	interpolator.setMaxLuminanceChangePerFrame(0);

	std::vector<TestTuple> test_cases_unlimited = {
			{"Test BEZ Limitu: Czarny -> Ciemny Nieb. -> Jasny Żółty -> Biały", {0,0,0}, {0.1f, 0.1f, 0.4f}, {0.9f, 0.9f, 0.2f}, {1,1,1} },
			{"Test BEZ Limitu: Czerwony -> Żółty -> Cyjan -> Niebieski", {1,0,0}, {1,1,0}, {0,1,1}, {0,0,1}},
			{"Test BEZ Limitu: Niebieski -> Zielony -> Czerwony -> Złoty", {0,0,1}, {0,1,0}, {1,0,0}, {1,1,0}},
			{"Test BEZ Limitu: Magenta -> Cyjan -> Żółty -> Zielony", {1,0,1}, {0,1,1}, {1,1,0}, {0,1,0}},
			{"Test BEZ Limitu: Cyjan -> Żółty -> Magenta -> Czerwony", {0,1,1}, {1,1,0}, {1,0,1}, {1,0,0}},
			{"Test BEZ Limitu: Niebieski (50%) -> Czerwony -> Zielony -> Złoty (50%)", {0.25f, 0.25f, 0.75f}, {1,0,0}, {0,1,0}, {0.75f, 0.75f, 0.25f}},
			{"Test BEZ Limitu: Magenta (50%) -> Niebieski -> Czerwony -> Zielony (50%)", {0.75f, 0.25f, 0.75f}, {0,0,1}, {1,0,0}, {0.25f, 0.75f, 0.25f}},
			{"Test BEZ Limitu: Niebieski (25%) -> Szary -> Jasny Szary -> Złoty (25%)", {0.0f, 0.0f, 0.25f}, {0.2f,0.2f,0.2f}, {0.8f,0.8f,0.8f}, {0.25f, 0.25f, 0.0f}},
			{"Test BEZ Limitu: Magenta (25%) -> Ciemny Cyjan -> Jasny Żółty -> Zielony (25%)", {0.25f, 0.0f, 0.25f}, {0.0f,0.2f,0.2f}, {0.8f,0.8f,0.0f}, {0.0f, 0.25f, 0.0f}},
			{"Test BEZ Limitu: Cyjan (25%) -> Fiolet -> Pomarańcz -> Czerwony (25%)", {0.0f, 0.25f, 0.25f}, {0.5f,0.0f,0.5f}, {1.0f,0.5f,0.0f}, {0.25f, 0.0f, 0.0f}},
			{"Test BEZ Limitu: Ciemny Nieb. -> Biały -> Czarny -> Jasny Żółty", {0,0,0.5f}, {1,1,1}, {0,0,0}, {1,1,0.8f}},
			{"Test BEZ Limitu: Jasny Czerw. -> Ciemny Nieb. -> Jasny Zielony -> Ciemny Cyjan", {1,0,0}, {0,0,0.2f}, {0.5f,1.0f,0.5f}, {0.1f, 0.4f, 0.4f}},
			{"Test BEZ Limitu: Zielony -> Fiolet -> Cyjan -> Czerwony", {0,1,0}, {0.5f,0,1.0f}, {0,1,1}, {1,0,0}},
			{"Test BEZ Limitu: Zielony -> Żółty -> Niebieski -> Magenta", {0,1,0}, {1,1,0}, {0,0,1}, {1,0,1}},
			{"Test BEZ Limitu: Biały -> Czerwony -> Zielony -> Niebieski", {1,1,1}, {1,0,0}, {0,1,0}, {0,0,1}},
			{"Test BEZ Limitu: Zielony -> Cyjan -> Czerwony -> Niebieski", {0,1,0}, {0,1,1}, {1,0,0}, {0,0,1}},
			{"Test BEZ Limitu: Test Odwrócenia (Biały -> Czarny -> Biały -> Czarny)", {1,1,1}, {0,0,0}, {1,1,1}, {0,0,0}},
			{"Test BEZ Limitu: Test Oscylacji (Żółty -> Niebieski -> Żółty -> Niebieski)", {1,1,0}, {0,0,1}, {1,1,0}, {0,0,1}},
			{"Test BEZ Limitu: Test Oscylacji (Magenta -> Zielony -> Magenta -> Zielony)", {1,0,1}, {0,1,0}, {1,0,1}, {0,1,0}}
	};
	for (const auto& test : test_cases_unlimited) {
		run_test_lambda(interpolator, test);
	}
}
