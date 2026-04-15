#include <led-drivers/other/DriverOtherRawHid.h>

#ifndef PCH_ENABLED
	#include <array>
	#include <algorithm>
	#include <cerrno>
	#include <cmath>
	#include <cstring>
	#include <limits>
	#include <string>
#endif

#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QJsonArray>
	#include <QRegularExpression>
#include <QTimer>
#include <QtEndian>
#include <linalg.h>

#include "RawHidTransferCurve3_4New.h"
#include "RawHidSolverLaddersRgbw.h"

#if defined(__linux__)
	#include <fcntl.h>
	#include <poll.h>
	#include <unistd.h>
#endif

namespace
{
	constexpr auto RUNTIME_INTERPOLATED_TRANSFER_CURVE_PREFIX = "runtime-interpolated:";
	constexpr uint16_t RGB12_BUCKET_MAX = 4095u;
	constexpr size_t SOLVER_HOST_MIN_BUCKET_COUNT = 2u;
	constexpr uint8_t SCENE_POLICY_MAGIC = 'S';
	constexpr uint8_t SOLVER_HOST_MAX_BFI = 4u;
	constexpr uint8_t TRANSFER_CURVE_FLAG_APPLIED_BY_HOST = 0x01u;
	constexpr uint8_t CALIBRATION_FLAG_APPLIED_BY_HOST = 0x02u;
	constexpr uint8_t DEFAULT_HIGHLIGHT_SHADOW_PERCENT = 6u;
	constexpr uint16_t DEFAULT_HIGHLIGHT_SHADOW_Q16_THRESHOLD = 14896u;
	constexpr uint8_t DEFAULT_HIGHLIGHT_SHADOW_MIN_PEAK_DELTA = 12u;
	constexpr uint8_t DEFAULT_HIGHLIGHT_SHADOW_UNIFORM_SPREAD_MAX = 8u;
	constexpr uint8_t DEFAULT_HIGHLIGHT_SHADOW_TRIGGER_MARGIN = 6u;
	constexpr uint8_t DEFAULT_HIGHLIGHT_SHADOW_MIN_SCENE_MEDIAN = 22u;
	constexpr uint8_t DEFAULT_HIGHLIGHT_SHADOW_MIN_SCENE_AVG = 26u;
	constexpr uint8_t DEFAULT_HIGHLIGHT_SHADOW_DIM_UNIFORM_MEDIAN = 52u;
	constexpr float DEFAULT_SCENE_POLICY_ENERGY_SMOOTH = 0.13f;
	constexpr float DEFAULT_SCENE_POLICY_HYSTERESIS = 0.35f;
	constexpr float DEFAULT_SCENE_POLICY_SMOOTH_ALPHA = 0.10f;
	constexpr double DEFAULT_SCENE_POLICY_FRAME_INTERVAL_SMOOTH = 0.10;
	constexpr uint8_t DEFAULT_SCENE_POLICY_ACTIVATE_STABLE_FRAMES = 16u;
	constexpr uint8_t DEFAULT_SCENE_POLICY_RELAX_STABLE_FRAMES = 12u;
	constexpr uint8_t DEFAULT_SCENE_POLICY_CHANGE_STABLE_FRAMES = 8u;
	constexpr uint8_t DEFAULT_SCENE_POLICY_LARGE_STEP_EXTRA_FRAMES = 4u;
	constexpr uint8_t DEFAULT_SCENE_POLICY_STABLE_BASE_FPS = 60u;
	constexpr uint8_t DEFAULT_SCENE_POLICY_STABLE_MIN_SCALE_X100 = 80u;
	constexpr uint8_t DEFAULT_SCENE_POLICY_STABLE_MAX_SCALE_X100 = 250u;
	constexpr std::array<float, 4> LEGACY_SCENE_OFFSET_BOUNDARIES = { 0.20f, 0.40f, 0.70f, 0.88f };
	constexpr std::array<float, 4> SOLVER_AWARE_SCENE_ENERGY_BOUNDARIES = { 3.0f, 7.0f, 12.0f, 18.0f };
	constexpr std::array<uint16_t, SOLVER_HOST_MAX_BFI + 1u> RGBW_LUMA_R_Q16_BY_BFI = { 5163u, 5372u, 5607u, 6563u, 5913u };
	constexpr std::array<uint16_t, SOLVER_HOST_MAX_BFI + 1u> RGBW_LUMA_G_Q16_BY_BFI = { 18487u, 18454u, 16528u, 17122u, 16780u };
	constexpr std::array<uint16_t, SOLVER_HOST_MAX_BFI + 1u> RGBW_LUMA_B_Q16_BY_BFI = { 3792u, 3834u, 4144u, 4782u, 4285u };
	constexpr std::array<uint16_t, SOLVER_HOST_MAX_BFI + 1u> RGBW_LUMA_W_Q16_BY_BFI = { 38093u, 37875u, 39256u, 37068u, 38557u };
	constexpr float DEFAULT_HOST_RGBW_GATED_UV_RADIUS = 0.020f;
	constexpr float DEFAULT_HOST_RGBW_GATED_POWER = 1.75f;
	constexpr float DEFAULT_HOST_RGBW_D65_STRENGTH = 0.35f;
	constexpr double D65_WHITE_X = 0.3127;
	constexpr double D65_WHITE_Y = 0.3290;
	constexpr std::array<double, 2> DEFAULT_HOST_RGBW_RED_XY = { 0.6845, 0.3155 };
	constexpr std::array<double, 2> DEFAULT_HOST_RGBW_GREEN_XY = { 0.1367, 0.7489 };
	constexpr std::array<double, 2> DEFAULT_HOST_RGBW_BLUE_XY = { 0.1301, 0.0662 };
	constexpr std::array<double, 2> DEFAULT_HOST_RGBW_WHITE_XY = { 0.3309, 0.3590 };

	inline std::array<double, 3> xyToXyz(const double x, const double y, const double Y = 1.0)
	{
		if (y <= 0.0)
			return { 0.0, 0.0, 0.0 };

		const double scale = Y / y;
		return {
			x * scale,
			Y,
			(1.0 - x - y) * scale
		};
	}

	inline std::array<double, 2> xyzToUvPrime(const std::array<double, 3>& xyz)
	{
		const double denominator = xyz[0] + (15.0 * xyz[1]) + (3.0 * xyz[2]);
		if (std::abs(denominator) <= 1e-12)
			return { 0.0, 0.0 };

		return {
			(4.0 * xyz[0]) / denominator,
			(9.0 * xyz[1]) / denominator
		};
	}

	inline double distance2d(const std::array<double, 2>& left, const std::array<double, 2>& right)
	{
		const double dx = left[0] - right[0];
		const double dy = left[1] - right[1];
		return std::sqrt((dx * dx) + (dy * dy));
	}

	inline bool solveRgbBasisVector(
		const std::array<std::array<double, 3>, 3>& basis,
		const std::array<double, 3>& target,
		std::array<double, 3>& solution)
	{
		const double a00 = basis[0][0];
		const double a01 = basis[1][0];
		const double a02 = basis[2][0];
		const double a10 = basis[0][1];
		const double a11 = basis[1][1];
		const double a12 = basis[2][1];
		const double a20 = basis[0][2];
		const double a21 = basis[1][2];
		const double a22 = basis[2][2];

		const double determinant =
			(a00 * ((a11 * a22) - (a12 * a21))) -
			(a01 * ((a10 * a22) - (a12 * a20))) +
			(a02 * ((a10 * a21) - (a11 * a20)));
		if (std::abs(determinant) <= 1e-12)
			return false;

		const double invDet = 1.0 / determinant;
		const double i00 = ((a11 * a22) - (a12 * a21)) * invDet;
		const double i01 = ((a02 * a21) - (a01 * a22)) * invDet;
		const double i02 = ((a01 * a12) - (a02 * a11)) * invDet;
		const double i10 = ((a12 * a20) - (a10 * a22)) * invDet;
		const double i11 = ((a00 * a22) - (a02 * a20)) * invDet;
		const double i12 = ((a02 * a10) - (a00 * a12)) * invDet;
		const double i20 = ((a10 * a21) - (a11 * a20)) * invDet;
		const double i21 = ((a01 * a20) - (a00 * a21)) * invDet;
		const double i22 = ((a00 * a11) - (a01 * a10)) * invDet;

		solution = {
			(i00 * target[0]) + (i01 * target[1]) + (i02 * target[2]),
			(i10 * target[0]) + (i11 * target[1]) + (i12 * target[2]),
			(i20 * target[0]) + (i21 * target[1]) + (i22 * target[2])
		};
		return true;
	}

	inline std::array<float, 3> normalizeMinModeVector(const std::array<double, 3>& vector)
	{
		double maxValue = 0.0;
		std::array<double, 3> clamped{};
		for (size_t index = 0u; index < vector.size(); ++index)
		{
			const double value = std::isfinite(vector[index]) ? std::max(0.0, vector[index]) : 0.0;
			clamped[index] = value;
			if (value > maxValue)
				maxValue = value;
		}

		if (maxValue <= 1e-12)
			return { 1.0f, 1.0f, 1.0f };

		return {
			static_cast<float>(clamped[0] / maxValue),
			static_cast<float>(clamped[1] / maxValue),
			static_cast<float>(clamped[2] / maxValue)
		};
	}

	inline std::array<double, 3> blendVector3(const std::array<double, 3>& measured, const std::array<double, 3>& corrected, const double blend)
	{
		const double clampedBlend = std::clamp(blend, 0.0, 1.0);
		return {
			measured[0] + ((corrected[0] - measured[0]) * clampedBlend),
			measured[1] + ((corrected[1] - measured[1]) * clampedBlend),
			measured[2] + ((corrected[2] - measured[2]) * clampedBlend)
		};
	}

	inline std::array<double, 2> blendVector2(const std::array<double, 2>& measured, const std::array<double, 2>& corrected, const double blend)
	{
		const double clampedBlend = std::clamp(blend, 0.0, 1.0);
		return {
			measured[0] + ((corrected[0] - measured[0]) * clampedBlend),
			measured[1] + ((corrected[1] - measured[1]) * clampedBlend)
		};
	}
	inline uint16_t floatToQ16(const float value)
	{
		const float clamped = std::clamp(value, 0.0f, 1.0f);
		return static_cast<uint16_t>(std::lround(clamped * 65535.0f));
	}

	inline size_t lutIndexFromQ16(const uint16_t q16, const uint32_t bucketCount)
	{
		if (bucketCount <= 1u)
			return 0u;
		return static_cast<size_t>((static_cast<uint64_t>(q16) * static_cast<uint64_t>(bucketCount - 1u) + 32767u) / 65535u);
	}

	inline uint16_t applyBuiltInTransferCurveQ16(const uint16_t q16, const uint8_t channel)
	{
		const size_t lutIndex = lutIndexFromQ16(q16, TemporalBFITransferCurve::BUCKET_COUNT);
		switch (channel)
		{
			case 0:
				return TemporalBFITransferCurve::TARGET_G[lutIndex];
			case 1:
				return TemporalBFITransferCurve::TARGET_R[lutIndex];
			case 2:
				return TemporalBFITransferCurve::TARGET_B[lutIndex];
			default:
				return TemporalBFITransferCurve::TARGET_W[lutIndex];
		}
	}

	inline uint16_t sampleTransferCurveLutQ16(const DriverOtherRawHid::CustomTransferCurveLut& lut, const uint16_t q16, const uint8_t channel)
	{
		if (lut.bucketCount <= 1u)
			return q16;

		const size_t lutIndex = lutIndexFromQ16(q16, lut.bucketCount);
		switch (channel)
		{
			case 0:
				return lut.channels[1][lutIndex];
			case 1:
				return lut.channels[0][lutIndex];
			case 2:
				return lut.channels[2][lutIndex];
			default:
				return lut.channels[3][lutIndex];
		}
	}

	inline QString sanitizeTransferCurveProfileKey(QString key)
	{
		key = key.trimmed().toLower();
		key.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
		key.remove(QRegularExpression(R"(^_+|_+$)"));
		return key.left(96);
	}

	inline QString sanitizeCalibrationHeaderProfileKey(QString key)
	{
		key = key.trimmed().toLower();
		key.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
		key.remove(QRegularExpression(R"(^_+|_+$)"));
		return key.left(96);
	}

	inline QString sanitizeRgbwLutProfileKey(QString key)
	{
		key = key.trimmed().toLower();
		key.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
		key.remove(QRegularExpression(R"(^_+|_+$)"));
		return key.left(96);
	}

	inline QString sanitizeSolverProfileKey(QString key)
	{
		key = key.trimmed().toLower();
		key.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
		key.remove(QRegularExpression(R"(^_+|_+$)"));
		return key.left(96);
	}

	inline QString stripSolverProfileComments(QString content)
	{
		content.remove(QRegularExpression(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption));
		content.remove(QRegularExpression(R"(^\s*//.*$)", QRegularExpression::MultilineOption));
		return content;
	}

	bool extractSolverProfileArrayBody(const QString& content, const QString& symbol, QString& body, QString& error)
	{
		const QRegularExpression declarationExpression(
			QString(R"(%1\s*\[\s*(?:\d+)?\s*\]\s*=\s*\{)").arg(QRegularExpression::escape(symbol)));
		const QRegularExpressionMatch declarationMatch = declarationExpression.match(content);
		if (!declarationMatch.hasMatch())
		{
			error = QString("Missing required array '%1'").arg(symbol);
			return false;
		}

		const int openBraceIndex = content.indexOf('{', declarationMatch.capturedStart());
		if (openBraceIndex < 0)
		{
			error = QString("Array '%1' is missing an opening brace").arg(symbol);
			return false;
		}

		int depth = 0;
		for (int index = openBraceIndex; index < content.size(); ++index)
		{
			const QChar character = content.at(index);
			if (character == '{')
				++depth;
			else if (character == '}')
			{
				--depth;
				if (depth == 0)
				{
					body = content.mid(openBraceIndex + 1, index - openBraceIndex - 1);
					return true;
				}
			}
		}

		error = QString("Array '%1' is missing a closing brace").arg(symbol);
		return false;
	}

	bool parseSolverProfileHeaderLadderArray(
		const QString& content,
		const QString& symbol,
		std::vector<DriverOtherRawHid::SolverLadderEntry>& output,
		QString& error)
	{
		QString body;
		if (!extractSolverProfileArrayBody(content, symbol, body, error))
			return false;

		const QRegularExpression entryExpression(
			R"(\{\s*((?:0x[0-9a-fA-F]+)|(?:\d+))\s*,\s*((?:0x[0-9a-fA-F]+)|(?:\d+))\s*,\s*((?:0x[0-9a-fA-F]+)|(?:\d+))\s*\})");
		QRegularExpressionMatchIterator iterator = entryExpression.globalMatch(body);

		output.clear();
		while (iterator.hasNext())
		{
			const QRegularExpressionMatch match = iterator.next();

			bool outputOk = false;
			bool valueOk = false;
			bool bfiOk = false;
			const uint outputQ16 = match.captured(1).toUInt(&outputOk, 0);
			const uint value = match.captured(2).toUInt(&valueOk, 0);
			const uint bfi = match.captured(3).toUInt(&bfiOk, 0);
			if (!outputOk || !valueOk || !bfiOk || outputQ16 > 65535u || value > 255u || bfi > SOLVER_HOST_MAX_BFI)
			{
				error = QString("Array '%1' contains an invalid ladder entry").arg(symbol);
				output.clear();
				return false;
			}

			output.push_back(DriverOtherRawHid::SolverLadderEntry{
				static_cast<uint16_t>(outputQ16),
				static_cast<uint8_t>(value),
				static_cast<uint8_t>(bfi),
				static_cast<uint8_t>(value),
				static_cast<uint8_t>(value)
			});
		}

		if (output.empty())
		{
			error = QString("Array '%1' must contain at least one ladder entry").arg(symbol);
			return false;
		}

		return true;
	}

	bool parseSolverProfileHeaderBoundArray(
		const QString& content,
		const QString& symbol,
		std::vector<DriverOtherRawHid::SolverLadderEntry>& target,
		const bool lowerBound,
		QString& error)
	{
		QString body;
		if (!extractSolverProfileArrayBody(content, symbol, body, error))
			return false;

		const QRegularExpression valueExpression(R"((?:0x[0-9a-fA-F]+)|(?:\d+))");
		QRegularExpressionMatchIterator iterator = valueExpression.globalMatch(body);
		std::vector<uint8_t> values;
		values.reserve(target.size());

		while (iterator.hasNext())
		{
			bool ok = false;
			const uint value = iterator.next().captured(0).toUInt(&ok, 0);
			if (!ok || value > 255u)
			{
				error = QString("Array '%1' contains an invalid 8-bit value").arg(symbol);
				return false;
			}
			values.push_back(static_cast<uint8_t>(value));
		}

		if (values.size() != target.size())
		{
			error = QString("Array '%1' contains %2 entries, expected %3")
				.arg(symbol)
				.arg(values.size())
				.arg(target.size());
			return false;
		}

		for (size_t index = 0u; index < target.size(); ++index)
		{
			if (lowerBound)
				target[index].lowerValue = values[index];
			else
				target[index].upperValue = values[index];
		}

		return true;
	}

	bool parseSolverProfileHeaderContent(
		const QString& content,
		std::array<std::vector<DriverOtherRawHid::SolverLadderEntry>, 4>& output,
		QString& error)
	{
		const QString cleaned = stripSolverProfileComments(content);
		output = {};

		if (!parseSolverProfileHeaderLadderArray(cleaned, "LADDER_R", output[1], error) ||
			!parseSolverProfileHeaderLadderArray(cleaned, "LADDER_G", output[0], error) ||
			!parseSolverProfileHeaderLadderArray(cleaned, "LADDER_B", output[2], error) ||
			!parseSolverProfileHeaderLadderArray(cleaned, "LADDER_W", output[3], error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_R_LOWER", output[1], true, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_G_LOWER", output[0], true, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_B_LOWER", output[2], true, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_W_LOWER", output[3], true, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_R_UPPER", output[1], false, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_G_UPPER", output[0], false, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_B_UPPER", output[2], false, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_W_UPPER", output[3], false, error))
		{
			return false;
		}

		for (auto& channelEntries : output)
		{
			std::sort(channelEntries.begin(), channelEntries.end(), [](const DriverOtherRawHid::SolverLadderEntry& left, const DriverOtherRawHid::SolverLadderEntry& right) {
				if (left.outputQ16 != right.outputQ16)
					return left.outputQ16 < right.outputQ16;
				if (left.bfi != right.bfi)
					return left.bfi > right.bfi;
				return left.value < right.value;
			});
		}

		return true;
	}

	inline uint16_t floatToBucket12(const float value)
	{
		const float clamped = std::clamp(value, 0.0f, 1.0f);
		return static_cast<uint16_t>(std::lround(clamped * RGB12_BUCKET_MAX));
	}

	inline uint16_t expand8ToBucket12(const uint8_t value)
	{
		return static_cast<uint16_t>((static_cast<uint32_t>(value) * RGB12_BUCKET_MAX + 127u) / 255u);
	}

	inline uint8_t floatToByte(const float value)
	{
		const float clamped = std::clamp(value, 0.0f, 1.0f);
		return static_cast<uint8_t>(std::lround(clamped * 255.0f));
	}

	inline uint16_t expand8ToQ16(const uint8_t value)
	{
		return static_cast<uint16_t>((static_cast<uint16_t>(value) << 8u) | value);
	}

	inline uint8_t clampPolicyBfiIndex(const uint8_t bfi)
	{
		return (bfi > SOLVER_HOST_MAX_BFI) ? SOLVER_HOST_MAX_BFI : bfi;
	}

	inline float computeWeightedLumaUnit(
		const float red,
		const float green,
		const float blue,
		const float white,
		const uint8_t redBfi,
		const uint8_t greenBfi,
		const uint8_t blueBfi,
		const uint8_t whiteBfi,
		const bool includeWhite)
	{
		const float redWeight = static_cast<float>(RGBW_LUMA_R_Q16_BY_BFI[clampPolicyBfiIndex(redBfi)]);
		const float greenWeight = static_cast<float>(RGBW_LUMA_G_Q16_BY_BFI[clampPolicyBfiIndex(greenBfi)]);
		const float blueWeight = static_cast<float>(RGBW_LUMA_B_Q16_BY_BFI[clampPolicyBfiIndex(blueBfi)]);
		const float whiteWeight = includeWhite ? static_cast<float>(RGBW_LUMA_W_Q16_BY_BFI[clampPolicyBfiIndex(whiteBfi)]) : 0.0f;
		const float denominator = redWeight + greenWeight + blueWeight + whiteWeight;

		if (denominator <= 0.0f)
			return 0.0f;

		const float weighted =
			(red * redWeight) +
			(green * greenWeight) +
			(blue * blueWeight) +
			(includeWhite ? (white * whiteWeight) : 0.0f);

		return std::clamp(weighted / denominator, 0.0f, 1.0f);
	}

	inline uint8_t computeRgbWeightedLuma8(const uint8_t red, const uint8_t green, const uint8_t blue)
	{
		return floatToByte(computeWeightedLumaUnit(
			static_cast<float>(red) / 255.0f,
			static_cast<float>(green) / 255.0f,
			static_cast<float>(blue) / 255.0f,
			0.0f,
			0u,
			0u,
			0u,
			0u,
			false));
	}

	inline float computeRgbWeightedLumaUnit(const float red, const float green, const float blue)
	{
		return computeWeightedLumaUnit(red, green, blue, 0.0f, 0u, 0u, 0u, 0u, false);
	}

	inline float q16ToUnitFloat(const uint16_t value)
	{
		return static_cast<float>(value) / 65535.0f;
	}

	inline float computeHostOwnedRgbwEnergyUnit(const DriverOtherRawHid::HostOwnedRgbwQ16& rgbw)
	{
		return computeWeightedLumaUnit(
			q16ToUnitFloat(rgbw.red),
			q16ToUnitFloat(rgbw.green),
			q16ToUnitFloat(rgbw.blue),
			q16ToUnitFloat(rgbw.white),
			0u,
			0u,
			0u,
			0u,
			true);
	}

	inline uint8_t energyUnitToByte(const float energy)
	{
		return static_cast<uint8_t>(std::clamp(std::lround(std::clamp(energy, 0.0f, 1.0f) * 255.0f), 0l, 255l));
	}

	inline uint8_t computeRgbwWeightedLuma8(const uint8_t red, const uint8_t green, const uint8_t blue, const uint8_t white)
	{
		return floatToByte(computeWeightedLumaUnit(
			static_cast<float>(red) / 255.0f,
			static_cast<float>(green) / 255.0f,
			static_cast<float>(blue) / 255.0f,
			static_cast<float>(white) / 255.0f,
			0u,
			0u,
			0u,
			0u,
			true));
	}

	template <size_t N>
	inline int8_t classifySceneOffsetTargetForBoundaries(const float value, const float hysteresis, const std::array<float, N>& boundaries)
	{
		if (value < boundaries[0] - hysteresis)
			return -2;
		if (value < boundaries[1] - hysteresis)
			return -1;
		if (value < boundaries[2] - hysteresis)
			return 0;
		if (value < boundaries[3] - hysteresis)
			return 1;
		return 2;
	}

