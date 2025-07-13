/* InfiniteYuvInterpolator.cpp
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


#include <infinite-color-engine/ColorSpace.h>
#include <infinite-color-engine/InfiniteYuvInterpolator.h>

using namespace linalg::aliases;

InfiniteYuvInterpolator::InfiniteYuvInterpolator() = default;

void InfiniteYuvInterpolator::setTransitionDuration(float durationMs)
{
	_initialDuration = std::max(1.0f, durationMs);
	_deltaMs = std::max(1.0f, durationMs);
}

void InfiniteYuvInterpolator::setMaxLuminanceChangePerFrame(float maxChangePerStep)
{
	_maxLuminanceChangePerStep = std::max(0.0f, maxChangePerStep);
}

void InfiniteYuvInterpolator::resetToColors(std::vector<float3> colors)
{
	size_t new_size = colors.size();
	_currentColorsRGB = std::move(colors);
	_targetColorsRGB = _currentColorsRGB;
	_wasClamped.assign(new_size, false);

	_currentColorsYUV.resize(new_size);
	for (size_t i = 0; i < new_size; ++i) {
		_currentColorsYUV[i] = ColorSpaceMath::rgb_to_bt709(_currentColorsRGB[i]);
	}

	_isAnimationComplete = true;
}

void InfiniteYuvInterpolator::setTargetColors(std::vector<float3> new_rgb_targets, float startTimeMs)
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

	std::vector<float3> startColorsRGB = _currentColorsRGB;
	_startColorsYUV.resize(new_size);
	_targetColorsYUV.resize(new_size);
	_wasClamped.assign(new_size, false);

	float new_distance = 0.0f;
	for (size_t i = 0; i < new_size; ++i)
	{
		_startColorsYUV[i] = ColorSpaceMath::rgb_to_bt709(startColorsRGB[i]);
		_targetColorsYUV[i] = ColorSpaceMath::rgb_to_bt709(_targetColorsRGB[i]);
		// Używamy dystansu w YUV do obliczeń
		new_distance += linalg::distance(_startColorsYUV[i], _targetColorsYUV[i]);
	}

	if (_isAnimationComplete)
	{
		// To jest nowa, pełna animacja
		_deltaMs = _initialDuration;
		_initialDistance = new_distance;
	}
	else {
		// To jest zmiana celu w trakcie animacji
		if (_initialDistance > 1e-5f)
		{
			float duration_scale = new_distance / _initialDistance;
			_deltaMs = _initialDuration * duration_scale;
		}
		else
		{
			// Unikaj dzielenia przez zero, jeśli poprzedni dystans był zerowy
			_deltaMs = _initialDuration;
		}
	}
	// Upewnij się, że czas nie jest zbyt krótki
	_deltaMs = std::max(10.0f, _deltaMs);


	_startTimeMs = startTimeMs;
	_isAnimationComplete = false;
}

void InfiniteYuvInterpolator::updateCurrentColors(float currentTimeMs)
{
	if (_isAnimationComplete)
	{
		return;
	}

	float elapsed = currentTimeMs - _startTimeMs;
	float t = (_deltaMs > 0.0f) ? elapsed / _deltaMs : 1.0f;

	_isAnimationComplete = (t >= 1.0f);

	size_t num_colors = _targetColorsRGB.size();
	if (_currentColorsRGB.size() != num_colors) _currentColorsRGB.resize(num_colors);

	for (size_t i = 0; i < num_colors; ++i)
	{
		// Szybka ścieżka dla animacji, które nie były limitowane i których czas minął
		if (t >= 1.0f && !_wasClamped[i])
		{
			_currentColorsRGB[i] = _targetColorsRGB[i];
			_currentColorsYUV[i] = _targetColorsYUV[i];
			continue;
		}

		float t_clamped = std::max(0.0f, std::min(t, 1.0f));
		float3 idealPosition = linalg::lerp(_startColorsYUV[i], _targetColorsYUV[i], t_clamped);
		float3 moveVec = idealPosition - _currentColorsYUV[i];

		float luminanceChangeNeeded = moveVec.x;

		if (_maxLuminanceChangePerStep > 0.0f && std::fabs(luminanceChangeNeeded) > _maxLuminanceChangePerStep)
		{
			float scale = _maxLuminanceChangePerStep / std::fabs(luminanceChangeNeeded);
			moveVec = moveVec * scale;
			_wasClamped[i] = true;
		}

		float3 nextPos = _currentColorsYUV[i] + moveVec;

		_currentColorsYUV[i] = nextPos;
		_currentColorsRGB[i] = ColorSpaceMath::bt709_to_rgb(nextPos);

		if (t >= 1.0f)
		{
			// Jesteśmy w fazie "doganiania"
			float3 diff = _currentColorsYUV[i] - _targetColorsYUV[i];
			if (std::max({ std::fabs(diff.x), std::fabs(diff.y), std::fabs(diff.z) }) < YUV_FINISH_THRESHOLD)
			{
				// "Maruder" w końcu dotarł do celu
				_currentColorsRGB[i] = _targetColorsRGB[i];
				_currentColorsYUV[i] = _targetColorsYUV[i];
			}
			else
			{
				// Czas minął, a ten kolor wciąż nie jest w celu, więc animacja musi trwać dalej
				_isAnimationComplete = false;
			}
		}
	}
}

SharedOutputColors InfiniteYuvInterpolator::getCurrentColors() const
{
	return std::make_shared<std::vector<linalg::vec<float, 3>>>(_currentColorsRGB);
}

void InfiniteYuvInterpolator::test()
{
	using TestTuple = std::tuple<std::string, float3, float3, float3, float3>;

	auto run_test_lambda = [](InfiniteYuvInterpolator& interpolator, const TestTuple& test)
		{
			const auto& name = std::get<0>(test);
			const auto& start_A = std::get<1>(test);
			const auto& interrupt_C = std::get<2>(test);
			const auto& interrupt_D = std::get<3>(test);
			const auto& final_B = std::get<4>(test);

			std::cout << "\n--- TEST: " << name << " ---\n";
			std::cout << "--------------------------------------------------\n";

			interpolator.resetToColors({ start_A });

			// Etap 1: Animacja A -> C
			interpolator.setTransitionDuration(150.0f);
			interpolator.setTargetColors({ interrupt_C }, 0.0f);

			bool retargeted_to_D = false;
			bool retargeted_to_B = false;

			for (float time_ms = 0; time_ms <= 1000; time_ms += 50)
			{
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

	InfiniteYuvInterpolator interpolator;
	interpolator.setMaxLuminanceChangePerFrame(0.02f);

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

	for (const auto& test : test_cases)
	{
		run_test_lambda(interpolator, test);
	}
}

