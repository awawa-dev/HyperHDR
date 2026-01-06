/* InfiniteHybridInterpolator.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
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
	#include <vector>
	#include <cassert>
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
	_maxLuminanceChangePerStep = maxYChangePerFrame;
}

void InfiniteHybridInterpolator::resetToColors(std::vector<float3> colors, float startTimeMs)
{
	_currentColorsYUV.clear();
	setTargetColors(std::move(colors), startTimeMs);
}

void InfiniteHybridInterpolator::setTargetColors(std::vector<float3>&& new_rgb_to_yuv_targets, float startTimeMs, bool debug) {
	if (new_rgb_to_yuv_targets.empty())
		return;

	const float delta = (!_isAnimationComplete) ? std::max(startTimeMs - _lastUpdate, 0.f) : 0.f;

	if (debug)
	{
		printf(" | Δ%.0f | time: %.0f | Y-limit: %.3f| stiffness: %.3f| damping: %.3f\n", delta, _initialDuration, _maxLuminanceChangePerStep, _stiffness, _damping);
	}

	startTimeMs -= delta;

	// convert incoming RGB to YUV
	_targetColorsRGB = new_rgb_to_yuv_targets;
	for (auto it_newTargetColorsYUV = new_rgb_to_yuv_targets.begin(); it_newTargetColorsYUV != new_rgb_to_yuv_targets.end(); ++it_newTargetColorsYUV)
	{
		*it_newTargetColorsYUV = ColorSpaceMath::rgb_to_bt709(*it_newTargetColorsYUV);
	}

	if (_currentColorsYUV.size() != new_rgb_to_yuv_targets.size() || _targetColorsYUV.size() != new_rgb_to_yuv_targets.size())
	{
		_lastUpdate = startTimeMs;
		_currentColorsYUV = new_rgb_to_yuv_targets;
		_targetColorsYUV = std::move(new_rgb_to_yuv_targets);
		_velocitiesYUV.assign(_currentColorsYUV.size(), float3{ 0,0,0 }); // NOWE
		_isAnimationComplete = true;
	}
	else
	{
		_targetColorsYUV = std::move(new_rgb_to_yuv_targets);
		_isAnimationComplete = false;
	}

	_startAnimationTimeMs = startTimeMs;
	_targetTime = startTimeMs + _initialDuration;
	_currentColorsRGB.reset();
}

void InfiniteHybridInterpolator::updateCurrentColors(float currentTimeMs) {
	if (_isAnimationComplete)
		return;

	float dt = std::max(currentTimeMs - _lastUpdate, 0.001f);
	_lastUpdate = currentTimeMs;

	auto computeChannelVec = [&](float3& cur, const float3& diff, float3& vel) -> bool {
		const float FINISH_COMPONENT_THRESHOLD = 0.0013732906f / 10.f;
		const float VELOCITY_THRESHOLD = 0.0005f;

		if (linalg::maxelem(linalg::abs(diff)) < FINISH_COMPONENT_THRESHOLD && // color match
			linalg::maxelem(linalg::abs(vel)) < VELOCITY_THRESHOLD) // speed should be almost zero
		{
			cur += diff;
			vel = float3{ 0,0,0 };
			return false;
		}
		else
		{
			float3 acc = _stiffness * diff - _damping * vel;

			// integracja (Euler semi-implicit)
			vel += acc * (dt * 0.001f);
			float3 step = vel * (dt * 0.001f);

			if (_maxLuminanceChangePerStep > 0.f && fabs(step[0]) > _maxLuminanceChangePerStep)
			{
				float scale = _maxLuminanceChangePerStep / fabs(step[0]);
				step *= scale;

				vel *= scale;
			}
			cur += step;

			return true;
		}
	};

	_isAnimationComplete = true;

	for (auto cur = _currentColorsYUV.begin(), tgt = _targetColorsYUV.begin(), vel = _velocitiesYUV.begin();
		cur != _currentColorsYUV.end() && tgt != _targetColorsYUV.end() && vel != _velocitiesYUV.end();
		++cur, ++tgt, ++vel)
	{
		if (computeChannelVec(*cur, *tgt - *cur, *vel))
		{
			_isAnimationComplete = false;
		}
	}

	_currentColorsRGB.reset();
}

SharedOutputColors InfiniteHybridInterpolator::getCurrentColors()
{
	if (!_currentColorsRGB.has_value())
	{
		if (_isAnimationComplete)
		{
			_currentColorsRGB = _targetColorsRGB;
		}
		else
		{
			std::vector<linalg::aliases::float3> tmp(_currentColorsYUV.size());
			std::transform(
				_currentColorsYUV.begin(),
				_currentColorsYUV.end(),
				tmp.begin(),
				[](const auto& yuv) { return linalg::clamp(ColorSpaceMath::bt709_to_rgb(yuv), 0.f, 1.0f); }
			);
			_currentColorsRGB = std::move(tmp);
		}
	}
	return std::make_shared<std::vector<linalg::vec<float, 3>>>(_currentColorsRGB.value());
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

		interpolator.resetToColors({ start_A }, 0.f);
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