	template <size_t N>
	inline bool isSceneOffsetInHysteresisWindowForBoundaries(const float value, const float configuredHysteresis, const std::array<float, N>& boundaries, const float domainMin, const float domainMax)
	{
		for (size_t index = 0; index < boundaries.size(); ++index)
		{
			const float boundary = boundaries[index];
			const float prevBoundary = (index == 0u) ? domainMin : boundaries[index - 1u];
			const float nextBoundary = (index + 1u < boundaries.size()) ? boundaries[index + 1u] : domainMax;
			const float lowerGap = boundary - prevBoundary;
			const float upperGap = nextBoundary - boundary;
			const float maxUsableHysteresis = std::max(0.0f, (std::min(lowerGap, upperGap) * 0.5f) - 0.001f);
			const float effectiveHysteresis = std::clamp(configuredHysteresis, 0.0f, maxUsableHysteresis);
			if (effectiveHysteresis > 0.0f && value >= boundary - effectiveHysteresis && value <= boundary + effectiveHysteresis)
				return true;
		}

		return false;
	}

	inline uint8_t scaleFramesByInputFps(
		const uint8_t baseFrames,
		const double inputFps,
		const uint8_t baseFps,
		const uint8_t minScaleX100,
		const uint8_t maxScaleX100)
	{
		if (baseFrames == 0u || inputFps <= 0.0)
			return baseFrames;

		uint32_t ratioX100 = static_cast<uint32_t>(std::lround((inputFps * 100.0) / static_cast<double>(baseFps)));
		if (ratioX100 < static_cast<uint32_t>(minScaleX100))
			ratioX100 = static_cast<uint32_t>(minScaleX100);
		else if (ratioX100 > static_cast<uint32_t>(maxScaleX100))
			ratioX100 = static_cast<uint32_t>(maxScaleX100);

		uint32_t scaled = (static_cast<uint32_t>(baseFrames) * ratioX100 + 50u) / 100u;
		if (scaled < 1u)
			scaled = 1u;
		if (scaled > 255u)
			scaled = 255u;
		return static_cast<uint8_t>(scaled);
	}

	inline uint8_t computeHighlightThresholdFromHistogram(const uint16_t* histogram, const uint16_t target)
	{
		uint16_t accumulated = 0;
		for (int level = 255; level >= 0; --level)
		{
			accumulated = static_cast<uint16_t>(accumulated + histogram[level]);
			if (accumulated >= target)
				return static_cast<uint8_t>(level);
		}
		return 255u;
	}

	inline uint8_t computeHistogramPercentileLevel(const uint16_t* histogram, const uint16_t population, const uint16_t permille)
	{
		if (population == 0u)
			return 0u;

		uint32_t target = ((uint32_t)population * permille + 999u) / 1000u;
		if (target == 0u)
			target = 1u;

		uint32_t accumulated = 0u;
		for (uint16_t level = 0; level < 256u; ++level)
		{
			accumulated += histogram[level];
			if (accumulated >= target)
				return static_cast<uint8_t>(level);
		}
		return 255u;
	}

	using SolverEncodedState = DriverOtherRawHid::SolverEncodedState;

	using SolverLookupTables = DriverOtherRawHid::SolverLookupTables;

	static constexpr uint8_t SOLVER_HOST_CHANNEL_COUNT = 4u;

	inline const char* scenePolicyModeToString(const DriverOtherRawHid::ScenePolicyMode mode)
	{
		switch (mode)
		{
			case DriverOtherRawHid::ScenePolicyMode::SolverAwareV2:
				return "solverAwareV2";
			case DriverOtherRawHid::ScenePolicyMode::SolverAwareV3:
				return "solverAwareV3";
			default:
				return "disabled";
		}
	}

	inline bool scenePolicyUsesHostSolver(const DriverOtherRawHid::ScenePolicyMode mode)
	{
		return mode == DriverOtherRawHid::ScenePolicyMode::SolverAwareV2 || mode == DriverOtherRawHid::ScenePolicyMode::SolverAwareV3;
	}

	inline bool scenePolicyUsesHighlightSelection(const DriverOtherRawHid::ScenePolicyMode mode)
	{
		return mode == DriverOtherRawHid::ScenePolicyMode::SolverAwareV2 || mode == DriverOtherRawHid::ScenePolicyMode::SolverAwareV3;
	}

	inline bool scenePolicyUsesDirectSolverEnergyModel(const DriverOtherRawHid::ScenePolicyMode mode)
	{
		return mode == DriverOtherRawHid::ScenePolicyMode::SolverAwareV3;
	}

	inline uint16_t absDiffU16(const uint16_t a, const uint16_t b)
	{
		return (a > b) ? static_cast<uint16_t>(a - b) : static_cast<uint16_t>(b - a);
	}

	inline size_t q16ToSolverBucketIndex(const uint16_t q16, const size_t bucketCount)
	{
		if (bucketCount <= 1u)
			return 0u;
		return static_cast<size_t>((static_cast<uint64_t>(q16) * static_cast<uint64_t>(bucketCount - 1u) + 32767u) / 65535u);
	}

	inline uint16_t allowedErrorQ16(const uint16_t targetQ16)
	{
		constexpr uint16_t minErrorQ16 = 64u;
		constexpr uint16_t relativeErrorDivisor = 24u;
		const uint16_t relative = static_cast<uint16_t>(targetQ16 / relativeErrorDivisor);
		return (relative > minErrorQ16) ? relative : minErrorQ16;
	}

	inline bool passesResolutionGuard(const uint8_t input8Approx, const uint8_t candidateValue)
	{
		constexpr uint8_t minValueRatioNumerator = 3u;
		constexpr uint8_t minValueRatioDenominator = 8u;
		constexpr uint8_t lowEndProtectThreshold = 48u;
		constexpr uint8_t lowEndMaxDrop = 10u;

		if (input8Approx == 0u)
			return candidateValue == 0u;

		const uint16_t minAllowedByRatio =
			(static_cast<uint16_t>(input8Approx) * minValueRatioNumerator) / minValueRatioDenominator;
		if (candidateValue < minAllowedByRatio)
			return false;

		if (input8Approx <= lowEndProtectThreshold)
		{
			const uint8_t minAllowedLow = (input8Approx > lowEndMaxDrop)
				? static_cast<uint8_t>(input8Approx - lowEndMaxDrop)
				: 0u;
			if (candidateValue < minAllowedLow)
				return false;
		}

		return true;
	}

	inline std::array<uint16_t, 4> applyHighlightPolicyQ16Scale(
		const std::array<uint16_t, 4>& processedQ16,
		const bool highlighted,
		const bool includeWhite,
		const uint16_t q16Threshold)
	{
		if (highlighted)
			return processedQ16;

		const uint16_t maxQ16 = includeWhite
			? std::max(std::max(processedQ16[0], processedQ16[1]), std::max(processedQ16[2], processedQ16[3]))
			: std::max(std::max(processedQ16[0], processedQ16[1]), processedQ16[2]);

		if (maxQ16 == 0u || maxQ16 <= q16Threshold)
			return processedQ16;

		std::array<uint16_t, 4> scaled = processedQ16;
		const size_t channelCount = includeWhite ? 4u : 3u;
		for (size_t index = 0u; index < channelCount; ++index)
		{
			const uint32_t scaledQ16 =
				(static_cast<uint32_t>(processedQ16[index]) * static_cast<uint32_t>(q16Threshold) + (maxQ16 / 2u)) /
				static_cast<uint32_t>(maxQ16);
			scaled[index] = static_cast<uint16_t>(std::min<uint32_t>(scaledQ16, 65535u));
		}

		return scaled;
	}

	inline bool highlightMaskBitSet(const std::vector<uint8_t>& highlightMask, const size_t index)
	{
		const size_t byteIndex = index >> 3u;
		if (byteIndex >= highlightMask.size())
			return false;
		return (highlightMask[byteIndex] & static_cast<uint8_t>(1u << (index & 7u))) != 0u;
	}

	inline uint32_t countHighlightMaskBits(const std::vector<uint8_t>& highlightMask)
	{
		uint32_t count = 0u;
		for (uint8_t bits : highlightMask)
		{
			while (bits != 0u)
			{
				bits = static_cast<uint8_t>(bits & static_cast<uint8_t>(bits - 1u));
				++count;
			}
		}
		return count;
	}

	inline void writeQ16RgbAs16Bit(uint8_t*& writer, const uint16_t red, const uint16_t green, const uint16_t blue)
	{
		*(writer++) = static_cast<uint8_t>(red >> 8);
		*(writer++) = static_cast<uint8_t>(red & 0xFFu);
		*(writer++) = static_cast<uint8_t>(green >> 8);
		*(writer++) = static_cast<uint8_t>(green & 0xFFu);
		*(writer++) = static_cast<uint8_t>(blue >> 8);
		*(writer++) = static_cast<uint8_t>(blue & 0xFFu);
	}

	inline void writeQ16RgbwAs16Bit(uint8_t*& writer, const uint16_t red, const uint16_t green, const uint16_t blue, const uint16_t white)
	{
		*(writer++) = static_cast<uint8_t>(red >> 8);
		*(writer++) = static_cast<uint8_t>(red & 0xFFu);
		*(writer++) = static_cast<uint8_t>(green >> 8);
		*(writer++) = static_cast<uint8_t>(green & 0xFFu);
		*(writer++) = static_cast<uint8_t>(blue >> 8);
		*(writer++) = static_cast<uint8_t>(blue & 0xFFu);
		*(writer++) = static_cast<uint8_t>(white >> 8);
		*(writer++) = static_cast<uint8_t>(white & 0xFFu);
	}

	inline uint16_t q16ToBucket12(const uint16_t valueQ16)
	{
		return static_cast<uint16_t>((static_cast<uint32_t>(valueQ16) * RGB12_BUCKET_MAX + 32767u) / 65535u);
	}

	inline void writeQ16RgbAs12BitCarry(uint8_t*& writer, const uint16_t redQ16, const uint16_t greenQ16, const uint16_t blueQ16)
	{
		const uint16_t red = q16ToBucket12(redQ16);
		const uint16_t green = q16ToBucket12(greenQ16);
		const uint16_t blue = q16ToBucket12(blueQ16);

		const uint8_t redBase = static_cast<uint8_t>(red >> 4);
		const uint8_t greenBase = static_cast<uint8_t>(green >> 4);
		const uint8_t blueBase = static_cast<uint8_t>(blue >> 4);
		const uint8_t rgCarry = static_cast<uint8_t>(((red & 0x0Fu) << 4) | (green & 0x0Fu));
		const uint8_t bCarryReserved = static_cast<uint8_t>((blue & 0x0Fu) << 4);

		*(writer++) = redBase;
		*(writer++) = greenBase;
		*(writer++) = blueBase;
		*(writer++) = rgCarry;
		*(writer++) = bCarryReserved;
	}

	inline void writeQ16RgbwAs12BitCarry(uint8_t*& writer, const uint16_t redQ16, const uint16_t greenQ16, const uint16_t blueQ16, const uint16_t whiteQ16)
	{
		const uint16_t red = q16ToBucket12(redQ16);
		const uint16_t green = q16ToBucket12(greenQ16);
		const uint16_t blue = q16ToBucket12(blueQ16);
		const uint16_t white = q16ToBucket12(whiteQ16);

		const uint8_t redBase = static_cast<uint8_t>(red >> 4);
		const uint8_t greenBase = static_cast<uint8_t>(green >> 4);
		const uint8_t blueBase = static_cast<uint8_t>(blue >> 4);
		const uint8_t whiteBase = static_cast<uint8_t>(white >> 4);
		const uint8_t rgCarry = static_cast<uint8_t>(((red & 0x0Fu) << 4) | (green & 0x0Fu));
		const uint8_t bwCarry = static_cast<uint8_t>(((blue & 0x0Fu) << 4) | (white & 0x0Fu));

		*(writer++) = redBase;
		*(writer++) = greenBase;
		*(writer++) = blueBase;
		*(writer++) = whiteBase;
		*(writer++) = rgCarry;
		*(writer++) = bwCarry;
	}

	inline void writeBucket12RgbwAs12BitCarry(uint8_t*& writer, const uint16_t red, const uint16_t green, const uint16_t blue, const uint16_t white)
	{
		const uint8_t redBase = static_cast<uint8_t>(red >> 4);
		const uint8_t greenBase = static_cast<uint8_t>(green >> 4);
		const uint8_t blueBase = static_cast<uint8_t>(blue >> 4);
		const uint8_t whiteBase = static_cast<uint8_t>(white >> 4);
		const uint8_t rgCarry = static_cast<uint8_t>(((red & 0x0Fu) << 4) | (green & 0x0Fu));
		const uint8_t bwCarry = static_cast<uint8_t>(((blue & 0x0Fu) << 4) | (white & 0x0Fu));

		*(writer++) = redBase;
		*(writer++) = greenBase;
		*(writer++) = blueBase;
		*(writer++) = whiteBase;
		*(writer++) = rgCarry;
		*(writer++) = bwCarry;
	}

	inline void writeBucket12AsCarry(uint8_t*& writer, const uint16_t bucket12)
	{
		const uint8_t coarse = static_cast<uint8_t>((bucket12 >> 4) & 0xFFu);
		const uint8_t carry = static_cast<uint8_t>(bucket12 & 0x0Fu);
		*(writer++) = coarse;
		*(writer++) = carry;
	}

	constexpr int DEFAULT_WRITE_TIMEOUT_MS = 1000;
	constexpr int DEFAULT_REPORT_SIZE = 64;
	constexpr int MAX_WRITE_TIMEOUTS = 5;
	constexpr int DEFAULT_RETRY_INTERVAL_MS = 2500;
	constexpr int HID_HEADER_SIZE = 4;
	constexpr uint8_t HID_MAGIC_0 = 'H';
	constexpr uint8_t HID_MAGIC_1 = 'D';
	constexpr uint8_t HID_LOG_MAGIC_1 = 'L';
	constexpr int DEFAULT_LOG_POLL_INTERVAL_FRAMES = 32;
	constexpr int DEFAULT_LOG_READ_BUDGET = 8;

	inline uint16_t addMod255Fast(uint16_t acc, uint16_t value)
	{
		acc = static_cast<uint16_t>(acc + value);
		if (acc >= 255u)
			acc = static_cast<uint16_t>(acc - 255u);
		if (acc >= 255u)
			acc = static_cast<uint16_t>(acc - 255u);
		return acc;
	}

	inline bool parseVidPidFromHidId(const QString& hidId, uint16_t& vid, uint16_t& pid)
	{
		const QStringList parts = hidId.split(':');
		if (parts.size() < 3)
			return false;

		bool okVid = false;
		bool okPid = false;
		const uint valueVid = parts[1].toUInt(&okVid, 16);
		const uint valuePid = parts[2].toUInt(&okPid, 16);

		if (!okVid || !okPid || valueVid > 0xFFFFu || valuePid > 0xFFFFu)
			return false;

		vid = static_cast<uint16_t>(valueVid);
		pid = static_cast<uint16_t>(valuePid);
		return true;
	}
}

DriverOtherRawHid::DriverOtherRawHid(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _devicePath("auto")
	, _isAutoDevicePath(true)
	, _vid(0x16C0)
	, _pid(0x04D6)
	, _delayAfterConnect_ms(0)
	, _writeTimeout_ms(DEFAULT_WRITE_TIMEOUT_MS)
	, _reportSize(DEFAULT_REPORT_SIZE)
	, _readLogChannel(true)
	, _logPollIntervalFrames(DEFAULT_LOG_POLL_INTERVAL_FRAMES)
	, _logReadBudget(DEFAULT_LOG_READ_BUDGET)
	, _logPollCounter(0)
	, _incomingLogLine("")
	, _frameDropCounter(0)
	, _fd(-1)
	, _configurationPath(QDir(QDir::homePath()).absoluteFilePath(".hyperhdr"))
	, _payloadMode(PayloadMode::Rgb16)
	, _transferCurveOwner(TransferCurveOwner::Teensy)
	, _transferCurveProfile(TransferCurveProfile::Curve3_4New)
	, _transferCurveCustomProfileKey("")
	, _transferCurveCustomLut{}
	, _runtimeInterpolatedTransferCurveActive(false)
	, _runtimeInterpolatedTransferCurveLowerProfileId("")
	, _runtimeInterpolatedTransferCurveUpperProfileId("")
	, _runtimeInterpolatedTransferCurveBlend(0)
	, _calibrationHeaderOwner(CalibrationHeaderOwner::Teensy)
	, _calibrationHeaderProfile(CalibrationHeaderProfile::Disabled)
	, _calibrationHeaderCustomProfileKey("")
	, _calibrationHeaderCustomLut{}
	, _rgbwLutProfileId("disabled")
	, _rgbwLutCustomGrid{}
	, _solverProfileId("builtin")
	, _solverLadders{}
	, _solverLookupTables{}
	, _hostColorTransformOrder(HostColorTransformOrder::TransferThenCalibration)
	, _hostRgbwLutTransferOrder(HostRgbwLutTransferOrder::TransferBeforeLut)
	, _scenePolicyMode(ScenePolicyMode::Disabled)
	, _scenePolicyEnableHighlight(true)
	, _highlightShadowQ16Threshold(DEFAULT_HIGHLIGHT_SHADOW_Q16_THRESHOLD)
	, _highlightShadowPercent(DEFAULT_HIGHLIGHT_SHADOW_PERCENT)
	, _highlightShadowMinPeakDelta(DEFAULT_HIGHLIGHT_SHADOW_MIN_PEAK_DELTA)
	, _highlightShadowUniformSpreadMax(DEFAULT_HIGHLIGHT_SHADOW_UNIFORM_SPREAD_MAX)
	, _highlightShadowTriggerMargin(DEFAULT_HIGHLIGHT_SHADOW_TRIGGER_MARGIN)
	, _highlightShadowMinSceneMedian(DEFAULT_HIGHLIGHT_SHADOW_MIN_SCENE_MEDIAN)
	, _highlightShadowMinSceneAvg(DEFAULT_HIGHLIGHT_SHADOW_MIN_SCENE_AVG)
	, _highlightShadowDimUniformMedian(DEFAULT_HIGHLIGHT_SHADOW_DIM_UNIFORM_MEDIAN)
	, _hostRgbwMode(HostRgbwMode::Disabled)
	, _enable_ice_rgbw(false)
	, _hostRgbwApplyD65Correction(false)
	, _hostRgbwD65Strength(DEFAULT_HOST_RGBW_D65_STRENGTH)
	, _ice_rgbw_temporal_dithering(false)
	, _hostRgbwGatedUvRadius(DEFAULT_HOST_RGBW_GATED_UV_RADIUS)
	, _hostRgbwGatedPower(DEFAULT_HOST_RGBW_GATED_POWER)
	, _hostRgbwCentroidRedX(static_cast<float>(DEFAULT_HOST_RGBW_RED_XY[0]))
	, _hostRgbwCentroidRedY(static_cast<float>(DEFAULT_HOST_RGBW_RED_XY[1]))
	, _hostRgbwCentroidGreenX(static_cast<float>(DEFAULT_HOST_RGBW_GREEN_XY[0]))
	, _hostRgbwCentroidGreenY(static_cast<float>(DEFAULT_HOST_RGBW_GREEN_XY[1]))
	, _hostRgbwCentroidBlueX(static_cast<float>(DEFAULT_HOST_RGBW_BLUE_XY[0]))
	, _hostRgbwCentroidBlueY(static_cast<float>(DEFAULT_HOST_RGBW_BLUE_XY[1]))
	, _hostRgbwCentroidWhiteX(static_cast<float>(DEFAULT_HOST_RGBW_WHITE_XY[0]))
	, _hostRgbwCentroidWhiteY(static_cast<float>(DEFAULT_HOST_RGBW_WHITE_XY[1]))
	, _ice_white_temperatur{ 1.0f, 1.0f, 1.0f }
	, _ice_white_mixer_threshold(0.0f)
	, _ice_white_led_intensity(1.8f)
	, _hostRgbwReferenceValid(false)
	, _white_channel_calibration(false)
	, _white_channel_limit(255)
	, _white_channel_red(255)
	, _white_channel_green(255)
	, _white_channel_blue(255)
	, _scenePolicyStatsFrames(0u)
	, _scenePolicyStatsHighlightFrames(0u)
	, _scenePolicyStatsHighlightedLeds(0u)
	, _scenePolicyStatsHighlightPeakLeds(0u)
	, _scenePolicyLastHighlightAvg(0u)
	, _scenePolicyLastHighlightMedian(0u)
	, _scenePolicyLastHighlightP95(0u)
	, _scenePolicyLastHighlightSpread(0u)
	, _scenePolicyLastHighlightGate(0u)
	, _scenePolicyLastHighlightCandidates(0u)
	, _scenePolicyLastHighlightRejectCode(0u)
	, _headerSize(6)
{
}

bool DriverOtherRawHid::parseHexOrDecimal(const QString& text, uint16_t& out) const
{
	bool ok = false;
	const uint value = text.trimmed().toUInt(&ok, 0);
	if (!ok || value > 0xFFFFu)
		return false;

	out = static_cast<uint16_t>(value);
	return true;
}

DriverOtherRawHid::PayloadMode DriverOtherRawHid::parsePayloadMode(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized == "rgb12carry" || normalized == "rgb12-carry" || normalized == "rgb12_carry")
		return PayloadMode::Rgb12Carry;
	return PayloadMode::Rgb16;
}

DriverOtherRawHid::TransferCurveOwner DriverOtherRawHid::parseTransferCurveOwner(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized.isEmpty() || normalized == "disabled" || normalized == "off" || normalized == "none")
		return TransferCurveOwner::Disabled;
	if (normalized == "hyperhdr" || normalized == "host")
		return TransferCurveOwner::HyperHdr;
	return TransferCurveOwner::Teensy;
}

DriverOtherRawHid::TransferCurveProfile DriverOtherRawHid::parseTransferCurveProfile(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized.isEmpty() || normalized == "disabled")
		return TransferCurveProfile::Disabled;
	if (normalized == "curve3_4_new" || normalized == "3_4_new" || normalized == "3.4_new")
		return TransferCurveProfile::Curve3_4New;
	if (normalized.startsWith("custom:"))
		return TransferCurveProfile::Custom;
	return TransferCurveProfile::Custom;
}

QString DriverOtherRawHid::parseCustomTransferCurveProfileKey(const QString& text) const
{
	QString normalized = text.trimmed();
	if (normalized.startsWith("custom:", Qt::CaseInsensitive))
		normalized.remove(0, QString("custom:").size());
	return sanitizeTransferCurveProfileKey(normalized);
}

QString DriverOtherRawHid::currentTransferCurveProfileId() const
{
	if (_runtimeInterpolatedTransferCurveActive)
		return "runtime-interpolated";

	if (_transferCurveProfile == TransferCurveProfile::Disabled)
		return "disabled";
	if (_transferCurveProfile == TransferCurveProfile::Curve3_4New)
		return "curve3_4_new";
	if (_transferCurveProfile == TransferCurveProfile::Custom)
		return _transferCurveCustomProfileKey.isEmpty() ? QString("custom:") : QString("custom:%1").arg(_transferCurveCustomProfileKey);
	return QString();
}

bool DriverOtherRawHid::isRuntimeInterpolatedTransferCurveActive() const
{
	return _runtimeInterpolatedTransferCurveActive;
}

