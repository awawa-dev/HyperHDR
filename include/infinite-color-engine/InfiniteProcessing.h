#pragma once

#ifndef PCH_ENABLED
	#include <QJsonDocument>
	#include <QMutex>
	#include <QVector>

	#include <vector>
#endif

#include <image/ColorRgb.h>
#include <infinite-color-engine/SharedOutputColors.h>
#include <utils/settings.h>
#include <utils/Components.h>
#include <utils/InternalClock.h>
#include <base/LedString.h>
#include <linalg.h>


template<typename T>
concept IsFloatVec3 = std::same_as<T, linalg::vec<float, 3>>;

template<typename T>
concept IsUint8Vec3 = std::same_as<T, linalg::vec<uint8_t, 3>>;

enum class CalibrationMode { None, Matrix, Lut };

struct CalibrationSnapshot
{
	CalibrationMode mode = CalibrationMode::None;
	std::vector<linalg::vec<float, 3>> lut;
	linalg::mat<float, 3, 3> primary_calib_matrix{};
};

class CalibrationData
{
public:
	using Snapshot = CalibrationSnapshot;

	void update(std::vector<linalg::vec<float, 3>>&& lut,
		linalg::mat<float, 3, 3>&& matrix,
		CalibrationMode mode)
	{
		_snapshot = std::make_shared<Snapshot>(
            Snapshot{ mode, std::move(lut), std::move(matrix) }
        );
	}

	std::shared_ptr<const CalibrationSnapshot> getSnapshot() const {
		return _snapshot;
	}

private:
	std::shared_ptr<Snapshot> _snapshot{ std::make_shared<Snapshot>() };
};

class InfiniteProcessing : public QObject
{
	Q_OBJECT

public:
	enum class TemperaturePreset
	{
		Disabled,
		Warm,
		Neutral,
		Cold,
		Custom
	};

	InfiniteProcessing();
	InfiniteProcessing(const QJsonDocument& config, Logger* _log);
	~InfiniteProcessing() = default;

	void setProcessingEnabled(bool enabled);
	void updateCurrentConfig(const QJsonObject& config);
	QJsonArray getCurrentConfig();

	void generateColorspace(
		bool use_primaries_only,
		ColorRgb target_red = { 255,0,0 },
		ColorRgb target_green = { 0,255,0 },
		ColorRgb target_blue = { 0,0,255 },
		ColorRgb target_cyan = { 0,255,255 },
		ColorRgb target_magenta = { 255,0,255 },
		ColorRgb target_yellow = { 255,255,0 },
		ColorRgb target_white = { 255,255,255 },
		ColorRgb target_black = { 0,0,0 }
	);
	std::shared_ptr<const CalibrationSnapshot> getColorspaceCalibrationSnapshot();
	void calibrateColorInColorspace(const std::shared_ptr<const CalibrationSnapshot>& calibrationSnapshot, linalg::vec<float, 3>& color) const;

	void generateUserGamma(float gamma);
	void applyUserGamma(linalg::vec<float, 3>& color) const;

	void setTemperature(TemperaturePreset preset, linalg::vec<float, 3> custom_tint);
	void applyTemperature(linalg::vec<float, 3>& color) const;

	void setBrightnessAndSaturation(float brightness, float saturation);
	void applyBrightnessAndSaturation(linalg::vec<float, 3>& color) const;

	void setMinimalBacklight(float minimalLevel, bool coloreBacklight);
	void applyMinimalBacklight(linalg::vec<float, 3>& color) const;

	static linalg::vec<float, 3> srgbNonlinearToLinear(IsFloatVec3 auto color);
	static linalg::vec<float, 3> srgbLinearToNonlinear(linalg::vec<float, 3> color);
	static linalg::vec<uint16_t, 3> srgbNonlinearToLinear(IsUint8Vec3 auto color);

	static void test();

public slots:
	void handleSignalIncomingColors(SharedOutputColors linearRgbColors);
	void handleSignalInstanceSettingsChanged(settings::type type, const QJsonDocument& config);

signals:
	void SignalFinalOutputColorsReady(SharedOutputColors nonlinearRgbColors);
	
private:
	bool _enabled;

	LedString::ColorOrder _colorOrder;

	static constexpr int LUT_SIZE = 512;	
	static constexpr int LUT_DIMENSION = 9;
	CalibrationData _colorCalibrationData;	

	bool _temperature_enabled;
	linalg::vec<float, 3> _temperature_tint;

	float _brightness;
	float _saturation;

	float _minimalBacklight;
	bool _coloredBacklight;

	Logger* _log;
	QJsonObject _currentConfig;

private:
	std::array<float, LUT_SIZE> _user_gamma_lut;

	template <typename T, size_t N, typename Generator>
	static constexpr std::array<T, N> make_lut(Generator&& func)
	{
		std::array<T, N> lut{};
		for (size_t i = 0; i < N; ++i)
		{
			lut[i] = func(i);
		}
		return lut;
	}

	inline static const std::array<float, LUT_SIZE> _srgb_to_linear_lut =
		make_lut<float, LUT_SIZE>([](size_t i) {
		float val = static_cast<float>(i) / (LUT_SIZE - 1);
		if (val <= 0.04045f)
		{
			return val / 12.92f;
		}
		else
		{
			return std::pow((val + 0.055f) / 1.055f, 2.4f);
		}
	});

	inline static const std::array<float, LUT_SIZE> _linear_to_srgb_lut =
		make_lut<float, LUT_SIZE>([](size_t i) {
		float val = static_cast<float>(i) / (LUT_SIZE - 1);
		if (val <= 0.0031308f)
		{
			return val * 12.92f;
		}
		else
		{
			return 1.055f * std::pow(val, 1.0f / 2.4f) - 0.055f;
		}
	});

	inline static const std::array<uint16_t, 256> _srgb_uint8_to_linear_uint16_lut =
		make_lut<uint16_t, 256>([](size_t i) {
		float srgb_val = static_cast<float>(i) / 255.0f;
		float linear_val;
		if (srgb_val <= 0.04045f)
		{
			linear_val = srgb_val / 12.92f;
		}
		else
		{
			linear_val = std::pow((srgb_val + 0.055f) / 1.055f, 2.4f);
		}
		return static_cast<uint16_t>(linear_val * 65535.0f + 0.5f); // +0.5f dla poprawnego zaokrąglenia
	});
};