QJsonObject DriverOtherRawHid::getRuntimeTransferCurveState() const
{
	QJsonObject state;
	state["supported"] = true;
	state["activeRuntime"] = true;
	state["ledDeviceType"] = getActiveDeviceType();
	state["owner"] = (_transferCurveOwner == TransferCurveOwner::HyperHdr) ? "hyperhdr" : ((_transferCurveOwner == TransferCurveOwner::Teensy) ? "teensy" : "disabled");
	state["profile"] = currentTransferCurveProfileId();
	state["profileExists"] = true;
	state["canSetProfile"] = (_transferCurveOwner == TransferCurveOwner::HyperHdr);
	state["interpolationActive"] = _runtimeInterpolatedTransferCurveActive;
	if (_runtimeInterpolatedTransferCurveActive)
	{
		state["interpolationLowerProfile"] = _runtimeInterpolatedTransferCurveLowerProfileId;
		state["interpolationUpperProfile"] = _runtimeInterpolatedTransferCurveUpperProfileId;
		state["interpolationBlend"] = _runtimeInterpolatedTransferCurveBlend;
		state["interpolationRatio"] = static_cast<double>(_runtimeInterpolatedTransferCurveBlend) / 255.0;
	}
	state["daytimeUpliftEnabled"] = _daytimeUpliftEnabled;
	if (_daytimeUpliftEnabled)
	{
		state["daytimeUpliftBlend"] = static_cast<int>(_daytimeUpliftBlend);
		state["daytimeUpliftRatio"] = static_cast<double>(_daytimeUpliftBlend) / 255.0;
		state["daytimeUpliftProfile"] = _daytimeUpliftProfileId.isEmpty() ? QStringLiteral("curve3_4_new") : _daytimeUpliftProfileId;
	}
	return state;
}

QString DriverOtherRawHid::setRuntimeTransferCurveProfile(QString profileId)
{
	profileId = profileId.trimmed();
	if (_transferCurveOwner != TransferCurveOwner::HyperHdr)
		return "Active transfer curve switching requires transferCurveOwner to be set to 'hyperhdr'";

	const bool requestInterpolatedRuntimeCurve = profileId.startsWith(RUNTIME_INTERPOLATED_TRANSFER_CURVE_PREFIX, Qt::CaseInsensitive);

	const TransferCurveProfile previousProfile = _transferCurveProfile;
	const QString previousCustomKey = _transferCurveCustomProfileKey;
	const CustomTransferCurveLut previousCustomLut = _transferCurveCustomLut;
	const bool previousInterpolationActive = _runtimeInterpolatedTransferCurveActive;
	const QString previousInterpolationLower = _runtimeInterpolatedTransferCurveLowerProfileId;
	const QString previousInterpolationUpper = _runtimeInterpolatedTransferCurveUpperProfileId;
	const int previousInterpolationBlend = _runtimeInterpolatedTransferCurveBlend;

	if (requestInterpolatedRuntimeCurve)
	{
		const QString encodedPayload = profileId.mid(static_cast<int>(std::strlen(RUNTIME_INTERPOLATED_TRANSFER_CURVE_PREFIX)));
		const QStringList parts = encodedPayload.split('|', Qt::KeepEmptyParts);
		if (parts.size() != 3)
			return "Interpolated runtime transfer curve payload is invalid";

		bool blendOk = false;
		const QString lowerProfileId = parts.at(0).trimmed();
		const QString upperProfileId = parts.at(1).trimmed();
		const int blend = parts.at(2).trimmed().toInt(&blendOk);
		if (lowerProfileId.isEmpty() || upperProfileId.isEmpty() || !blendOk || blend <= 0 || blend >= 255)
			return "Interpolated runtime transfer curve payload is invalid";

		CustomTransferCurveLut interpolatedLut;
		QString error;
		if (!buildRuntimeInterpolatedTransferCurveLut(lowerProfileId, upperProfileId, blend, interpolatedLut, error))
			return error.isEmpty() ? "Could not build interpolated runtime transfer curve" : error;

		_transferCurveProfile = TransferCurveProfile::Custom;
		_transferCurveCustomProfileKey.clear();
		_transferCurveCustomLut = interpolatedLut;
		_runtimeInterpolatedTransferCurveActive = true;
		_runtimeInterpolatedTransferCurveLowerProfileId = lowerProfileId;
		_runtimeInterpolatedTransferCurveUpperProfileId = upperProfileId;
		_runtimeInterpolatedTransferCurveBlend = blend;

		_ice_rgbw_carry_error.clear();
		createHeader();

		Info(_log, "Runtime transfer curve interpolated without device reset: {:s} + {:s} ({:d}/255)", lowerProfileId, upperProfileId, blend);
		requestCurrentFrameRefresh();
		return QString();
	}

	const TransferCurveProfile requestedProfile = parseTransferCurveProfile(profileId);
	const QString requestedCustomKey = (requestedProfile == TransferCurveProfile::Custom) ? parseCustomTransferCurveProfileKey(profileId) : QString();
	if (requestedProfile == TransferCurveProfile::Custom && requestedCustomKey.isEmpty())
		return "Custom transfer curve profile id is invalid";

	_transferCurveProfile = requestedProfile;
	_transferCurveCustomProfileKey = requestedCustomKey;
	_runtimeInterpolatedTransferCurveActive = false;
	_runtimeInterpolatedTransferCurveLowerProfileId.clear();
	_runtimeInterpolatedTransferCurveUpperProfileId.clear();
	_runtimeInterpolatedTransferCurveBlend = 0;
	if (_transferCurveProfile == TransferCurveProfile::Disabled || _transferCurveProfile == TransferCurveProfile::Curve3_4New)
		_transferCurveCustomLut = {};
	else if (_transferCurveProfile == TransferCurveProfile::Custom)
	{
		if (!loadCustomTransferCurveProfile())
		{
			_transferCurveProfile = previousProfile;
			_transferCurveCustomProfileKey = previousCustomKey;
			_transferCurveCustomLut = previousCustomLut;
			_runtimeInterpolatedTransferCurveActive = previousInterpolationActive;
			_runtimeInterpolatedTransferCurveLowerProfileId = previousInterpolationLower;
			_runtimeInterpolatedTransferCurveUpperProfileId = previousInterpolationUpper;
			_runtimeInterpolatedTransferCurveBlend = previousInterpolationBlend;
			return QString("Transfer header profile not found or invalid: custom:%1").arg(requestedCustomKey);
		}
	}

	_ice_rgbw_carry_error.clear();
	createHeader();

	Info(_log, "Runtime transfer curve switched without device reset: {:s}", currentTransferCurveProfileId());
	requestCurrentFrameRefresh();
	return QString();
}

bool DriverOtherRawHid::resolveDaytimeUpliftProfileLut(const QString& profileId, CustomTransferCurveLut& lut) const
{
	lut = {};
	const QString normalized = profileId.trimmed().toLower();
	if (normalized.isEmpty() || normalized == "curve3_4_new" || normalized == "sdr")
		return true;
	if (normalized.startsWith("custom:"))
	{
		const QString key = normalized.mid(static_cast<int>(std::strlen("custom:")));
		if (key.isEmpty())
			return false;
		return loadCustomTransferCurveProfileLut(key, lut);
	}
	return false;
}

QString DriverOtherRawHid::setDaytimeUplift(bool enabled, int blend, const QString& profileId)
{
	if (_transferCurveOwner != TransferCurveOwner::HyperHdr)
		return "Daytime uplift requires transferCurveOwner to be set to 'hyperhdr'";

	const uint8_t clampedBlend = static_cast<uint8_t>(qBound(0, blend, 255));

	if (!enabled || clampedBlend == 0)
	{
		const bool wasActive = _daytimeUpliftEnabled && _daytimeUpliftBlend > 0;
		_daytimeUpliftEnabled = false;
		_daytimeUpliftBlend = 0;
		_daytimeUpliftProfileId.clear();
		_daytimeUpliftLut = {};
		if (wasActive)
		{
			_ice_rgbw_carry_error.clear();
			createHeader();
			Info(_log, "Daytime uplift disabled");
			requestCurrentFrameRefresh();
		}
		return QString();
	}

	CustomTransferCurveLut resolvedLut;
	if (!resolveDaytimeUpliftProfileLut(profileId, resolvedLut))
		return QString("Daytime uplift profile not found or invalid: %1").arg(profileId);

	const bool profileChanged = (_daytimeUpliftProfileId != profileId);
	const bool blendChanged = (_daytimeUpliftBlend != clampedBlend);
	const bool enableChanged = !_daytimeUpliftEnabled;

	_daytimeUpliftEnabled = true;
	_daytimeUpliftBlend = clampedBlend;
	_daytimeUpliftProfileId = profileId;
	_daytimeUpliftLut = resolvedLut;

	if (enableChanged || profileChanged || blendChanged)
	{
		_ice_rgbw_carry_error.clear();
		createHeader();
		Info(_log, "Daytime uplift {:s}: {:s} ({:d}/255)", enableChanged ? "enabled" : "updated", profileId.isEmpty() ? "curve3_4_new" : profileId, static_cast<int>(clampedBlend));
		requestCurrentFrameRefresh();
	}
	return QString();
}

DriverOtherRawHid::CalibrationHeaderOwner DriverOtherRawHid::parseCalibrationHeaderOwner(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized.isEmpty() || normalized == "disabled" || normalized == "off" || normalized == "none")
		return CalibrationHeaderOwner::Disabled;
	if (normalized == "hyperhdr" || normalized == "host")
		return CalibrationHeaderOwner::HyperHdr;
	return CalibrationHeaderOwner::Teensy;
}

DriverOtherRawHid::CalibrationHeaderProfile DriverOtherRawHid::parseCalibrationHeaderProfile(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized.isEmpty() || normalized == "disabled")
		return CalibrationHeaderProfile::Disabled;
	if (normalized.startsWith("custom:"))
		return CalibrationHeaderProfile::Custom;
	return CalibrationHeaderProfile::Custom;
}

QString DriverOtherRawHid::parseCustomCalibrationHeaderProfileKey(const QString& text) const
{
	QString normalized = text.trimmed();
	if (normalized.startsWith("custom:", Qt::CaseInsensitive))
		normalized.remove(0, QString("custom:").size());
	return sanitizeCalibrationHeaderProfileKey(normalized);
}

QString DriverOtherRawHid::parseCustomRgbwLutProfileKey(const QString& text) const
{
	QString normalized = text.trimmed();
	if (normalized.isEmpty())
		return QString();

	if (normalized.compare("disabled", Qt::CaseInsensitive) == 0)
		return QString();

	if (normalized.startsWith("custom:", Qt::CaseInsensitive))
		normalized.remove(0, QString("custom:").size());

	return sanitizeRgbwLutProfileKey(normalized);
}

QString DriverOtherRawHid::parseSolverProfileId(const QString& text) const
{
	QString normalized = text.trimmed();
	if (normalized.isEmpty())
		return "builtin";

	const QString lowered = normalized.toLower();
	if (lowered == "builtin" || lowered == "built-in" || lowered == "default")
		return "builtin";

	if (lowered.startsWith("custom:"))
		normalized.remove(0, QString("custom:").size());

	const QString slug = sanitizeSolverProfileKey(normalized);
	return slug.isEmpty() ? QString("builtin") : QString("custom:%1").arg(slug);
}

DriverOtherRawHid::HostColorTransformOrder DriverOtherRawHid::parseCalibrationTransferOrder(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized == "calibration_then_transfer" || normalized == "calibration-then-transfer" || normalized == "calibration_then_transfer")
		return HostColorTransformOrder::CalibrationThenTransfer;
	return HostColorTransformOrder::TransferThenCalibration;
}

DriverOtherRawHid::HostRgbwLutTransferOrder DriverOtherRawHid::parseRgbwLutTransferOrder(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized == "lut_then_transfer" || normalized == "lut-then-transfer" || normalized == "after_lut" || normalized == "after-lut")
		return HostRgbwLutTransferOrder::LutThenTransfer;
	return HostRgbwLutTransferOrder::TransferBeforeLut;
}

bool DriverOtherRawHid::isTransferCurveEnabled() const
{
	if (_transferCurveOwner == TransferCurveOwner::Disabled)
		return false;
	if (_transferCurveProfile == TransferCurveProfile::Disabled)
		return false;
	if (_transferCurveProfile == TransferCurveProfile::Custom)
		return _transferCurveCustomLut.bucketCount > 1u;
	return true;
}

uint8_t DriverOtherRawHid::transferCurveProfileId() const
{
	if (_transferCurveOwner == TransferCurveOwner::Disabled)
		return 0u;
	return (_transferCurveProfile == TransferCurveProfile::Curve3_4New) ? 1u : 0u;
}

uint16_t DriverOtherRawHid::applyTransferCurveQ16(const uint16_t q16, const uint8_t channel) const
{
	uint16_t result = q16;
	if (isTransferCurveEnabled())
	{
		if (_transferCurveProfile == TransferCurveProfile::Curve3_4New)
			result = applyBuiltInTransferCurveQ16(q16, channel);
		else if (_transferCurveProfile == TransferCurveProfile::Custom && _transferCurveCustomLut.bucketCount > 1u)
			result = sampleTransferCurveLutQ16(_transferCurveCustomLut, q16, channel);
	}

	if (_daytimeUpliftEnabled && _daytimeUpliftBlend > 0)
	{
		const uint16_t upliftValue = (_daytimeUpliftLut.bucketCount > 1u)
			? sampleTransferCurveLutQ16(_daytimeUpliftLut, q16, channel)
			: applyBuiltInTransferCurveQ16(q16, channel);
		result = static_cast<uint16_t>(
			(static_cast<uint32_t>(result) * static_cast<uint32_t>(255u - _daytimeUpliftBlend) +
			 static_cast<uint32_t>(upliftValue) * static_cast<uint32_t>(_daytimeUpliftBlend) + 127u) / 255u);
	}

	return result;
}

float DriverOtherRawHid::applyTransferCurveUnit(const float unit) const
{
	if (!isTransferCurveEnabled())
		return std::clamp(unit, 0.0f, 1.0f);
	return q16ToUnitFloat(applyTransferCurveQ16(floatToQ16(unit), 3u));
}

bool DriverOtherRawHid::isCalibrationHeaderEnabled() const
{
	if (_calibrationHeaderOwner == CalibrationHeaderOwner::Disabled)
		return false;
	if (_calibrationHeaderProfile == CalibrationHeaderProfile::Disabled)
		return false;
	if (_calibrationHeaderProfile == CalibrationHeaderProfile::Custom)
		return _calibrationHeaderCustomLut.bucketCount > 1u;
	return false;
}

uint16_t DriverOtherRawHid::applyCalibrationHeaderQ16(const uint16_t q16, const uint8_t channel) const
{
	if (!isCalibrationHeaderEnabled())
		return q16;

	if (_calibrationHeaderProfile != CalibrationHeaderProfile::Custom || _calibrationHeaderCustomLut.bucketCount <= 1u)
		return q16;

	const size_t lutIndex = lutIndexFromQ16(q16, _calibrationHeaderCustomLut.bucketCount);
	switch (channel)
	{
		case 0:
			return _calibrationHeaderCustomLut.channels[1][lutIndex];
		case 1:
			return _calibrationHeaderCustomLut.channels[0][lutIndex];
		case 2:
			return _calibrationHeaderCustomLut.channels[2][lutIndex];
		default:
			return _calibrationHeaderCustomLut.channels[3][lutIndex];
	}
}

uint16_t DriverOtherRawHid::applyHostChannelPipelineQ16(const uint16_t q16, const uint8_t channel) const
{
	const bool hostAppliesTransferCurve = shouldApplyTransferCurveOnHost();
	const bool hostAppliesCalibration = shouldApplyCalibrationOnHost();
	if (!hostAppliesTransferCurve && !hostAppliesCalibration)
		return q16;
	if (hostAppliesTransferCurve && hostAppliesCalibration)
	{
		if (_hostColorTransformOrder == HostColorTransformOrder::CalibrationThenTransfer)
			return applyTransferCurveQ16(applyCalibrationHeaderQ16(q16, channel), channel);
		return applyCalibrationHeaderQ16(applyTransferCurveQ16(q16, channel), channel);
	}
	if (hostAppliesCalibration)
		return applyCalibrationHeaderQ16(q16, channel);
	return applyTransferCurveQ16(q16, channel);
}

bool DriverOtherRawHid::loadCustomTransferCurveProfile()
{
	_transferCurveCustomLut = {};
	return loadCustomTransferCurveProfileLut(_transferCurveCustomProfileKey, _transferCurveCustomLut);
}

bool DriverOtherRawHid::loadCustomTransferCurveProfileLut(const QString& profileKey, CustomTransferCurveLut& lut) const
{
	lut = {};
	if (profileKey.isEmpty())
		return false;

	const QString jsonPath = QDir(QDir::cleanPath(_configurationPath + QDir::separator() + "transfer_headers"))
		.filePath(profileKey + ".json");
	QFile file(jsonPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		Warning(_log, "Could not open transfer curve profile file: {:s}", jsonPath);
		return false;
	}

	const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
	if (!document.isObject())
	{
		Warning(_log, "Invalid transfer curve profile JSON: {:s}", jsonPath);
		return false;
	}

	const QJsonObject object = document.object();
	const uint32_t bucketCount = static_cast<uint32_t>(object["bucketCount"].toInt());
	const QJsonObject channels = object["channels"].toObject();
	const QJsonArray red = channels["red"].toArray();
	const QJsonArray green = channels["green"].toArray();
	const QJsonArray blue = channels["blue"].toArray();
	const QJsonArray white = channels["white"].toArray();
	if (bucketCount <= 1u ||
		red.size() != static_cast<qsizetype>(bucketCount) ||
		green.size() != static_cast<qsizetype>(bucketCount) ||
		blue.size() != static_cast<qsizetype>(bucketCount) ||
		white.size() != static_cast<qsizetype>(bucketCount))
	{
		Warning(_log, "Transfer curve profile has invalid channel lengths: {:s}", jsonPath);
		return false;
	}

	lut.bucketCount = bucketCount;
	for (std::vector<uint16_t>& channelValues : lut.channels)
	{
		channelValues.clear();
		channelValues.reserve(bucketCount);
	}

	auto appendChannel = [&](const QJsonArray& source, std::vector<uint16_t>& target) {
		for (const QJsonValue& entry : source)
		{
			const int value = entry.toInt(-1);
			if (value < 0 || value > 65535)
				return false;
			target.push_back(static_cast<uint16_t>(value));
		}
		return true;
	};

	if (!appendChannel(red, lut.channels[0]) ||
		!appendChannel(green, lut.channels[1]) ||
		!appendChannel(blue, lut.channels[2]) ||
		!appendChannel(white, lut.channels[3]))
	{
		Warning(_log, "Transfer curve profile contains invalid channel values: {:s}", jsonPath);
		return false;
	}

	return true;
}

bool DriverOtherRawHid::buildRuntimeInterpolatedTransferCurveLut(const QString& lowerProfileId, const QString& upperProfileId, const int blend, CustomTransferCurveLut& lut, QString& error) const
{
	struct ResolvedTransferCurveData
	{
		TransferCurveProfile profile = TransferCurveProfile::Disabled;
		CustomTransferCurveLut customLut;

		uint32_t bucketCount() const
		{
			if (profile == TransferCurveProfile::Curve3_4New)
				return TemporalBFITransferCurve::BUCKET_COUNT;
			if (profile == TransferCurveProfile::Custom)
				return customLut.bucketCount;
			return 0u;
		}
	};

	auto resolveProfile = [this, &error](const QString& profileId, ResolvedTransferCurveData& resolved) {
		resolved = ResolvedTransferCurveData{};
		const QString normalizedProfileId = profileId.trimmed();
		if (normalizedProfileId.isEmpty())
		{
			error = "Transfer interpolation profile id is empty";
			return false;
		}

		resolved.profile = parseTransferCurveProfile(normalizedProfileId);
		if (resolved.profile == TransferCurveProfile::Custom)
		{
			const QString customKey = parseCustomTransferCurveProfileKey(normalizedProfileId);
			if (customKey.isEmpty())
			{
				error = QString("Transfer interpolation profile id is invalid: %1").arg(normalizedProfileId);
				return false;
			}
			if (!loadCustomTransferCurveProfileLut(customKey, resolved.customLut))
			{
				error = QString("Transfer interpolation profile not found or invalid: %1").arg(normalizedProfileId);
				return false;
			}
		}
		return true;
	};

	auto sampleResolvedQ16 = [](const ResolvedTransferCurveData& resolved, const uint16_t q16, const uint8_t channel) {
		if (resolved.profile == TransferCurveProfile::Disabled)
			return q16;
		if (resolved.profile == TransferCurveProfile::Curve3_4New)
			return applyBuiltInTransferCurveQ16(q16, channel);
		return sampleTransferCurveLutQ16(resolved.customLut, q16, channel);
	};

	lut = {};
	if (blend <= 0 || blend >= 255)
	{
		error = "Transfer interpolation blend must be between 1 and 254";
		return false;
	}

	ResolvedTransferCurveData lowerResolved;
	ResolvedTransferCurveData upperResolved;
	if (!resolveProfile(lowerProfileId, lowerResolved) || !resolveProfile(upperProfileId, upperResolved))
		return false;

	const uint32_t bucketCount = std::max<uint32_t>(2u, std::max(lowerResolved.bucketCount(), upperResolved.bucketCount()));
	lut.bucketCount = bucketCount;
	for (std::vector<uint16_t>& channelValues : lut.channels)
	{
		channelValues.clear();
		channelValues.reserve(bucketCount);
	}

	for (uint32_t index = 0u; index < bucketCount; ++index)
	{
		const uint16_t q16 = static_cast<uint16_t>((static_cast<uint64_t>(index) * 65535u + static_cast<uint64_t>((bucketCount - 1u) / 2u)) / static_cast<uint64_t>(bucketCount - 1u));
		for (uint8_t channel = 0u; channel < 4u; ++channel)
		{
			const uint16_t lowerValue = sampleResolvedQ16(lowerResolved, q16, channel);
			const uint16_t upperValue = sampleResolvedQ16(upperResolved, q16, channel);
			const uint16_t blendedValue = static_cast<uint16_t>((static_cast<uint32_t>(lowerValue) * static_cast<uint32_t>(255 - blend) + static_cast<uint32_t>(upperValue) * static_cast<uint32_t>(blend) + 127u) / 255u);
			lut.channels[channel].push_back(blendedValue);
		}
	}

	return true;
}

bool DriverOtherRawHid::loadCustomCalibrationHeaderProfile()
{
	_calibrationHeaderCustomLut = {};
	if (_calibrationHeaderCustomProfileKey.isEmpty())
		return false;

	const QString jsonPath = QDir(QDir::cleanPath(_configurationPath + QDir::separator() + "calibration_headers"))
		.filePath(_calibrationHeaderCustomProfileKey + ".json");
	QFile file(jsonPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		Warning(_log, "Could not open calibration header profile file: {:s}", jsonPath);
		return false;
	}

	const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
	if (!document.isObject())
	{
		Warning(_log, "Invalid calibration header profile JSON: {:s}", jsonPath);
		return false;
	}

	const QJsonObject object = document.object();
	const uint32_t bucketCount = static_cast<uint32_t>(object["bucketCount"].toInt());
	const QJsonObject channels = object["channels"].toObject();
	const QJsonArray red = channels["red"].toArray();
	const QJsonArray green = channels["green"].toArray();
	const QJsonArray blue = channels["blue"].toArray();
	const QJsonArray white = channels["white"].toArray();
	if (bucketCount <= 1u ||
		red.size() != static_cast<qsizetype>(bucketCount) ||
		green.size() != static_cast<qsizetype>(bucketCount) ||
		blue.size() != static_cast<qsizetype>(bucketCount) ||
		white.size() != static_cast<qsizetype>(bucketCount))
	{
		Warning(_log, "Calibration header profile has invalid channel lengths: {:s}", jsonPath);
		return false;
	}

	_calibrationHeaderCustomLut.bucketCount = bucketCount;
	for (std::vector<uint16_t>& channelValues : _calibrationHeaderCustomLut.channels)
	{
		channelValues.clear();
		channelValues.reserve(bucketCount);
	}

	auto appendChannel = [&](const QJsonArray& source, std::vector<uint16_t>& target) {
		for (const QJsonValue& entry : source)
		{
			const int value = entry.toInt(-1);
			if (value < 0 || value > 65535)
				return false;
			target.push_back(static_cast<uint16_t>(value));
		}
		return true;
	};

	if (!appendChannel(red, _calibrationHeaderCustomLut.channels[0]) ||
		!appendChannel(green, _calibrationHeaderCustomLut.channels[1]) ||
		!appendChannel(blue, _calibrationHeaderCustomLut.channels[2]) ||
		!appendChannel(white, _calibrationHeaderCustomLut.channels[3]))
	{
		_calibrationHeaderCustomLut = {};
		Warning(_log, "Calibration header profile contains invalid channel values: {:s}", jsonPath);
		return false;
	}

	return true;
}

bool DriverOtherRawHid::loadCustomRgbwLutProfile()
{
	_rgbwLutCustomGrid = {};
	if (!_rgbwLutProfileId.startsWith("custom:"))
		return false;

	const QString slug = sanitizeRgbwLutProfileKey(_rgbwLutProfileId.mid(QString("custom:").size()));
	if (slug.isEmpty())
		return false;

	const QString jsonPath = QDir(QDir::cleanPath(_configurationPath + QDir::separator() + "rgbw_lut_headers"))
		.filePath(slug + ".json");
	QFile file(jsonPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		Warning(_log, "Could not open RGBW LUT profile file: {:s}", jsonPath);
		return false;
	}

	const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
	if (!document.isObject())
	{
		Warning(_log, "Invalid RGBW LUT profile JSON: {:s}", jsonPath);
		return false;
	}

	const QJsonObject object = document.object();
	const uint32_t gridSize = static_cast<uint32_t>(object["gridSize"].toInt());
	const uint32_t entryCount = static_cast<uint32_t>(object["entryCount"].toInt());
	const uint32_t sourceGridSize = static_cast<uint32_t>(object["sourceGridSize"].toInt(gridSize));
	const uint32_t axisMin = static_cast<uint32_t>(object["axisMin"].toInt(0));
	const uint32_t axisMax = static_cast<uint32_t>(object["axisMax"].toInt(65535));
	const bool requires3dInterpolation = object["requires3dInterpolation"].toBool(true);

	const uint64_t expectedEntryCount = static_cast<uint64_t>(gridSize) * static_cast<uint64_t>(gridSize) * static_cast<uint64_t>(gridSize);
	const QJsonObject channels = object["channels"].toObject();
	const QJsonArray red = channels["red"].toArray();
	const QJsonArray green = channels["green"].toArray();
	const QJsonArray blue = channels["blue"].toArray();
	const QJsonArray white = channels["white"].toArray();
	if (gridSize < 2u || entryCount == 0u || expectedEntryCount != static_cast<uint64_t>(entryCount) || axisMax <= axisMin ||
		red.size() != static_cast<qsizetype>(entryCount) ||
		green.size() != static_cast<qsizetype>(entryCount) ||
		blue.size() != static_cast<qsizetype>(entryCount) ||
		white.size() != static_cast<qsizetype>(entryCount))
	{
		Warning(_log, "RGBW LUT profile has invalid dimensions or channel lengths: {:s}", jsonPath);
		return false;
	}

	_rgbwLutCustomGrid.sourceGridSize = sourceGridSize;
	_rgbwLutCustomGrid.gridSize = gridSize;
	_rgbwLutCustomGrid.entryCount = entryCount;
	_rgbwLutCustomGrid.axisMin = axisMin;
	_rgbwLutCustomGrid.axisMax = axisMax;
	_rgbwLutCustomGrid.requires3dInterpolation = requires3dInterpolation;
	for (std::vector<uint16_t>& channelValues : _rgbwLutCustomGrid.channels)
	{
		channelValues.clear();
		channelValues.reserve(entryCount);
	}

	auto appendChannel = [&](const QJsonArray& source, std::vector<uint16_t>& target) {
		for (const QJsonValue& entry : source)
		{
			const int value = entry.toInt(-1);
			if (value < 0 || value > 65535)
				return false;
			target.push_back(static_cast<uint16_t>(value));
		}
		return true;
	};

	if (!appendChannel(red, _rgbwLutCustomGrid.channels[0]) ||
		!appendChannel(green, _rgbwLutCustomGrid.channels[1]) ||
		!appendChannel(blue, _rgbwLutCustomGrid.channels[2]) ||
		!appendChannel(white, _rgbwLutCustomGrid.channels[3]))
	{
		_rgbwLutCustomGrid = {};
		Warning(_log, "RGBW LUT profile contains invalid channel values: {:s}", jsonPath);
		return false;
	}

	return true;
}

void DriverOtherRawHid::loadBuiltInSolverProfile()
{
	_solverLadders = {};

	auto loadChannel = [&](const auto* ladder, const uint16_t count, const uint8_t channel) {
		auto& entries = _solverLadders[channel];
		entries.clear();
		entries.reserve(count);
		for (uint16_t index = 0u; index < count; ++index)
		{
			const auto& source = ladder[index];
			entries.push_back(SolverLadderEntry{
				source.outputQ16,
				source.value,
				source.bfi,
				source.value,
				source.value
			});
		}
	};

	loadChannel(RawHidSolverLaddersRgbw::kChannel0, RawHidSolverLaddersRgbw::kChannel0Count, 0u);
	loadChannel(RawHidSolverLaddersRgbw::kChannel1, RawHidSolverLaddersRgbw::kChannel1Count, 1u);
	loadChannel(RawHidSolverLaddersRgbw::kChannel2, RawHidSolverLaddersRgbw::kChannel2Count, 2u);
	loadChannel(RawHidSolverLaddersRgbw::kChannel3, RawHidSolverLaddersRgbw::kChannel3Count, 3u);
}

bool DriverOtherRawHid::loadCustomSolverProfile()
{
	if (!_solverProfileId.startsWith("custom:"))
		return false;

	const QString slug = sanitizeSolverProfileKey(_solverProfileId.mid(QString("custom:").size()));
	if (slug.isEmpty())
		return false;

	std::array<std::vector<SolverLadderEntry>, 4> loadedLadders{};
	const QString directoryPath = QDir(QDir::cleanPath(_configurationPath + QDir::separator() + "solver_profiles")).absolutePath();
	const QString combinedHeaderPath = QDir(directoryPath).filePath(slug + ".h");
	const QString combinedJsonPath = QDir(directoryPath).filePath(slug + ".json");
	const QString splitDirectoryPath = QDir(directoryPath).filePath(slug);

	auto loadHeaderFile = [&](const QString& path, std::array<std::vector<SolverLadderEntry>, 4>& target) {
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		QString error;
		if (!parseSolverProfileHeaderContent(QString::fromUtf8(file.readAll()), target, error))
		{
			Warning(_log, "Solver profile '{:s}' could not parse header '{:s}': {:s}", slug, path, error);
			return false;
		}

		return true;
	};

	auto parseEntryArray = [&](const QJsonArray& source, std::vector<SolverLadderEntry>& target, const QString& label) {
		target.clear();
		target.reserve(source.size());
		for (const QJsonValue& entryValue : source)
		{
			if (!entryValue.isObject())
				return false;
			const QJsonObject entry = entryValue.toObject();
			const int outputQ16 = entry.contains("output_q16") ? entry["output_q16"].toInt(-1) : entry["outputQ16"].toInt(-1);
			const int value = entry.contains("value") ? entry["value"].toInt(-1) : entry["upper_value"].toInt(-1);
			const int bfi = entry["bfi"].toInt(-1);
			const int lowerValue = entry.contains("lower_value") ? entry["lower_value"].toInt(value) : value;
			const int upperValue = entry.contains("upper_value") ? entry["upper_value"].toInt(value) : value;
			if (outputQ16 < 0 || outputQ16 > 65535 || value < 0 || value > 255 || bfi < 0 || bfi > SOLVER_HOST_MAX_BFI || lowerValue < 0 || lowerValue > 255 || upperValue < 0 || upperValue > 255)
			{
				Warning(_log, "Solver profile '{:s}' contains an invalid {:s} entry.", slug, label);
				return false;
			}
			target.push_back(SolverLadderEntry{
				static_cast<uint16_t>(outputQ16),
				static_cast<uint8_t>(value),
				static_cast<uint8_t>(bfi),
				static_cast<uint8_t>(lowerValue),
				static_cast<uint8_t>(upperValue)
			});
		}

		std::sort(target.begin(), target.end(), [](const SolverLadderEntry& left, const SolverLadderEntry& right) {
			if (left.outputQ16 != right.outputQ16)
				return left.outputQ16 < right.outputQ16;
			if (left.bfi != right.bfi)
				return left.bfi > right.bfi;
			return left.value < right.value;
		});

		return !target.empty();
	};

	auto loadJsonFile = [&](const QString& path, std::vector<SolverLadderEntry>& target, const QString& label) {
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return false;

		const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
		if (document.isArray())
			return parseEntryArray(document.array(), target, label);
		if (!document.isObject())
			return false;

		const QJsonObject object = document.object();
		const QJsonObject channels = object["channels"].toObject();
		if (channels.isEmpty())
			return false;
		const QString key = (label == "red") ? "red" : (label == "green") ? "green" : (label == "blue") ? "blue" : "white";
		return parseEntryArray(channels[key].toArray(), target, label);
	};

	bool loaded = false;
	if (QFileInfo::exists(combinedHeaderPath))
	{
		loaded = loadHeaderFile(combinedHeaderPath, loadedLadders);
	}
	else if (QFileInfo::exists(combinedJsonPath))
	{
		loaded =
			loadJsonFile(combinedJsonPath, loadedLadders[1], "red") &&
			loadJsonFile(combinedJsonPath, loadedLadders[0], "green") &&
			loadJsonFile(combinedJsonPath, loadedLadders[2], "blue") &&
			loadJsonFile(combinedJsonPath, loadedLadders[3], "white");
	}
	else if (QFileInfo(splitDirectoryPath).isDir())
	{
		loaded =
			loadJsonFile(QDir(splitDirectoryPath).filePath("r_temporal_ladder.json"), loadedLadders[1], "red") &&
			loadJsonFile(QDir(splitDirectoryPath).filePath("g_temporal_ladder.json"), loadedLadders[0], "green") &&
			loadJsonFile(QDir(splitDirectoryPath).filePath("b_temporal_ladder.json"), loadedLadders[2], "blue") &&
			loadJsonFile(QDir(splitDirectoryPath).filePath("w_temporal_ladder.json"), loadedLadders[3], "white");
	}

	if (!loaded)
		return false;

	_solverLadders = std::move(loadedLadders);
	return true;
}

void DriverOtherRawHid::rebuildSolverLookupTables()
{
	_solverLookupTables = {};
	for (uint8_t channel = 0u; channel < SOLVER_HOST_CHANNEL_COUNT; ++channel)
	{
		const size_t ladderCount = solverLadderForChannel(channel).size();
		const size_t bucketCount = std::max<size_t>(ladderCount, SOLVER_HOST_MIN_BUCKET_COUNT);
		_solverLookupTables.bucketCounts[channel] = bucketCount;
		_solverLookupTables.baselineBfi[channel].assign(bucketCount, 0u);
		_solverLookupTables.baselineOutputQ16[channel].assign(bucketCount, 0u);

		for (size_t index = 0u; index < bucketCount; ++index)
		{
			const uint16_t q16 = static_cast<uint16_t>((static_cast<uint64_t>(index) * 65535u) / static_cast<uint64_t>(bucketCount - 1u));
			const SolverEncodedState baseline = solveStateForQ16(q16, channel, 0u, SOLVER_HOST_MAX_BFI, true);
			_solverLookupTables.baselineBfi[channel][index] = baseline.bfi;
			_solverLookupTables.baselineOutputQ16[channel][index] = baseline.outputQ16;
		}
	}
}

const std::vector<DriverOtherRawHid::SolverLadderEntry>& DriverOtherRawHid::solverLadderForChannel(const uint8_t channel) const
{
	return _solverLadders[(channel < SOLVER_HOST_CHANNEL_COUNT) ? channel : 3u];
}

DriverOtherRawHid::SolverEncodedState DriverOtherRawHid::solveStateForQ16(
	const uint16_t targetQ16,
	const uint8_t channel,
	const uint8_t minBfi,
	const uint8_t maxBfi,
	const bool preferHigherBfi) const
{
	SolverEncodedState out{};
	if (targetQ16 == 0u)
		return out;

	const auto& ladder = solverLadderForChannel(channel);
	if (ladder.empty())
		return out;

	uint8_t input8Approx = static_cast<uint8_t>((static_cast<uint32_t>(targetQ16) * 255u + 32767u) / 65535u);
	if (input8Approx == 0u)
		input8Approx = 1u;

	const uint16_t tolerance = allowedErrorQ16(targetQ16);
	bool foundInTolerance = false;
	size_t bestIndex = 0u;
	uint16_t bestError = 0xFFFFu;

	for (size_t index = 0u; index < ladder.size(); ++index)
	{
		const auto& entry = ladder[index];
		if (entry.bfi < minBfi || entry.bfi > maxBfi)
			continue;
		if (!passesResolutionGuard(input8Approx, entry.value))
			continue;

		const uint16_t error = absDiffU16(entry.outputQ16, targetQ16);
		if (error > tolerance)
			continue;

		if (!foundInTolerance)
		{
			foundInTolerance = true;
			bestIndex = index;
			bestError = error;
			continue;
		}

		const auto& bestEntry = ladder[bestIndex];
		if (error < bestError)
		{
			bestIndex = index;
			bestError = error;
			continue;
		}
		if (error > bestError)
			continue;

		if (preferHigherBfi)
		{
			if (entry.bfi > bestEntry.bfi)
			{
				bestIndex = index;
				continue;
			}
			if (entry.bfi < bestEntry.bfi)
				continue;
		}

		if (entry.value > bestEntry.value)
			bestIndex = index;
	}

	if (!foundInTolerance)
	{
		bool foundFloor = false;
		for (size_t index = 0u; index < ladder.size(); ++index)
		{
			const auto& entry = ladder[index];
			if (entry.bfi < minBfi || entry.bfi > maxBfi)
				continue;
			if (entry.outputQ16 > targetQ16)
				break;
			if (!passesResolutionGuard(input8Approx, entry.value))
				continue;
			foundFloor = true;
			bestIndex = index;
		}

		if (!foundFloor)
		{
			uint32_t nearestError = 0xFFFFFFFFu;
			for (size_t index = 0u; index < ladder.size(); ++index)
			{
				const auto& entry = ladder[index];
				if (entry.bfi < minBfi || entry.bfi > maxBfi)
					continue;
				const uint32_t error = (entry.outputQ16 > targetQ16)
					? static_cast<uint32_t>(entry.outputQ16 - targetQ16)
					: static_cast<uint32_t>(targetQ16 - entry.outputQ16);
				if (error < nearestError)
				{
					nearestError = error;
					bestIndex = index;
					continue;
				}
				if (error == nearestError)
				{
					const auto& bestEntry = ladder[bestIndex];
					if (preferHigherBfi)
					{
						if (entry.bfi > bestEntry.bfi)
						{
							bestIndex = index;
							continue;
						}
						if (entry.bfi < bestEntry.bfi)
							continue;
					}
					if (entry.value > bestEntry.value)
						bestIndex = index;
				}
			}
		}
	}

	const auto& best = ladder[bestIndex];
	out.value = best.value;
	out.bfi = best.bfi;
	out.outputQ16 = best.outputQ16;
	return out;
}

DriverOtherRawHid::SolverEncodedState DriverOtherRawHid::solveBaselineStateForQ16(const uint16_t targetQ16, const uint8_t channel) const
{
	const size_t bucketCount = _solverLookupTables.bucketCounts[channel];
	const size_t index = q16ToSolverBucketIndex(targetQ16, bucketCount);
	return SolverEncodedState{
		0u,
		_solverLookupTables.baselineBfi[channel][index],
		_solverLookupTables.baselineOutputQ16[channel][index]
	};
}

float DriverOtherRawHid::computeLegacySolvedRgbWeightedLumaUnit(const uint16_t redQ16, const uint16_t greenQ16, const uint16_t blueQ16) const
{
	const SolverEncodedState baselineRed = solveBaselineStateForQ16(redQ16, 1u);
	const SolverEncodedState baselineGreen = solveBaselineStateForQ16(greenQ16, 0u);
	const SolverEncodedState baselineBlue = solveBaselineStateForQ16(blueQ16, 2u);

	return computeWeightedLumaUnit(
		q16ToUnitFloat(redQ16),
		q16ToUnitFloat(greenQ16),
		q16ToUnitFloat(blueQ16),
		0.0f,
		baselineRed.bfi,
		baselineGreen.bfi,
		baselineBlue.bfi,
		0u,
		false);
}

float DriverOtherRawHid::computeDirectSolvedRgbWeightedLumaUnit(const uint16_t redQ16, const uint16_t greenQ16, const uint16_t blueQ16) const
{
	const SolverEncodedState baselineRed = solveBaselineStateForQ16(redQ16, 1u);
	const SolverEncodedState baselineGreen = solveBaselineStateForQ16(greenQ16, 0u);
	const SolverEncodedState baselineBlue = solveBaselineStateForQ16(blueQ16, 2u);

	return computeWeightedLumaUnit(
		q16ToUnitFloat(baselineRed.outputQ16),
		q16ToUnitFloat(baselineGreen.outputQ16),
		q16ToUnitFloat(baselineBlue.outputQ16),
		0.0f,
		0u,
		0u,
		0u,
		0u,
		false);
}

float DriverOtherRawHid::computeLegacySolvedRgbwWeightedLumaUnit(const HostOwnedRgbwQ16& rgbw) const
{
	const SolverEncodedState baselineRed = solveBaselineStateForQ16(rgbw.red, 1u);
	const SolverEncodedState baselineGreen = solveBaselineStateForQ16(rgbw.green, 0u);
	const SolverEncodedState baselineBlue = solveBaselineStateForQ16(rgbw.blue, 2u);
	const SolverEncodedState baselineWhite = solveBaselineStateForQ16(rgbw.white, 3u);

	return computeWeightedLumaUnit(
		q16ToUnitFloat(rgbw.red),
		q16ToUnitFloat(rgbw.green),
		q16ToUnitFloat(rgbw.blue),
		q16ToUnitFloat(rgbw.white),
		baselineRed.bfi,
		baselineGreen.bfi,
		baselineBlue.bfi,
		baselineWhite.bfi,
		true);
}

float DriverOtherRawHid::computeDirectSolvedRgbwWeightedLumaUnit(const HostOwnedRgbwQ16& rgbw) const
{
	const SolverEncodedState baselineRed = solveBaselineStateForQ16(rgbw.red, 1u);
	const SolverEncodedState baselineGreen = solveBaselineStateForQ16(rgbw.green, 0u);
	const SolverEncodedState baselineBlue = solveBaselineStateForQ16(rgbw.blue, 2u);
	const SolverEncodedState baselineWhite = solveBaselineStateForQ16(rgbw.white, 3u);

	return computeWeightedLumaUnit(
		q16ToUnitFloat(baselineRed.outputQ16),
		q16ToUnitFloat(baselineGreen.outputQ16),
		q16ToUnitFloat(baselineBlue.outputQ16),
		q16ToUnitFloat(baselineWhite.outputQ16),
		0u,
		0u,
		0u,
		0u,
		true);
}

void DriverOtherRawHid::rebuildHostRgbwReferenceData()
{
	_hostRgbwReferenceValid = false;
	_hostRgbwMeasuredExactVector = { 1.0, 1.0, 1.0 };
	_hostRgbwD65ExactVector = { 1.0, 1.0, 1.0 };
	_hostRgbwMeasuredMinVector = { 1.0f, 1.0f, 1.0f };
	_hostRgbwD65MinVector = { 1.0f, 1.0f, 1.0f };
	_hostRgbwMeasuredWhiteUv = { 0.0, 0.0 };
	_hostRgbwD65WhiteUv = xyzToUvPrime(xyToXyz(D65_WHITE_X, D65_WHITE_Y));

	const std::array<double, 3> redXyz = xyToXyz(_hostRgbwCentroidRedX, _hostRgbwCentroidRedY);
	const std::array<double, 3> greenXyz = xyToXyz(_hostRgbwCentroidGreenX, _hostRgbwCentroidGreenY);
	const std::array<double, 3> blueXyz = xyToXyz(_hostRgbwCentroidBlueX, _hostRgbwCentroidBlueY);
	const std::array<double, 3> whiteXyz = xyToXyz(_hostRgbwCentroidWhiteX, _hostRgbwCentroidWhiteY);
	const std::array<double, 3> d65Xyz = xyToXyz(D65_WHITE_X, D65_WHITE_Y);

	_hostRgbwRgbBasis[0] = redXyz;
	_hostRgbwRgbBasis[1] = greenXyz;
	_hostRgbwRgbBasis[2] = blueXyz;
	_hostRgbwMeasuredWhiteUv = xyzToUvPrime(whiteXyz);

	std::array<double, 3> measuredVector{};
	std::array<double, 3> d65Vector{};
	if (!solveRgbBasisVector(_hostRgbwRgbBasis, whiteXyz, measuredVector) || !solveRgbBasisVector(_hostRgbwRgbBasis, d65Xyz, d65Vector))
		return;

	_hostRgbwMeasuredExactVector = measuredVector;
	_hostRgbwD65ExactVector = d65Vector;
	_hostRgbwMeasuredMinVector = normalizeMinModeVector(measuredVector);
	_hostRgbwD65MinVector = normalizeMinModeVector(d65Vector);
	_hostRgbwReferenceValid = true;
}

uint16_t DriverOtherRawHid::prepareHostRgbwInputQ16(const uint16_t q16, const uint8_t channel) const
{
	if (!isHostOwnedRgbwEnabled())
		return q16;
	if (isLutHeaderRgbwModeActive())
	{
		if (_hostRgbwLutTransferOrder == HostRgbwLutTransferOrder::TransferBeforeLut && shouldApplyTransferCurveOnHost())
			return applyTransferCurveQ16(q16, channel);
		return q16;
	}
	return applyHostChannelPipelineQ16(q16, channel);
}

DriverOtherRawHid::HostOwnedRgbwQ16 DriverOtherRawHid::applyTransferCurveToHostOwnedRgbw(const HostOwnedRgbwQ16& rgbw) const
{
	if (!shouldApplyTransferCurveOnHost())
		return rgbw;

	return HostOwnedRgbwQ16{
		applyTransferCurveQ16(rgbw.red, 1u),
		applyTransferCurveQ16(rgbw.green, 0u),
		applyTransferCurveQ16(rgbw.blue, 2u),
		applyTransferCurveQ16(rgbw.white, 3u)
	};
}

float DriverOtherRawHid::computeHostRgbwGatedWhiteness(const uint16_t redQ16, const uint16_t greenQ16, const uint16_t blueQ16) const
{
	if (!_hostRgbwReferenceValid)
		return 0.0f;

	const std::array<double, 3> xyz = {
		(_hostRgbwRgbBasis[0][0] * q16ToUnitFloat(redQ16)) + (_hostRgbwRgbBasis[1][0] * q16ToUnitFloat(greenQ16)) + (_hostRgbwRgbBasis[2][0] * q16ToUnitFloat(blueQ16)),
		(_hostRgbwRgbBasis[0][1] * q16ToUnitFloat(redQ16)) + (_hostRgbwRgbBasis[1][1] * q16ToUnitFloat(greenQ16)) + (_hostRgbwRgbBasis[2][1] * q16ToUnitFloat(blueQ16)),
		(_hostRgbwRgbBasis[0][2] * q16ToUnitFloat(redQ16)) + (_hostRgbwRgbBasis[1][2] * q16ToUnitFloat(greenQ16)) + (_hostRgbwRgbBasis[2][2] * q16ToUnitFloat(blueQ16))
	};
	const std::array<double, 2> uvTarget = xyzToUvPrime(xyz);
	const std::array<double, 2> uvWhite = _hostRgbwApplyD65Correction
		? blendVector2(_hostRgbwMeasuredWhiteUv, _hostRgbwD65WhiteUv, _hostRgbwD65Strength)
		: _hostRgbwMeasuredWhiteUv;
	const float radius = std::max(_hostRgbwGatedUvRadius, 0.0f);
	if (radius <= 0.0f)
		return 0.0f;

	const double distance = distance2d(uvTarget, uvWhite);
	if (distance >= static_cast<double>(radius))
		return 0.0f;

	const double unit = 1.0 - (distance / static_cast<double>(radius));
	const double power = std::max(static_cast<double>(_hostRgbwGatedPower), 0.0001);
	return static_cast<float>(std::pow(std::clamp(unit, 0.0, 1.0), power));
}

DriverOtherRawHid::HostOwnedRgbwQ16 DriverOtherRawHid::buildHostRgbwPolicySample(const uint16_t redQ16, const uint16_t greenQ16, const uint16_t blueQ16) const
{
	if (isLutHeaderRgbwModeActive())
		return sampleCustomRgbwLut(redQ16, greenQ16, blueQ16);

	const double redUnit = q16ToUnitFloat(redQ16);
	const double greenUnit = q16ToUnitFloat(greenQ16);
	const double blueUnit = q16ToUnitFloat(blueQ16);

	auto buildFromScaleAndVector = [&](const double whiteScale, const std::array<double, 3>& vector) {
		const double clampedWhite = std::clamp(whiteScale, 0.0, 1.0);
		const double residualRed = std::max(0.0, redUnit - (vector[0] * clampedWhite));
		const double residualGreen = std::max(0.0, greenUnit - (vector[1] * clampedWhite));
		const double residualBlue = std::max(0.0, blueUnit - (vector[2] * clampedWhite));
		return HostOwnedRgbwQ16{
			floatToQ16(static_cast<float>(residualRed)),
			floatToQ16(static_cast<float>(residualGreen)),
			floatToQ16(static_cast<float>(residualBlue)),
			floatToQ16(static_cast<float>(clampedWhite))
		};
	};

	const std::array<double, 3> equalVector = { 1.0, 1.0, 1.0 };
	const std::array<double, 3> measuredMinVector = {
		static_cast<double>(_hostRgbwMeasuredMinVector[0]),
		static_cast<double>(_hostRgbwMeasuredMinVector[1]),
		static_cast<double>(_hostRgbwMeasuredMinVector[2])
	};
	const std::array<double, 3> d65MinVector = {
		static_cast<double>(_hostRgbwD65MinVector[0]),
		static_cast<double>(_hostRgbwD65MinVector[1]),
		static_cast<double>(_hostRgbwD65MinVector[2])
	};
	const std::array<double, 3> minVector = [&]() {
		if (_hostRgbwMode == HostRgbwMode::ClassicMin)
			return _hostRgbwApplyD65Correction ? blendVector3(equalVector, d65MinVector, _hostRgbwD65Strength) : equalVector;
		if (!_hostRgbwReferenceValid)
			return equalVector;
		return _hostRgbwApplyD65Correction
			? blendVector3(measuredMinVector, d65MinVector, _hostRgbwD65Strength)
			: measuredMinVector;
	}();
	const std::array<double, 3> exactVector = [&]() {
		if (!_hostRgbwReferenceValid)
			return equalVector;
		return _hostRgbwApplyD65Correction
			? blendVector3(_hostRgbwMeasuredExactVector, _hostRgbwD65ExactVector, _hostRgbwD65Strength)
			: _hostRgbwMeasuredExactVector;
	}();

	switch (_hostRgbwMode)
	{
		case HostRgbwMode::CentroidMin:
			return buildFromScaleAndVector(std::min({ redUnit, greenUnit, blueUnit }), minVector);
		case HostRgbwMode::GatedMin:
			return buildFromScaleAndVector(
				std::min({ redUnit, greenUnit, blueUnit }) * static_cast<double>(computeHostRgbwGatedWhiteness(redQ16, greenQ16, blueQ16)),
				minVector);
		case HostRgbwMode::ExactWhite:
		{
			double whiteScale = std::numeric_limits<double>::infinity();
			for (size_t index = 0u; index < exactVector.size(); ++index)
			{
				const double coefficient = exactVector[index];
				if (!(coefficient > 1e-9) || !std::isfinite(coefficient))
					continue;
				const double channelUnit = (index == 0u) ? redUnit : (index == 1u) ? greenUnit : blueUnit;
				whiteScale = std::min(whiteScale, channelUnit / coefficient);
			}
			if (!std::isfinite(whiteScale) || whiteScale < 0.0)
				whiteScale = 0.0;
			return buildFromScaleAndVector(whiteScale, exactVector);
		}
		case HostRgbwMode::ClassicMin:
			return buildFromScaleAndVector(std::min({ redUnit, greenUnit, blueUnit }), minVector);
		case HostRgbwMode::LutHeader:
			return buildFromScaleAndVector(std::min({ redUnit, greenUnit, blueUnit }), equalVector);
		case HostRgbwMode::Disabled:
		default:
			return buildFromScaleAndVector(std::min({ redUnit, greenUnit, blueUnit }), equalVector);
	}
}

DriverOtherRawHid::HostOwnedRgbwQ16 DriverOtherRawHid::sampleCustomRgbwLut(const uint16_t redQ16, const uint16_t greenQ16, const uint16_t blueQ16) const
{
	if (_rgbwLutCustomGrid.gridSize < 2u || _rgbwLutCustomGrid.entryCount == 0u)
		return HostOwnedRgbwQ16{ redQ16, greenQ16, blueQ16, 0u };

	const double axisMin = static_cast<double>(_rgbwLutCustomGrid.axisMin);
	const double axisSpan = static_cast<double>(_rgbwLutCustomGrid.axisMax - _rgbwLutCustomGrid.axisMin);
	const double gridMax = static_cast<double>(_rgbwLutCustomGrid.gridSize - 1u);
	const auto coordinateFor = [&](const uint16_t q16) {
		if (axisSpan <= 0.0)
			return 0.0;
		const double normalized = std::clamp((static_cast<double>(q16) - axisMin) / axisSpan, 0.0, 1.0);
		return normalized * gridMax;
	};

	const double redCoord = coordinateFor(redQ16);
	const double greenCoord = coordinateFor(greenQ16);
	const double blueCoord = coordinateFor(blueQ16);

	const uint32_t r0 = static_cast<uint32_t>(std::floor(redCoord));
	const uint32_t g0 = static_cast<uint32_t>(std::floor(greenCoord));
	const uint32_t b0 = static_cast<uint32_t>(std::floor(blueCoord));
	const uint32_t r1 = std::min(r0 + 1u, _rgbwLutCustomGrid.gridSize - 1u);
	const uint32_t g1 = std::min(g0 + 1u, _rgbwLutCustomGrid.gridSize - 1u);
	const uint32_t b1 = std::min(b0 + 1u, _rgbwLutCustomGrid.gridSize - 1u);
	const double tr = redCoord - static_cast<double>(r0);
	const double tg = greenCoord - static_cast<double>(g0);
	const double tb = blueCoord - static_cast<double>(b0);

	const auto sampleChannel = [&](const std::vector<uint16_t>& values) {
		const auto indexAt = [&](const uint32_t r, const uint32_t g, const uint32_t b) {
			return static_cast<size_t>((static_cast<uint64_t>(r) * static_cast<uint64_t>(_rgbwLutCustomGrid.gridSize) + static_cast<uint64_t>(g)) * static_cast<uint64_t>(_rgbwLutCustomGrid.gridSize) + static_cast<uint64_t>(b));
		};

		const double c000 = static_cast<double>(values[indexAt(r0, g0, b0)]);
		const double c001 = static_cast<double>(values[indexAt(r0, g0, b1)]);
		const double c010 = static_cast<double>(values[indexAt(r0, g1, b0)]);
		const double c011 = static_cast<double>(values[indexAt(r0, g1, b1)]);
		const double c100 = static_cast<double>(values[indexAt(r1, g0, b0)]);
		const double c101 = static_cast<double>(values[indexAt(r1, g0, b1)]);
		const double c110 = static_cast<double>(values[indexAt(r1, g1, b0)]);
		const double c111 = static_cast<double>(values[indexAt(r1, g1, b1)]);

		const double c00 = (c000 * (1.0 - tb)) + (c001 * tb);
		const double c01 = (c010 * (1.0 - tb)) + (c011 * tb);
		const double c10 = (c100 * (1.0 - tb)) + (c101 * tb);
		const double c11 = (c110 * (1.0 - tb)) + (c111 * tb);
		const double c0 = (c00 * (1.0 - tg)) + (c01 * tg);
		const double c1 = (c10 * (1.0 - tg)) + (c11 * tg);
		return static_cast<uint16_t>(std::clamp(std::lround((c0 * (1.0 - tr)) + (c1 * tr)), 0l, 65535l));
	};

	return HostOwnedRgbwQ16{
		sampleChannel(_rgbwLutCustomGrid.channels[0]),
		sampleChannel(_rgbwLutCustomGrid.channels[1]),
		sampleChannel(_rgbwLutCustomGrid.channels[2]),
		sampleChannel(_rgbwLutCustomGrid.channels[3])
	};
}

DriverOtherRawHid::ScenePolicyMode DriverOtherRawHid::parseScenePolicyMode(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized == "solverawarev2" || normalized == "solver-aware-v2" || normalized == "solver_aware_v2")
		return ScenePolicyMode::SolverAwareV2;
	if (normalized == "solverawarev3" || normalized == "solver-aware-v3" || normalized == "solver_aware_v3")
		return ScenePolicyMode::SolverAwareV3;
	return ScenePolicyMode::Disabled;
}

DriverOtherRawHid::HostRgbwMode DriverOtherRawHid::parseHostRgbwMode(const QString& text) const
{
	const QString normalized = text.trimmed().toLower();
	if (normalized == "ice" || normalized == "ice_rgbw" || normalized == "ice-rgbw")
		return HostRgbwMode::Ice;
	if (normalized == "classicmin" || normalized == "classic_min" || normalized == "classic-min" || normalized == "minrgb" || normalized == "min_rgb")
		return HostRgbwMode::ClassicMin;
	if (normalized == "centroidmin" || normalized == "centroid_min" || normalized == "centroid-min" || normalized == "measuredmin" || normalized == "measured_min")
		return HostRgbwMode::CentroidMin;
	if (normalized == "gatedmin" || normalized == "gated_min" || normalized == "gated-min")
		return HostRgbwMode::GatedMin;
	if (normalized == "lutheader" || normalized == "lut_header" || normalized == "lut-header" || normalized == "rgbwlut" || normalized == "rgbw_lut" || normalized == "rgbw-lut")
		return HostRgbwMode::LutHeader;
	if (normalized == "exactwhite" || normalized == "exact_white" || normalized == "exact-white")
		return HostRgbwMode::ExactWhite;
	return HostRgbwMode::Disabled;
}

bool DriverOtherRawHid::shouldApplyTransferCurveOnHost() const
{
	return isTransferCurveEnabled() && ((_transferCurveOwner == TransferCurveOwner::HyperHdr) || isHostOwnedRgbwEnabled());
}

bool DriverOtherRawHid::isLutHeaderRgbwModeActive() const
{
	return _hostRgbwMode == HostRgbwMode::LutHeader && _rgbwLutCustomGrid.entryCount > 0u;
}

bool DriverOtherRawHid::shouldApplyCalibrationOnHost() const
{
	if (isLutHeaderRgbwModeActive())
		return true;
	return isCalibrationHeaderEnabled() && ((_calibrationHeaderOwner == CalibrationHeaderOwner::HyperHdr) || isHostOwnedRgbwEnabled());
}

bool DriverOtherRawHid::isHostOwnedRgbwEnabled() const
{
	return _hostRgbwMode != HostRgbwMode::Disabled;
}

bool DriverOtherRawHid::isIceRgbwMode() const
{
	return _hostRgbwMode == HostRgbwMode::Ice;
}

size_t DriverOtherRawHid::highlightMaskBytes() const
{
	return (_ledCount == 0u) ? 0u : ((_ledCount + 7u) / 8u);
}

bool DriverOtherRawHid::init(QJsonObject deviceConfig)
{
	if (!LedDevice::init(deviceConfig))
		return false;

	_devicePath = deviceConfig["output"].toString("auto").trimmed();
	if (_devicePath.isEmpty())
		_devicePath = "auto";
	_isAutoDevicePath = (_devicePath.compare("auto", Qt::CaseInsensitive) == 0);

	if (!_isAutoDevicePath)
	{
		if (_devicePath.startsWith(QLatin1String("/dev/")))
		{
			// keep explicit /dev/hidrawX path as-is
		}
		else if (_devicePath.startsWith(QLatin1String("hidraw"), Qt::CaseInsensitive))
		{
			_devicePath.prepend("/dev/");
		}
	}

	uint16_t parsed = 0;
	if (parseHexOrDecimal(deviceConfig["VID"].toString("0x16C0"), parsed))
		_vid = parsed;
	else
		Warning(_log, "Invalid RawHID VID value. Falling back to 0x{:04x}", _vid);

	if (parseHexOrDecimal(deviceConfig["PID"].toString("0x0486"), parsed))
		_pid = parsed;
	else
		Warning(_log, "Invalid RawHID PID value. Falling back to 0x{:04x}", _pid);

	_delayAfterConnect_ms = deviceConfig["delayAfterConnect"].toInt(0);
	_writeTimeout_ms = deviceConfig["writeTimeout"].toInt(DEFAULT_WRITE_TIMEOUT_MS);
	if (_writeTimeout_ms < 0)
		_writeTimeout_ms = 0;

	_reportSize = deviceConfig["reportSize"].toInt(DEFAULT_REPORT_SIZE);
	if (_reportSize < 8)
		_reportSize = 8;
	else if (_reportSize > 1024)
		_reportSize = 1024;

	_readLogChannel = deviceConfig["readLogChannel"].toBool(true);
	_logPollIntervalFrames = deviceConfig["logPollIntervalFrames"].toInt(DEFAULT_LOG_POLL_INTERVAL_FRAMES);
	if (_logPollIntervalFrames < 1)
		_logPollIntervalFrames = 1;
	else if (_logPollIntervalFrames > 1024)
		_logPollIntervalFrames = 1024;

	_logReadBudget = deviceConfig["logReadBudget"].toInt(DEFAULT_LOG_READ_BUDGET);
	if (_logReadBudget < 1)
		_logReadBudget = 1;
	else if (_logReadBudget > 64)
		_logReadBudget = 64;

	_configurationPath = deviceConfig["configurationPath"].toString(_configurationPath).trimmed();
	if (_configurationPath.isEmpty())
		_configurationPath = QDir(QDir::homePath()).absoluteFilePath(".hyperhdr");

	_payloadMode = parsePayloadMode(deviceConfig["payloadMode"].toString("rgb16"));
	_transferCurveOwner = parseTransferCurveOwner(deviceConfig["transferCurveOwner"].toString("teensy"));
	const QString transferCurveProfileText = deviceConfig["transferCurveProfile"].toString("curve3_4_new");
	_transferCurveProfile = parseTransferCurveProfile(transferCurveProfileText);
	_transferCurveCustomProfileKey = (_transferCurveProfile == TransferCurveProfile::Custom)
		? parseCustomTransferCurveProfileKey(transferCurveProfileText)
		: QString();
	if (_transferCurveOwner == TransferCurveOwner::Disabled)
	{
		_transferCurveProfile = TransferCurveProfile::Disabled;
		_transferCurveCustomProfileKey.clear();
		_transferCurveCustomLut = {};
	}
	else if (_transferCurveOwner == TransferCurveOwner::Teensy)
	{
		_transferCurveProfile = TransferCurveProfile::Curve3_4New;
		_transferCurveCustomProfileKey.clear();
		_transferCurveCustomLut = {};
	}
	else if (_transferCurveProfile == TransferCurveProfile::Custom)
	{
		if (!loadCustomTransferCurveProfile())
		{
			Warning(_log, "Falling back to disabled transfer curve because custom profile '{:s}' could not be loaded.", _transferCurveCustomProfileKey);
			_transferCurveProfile = TransferCurveProfile::Disabled;
			_transferCurveCustomProfileKey.clear();
		}
	}
	_calibrationHeaderOwner = parseCalibrationHeaderOwner(deviceConfig["calibrationHeaderOwner"].toString("teensy"));
	const QString calibrationHeaderProfileText = deviceConfig["calibrationHeaderProfile"].toString("disabled");
	_calibrationHeaderProfile = parseCalibrationHeaderProfile(calibrationHeaderProfileText);
	_calibrationHeaderCustomProfileKey = (_calibrationHeaderProfile == CalibrationHeaderProfile::Custom)
		? parseCustomCalibrationHeaderProfileKey(calibrationHeaderProfileText)
		: QString();
	_hostColorTransformOrder = parseCalibrationTransferOrder(deviceConfig["calibrationTransferOrder"].toString("transfer_then_calibration"));
	_hostRgbwLutTransferOrder = parseRgbwLutTransferOrder(deviceConfig["rgbwLutTransferOrder"].toString("transfer_before_lut"));
	if (_calibrationHeaderOwner == CalibrationHeaderOwner::Disabled)
	{
		_calibrationHeaderProfile = CalibrationHeaderProfile::Disabled;
		_calibrationHeaderCustomProfileKey.clear();
		_calibrationHeaderCustomLut = {};
	}
	else if (_calibrationHeaderOwner == CalibrationHeaderOwner::Teensy)
	{
		_calibrationHeaderProfile = CalibrationHeaderProfile::Disabled;
		_calibrationHeaderCustomProfileKey.clear();
		_calibrationHeaderCustomLut = {};
	}
	else if (_calibrationHeaderProfile == CalibrationHeaderProfile::Custom)
	{
		if (!loadCustomCalibrationHeaderProfile())
		{
			Warning(_log, "Falling back to disabled calibration header because custom profile '{:s}' could not be loaded.", _calibrationHeaderCustomProfileKey);
			_calibrationHeaderProfile = CalibrationHeaderProfile::Disabled;
			_calibrationHeaderCustomProfileKey.clear();
		}
	}
	_solverProfileId = parseSolverProfileId(deviceConfig["solverProfile"].toString("builtin"));
	loadBuiltInSolverProfile();
	if (_solverProfileId.startsWith("custom:") && !loadCustomSolverProfile())
	{
		Warning(_log, "Falling back to built-in solver profile because custom profile '{:s}' could not be loaded.", _solverProfileId);
		_solverProfileId = "builtin";
		loadBuiltInSolverProfile();
	}
	rebuildSolverLookupTables();
	_scenePolicyMode = parseScenePolicyMode(deviceConfig["scenePolicyMode"].toString("disabled"));
	_scenePolicyEnableHighlight = deviceConfig["scenePolicyEnableHighlight"].toBool(true);
	_highlightShadowQ16Threshold = static_cast<uint16_t>(std::clamp(deviceConfig["highlightShadowQ16Threshold"].toInt(DEFAULT_HIGHLIGHT_SHADOW_Q16_THRESHOLD), 0, 65535));
	_highlightShadowPercent = static_cast<uint8_t>(std::clamp(deviceConfig["highlightShadowPercent"].toInt(DEFAULT_HIGHLIGHT_SHADOW_PERCENT), 0, 100));
	_highlightShadowMinPeakDelta = static_cast<uint8_t>(std::clamp(deviceConfig["highlightShadowMinPeakDelta"].toInt(DEFAULT_HIGHLIGHT_SHADOW_MIN_PEAK_DELTA), 0, 255));
	_highlightShadowUniformSpreadMax = static_cast<uint8_t>(std::clamp(deviceConfig["highlightShadowUniformSpreadMax"].toInt(DEFAULT_HIGHLIGHT_SHADOW_UNIFORM_SPREAD_MAX), 0, 255));
	_highlightShadowTriggerMargin = static_cast<uint8_t>(std::clamp(deviceConfig["highlightShadowTriggerMargin"].toInt(DEFAULT_HIGHLIGHT_SHADOW_TRIGGER_MARGIN), 0, 255));
	_highlightShadowMinSceneMedian = static_cast<uint8_t>(std::clamp(deviceConfig["highlightShadowMinSceneMedian"].toInt(DEFAULT_HIGHLIGHT_SHADOW_MIN_SCENE_MEDIAN), 0, 255));
	_highlightShadowMinSceneAvg = static_cast<uint8_t>(std::clamp(deviceConfig["highlightShadowMinSceneAvg"].toInt(DEFAULT_HIGHLIGHT_SHADOW_MIN_SCENE_AVG), 0, 255));
	_highlightShadowDimUniformMedian = static_cast<uint8_t>(std::clamp(deviceConfig["highlightShadowDimUniformMedian"].toInt(DEFAULT_HIGHLIGHT_SHADOW_DIM_UNIFORM_MEDIAN), 0, 255));
	_hostRgbwMode = parseHostRgbwMode(deviceConfig["rgbwMode"].toString("disabled"));
	_rgbwLutProfileId = deviceConfig["rgbwLutProfile"].toString("disabled").trimmed();
	if (_rgbwLutProfileId.isEmpty())
		_rgbwLutProfileId = "disabled";
	if (_rgbwLutProfileId.compare("disabled", Qt::CaseInsensitive) != 0 && !_rgbwLutProfileId.startsWith("custom:", Qt::CaseInsensitive))
		_rgbwLutProfileId = QString("custom:%1").arg(parseCustomRgbwLutProfileKey(_rgbwLutProfileId));
	_enable_ice_rgbw = deviceConfig["enable_ice_rgbw"].toBool(false);
	if (_hostRgbwMode == HostRgbwMode::Disabled && _enable_ice_rgbw)
		_hostRgbwMode = HostRgbwMode::Ice;
	if (_hostRgbwMode == HostRgbwMode::LutHeader)
	{
		if (_rgbwLutProfileId == "disabled" || !loadCustomRgbwLutProfile())
		{
			Warning(_log, "Falling back to disabled RawHID RGBW LUT mode because profile '{:s}' could not be loaded.", _rgbwLutProfileId);
			_hostRgbwMode = HostRgbwMode::Disabled;
			_rgbwLutProfileId = "disabled";
			_rgbwLutCustomGrid = {};
		}
	}
	else
	{
		_rgbwLutCustomGrid = {};
	}
	_hostRgbwApplyD65Correction = deviceConfig["rgbwApplyD65Correction"].toBool(false);
	_hostRgbwD65Strength = static_cast<float>(std::clamp(deviceConfig["rgbwD65Strength"].toDouble(DEFAULT_HOST_RGBW_D65_STRENGTH), 0.0, 1.0));
	_ice_rgbw_temporal_dithering = deviceConfig["ice_rgbw_temporal_dithering"].toBool(false);
	_hostRgbwGatedUvRadius = static_cast<float>(deviceConfig["rgbwGatedUvRadius"].toDouble(DEFAULT_HOST_RGBW_GATED_UV_RADIUS));
	_hostRgbwGatedPower = static_cast<float>(deviceConfig["rgbwGatedPower"].toDouble(DEFAULT_HOST_RGBW_GATED_POWER));
	_hostRgbwCentroidRedX = static_cast<float>(deviceConfig["rgbwCentroidRedX"].toDouble(DEFAULT_HOST_RGBW_RED_XY[0]));
	_hostRgbwCentroidRedY = static_cast<float>(deviceConfig["rgbwCentroidRedY"].toDouble(DEFAULT_HOST_RGBW_RED_XY[1]));
	_hostRgbwCentroidGreenX = static_cast<float>(deviceConfig["rgbwCentroidGreenX"].toDouble(DEFAULT_HOST_RGBW_GREEN_XY[0]));
	_hostRgbwCentroidGreenY = static_cast<float>(deviceConfig["rgbwCentroidGreenY"].toDouble(DEFAULT_HOST_RGBW_GREEN_XY[1]));
	_hostRgbwCentroidBlueX = static_cast<float>(deviceConfig["rgbwCentroidBlueX"].toDouble(DEFAULT_HOST_RGBW_BLUE_XY[0]));
	_hostRgbwCentroidBlueY = static_cast<float>(deviceConfig["rgbwCentroidBlueY"].toDouble(DEFAULT_HOST_RGBW_BLUE_XY[1]));
	_hostRgbwCentroidWhiteX = static_cast<float>(deviceConfig["rgbwCentroidWhiteX"].toDouble(DEFAULT_HOST_RGBW_WHITE_XY[0]));
	_hostRgbwCentroidWhiteY = static_cast<float>(deviceConfig["rgbwCentroidWhiteY"].toDouble(DEFAULT_HOST_RGBW_WHITE_XY[1]));
	_ice_white_mixer_threshold = deviceConfig["ice_white_mixer_threshold"].toDouble(0.0);
	_ice_white_led_intensity = deviceConfig["ice_white_led_intensity"].toDouble(1.8);
	_ice_white_temperatur.x = deviceConfig.contains("ice_white_temperatur_red") ? deviceConfig["ice_white_temperatur_red"].toDouble(1.0) : deviceConfig["ice_white_temperatur_r"].toDouble(1.0);
	_ice_white_temperatur.y = deviceConfig.contains("ice_white_temperatur_green") ? deviceConfig["ice_white_temperatur_green"].toDouble(1.0) : deviceConfig["ice_white_temperatur_g"].toDouble(1.0);
	_ice_white_temperatur.z = deviceConfig.contains("ice_white_temperatur_blue") ? deviceConfig["ice_white_temperatur_blue"].toDouble(1.0) : deviceConfig["ice_white_temperatur_b"].toDouble(1.0);
	_ice_rgbw_carry_error.clear();
	rebuildHostRgbwReferenceData();

	_maxRetry = _devConfig["maxRetry"].toInt(60);

	_white_channel_calibration = deviceConfig["white_channel_calibration"].toBool(false);
	_white_channel_limit = qMin(qRound(deviceConfig["white_channel_limit"].toDouble(1) * 255.0 / 100.0), 255);
	_white_channel_red = qMin(deviceConfig["white_channel_red"].toInt(255), 255);
	_white_channel_green = qMin(deviceConfig["white_channel_green"].toInt(255), 255);
	_white_channel_blue = qMin(deviceConfig["white_channel_blue"].toInt(255), 255);

	if (isHostOwnedRgbwEnabled() && _white_channel_calibration)
	{
		Warning(_log, "RawHID host RGBW modes conflict with legacy white-channel trailer. Disabling legacy white calibration.");
		_white_channel_calibration = false;
	}

	createHeader();

	Debug(_log, "Device path    : {:s}", _devicePath);
	Debug(_log, "VID/PID        : 0x{:04x}/0x{:04x}", _vid, _pid);
	Debug(_log, "Report size    : {:d} bytes", _reportSize);
	Debug(_log, "Write timeout  : {:d}ms ({:s})", _writeTimeout_ms, (_writeTimeout_ms > 0) ? "blocking" : "non-blocking");
	Debug(_log, "Read log chan  : {:s}", (_readLogChannel) ? "ON" : "OFF");
	Debug(_log, "Log poll every : {:d} frame(s)", _logPollIntervalFrames);
	Debug(_log, "Log read budget: {:d} report(s)", _logReadBudget);
	Debug(_log, "Delayed open   : {:d}", _delayAfterConnect_ms);
	Debug(_log, "Retry limit    : {:d}", _maxRetry);
	Debug(_log, "Payload mode   : {:s}", (_payloadMode == PayloadMode::Rgb12Carry) ? "rgb12carry" : "rgb16");
	const QString transferCurveLabel = (_transferCurveProfile == TransferCurveProfile::Curve3_4New)
		? QString("3_4_new")
		: (_transferCurveProfile == TransferCurveProfile::Custom)
			? QString("custom:%1").arg(_transferCurveCustomProfileKey)
			: QString("disabled");
	Debug(_log, "Transfer curve : owner={:s}, profile={:s}",
		(_transferCurveOwner == TransferCurveOwner::HyperHdr) ? "hyperhdr" :
		(_transferCurveOwner == TransferCurveOwner::Teensy) ? "teensy" : "disabled",
		transferCurveLabel);
	const QString calibrationHeaderLabel = (_calibrationHeaderProfile == CalibrationHeaderProfile::Custom)
		? QString("custom:%1").arg(_calibrationHeaderCustomProfileKey)
		: QString("disabled");
	Debug(_log, "Calibration hdr: owner={:s}, profile={:s}, order={:s}",
		(_calibrationHeaderOwner == CalibrationHeaderOwner::HyperHdr) ? "hyperhdr" :
		(_calibrationHeaderOwner == CalibrationHeaderOwner::Teensy) ? "teensy" : "disabled",
		calibrationHeaderLabel,
		(_hostColorTransformOrder == HostColorTransformOrder::CalibrationThenTransfer) ? "calibration_then_transfer" : "transfer_then_calibration");
	Debug(_log, "Solver profile  : {:s}", _solverProfileId);
	const char* hostRgbwModeText =
		(_hostRgbwMode == HostRgbwMode::Ice) ? "ice" :
		(_hostRgbwMode == HostRgbwMode::ClassicMin) ? "classicMin" :
		(_hostRgbwMode == HostRgbwMode::CentroidMin) ? "centroidMin" :
		(_hostRgbwMode == HostRgbwMode::GatedMin) ? "gatedMin" :
		(_hostRgbwMode == HostRgbwMode::LutHeader) ? "lutHeader" :
		(_hostRgbwMode == HostRgbwMode::ExactWhite) ? "exactWhite" : "disabled";
	Debug(_log, "Host RGBW      : mode={:s}, d65={:s}, validBasis={:s}",
		hostRgbwModeText,
		_hostRgbwApplyD65Correction ? "ON" : "OFF",
		_hostRgbwReferenceValid ? "yes" : "no");
	Debug(_log, "Host RGBW LUT  : profile={:s}, grid={:d}, source={:d}, entries={:d}",
		_rgbwLutProfileId,
		_rgbwLutCustomGrid.gridSize,
		_rgbwLutCustomGrid.sourceGridSize,
		_rgbwLutCustomGrid.entryCount);
	Debug(_log, "Host RGBW LUT  : transferOrder={:s}",
		(_hostRgbwLutTransferOrder == HostRgbwLutTransferOrder::LutThenTransfer) ? "lut_then_transfer" : "transfer_before_lut");
	Debug(_log, "Host RGBW D65  : strength={:.2f}", _hostRgbwD65Strength);
	Debug(_log, "ICE RGBW       : temp=[{:.2f},{:.2f},{:.2f}], mixer={:.2f}, intensity={:.2f}, temporal_dither={:s}",
		_ice_white_temperatur.x,
		_ice_white_temperatur.y,
		_ice_white_temperatur.z,
		_ice_white_mixer_threshold,
		_ice_white_led_intensity,
		_ice_rgbw_temporal_dithering ? "ON" : "OFF");
	Debug(_log, "RGBW centroids : R=[{:.4f},{:.4f}] G=[{:.4f},{:.4f}] B=[{:.4f},{:.4f}] W=[{:.4f},{:.4f}] gated[r={:.4f},p={:.2f}]",
		_hostRgbwCentroidRedX,
		_hostRgbwCentroidRedY,
		_hostRgbwCentroidGreenX,
		_hostRgbwCentroidGreenY,
		_hostRgbwCentroidBlueX,
		_hostRgbwCentroidBlueY,
		_hostRgbwCentroidWhiteX,
		_hostRgbwCentroidWhiteY,
		_hostRgbwGatedUvRadius,
		_hostRgbwGatedPower);
	Debug(_log, "Host solver policy: {:s}, highlight={:s}",
		scenePolicyModeToString(_scenePolicyMode),
		_scenePolicyEnableHighlight ? "ON" : "OFF");
	Debug(_log, "Highlight tune : q16Cap={:d}, pct={:d}, peak={:d}, spread={:d}, margin={:d}, minMedian={:d}, minAvg={:d}, dimMedian={:d}",
		_highlightShadowQ16Threshold,
		_highlightShadowPercent,
		_highlightShadowMinPeakDelta,
		_highlightShadowUniformSpreadMax,
		_highlightShadowTriggerMargin,
		_highlightShadowMinSceneMedian,
		_highlightShadowMinSceneAvg,
		_highlightShadowDimUniformMedian);

	if (_white_channel_calibration)
		Debug(_log, "White channel limit: {:d}, red: {:d}, green: {:d}, blue: {:d}", _white_channel_limit, _white_channel_red, _white_channel_green, _white_channel_blue);

	Debug(_log, "RawHID driver initialized for AWA payload transport");

	return true;
}

void DriverOtherRawHid::createHeader()
{
	unsigned int totalLedCount = _ledCount;
	if (totalLedCount > 0)
		totalLedCount -= 1;

	const bool hostOwnedRgbw = isHostOwnedRgbwEnabled();
	const uint64_t rgbDataLength = static_cast<uint64_t>(_ledCount) * (hostOwnedRgbw ? ((_payloadMode == PayloadMode::Rgb12Carry) ? 6u : 8u) : ((_payloadMode == PayloadMode::Rgb12Carry) ? 5u : 6u));
	const uint64_t trailerLength = (_white_channel_calibration ? 4u : 0u) + 2u + 3u;
	_ledBuffer.resize(static_cast<uint64_t>(_headerSize) + rgbDataLength + trailerLength, 0x00);

	_ledBuffer[0] = 'A';
	_ledBuffer[1] = hostOwnedRgbw ? 'T' : 't';
	if (_payloadMode == PayloadMode::Rgb12Carry)
		_ledBuffer[2] = hostOwnedRgbw ? 'c' : (_white_channel_calibration ? 'C' : 'c');
	else
		_ledBuffer[2] = hostOwnedRgbw ? 'b' : (_white_channel_calibration ? 'B' : 'b');
	qToBigEndian<quint16>(static_cast<quint16>(totalLedCount), &_ledBuffer[3]);
	_ledBuffer[5] = _ledBuffer[3] ^ _ledBuffer[4] ^ 0x55;

	Debug(_log, "RawHID AWA header for {:d} leds: {:c} {:c} {:c} {:d} {:d} {:d}", _ledCount,
		_ledBuffer[0], _ledBuffer[1], _ledBuffer[2], _ledBuffer[3], _ledBuffer[4], _ledBuffer[5]);
}

void DriverOtherRawHid::writeColorAs16Bit(uint8_t*& writer, const ColorRgb& rgb) const
{
	const uint16_t red = (static_cast<uint16_t>(rgb.red) << 8) | rgb.red;
	const uint16_t green = (static_cast<uint16_t>(rgb.green) << 8) | rgb.green;
	const uint16_t blue = (static_cast<uint16_t>(rgb.blue) << 8) | rgb.blue;

	*(writer++) = static_cast<uint8_t>(red >> 8);
	*(writer++) = static_cast<uint8_t>(red & 0xFF);
	*(writer++) = static_cast<uint8_t>(green >> 8);
	*(writer++) = static_cast<uint8_t>(green & 0xFF);
	*(writer++) = static_cast<uint8_t>(blue >> 8);
	*(writer++) = static_cast<uint8_t>(blue & 0xFF);
}

void DriverOtherRawHid::writeColorAs16Bit(uint8_t*& writer, const linalg::aliases::float3& rgb) const
{
	const uint16_t red = floatToQ16(rgb.x);
	const uint16_t green = floatToQ16(rgb.y);
	const uint16_t blue = floatToQ16(rgb.z);

	*(writer++) = static_cast<uint8_t>(red >> 8);
	*(writer++) = static_cast<uint8_t>(red & 0xFF);
	*(writer++) = static_cast<uint8_t>(green >> 8);
	*(writer++) = static_cast<uint8_t>(green & 0xFF);
	*(writer++) = static_cast<uint8_t>(blue >> 8);
	*(writer++) = static_cast<uint8_t>(blue & 0xFF);
}

void DriverOtherRawHid::writeColorAs12BitCarry(uint8_t*& writer, const ColorRgb& rgb) const
{
	const uint16_t red = expand8ToBucket12(rgb.red);
	const uint16_t green = expand8ToBucket12(rgb.green);
	const uint16_t blue = expand8ToBucket12(rgb.blue);

	const uint8_t redBase = static_cast<uint8_t>(red >> 4);
	const uint8_t greenBase = static_cast<uint8_t>(green >> 4);
	const uint8_t blueBase = static_cast<uint8_t>(blue >> 4);
	const uint8_t rgCarry = static_cast<uint8_t>(((red & 0x0Fu) << 4) | (green & 0x0Fu));
	const uint8_t bCarryReserved = static_cast<uint8_t>((blue & 0x0Fu) << 4);

	*(writer++) = redBase;
	*(writer++) = greenBase;
	*(writer++) = blueBase;
	*(writer++) = rgCarry;
	*(writer++) = bCarryReserved;
}

void DriverOtherRawHid::writeColorAs12BitCarry(uint8_t*& writer, const linalg::aliases::float3& rgb) const
{
	const uint16_t red = floatToBucket12(rgb.x);
	const uint16_t green = floatToBucket12(rgb.y);
	const uint16_t blue = floatToBucket12(rgb.z);

	const uint8_t redBase = static_cast<uint8_t>(red >> 4);
	const uint8_t greenBase = static_cast<uint8_t>(green >> 4);
	const uint8_t blueBase = static_cast<uint8_t>(blue >> 4);
	const uint8_t rgCarry = static_cast<uint8_t>(((red & 0x0Fu) << 4) | (green & 0x0Fu));
	const uint8_t bCarryReserved = static_cast<uint8_t>((blue & 0x0Fu) << 4);

	*(writer++) = redBase;
	*(writer++) = greenBase;
	*(writer++) = blueBase;
	*(writer++) = rgCarry;
	*(writer++) = bCarryReserved;
}

void DriverOtherRawHid::whiteChannelExtension(uint8_t*& writer) const
{
	if (_white_channel_calibration)
	{
		*(writer++) = _white_channel_limit;
		*(writer++) = _white_channel_red;
		*(writer++) = _white_channel_green;
		*(writer++) = _white_channel_blue;
	}
}

void DriverOtherRawHid::transferCurveExtension(uint8_t*& writer) const
{
	uint8_t flags = 0u;
	if (shouldApplyTransferCurveOnHost())
		flags |= TRANSFER_CURVE_FLAG_APPLIED_BY_HOST;
	if (shouldApplyCalibrationOnHost())
		flags |= CALIBRATION_FLAG_APPLIED_BY_HOST;

	*(writer++) = flags;
	*(writer++) = transferCurveProfileId();
}

std::array<uint16_t, 4> DriverOtherRawHid::quantizeHostOwnedRgbwCarry(size_t ledIndex, uint16_t redQ16, uint16_t greenQ16, uint16_t blueQ16, uint16_t whiteQ16)
{
	const std::array<uint16_t, 4> q16Values{ redQ16, greenQ16, blueQ16, whiteQ16 };
	std::array<uint16_t, 4> bucketValues{};

	if (!_ice_rgbw_temporal_dithering || _payloadMode != PayloadMode::Rgb12Carry)
	{
		for (size_t channel = 0; channel < bucketValues.size(); ++channel)
			bucketValues[channel] = q16ToBucket12(q16Values[channel]);
		return bucketValues;
	}

	if (_ice_rgbw_carry_error.size() != _ledCount)
		_ice_rgbw_carry_error.assign(_ledCount, std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f });

	auto& errors = _ice_rgbw_carry_error[ledIndex];
	for (size_t channel = 0; channel < bucketValues.size(); ++channel)
	{
		const float idealBucket = (static_cast<float>(q16Values[channel]) * static_cast<float>(RGB12_BUCKET_MAX) / 65535.0f) + errors[channel];
		const float quantizedBucket = std::clamp(std::round(idealBucket), 0.0f, static_cast<float>(RGB12_BUCKET_MAX));
		errors[channel] = idealBucket - quantizedBucket;
		bucketValues[channel] = static_cast<uint16_t>(quantizedBucket);
	}

	return bucketValues;
}

void DriverOtherRawHid::captureHostPolicyProbeSample(
	size_t ledIndex,
	bool highlighted,
	bool hostOwnedRgbw,
	const std::array<uint16_t, 4>& processedQ16,
	const std::array<uint16_t, 4>& policyQ16,
	const std::array<uint16_t, 4>& baselineQ16,
	const std::array<uint8_t, 4>& baselineBfi,
	const std::array<uint8_t, 4>& finalBfi,
	const std::array<uint16_t, 4>& emittedQ16)
{
	bool clamped = false;
	for (size_t index = 0u; index < processedQ16.size(); ++index)
	{
		if (processedQ16[index] != policyQ16[index])
		{
			clamped = true;
			break;
		}
	}

	HostPolicyProbeSample* target = nullptr;
	if (highlighted)
		target = &_hostPolicyProbeHighlighted;
	else if (clamped)
		target = &_hostPolicyProbeCapped;

	if (target == nullptr || target->valid)
		return;

	target->valid = true;
	target->highlighted = highlighted;
	target->hostOwnedRgbw = hostOwnedRgbw;
	target->clamped = clamped;
	target->ledIndex = ledIndex;
	target->processedQ16 = processedQ16;
	target->policyQ16 = policyQ16;
	target->baselineQ16 = baselineQ16;
	target->emittedQ16 = emittedQ16;
	target->baselineBfi = baselineBfi;
	target->finalBfi = finalBfi;
}
void DriverOtherRawHid::accumulateScenePolicyStats(const uint32_t highlightedLeds)
{
	if (_scenePolicyMode == ScenePolicyMode::Disabled)
		return;

	++_scenePolicyStatsFrames;

	if (highlightedLeds > 0u)
	{
		++_scenePolicyStatsHighlightFrames;
		_scenePolicyStatsHighlightedLeds += highlightedLeds;
		if (highlightedLeds > _scenePolicyStatsHighlightPeakLeds)
			_scenePolicyStatsHighlightPeakLeds = highlightedLeds;
	}

	const uint32_t logIntervalFrames = static_cast<uint32_t>(std::max(_logPollIntervalFrames, 1));

	if (_scenePolicyStatsFrames < logIntervalFrames)
		return;

	double averageHighlightedLeds = 0.0;
	if (_scenePolicyStatsHighlightFrames > 0u)
		averageHighlightedLeds = static_cast<double>(_scenePolicyStatsHighlightedLeds) / static_cast<double>(_scenePolicyStatsHighlightFrames);

	const char* payloadModeText = (_payloadMode == PayloadMode::Rgb12Carry) ? "rgb12carry" : "rgb16";
	const char* scenePolicyModeText = scenePolicyModeToString(_scenePolicyMode);
	const char* highlightRejectText =
		(_scenePolicyLastHighlightRejectCode == 1u) ? "disabled" :
		(_scenePolicyLastHighlightRejectCode == 2u) ? "empty" :
		(_scenePolicyLastHighlightRejectCode == 3u) ? "sceneFloor" :
		(_scenePolicyLastHighlightRejectCode == 4u) ? "dimUniform" :
		(_scenePolicyLastHighlightRejectCode == 5u) ? "noCandidates" : "none";

	Info(_log,
		"RawHID host policy stats: payload={:s}, mode={:s}, highlight={:s}, frames={:d}, pollFrames={:d}, hlScene[avg={:d},med={:d},p95={:d},spread={:d},gate={:d},cand={:d},reject={:s}], highlightFrames={:d}, highlightAvgLeds={:.1f}, highlightPeakLeds={:d}",
		payloadModeText,
		scenePolicyModeText,
		_scenePolicyEnableHighlight ? "ON" : "OFF",
		_scenePolicyStatsFrames,
		logIntervalFrames,
		_scenePolicyLastHighlightAvg,
		_scenePolicyLastHighlightMedian,
		_scenePolicyLastHighlightP95,
		_scenePolicyLastHighlightSpread,
		_scenePolicyLastHighlightGate,
		_scenePolicyLastHighlightCandidates,
		highlightRejectText,
		_scenePolicyStatsHighlightFrames,
		averageHighlightedLeds,
		_scenePolicyStatsHighlightPeakLeds);

	auto logProbeSample = [&](const char* kind, const HostPolicyProbeSample& sample) {
		if (!sample.valid)
			return;

		Info(_log,
			"RawHID solver probe: kind={:s}, led={:d}, rgbw={:s}, highlighted={:s}, clamped={:s}, q16[processed=[{:d},{:d},{:d},{:d}], policy=[{:d},{:d},{:d},{:d}], baseline=[{:d},{:d},{:d},{:d}], emitted=[{:d},{:d},{:d},{:d}]], bfi[baseline=[{:d},{:d},{:d},{:d}], final=[{:d},{:d},{:d},{:d}]]",
			kind,
			static_cast<int>(sample.ledIndex),
			sample.hostOwnedRgbw ? "yes" : "no",
			sample.highlighted ? "yes" : "no",
			sample.clamped ? "yes" : "no",
			sample.processedQ16[0], sample.processedQ16[1], sample.processedQ16[2], sample.processedQ16[3],
			sample.policyQ16[0], sample.policyQ16[1], sample.policyQ16[2], sample.policyQ16[3],
			sample.baselineQ16[0], sample.baselineQ16[1], sample.baselineQ16[2], sample.baselineQ16[3],
			sample.emittedQ16[0], sample.emittedQ16[1], sample.emittedQ16[2], sample.emittedQ16[3],
			static_cast<int>(sample.baselineBfi[0]), static_cast<int>(sample.baselineBfi[1]), static_cast<int>(sample.baselineBfi[2]), static_cast<int>(sample.baselineBfi[3]),
			static_cast<int>(sample.finalBfi[0]), static_cast<int>(sample.finalBfi[1]), static_cast<int>(sample.finalBfi[2]), static_cast<int>(sample.finalBfi[3]));
	};

	logProbeSample("highlight", _hostPolicyProbeHighlighted);
	logProbeSample("capped", _hostPolicyProbeCapped);

	_scenePolicyStatsFrames = 0u;
	_scenePolicyStatsHighlightFrames = 0u;
	_scenePolicyStatsHighlightedLeds = 0u;
	_scenePolicyStatsHighlightPeakLeds = 0u;
	_hostPolicyProbeHighlighted = {};
	_hostPolicyProbeCapped = {};
}

bool DriverOtherRawHid::shouldUseEmulatedRgbwPolicy() const
{
	return !isHostOwnedRgbwEnabled();
}

DriverOtherRawHid::HostOwnedRgbwQ16 DriverOtherRawHid::emulatePolicyRgbwSample(const uint16_t redQ16, const uint16_t greenQ16, const uint16_t blueQ16) const
{
	return buildHostRgbwPolicySample(redQ16, greenQ16, blueQ16);
}

void DriverOtherRawHid::buildHostOwnedRgbwFrame(const SharedOutputColors& nonlinearRgbColors, std::vector<HostOwnedRgbwQ16>& rgbwFrame)
{
	rgbwFrame.clear();
	if (nonlinearRgbColors == nullptr || nonlinearRgbColors->empty())
		return;
	const bool applyTransferAfterLutHeader =
		isLutHeaderRgbwModeActive() &&
		_hostRgbwLutTransferOrder == HostRgbwLutTransferOrder::LutThenTransfer &&
		shouldApplyTransferCurveOnHost();

	if (isIceRgbwMode())
	{
		auto processedColors = std::vector<linalg::aliases::float3>{};
		processedColors.reserve(nonlinearRgbColors->size());
		for (const auto& rgb : *nonlinearRgbColors)
		{
			processedColors.push_back(linalg::aliases::float3{
				q16ToUnitFloat(prepareHostRgbwInputQ16(floatToQ16(rgb.x), 1u)),
				q16ToUnitFloat(prepareHostRgbwInputQ16(floatToQ16(rgb.y), 0u)),
				q16ToUnitFloat(prepareHostRgbwInputQ16(floatToQ16(rgb.z), 2u))
			});
		}
		_infiniteColorEngineRgbw.renderRgbwQ16Frame(processedColors, _ice_white_mixer_threshold, _ice_white_led_intensity, _ice_white_temperatur, rgbwFrame);
		return;
	}

	rgbwFrame.reserve(nonlinearRgbColors->size());
	for (const auto& rgb : *nonlinearRgbColors)
	{
		HostOwnedRgbwQ16 sample = buildHostRgbwPolicySample(
			prepareHostRgbwInputQ16(floatToQ16(rgb.x), 1u),
			prepareHostRgbwInputQ16(floatToQ16(rgb.y), 0u),
			prepareHostRgbwInputQ16(floatToQ16(rgb.z), 2u));
		if (applyTransferAfterLutHeader)
			sample = applyTransferCurveToHostOwnedRgbw(sample);
		rgbwFrame.push_back(sample);
	}
}

void DriverOtherRawHid::buildPolicyRgbwFrame(const SharedOutputColors& nonlinearRgbColors, std::vector<HostOwnedRgbwQ16>& rgbwFrame) const
{
	rgbwFrame.clear();
	if (nonlinearRgbColors == nullptr || nonlinearRgbColors->empty())
		return;

	rgbwFrame.reserve(nonlinearRgbColors->size());
	for (const auto& rgb : *nonlinearRgbColors)
		rgbwFrame.push_back(emulatePolicyRgbwSample(floatToQ16(rgb.x), floatToQ16(rgb.y), floatToQ16(rgb.z)));
}

void DriverOtherRawHid::buildPolicyRgbwFrame(const std::vector<ColorRgb>& ledValues, std::vector<HostOwnedRgbwQ16>& rgbwFrame) const
{
	rgbwFrame.clear();
	if (ledValues.empty())
		return;

	rgbwFrame.reserve(ledValues.size());
	for (const ColorRgb& rgb : ledValues)
		rgbwFrame.push_back(emulatePolicyRgbwSample(expand8ToQ16(rgb.red), expand8ToQ16(rgb.green), expand8ToQ16(rgb.blue)));
}

void DriverOtherRawHid::computeHighlightShadowMask(const SharedOutputColors& nonlinearRgbColors, std::vector<uint8_t>& highlightMask) const
{
	highlightMask.assign(highlightMaskBytes(), 0u);
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightAvg = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightMedian = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightP95 = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightSpread = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightGate = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightCandidates = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 0u;
	if (!_scenePolicyEnableHighlight || !scenePolicyUsesHighlightSelection(_scenePolicyMode))
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 1u;
		return;
	}
	if (nonlinearRgbColors == nullptr || nonlinearRgbColors->empty())
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 2u;
		return;
	}

	const size_t ledCount = nonlinearRgbColors->size();
	const bool solverAwarePolicy = scenePolicyUsesHostSolver(_scenePolicyMode);
	const bool directSolverEnergy = scenePolicyUsesDirectSolverEnergyModel(_scenePolicyMode);
	const bool hostAppliesColorProcessing =
		((_transferCurveOwner == TransferCurveOwner::HyperHdr) && isTransferCurveEnabled()) ||
		((_calibrationHeaderOwner == CalibrationHeaderOwner::HyperHdr) && isCalibrationHeaderEnabled());
	std::vector<uint8_t> energies;
	energies.reserve(ledCount);
	uint16_t histogram[256] = { 0 };
	uint32_t sum = 0u;

	for (const auto& rgb : *nonlinearRgbColors)
	{
		const uint8_t energy = [&]() {
			if (hostAppliesColorProcessing)
			{
				const uint16_t redQ16 = applyHostChannelPipelineQ16(floatToQ16(rgb.x), 1u);
				const uint16_t greenQ16 = applyHostChannelPipelineQ16(floatToQ16(rgb.y), 0u);
				const uint16_t blueQ16 = applyHostChannelPipelineQ16(floatToQ16(rgb.z), 2u);
				const float processedEnergyUnit = solverAwarePolicy
					? (directSolverEnergy
						? computeDirectSolvedRgbWeightedLumaUnit(redQ16, greenQ16, blueQ16)
						: computeLegacySolvedRgbWeightedLumaUnit(redQ16, greenQ16, blueQ16))
					: computeWeightedLumaUnit(q16ToUnitFloat(redQ16), q16ToUnitFloat(greenQ16), q16ToUnitFloat(blueQ16), 0.0f, 0u, 0u, 0u, 0u, false);
				return energyUnitToByte(processedEnergyUnit);
			}
			const uint16_t redQ16 = floatToQ16(rgb.x);
			const uint16_t greenQ16 = floatToQ16(rgb.y);
			const uint16_t blueQ16 = floatToQ16(rgb.z);
			const uint16_t estimatedRedQ16 = isTransferCurveEnabled() ? applyTransferCurveQ16(redQ16, 1u) : redQ16;
			const uint16_t estimatedGreenQ16 = isTransferCurveEnabled() ? applyTransferCurveQ16(greenQ16, 0u) : greenQ16;
			const uint16_t estimatedBlueQ16 = isTransferCurveEnabled() ? applyTransferCurveQ16(blueQ16, 2u) : blueQ16;
			const float rawEnergyUnit = solverAwarePolicy
				? (directSolverEnergy
					? computeDirectSolvedRgbWeightedLumaUnit(estimatedRedQ16, estimatedGreenQ16, estimatedBlueQ16)
					: computeLegacySolvedRgbWeightedLumaUnit(redQ16, greenQ16, blueQ16))
				: computeRgbWeightedLumaUnit(rgb.x, rgb.y, rgb.z);
			return energyUnitToByte(applyTransferCurveUnit(rawEnergyUnit));
		}();
		energies.push_back(energy);
		histogram[energy]++;
		sum += energy;
	}

	const uint8_t avg = static_cast<uint8_t>((sum + (ledCount / 2u)) / ledCount);
	const uint8_t p05 = computeHistogramPercentileLevel(histogram, static_cast<uint16_t>(ledCount), 50u);
	const uint8_t p50 = computeHistogramPercentileLevel(histogram, static_cast<uint16_t>(ledCount), 500u);
	const uint8_t p95 = computeHistogramPercentileLevel(histogram, static_cast<uint16_t>(ledCount), 950u);
	const uint8_t spread = static_cast<uint8_t>(p95 - p05);
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightAvg = avg;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightMedian = p50;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightP95 = p95;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightSpread = spread;

	if (avg < _highlightShadowMinSceneAvg || p50 < _highlightShadowMinSceneMedian)
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 3u;
		return;
	}
	if (spread <= _highlightShadowUniformSpreadMax && p50 < _highlightShadowDimUniformMedian)
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 4u;
		return;
	}

	uint16_t cap = static_cast<uint16_t>(((ledCount * _highlightShadowPercent) + 99u) / 100u);
	if (cap == 0u)
		cap = 1u;
	if (cap > ledCount)
		cap = static_cast<uint16_t>(ledCount);

	const uint8_t threshold = computeHighlightThresholdFromHistogram(histogram, cap);
	const uint16_t triggerThreshold = std::min<uint16_t>(255u, static_cast<uint16_t>(threshold) + _highlightShadowTriggerMargin);
	const uint16_t contrastGate = std::min<uint16_t>(255u, static_cast<uint16_t>(avg) + _highlightShadowMinPeakDelta);
	const uint8_t gate = static_cast<uint8_t>(std::max(triggerThreshold, contrastGate));
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightGate = gate;

	std::vector<std::pair<uint8_t, uint16_t>> candidates;
	candidates.reserve(ledCount);
	for (uint16_t index = 0; index < ledCount; ++index)
	{
		const uint8_t energy = energies[index];
		if (energy >= gate)
			candidates.emplace_back(energy, index);
	}

	if (candidates.empty())
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 5u;
		return;
	}
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightCandidates = static_cast<uint16_t>(std::min<size_t>(candidates.size(), 0xFFFFu));

	std::stable_sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.first > rhs.first;
	});

	if (candidates.size() > cap)
		candidates.resize(cap);

	for (const auto& candidate : candidates)
	{
		const uint16_t index = candidate.second;
		highlightMask[index >> 3] |= static_cast<uint8_t>(1u << (index & 7u));
	}
}

void DriverOtherRawHid::computeHighlightShadowMask(const std::vector<ColorRgb>& ledValues, std::vector<uint8_t>& highlightMask) const
{
	highlightMask.assign(highlightMaskBytes(), 0u);
	if (!_scenePolicyEnableHighlight || !scenePolicyUsesHighlightSelection(_scenePolicyMode))
		return;
	if (ledValues.empty())
		return;

	auto shared = std::make_shared<std::vector<linalg::aliases::float3>>();
	shared->reserve(ledValues.size());
	for (const ColorRgb& rgb : ledValues)
	{
		shared->push_back(linalg::aliases::float3{
			static_cast<float>(rgb.red) / 255.0f,
			static_cast<float>(rgb.green) / 255.0f,
			static_cast<float>(rgb.blue) / 255.0f
		});
	}
	computeHighlightShadowMask(shared, highlightMask);
}

void DriverOtherRawHid::computeHighlightShadowMask(const std::vector<DriverOtherRawHid::HostOwnedRgbwQ16>& rgbwFrame, std::vector<uint8_t>& highlightMask) const
{
	highlightMask.assign(highlightMaskBytes(), 0u);
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightAvg = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightMedian = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightP95 = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightSpread = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightGate = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightCandidates = 0u;
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 0u;
	if (!_scenePolicyEnableHighlight || !scenePolicyUsesHighlightSelection(_scenePolicyMode))
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 1u;
		return;
	}
	if (rgbwFrame.empty())
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 2u;
		return;
	}

	const size_t ledCount = rgbwFrame.size();
	const bool solverAwarePolicy = scenePolicyUsesHostSolver(_scenePolicyMode);
	const bool directSolverEnergy = scenePolicyUsesDirectSolverEnergyModel(_scenePolicyMode);
	const bool hostAppliesColorProcessing = !isHostOwnedRgbwEnabled() && (shouldApplyTransferCurveOnHost() || shouldApplyCalibrationOnHost());
	std::vector<uint16_t> energiesQ16;
	std::vector<uint16_t> sortedEnergiesQ16;
	const auto q16ToByteRounded = [](const uint16_t q16) {
		return static_cast<uint8_t>((static_cast<uint32_t>(q16) * 255u + 32767u) / 65535u);
	};
	const auto q16PercentileFromSorted = [](const std::vector<uint16_t>& sortedValues, const uint16_t permille) {
		if (sortedValues.empty())
			return static_cast<uint16_t>(0u);

		uint64_t target = (static_cast<uint64_t>(sortedValues.size()) * static_cast<uint64_t>(permille) + 999u) / 1000u;
		if (target == 0u)
			target = 1u;

		const size_t index = static_cast<size_t>(std::min<uint64_t>(target - 1u, sortedValues.size() - 1u));
		return sortedValues[index];
	};
	energiesQ16.reserve(ledCount);
	sortedEnergiesQ16.reserve(ledCount);
	uint64_t sumQ16 = 0u;

	for (const DriverOtherRawHid::HostOwnedRgbwQ16& rgbw : rgbwFrame)
	{
		const uint16_t energyQ16 = [&]() {
			if (hostAppliesColorProcessing)
			{
				const DriverOtherRawHid::HostOwnedRgbwQ16 processed{
					applyHostChannelPipelineQ16(rgbw.red, 1u),
					applyHostChannelPipelineQ16(rgbw.green, 0u),
					applyHostChannelPipelineQ16(rgbw.blue, 2u),
					applyHostChannelPipelineQ16(rgbw.white, 3u)
				};
				const float processedEnergyUnit = solverAwarePolicy
					? (directSolverEnergy ? computeDirectSolvedRgbwWeightedLumaUnit(processed) : computeLegacySolvedRgbwWeightedLumaUnit(processed))
					: computeHostOwnedRgbwEnergyUnit(processed);
				return floatToQ16(processedEnergyUnit);
			}
			DriverOtherRawHid::HostOwnedRgbwQ16 estimated = rgbw;
			if (isTransferCurveEnabled())
			{
				estimated.red = applyTransferCurveQ16(rgbw.red, 1u);
				estimated.green = applyTransferCurveQ16(rgbw.green, 0u);
				estimated.blue = applyTransferCurveQ16(rgbw.blue, 2u);
				estimated.white = applyTransferCurveQ16(rgbw.white, 3u);
			}
			const float rawEnergyUnit = solverAwarePolicy
				? (directSolverEnergy ? computeDirectSolvedRgbwWeightedLumaUnit(estimated) : computeLegacySolvedRgbwWeightedLumaUnit(rgbw))
				: computeHostOwnedRgbwEnergyUnit(rgbw);
			return floatToQ16(applyTransferCurveUnit(rawEnergyUnit));
		}();
		energiesQ16.push_back(energyQ16);
		sortedEnergiesQ16.push_back(energyQ16);
		sumQ16 += energyQ16;
	}

	std::sort(sortedEnergiesQ16.begin(), sortedEnergiesQ16.end());

	const uint16_t avgQ16 = static_cast<uint16_t>((sumQ16 + (ledCount / 2u)) / ledCount);
	const uint16_t p05Q16 = q16PercentileFromSorted(sortedEnergiesQ16, 50u);
	const uint16_t p50Q16 = q16PercentileFromSorted(sortedEnergiesQ16, 500u);
	const uint16_t p95Q16 = q16PercentileFromSorted(sortedEnergiesQ16, 950u);
	const uint16_t spreadQ16 = (p95Q16 > p05Q16) ? static_cast<uint16_t>(p95Q16 - p05Q16) : static_cast<uint16_t>(0u);
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightAvg = q16ToByteRounded(avgQ16);
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightMedian = q16ToByteRounded(p50Q16);
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightP95 = q16ToByteRounded(p95Q16);
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightSpread = q16ToByteRounded(spreadQ16);

	if (avgQ16 < expand8ToQ16(_highlightShadowMinSceneAvg) || p50Q16 < expand8ToQ16(_highlightShadowMinSceneMedian))
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 3u;
		return;
	}
	if (spreadQ16 <= expand8ToQ16(_highlightShadowUniformSpreadMax)
		&& p50Q16 < expand8ToQ16(_highlightShadowDimUniformMedian))
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 4u;
		return;
	}

	uint16_t cap = static_cast<uint16_t>(((ledCount * _highlightShadowPercent) + 99u) / 100u);
	if (cap == 0u)
		cap = 1u;
	if (cap > ledCount)
		cap = static_cast<uint16_t>(ledCount);

	const size_t firstSelectedIndex = sortedEnergiesQ16.size() - cap;
	const uint16_t thresholdQ16 = sortedEnergiesQ16[firstSelectedIndex];
	const uint16_t triggerThresholdQ16 = static_cast<uint16_t>(std::min<uint32_t>(65535u, static_cast<uint32_t>(thresholdQ16) + static_cast<uint32_t>(expand8ToQ16(_highlightShadowTriggerMargin))));
	const uint16_t contrastGateQ16 = static_cast<uint16_t>(std::min<uint32_t>(65535u, static_cast<uint32_t>(avgQ16) + static_cast<uint32_t>(expand8ToQ16(_highlightShadowMinPeakDelta))));
	const uint16_t gateQ16 = std::max(triggerThresholdQ16, contrastGateQ16);
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightGate = q16ToByteRounded(gateQ16);

	std::vector<std::pair<uint16_t, uint16_t>> candidates;
	candidates.reserve(ledCount);
	for (uint16_t index = 0; index < ledCount; ++index)
	{
		const uint16_t energyQ16 = energiesQ16[index];
		if (energyQ16 >= gateQ16)
			candidates.emplace_back(energyQ16, index);
	}

	if (candidates.empty())
	{
		const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightRejectCode = 5u;
		return;
	}
	const_cast<DriverOtherRawHid*>(this)->_scenePolicyLastHighlightCandidates = static_cast<uint16_t>(std::min<size_t>(candidates.size(), 0xFFFFu));

	std::stable_sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.first > rhs.first;
	});

	if (candidates.size() > cap)
		candidates.resize(cap);

	for (const auto& candidate : candidates)
	{
		const uint16_t index = candidate.second;
		highlightMask[index >> 3] |= static_cast<uint8_t>(1u << (index & 7u));
	}
}

QString DriverOtherRawHid::discoverFirst()
{
#if defined(__linux__)
	QDir dir("/sys/class/hidraw");
	const QStringList entries = dir.entryList(QStringList() << "hidraw*", QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

	for (const QString& entry : entries)
	{
		const QString ueventPath = QString("/sys/class/hidraw/%1/device/uevent").arg(entry);
		QFile ueventFile(ueventPath);
		if (!ueventFile.open(QIODevice::ReadOnly | QIODevice::Text))
			continue;

		QString hidId;
		while (!ueventFile.atEnd())
		{
			const QString line = QString::fromLatin1(ueventFile.readLine()).trimmed();
			if (line.startsWith("HID_ID="))
			{
				hidId = line.mid(7);
				break;
			}
		}

		uint16_t vid = 0;
		uint16_t pid = 0;
		if (parseVidPidFromHidId(hidId, vid, pid) && vid == _vid && pid == _pid)
		{
			const QString devicePath = QString("/dev/%1").arg(entry);
			Info(_log, "RawHID auto-discovery found device: {:s}", devicePath);
			return devicePath;
		}
	}
#endif

	return QString();
}

QJsonObject DriverOtherRawHid::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;

	deviceList.push_back(QJsonObject{
		{"value", "auto"},
		{"name", "Auto"}
	});

#if defined(__linux__)
	QDir dir("/sys/class/hidraw");
	const QStringList entries = dir.entryList(QStringList() << "hidraw*", QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

	for (const QString& entry : entries)
	{
		const QString ueventPath = QString("/sys/class/hidraw/%1/device/uevent").arg(entry);
		QFile ueventFile(ueventPath);
		if (!ueventFile.open(QIODevice::ReadOnly | QIODevice::Text))
			continue;

		QString hidId;
		while (!ueventFile.atEnd())
		{
			const QString line = QString::fromLatin1(ueventFile.readLine()).trimmed();
			if (line.startsWith("HID_ID="))
			{
				hidId = line.mid(7);
				break;
			}
		}

		uint16_t vid = 0;
		uint16_t pid = 0;
		if (!parseVidPidFromHidId(hidId, vid, pid))
			continue;

		const QString devicePath = QString("/dev/%1").arg(entry);
		const QString name = QString("%1 (VID: 0x%2 PID: 0x%3)")
			.arg(devicePath)
			.arg(vid, 4, 16, QChar('0'))
			.arg(pid, 4, 16, QChar('0'));

		deviceList.push_back(QJsonObject{
			{"value", devicePath},
			{"name", name}
		});
	}
#endif

	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	devicesDiscovered.insert("devices", deviceList);

	return devicesDiscovered;
}

int DriverOtherRawHid::open()
{
	int retval = -1;
	_isDeviceReady = false;

	if (_fd >= 0)
		close();

#if !defined(__linux__)
	this->setInError("RawHID transport is currently supported on Linux only.");
	return retval;
#else
	const QString path = _isAutoDevicePath ? discoverFirst() : _devicePath;

	if (path.isEmpty())
	{
		this->setInError(QString("No RawHID device found for VID/PID 0x%1/0x%2")
			.arg(_vid, 4, 16, QChar('0'))
			.arg(_pid, 4, 16, QChar('0')));
		setupRetry(DEFAULT_RETRY_INTERVAL_MS);
		return retval;
	}

	_fd = ::open(path.toUtf8().constData(), O_RDWR | O_NONBLOCK);
	if (_fd < 0)
	{
		this->setInError(QString("Failed to open RawHID device %1: %2")
			.arg(path)
			.arg(QString::fromLocal8Bit(std::strerror(errno))));
		setupRetry(DEFAULT_RETRY_INTERVAL_MS);
		return retval;
	}

	_devicePath = path;
	_frameDropCounter = 0;
	_logPollCounter = 0;
	_incomingLogLine.clear();
	Debug(_log, "RawHID opened: {:s}", _devicePath);

	if (_delayAfterConnect_ms > 0)
	{
		QEventLoop loop;
		QTimer::singleShot(_delayAfterConnect_ms, &loop, &QEventLoop::quit);
		loop.exec();
	}

	_isDeviceReady = true;
	if (_readLogChannel)
		drainIncomingReports();
	retval = 0;
	return retval;
#endif
}

int DriverOtherRawHid::close()
{
	_isDeviceReady = false;
	_logPollCounter = 0;
	_incomingLogLine.clear();

#if defined(__linux__)
	if (_fd >= 0)
	{
		::close(_fd);
		_fd = -1;
	}
#endif

	return 0;
}

void DriverOtherRawHid::setInError(const QString& errorMsg)
{
	close();
	LedDevice::setInError(errorMsg);
}

void DriverOtherRawHid::handleIncomingLogPayload(const uint8_t* payload, uint16_t payloadSize)
{
	if (payload == nullptr || payloadSize == 0)
		return;

	QString chunk = QString::fromUtf8(reinterpret_cast<const char*>(payload), payloadSize);
	if (chunk.isEmpty())
		chunk = QString::fromLatin1(reinterpret_cast<const char*>(payload), payloadSize);

	if (chunk.isEmpty())
		return;

	chunk.replace('\r', '\n');
	_incomingLogLine.append(chunk);

	int splitIndex = 0;
	while ((splitIndex = _incomingLogLine.indexOf('\n')) >= 0)
	{
		QString line = _incomingLogLine.left(splitIndex).trimmed();
		_incomingLogLine.remove(0, splitIndex + 1);
		if (!line.isEmpty())
			Info(_log, "RawHID sideband: {:s}", line);
	}

	if (_incomingLogLine.size() > 384)
	{
		QString line = _incomingLogLine.trimmed();
		if (!line.isEmpty())
			Info(_log, "RawHID sideband: {:s}", line);
		_incomingLogLine.clear();
	}
}

void DriverOtherRawHid::handleIncomingReport(const uint8_t* report, size_t reportSize)
{
	if (report == nullptr || reportSize < static_cast<size_t>(HID_HEADER_SIZE))
		return;

	if (report[0] != HID_MAGIC_0)
		return;

	const uint8_t channel = report[1];
	if (channel != HID_LOG_MAGIC_1)
		return;

	uint16_t payloadSize = static_cast<uint16_t>((static_cast<uint16_t>(report[2]) << 8) | report[3]);
	const size_t maxPayload = reportSize - static_cast<size_t>(HID_HEADER_SIZE);
	if (payloadSize > maxPayload)
		payloadSize = static_cast<uint16_t>(maxPayload);

	handleIncomingLogPayload(report + HID_HEADER_SIZE, payloadSize);
}

int DriverOtherRawHid::drainIncomingReports()
{
#if !defined(__linux__)
	return 0;
#else
	if (!_readLogChannel || _fd < 0)
		return 0;

	std::vector<uint8_t> report(static_cast<size_t>(_reportSize), 0x00);
	int reportsRead = 0;

	while (reportsRead < _logReadBudget)
	{
		const ssize_t readLen = ::read(_fd, report.data(), report.size());
		if (readLen <= 0)
		{
			if (readLen < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
			{
				Debug(_log, "RawHID read error on {:s}: {:s}",
					_devicePath,
					QString::fromLocal8Bit(std::strerror(errno)));
			}
			break;
		}

		handleIncomingReport(report.data(), static_cast<size_t>(readLen));
		reportsRead++;
	}

	return reportsRead;
#endif
}

int DriverOtherRawHid::writeFrameAsReports(const uint8_t* frameData, size_t frameSize)
{
#if !defined(__linux__)
	Q_UNUSED(frameData);
	Q_UNUSED(frameSize);
	return -1;
#else
	if (_fd < 0)
	{
		if (open() < 0)
			return -1;
	}

	const int payloadPerReport = _reportSize - HID_HEADER_SIZE;
	if (payloadPerReport <= 0)
	{
		this->setInError("Invalid RawHID report size. Expected at least 8 bytes.");
		return -1;
	}

	std::vector<uint8_t> report(static_cast<size_t>(_reportSize), 0x00);
	size_t offset = 0;

	while (offset < frameSize)
	{
		const size_t remaining = frameSize - offset;
		const size_t chunk = qMin(remaining, static_cast<size_t>(payloadPerReport));

		report[0] = HID_MAGIC_0;
		report[1] = HID_MAGIC_1;
		report[2] = static_cast<uint8_t>((chunk >> 8) & 0xFF);
		report[3] = static_cast<uint8_t>(chunk & 0xFF);
		memcpy(report.data() + HID_HEADER_SIZE, frameData + offset, chunk);

		ssize_t written = ::write(_fd, report.data(), report.size());
		if (written != static_cast<ssize_t>(report.size()))
		{
			if (written >= 0)
			{
				this->setInError(QString("RawHID partial write on %1: sent %2 of %3 bytes")
					.arg(_devicePath)
					.arg(written)
					.arg(report.size()));

				if (_maxRetry > 0 && !_signalTerminate)
					QTimer::singleShot(2000, this, [this]() { if (!_signalTerminate) enable(); });

				return -1;
			}

			if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			{
				if (_writeTimeout_ms > 0)
				{
					pollfd pfd;
					pfd.fd = _fd;
					pfd.events = POLLOUT;
					pfd.revents = 0;

					const int pollResult = ::poll(&pfd, 1, _writeTimeout_ms);
					if (pollResult > 0)
						continue;
					if (pollResult < 0)
					{
						this->setInError(QString("RawHID poll error on %1: %2")
							.arg(_devicePath)
							.arg(QString::fromLocal8Bit(std::strerror(errno))));

						if (_maxRetry > 0 && !_signalTerminate)
							QTimer::singleShot(2000, this, [this]() { if (!_signalTerminate) enable(); });

						return -1;
					}
				}

				Debug(_log, "RawHID write timeout after {:d}ms: {:d} frames already dropped", _writeTimeout_ms, _frameDropCounter);
				++_frameDropCounter;
				if (_frameDropCounter > MAX_WRITE_TIMEOUTS)
				{
					this->setInError(QString("Timeout writing RawHID data to %1").arg(_devicePath));
					return -1;
				}

				if (_maxRetry > 0 && !_signalTerminate)
					QTimer::singleShot(2000, this, [this]() { if (!_signalTerminate) enable(); });
				return 0;
			}

			this->setInError(QString("RawHID write error on %1: %2")
				.arg(_devicePath)
				.arg(QString::fromLocal8Bit(std::strerror(errno))));

			if (_maxRetry > 0 && !_signalTerminate)
				QTimer::singleShot(2000, this, [this]() { if (!_signalTerminate) enable(); });

			return -1;
		}

		offset += chunk;
	}

	if (_readLogChannel)
	{
		_logPollCounter++;
		if (_logPollCounter >= _logPollIntervalFrames)
		{
			_logPollCounter = 0;
			drainIncomingReports();
		}
	}

	_frameDropCounter = 0;
	return static_cast<int>(frameSize);
#endif
}

std::pair<bool, int> DriverOtherRawHid::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (nonlinearRgbColors == nullptr || nonlinearRgbColors->empty())
		return { true, 0 };

	if (_ledCount != nonlinearRgbColors->size())
	{
		Warning(_log, "RawHID led count has changed (old: {:d}, new: {:d}). Rebuilding header.", _ledCount, nonlinearRgbColors->size());
		_ledCount = static_cast<uint>(nonlinearRgbColors->size());
		_ledRGBCount = _ledCount * 3;
		_ledRGBWCount = _ledCount * 4;
		createHeader();
	}

	const bool highlightPolicyActive = _scenePolicyEnableHighlight && scenePolicyUsesHighlightSelection(_scenePolicyMode);
	const bool solverAwareHostPolicy = scenePolicyUsesHostSolver(_scenePolicyMode);
	const bool hostAppliesTransferCurve = shouldApplyTransferCurveOnHost();
	const bool hostAppliesCalibration = shouldApplyCalibrationOnHost();
	const bool hostAppliesColorProcessing = hostAppliesTransferCurve || hostAppliesCalibration;
	const bool hostOwnedRgbw = isHostOwnedRgbwEnabled();
	const bool emulatedPolicyRgbw = shouldUseEmulatedRgbwPolicy() && highlightPolicyActive;
	const uint64_t rgbDataLength = static_cast<uint64_t>(nonlinearRgbColors->size()) * (hostOwnedRgbw ? ((_payloadMode == PayloadMode::Rgb12Carry) ? 6u : 8u) : ((_payloadMode == PayloadMode::Rgb12Carry) ? 5u : 6u));
	const uint64_t trailerLength = (_white_channel_calibration ? 4u : 0u) + 2u + 3u;
	const uint64_t requiredLength = static_cast<uint64_t>(_headerSize) + rgbDataLength + trailerLength;
	if (requiredLength > _ledBuffer.size())
	{
		createHeader();
		if (requiredLength > _ledBuffer.size())
			return { true, -1 };
	}

	uint8_t* writer = _ledBuffer.data() + _headerSize;
	uint8_t* hasher = writer;
	std::vector<DriverOtherRawHid::HostOwnedRgbwQ16> rgbwFrame;
	if (!hostOwnedRgbw || !isIceRgbwMode() || _payloadMode != PayloadMode::Rgb12Carry || !_ice_rgbw_temporal_dithering)
		_ice_rgbw_carry_error.clear();
	if (hostOwnedRgbw)
		buildHostOwnedRgbwFrame(nonlinearRgbColors, rgbwFrame);
	else if (emulatedPolicyRgbw)
		buildPolicyRgbwFrame(nonlinearRgbColors, rgbwFrame);

	std::vector<uint8_t> highlightMask;
	if (highlightPolicyActive)
	{
		if (hostOwnedRgbw || emulatedPolicyRgbw)
			computeHighlightShadowMask(rgbwFrame, highlightMask);
		else
			computeHighlightShadowMask(nonlinearRgbColors, highlightMask);
	}
	if (solverAwareHostPolicy || highlightPolicyActive)
		accumulateScenePolicyStats(countHighlightMaskBits(highlightMask));

	size_t index = 0u;
	for (const auto& rgb : *nonlinearRgbColors)
	{
		if (hostOwnedRgbw)
		{
			const DriverOtherRawHid::HostOwnedRgbwQ16& sample = rgbwFrame[index];
			const uint16_t redQ16 = sample.red;
			const uint16_t greenQ16 = sample.green;
			const uint16_t blueQ16 = sample.blue;
			const uint16_t whiteQ16 = sample.white;

			if (solverAwareHostPolicy)
			{
				const bool highlighted = highlightMaskBitSet(highlightMask, index);
				const std::array<uint16_t, 4> processedPolicyQ16{ redQ16, greenQ16, blueQ16, whiteQ16 };
				const std::array<uint16_t, 4> scaledPolicyQ16 =
					applyHighlightPolicyQ16Scale(processedPolicyQ16, highlighted, true, _highlightShadowQ16Threshold);
				const uint16_t emittedRedQ16 = scaledPolicyQ16[0];
				const uint16_t emittedGreenQ16 = scaledPolicyQ16[1];
				const uint16_t emittedBlueQ16 = scaledPolicyQ16[2];
				const uint16_t emittedWhiteQ16 = scaledPolicyQ16[3];

				captureHostPolicyProbeSample(
					index,
					highlighted,
					true,
					std::array<uint16_t, 4>{ redQ16, greenQ16, blueQ16, whiteQ16 },
					scaledPolicyQ16,
					std::array<uint16_t, 4>{ 0u, 0u, 0u, 0u },
					std::array<uint8_t, 4>{ 0u, 0u, 0u, 0u },
					std::array<uint8_t, 4>{ 0u, 0u, 0u, 0u },
					std::array<uint16_t, 4>{ emittedRedQ16, emittedGreenQ16, emittedBlueQ16, emittedWhiteQ16 });

				if (_payloadMode == PayloadMode::Rgb12Carry)
				{
					const auto buckets = quantizeHostOwnedRgbwCarry(index, emittedRedQ16, emittedGreenQ16, emittedBlueQ16, emittedWhiteQ16);
					writeBucket12RgbwAs12BitCarry(writer, buckets[0], buckets[1], buckets[2], buckets[3]);
				}
				else
					writeQ16RgbwAs16Bit(writer, emittedRedQ16, emittedGreenQ16, emittedBlueQ16, emittedWhiteQ16);
			}
			else
			{
				if (_payloadMode == PayloadMode::Rgb12Carry)
				{
					const auto buckets = quantizeHostOwnedRgbwCarry(index, redQ16, greenQ16, blueQ16, whiteQ16);
					writeBucket12RgbwAs12BitCarry(writer, buckets[0], buckets[1], buckets[2], buckets[3]);
				}
				else
					writeQ16RgbwAs16Bit(writer, redQ16, greenQ16, blueQ16, whiteQ16);
			}
		}
		else if (solverAwareHostPolicy)
		{
			const bool highlighted = highlightMaskBitSet(highlightMask, index);
			const uint16_t rawRedQ16 = floatToQ16(rgb.x);
			const uint16_t rawGreenQ16 = floatToQ16(rgb.y);
			const uint16_t rawBlueQ16 = floatToQ16(rgb.z);
			const uint16_t redQ16 = hostAppliesColorProcessing ? applyHostChannelPipelineQ16(rawRedQ16, 1u) : rawRedQ16;
			const uint16_t greenQ16 = hostAppliesColorProcessing ? applyHostChannelPipelineQ16(rawGreenQ16, 0u) : rawGreenQ16;
			const uint16_t blueQ16 = hostAppliesColorProcessing ? applyHostChannelPipelineQ16(rawBlueQ16, 2u) : rawBlueQ16;
			const std::array<uint16_t, 4> processedPolicyQ16{ redQ16, greenQ16, blueQ16, 0u };
			const std::array<uint16_t, 4> scaledPolicyQ16 =
				applyHighlightPolicyQ16Scale(processedPolicyQ16, highlighted, false, _highlightShadowQ16Threshold);
			const uint16_t emittedRedQ16 = scaledPolicyQ16[0];
			const uint16_t emittedGreenQ16 = scaledPolicyQ16[1];
			const uint16_t emittedBlueQ16 = scaledPolicyQ16[2];

			captureHostPolicyProbeSample(
				index,
				highlighted,
				false,
				std::array<uint16_t, 4>{ redQ16, greenQ16, blueQ16, 0u },
				scaledPolicyQ16,
				std::array<uint16_t, 4>{ 0u, 0u, 0u, 0u },
				std::array<uint8_t, 4>{ 0u, 0u, 0u, 0u },
				std::array<uint8_t, 4>{ 0u, 0u, 0u, 0u },
				std::array<uint16_t, 4>{ emittedRedQ16, emittedGreenQ16, emittedBlueQ16, 0u });

			if (_payloadMode == PayloadMode::Rgb12Carry)
				writeQ16RgbAs12BitCarry(writer, emittedRedQ16, emittedGreenQ16, emittedBlueQ16);
			else
				writeQ16RgbAs16Bit(writer, emittedRedQ16, emittedGreenQ16, emittedBlueQ16);
		}
		else if (hostAppliesColorProcessing)
		{
			const uint16_t redQ16 = applyHostChannelPipelineQ16(floatToQ16(rgb.x), 1u);
			const uint16_t greenQ16 = applyHostChannelPipelineQ16(floatToQ16(rgb.y), 0u);
			const uint16_t blueQ16 = applyHostChannelPipelineQ16(floatToQ16(rgb.z), 2u);

			if (_payloadMode == PayloadMode::Rgb12Carry)
				writeQ16RgbAs12BitCarry(writer, redQ16, greenQ16, blueQ16);
			else
				writeQ16RgbAs16Bit(writer, redQ16, greenQ16, blueQ16);
		}
		else if (_payloadMode == PayloadMode::Rgb12Carry)
		{
			writeColorAs12BitCarry(writer, rgb);
		}
		else
		{
			writeColorAs16Bit(writer, rgb);
		}
		index++;
	}

	whiteChannelExtension(writer);
	transferCurveExtension(writer);

	uint16_t fletcher1 = 0;
	uint16_t fletcher2 = 0;
	uint16_t fletcherExt = 0;
	uint8_t position = 0;

	while (hasher < writer)
	{
		const uint8_t byte = *(hasher++);
		fletcherExt = addMod255Fast(fletcherExt, static_cast<uint16_t>(byte ^ (position++)));
		fletcher1 = addMod255Fast(fletcher1, byte);
		fletcher2 = addMod255Fast(fletcher2, fletcher1);
	}

	*(writer++) = static_cast<uint8_t>(fletcher1);
	*(writer++) = static_cast<uint8_t>(fletcher2);
	*(writer++) = static_cast<uint8_t>((fletcherExt != 0x41) ? fletcherExt : 0xaa);

	const size_t frameLength = static_cast<size_t>(writer - _ledBuffer.data());
	return { true, writeFrameAsReports(_ledBuffer.data(), frameLength) };
}

int DriverOtherRawHid::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	if (isHostOwnedRgbwEnabled())
	{
		auto nonlinearRgbColors = std::make_shared<std::vector<linalg::aliases::float3>>();
		nonlinearRgbColors->reserve(ledValues.size());
		for (const ColorRgb& rgb : ledValues)
		{
			nonlinearRgbColors->push_back(linalg::aliases::float3{
				static_cast<float>(rgb.red) / 255.0f,
				static_cast<float>(rgb.green) / 255.0f,
				static_cast<float>(rgb.blue) / 255.0f
			});
		}
		return writeInfiniteColors(nonlinearRgbColors).second;
	}

	if (_ledCount != ledValues.size())
	{
		Warning(_log, "RawHID led count has changed (old: {:d}, new: {:d}). Rebuilding header.", _ledCount, ledValues.size());
		_ledCount = static_cast<uint>(ledValues.size());
		_ledRGBCount = _ledCount * 3;
		_ledRGBWCount = _ledCount * 4;
		createHeader();
	}

	const bool highlightPolicyActive = _scenePolicyEnableHighlight && scenePolicyUsesHighlightSelection(_scenePolicyMode);
	const bool solverAwareHostPolicy = scenePolicyUsesHostSolver(_scenePolicyMode);
	const bool hostAppliesTransferCurve = (_transferCurveOwner == TransferCurveOwner::HyperHdr) && isTransferCurveEnabled();
	const bool hostAppliesCalibration = (_calibrationHeaderOwner == CalibrationHeaderOwner::HyperHdr) && isCalibrationHeaderEnabled();
	const bool hostAppliesColorProcessing = hostAppliesTransferCurve || hostAppliesCalibration;
	const bool emulatedPolicyRgbw = shouldUseEmulatedRgbwPolicy() && highlightPolicyActive;
	const uint64_t rgbDataLength = static_cast<uint64_t>(ledValues.size()) * ((_payloadMode == PayloadMode::Rgb12Carry) ? 5u : 6u);
	const uint64_t trailerLength = (_white_channel_calibration ? 4u : 0u) + 2u + 3u;
	const uint64_t requiredLength = static_cast<uint64_t>(_headerSize) + rgbDataLength + trailerLength;
	if (requiredLength > _ledBuffer.size())
	{
		createHeader();
		if (requiredLength > _ledBuffer.size())
			return -1;
	}

	uint8_t* writer = _ledBuffer.data() + _headerSize;
	uint8_t* hasher = writer;
	std::vector<DriverOtherRawHid::HostOwnedRgbwQ16> rgbwFrame;
	if (emulatedPolicyRgbw)
		buildPolicyRgbwFrame(ledValues, rgbwFrame);
	std::vector<uint8_t> highlightMask;
	if (highlightPolicyActive)
	{
		if (emulatedPolicyRgbw)
			computeHighlightShadowMask(rgbwFrame, highlightMask);
		else
			computeHighlightShadowMask(ledValues, highlightMask);
	}
	if (solverAwareHostPolicy || highlightPolicyActive)
		accumulateScenePolicyStats(countHighlightMaskBits(highlightMask));

	size_t index = 0u;
	for (const ColorRgb& rgb : ledValues)
	{
		if (solverAwareHostPolicy)
		{
			const bool highlighted = highlightMaskBitSet(highlightMask, index);
			const uint16_t rawRedQ16 = static_cast<uint16_t>((static_cast<uint16_t>(rgb.red) << 8) | rgb.red);
			const uint16_t rawGreenQ16 = static_cast<uint16_t>((static_cast<uint16_t>(rgb.green) << 8) | rgb.green);
			const uint16_t rawBlueQ16 = static_cast<uint16_t>((static_cast<uint16_t>(rgb.blue) << 8) | rgb.blue);
			const uint16_t redQ16 = hostAppliesColorProcessing ? applyHostChannelPipelineQ16(rawRedQ16, 1u) : rawRedQ16;
			const uint16_t greenQ16 = hostAppliesColorProcessing ? applyHostChannelPipelineQ16(rawGreenQ16, 0u) : rawGreenQ16;
			const uint16_t blueQ16 = hostAppliesColorProcessing ? applyHostChannelPipelineQ16(rawBlueQ16, 2u) : rawBlueQ16;
			const std::array<uint16_t, 4> processedPolicyQ16{ redQ16, greenQ16, blueQ16, 0u };
			const std::array<uint16_t, 4> scaledPolicyQ16 =
				applyHighlightPolicyQ16Scale(processedPolicyQ16, highlighted, false, _highlightShadowQ16Threshold);
			const uint16_t emittedRedQ16 = scaledPolicyQ16[0];
			const uint16_t emittedGreenQ16 = scaledPolicyQ16[1];
			const uint16_t emittedBlueQ16 = scaledPolicyQ16[2];

			captureHostPolicyProbeSample(
				index,
				highlighted,
				false,
				std::array<uint16_t, 4>{ redQ16, greenQ16, blueQ16, 0u },
				scaledPolicyQ16,
				std::array<uint16_t, 4>{ 0u, 0u, 0u, 0u },
				std::array<uint8_t, 4>{ 0u, 0u, 0u, 0u },
				std::array<uint8_t, 4>{ 0u, 0u, 0u, 0u },
				std::array<uint16_t, 4>{ emittedRedQ16, emittedGreenQ16, emittedBlueQ16, 0u });

			if (_payloadMode == PayloadMode::Rgb12Carry)
				writeQ16RgbAs12BitCarry(writer, emittedRedQ16, emittedGreenQ16, emittedBlueQ16);
			else
				writeQ16RgbAs16Bit(writer, emittedRedQ16, emittedGreenQ16, emittedBlueQ16);
		}
		else if (hostAppliesColorProcessing)
		{
			const uint16_t redQ16 = applyHostChannelPipelineQ16(static_cast<uint16_t>((static_cast<uint16_t>(rgb.red) << 8) | rgb.red), 1u);
			const uint16_t greenQ16 = applyHostChannelPipelineQ16(static_cast<uint16_t>((static_cast<uint16_t>(rgb.green) << 8) | rgb.green), 0u);
			const uint16_t blueQ16 = applyHostChannelPipelineQ16(static_cast<uint16_t>((static_cast<uint16_t>(rgb.blue) << 8) | rgb.blue), 2u);

			if (_payloadMode == PayloadMode::Rgb12Carry)
				writeQ16RgbAs12BitCarry(writer, redQ16, greenQ16, blueQ16);
			else
				writeQ16RgbAs16Bit(writer, redQ16, greenQ16, blueQ16);
		}
		else if (_payloadMode == PayloadMode::Rgb12Carry)
		{
			writeColorAs12BitCarry(writer, rgb);
		}
		else
		{
			writeColorAs16Bit(writer, rgb);
		}
		index++;
	}

	whiteChannelExtension(writer);
	transferCurveExtension(writer);

	uint16_t fletcher1 = 0;
	uint16_t fletcher2 = 0;
	uint16_t fletcherExt = 0;
	uint8_t position = 0;

	while (hasher < writer)
	{
		const uint8_t byte = *(hasher++);
		fletcherExt = addMod255Fast(fletcherExt, static_cast<uint16_t>(byte ^ (position++)));
		fletcher1 = addMod255Fast(fletcher1, byte);
		fletcher2 = addMod255Fast(fletcher2, fletcher1);
	}

	*(writer++) = static_cast<uint8_t>(fletcher1);
	*(writer++) = static_cast<uint8_t>(fletcher2);
	*(writer++) = static_cast<uint8_t>((fletcherExt != 0x41) ? fletcherExt : 0xaa);

	const size_t frameLength = static_cast<size_t>(writer - _ledBuffer.data());
	return writeFrameAsReports(_ledBuffer.data(), frameLength);
}

LedDevice* DriverOtherRawHid::construct(const QJsonObject& deviceConfig)
{
	return new DriverOtherRawHid(deviceConfig);
}

bool DriverOtherRawHid::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("rawhid", "leds_group_3_serial", DriverOtherRawHid::construct);
