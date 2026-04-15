#ifndef PCH_ENABLED	
	#include <array>
	#include <QResource>
	#include <QBuffer>
	#include <QByteArray>
	#include <QTimer>
	#include <QList>
	#include <QHostAddress>
	#include <QDateTime>
	#include <QMap>
	#include <QMultiMap>
	#include <QMutex>
	#include <QMutexLocker>
	#include <QDir>
	#include <QFileInfo>
	#include <QNetworkReply>
	#include <QRegularExpression>

	#include <chrono>
	#include <csignal>
#endif

#include <QCoreApplication>
#include <QHostInfo>
#include <QTimeZone>

#include <HyperhdrConfig.h>
#include <api/HyperAPI.h>
#include <led-drivers/LedDeviceWrapper.h>
#include <led-drivers/LedDeviceManufactory.h>
#include <led-drivers/net/ProviderRestApi.h>

#include <base/GrabberWrapper.h>
#include <base/SystemWrapper.h>
#include <base/SoundCapture.h>

#include <base/ImageToLedManager.h>
#include <base/AccessManager.h>
#include <flatbuffers/server/FlatBuffersServer.h>
#include <json-utils/jsonschema/QJsonUtils.h>
#include <json-utils/jsonschema/QJsonSchemaChecker.h>
#include <json-utils/JsonUtils.h>
#include <performance-counters/PerformanceCounters.h>

// bonjour wrapper
#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>
#endif

using namespace hyperhdr;

namespace
{
	struct ParsedTransferCurveProfile
	{
		uint32_t bucketCount = 0u;
		QVector<uint16_t> red;
		QVector<uint16_t> green;
		QVector<uint16_t> blue;
		QVector<uint16_t> white;
	};

	struct ParsedCalibrationHeaderProfile
	{
		uint32_t bucketCount = 0u;
		QVector<uint16_t> red;
		QVector<uint16_t> green;
		QVector<uint16_t> blue;
		QVector<uint16_t> white;
	};

	struct ParsedRgbwLutHeaderProfile
	{
		uint32_t sourceGridSize = 0u;
		uint32_t gridSize = 0u;
		uint32_t entryCount = 0u;
		uint32_t axisMin = 0u;
		uint32_t axisMax = 65535u;
		bool requires3dInterpolation = true;
		QVector<uint16_t> red;
		QVector<uint16_t> green;
		QVector<uint16_t> blue;
		QVector<uint16_t> white;
	};

	struct ParsedSolverProfileEntry
	{
		uint16_t outputQ16 = 0u;
		uint8_t value = 0u;
		uint8_t bfi = 0u;
		uint8_t lowerValue = 0u;
		uint8_t upperValue = 0u;
		QString mode;
	};

	struct ParsedSolverProfile
	{
		std::array<QVector<ParsedSolverProfileEntry>, 4> channels;
	};

	QString transferHeadersDirectoryPath(const QString& rootPath)
	{
		return QDir::cleanPath(rootPath + QDir::separator() + "transfer_headers");
	}

	QString calibrationHeadersDirectoryPath(const QString& rootPath)
	{
		return QDir::cleanPath(rootPath + QDir::separator() + "calibration_headers");
	}

	QString rgbwLutHeadersDirectoryPath(const QString& rootPath)
	{
		return QDir::cleanPath(rootPath + QDir::separator() + "rgbw_lut_headers");
	}

	QString solverProfilesDirectoryPath(const QString& rootPath)
	{
		return QDir::cleanPath(rootPath + QDir::separator() + "solver_profiles");
	}

	QMutex& reportedVideoStreamStateMutex()
	{
		static QMutex mutex;
		return mutex;
	}

	QMap<int, QJsonObject>& reportedVideoStreamStates()
	{
		static QMap<int, QJsonObject> states;
		return states;
	}

	constexpr double kAutomaticLutBucketHysteresisRatio = 0.05;
	constexpr double kAutomaticLutBucketHysteresisMin = 4.0;
	constexpr double kAutomaticLutBucketHysteresisMax = 30.0;
	constexpr auto kRuntimeInterpolatedTransferProfilePrefix = "runtime-interpolated:";
	constexpr double kDefaultDolbyVisionSceneIndexHighlightWeight = 0.25;
	constexpr double kDolbyVisionSceneIndexHighlightGateAverageFloorNits = 100.0;
	constexpr double kDolbyVisionSceneIndexHighlightGateThreshold = 5.0;
	constexpr double kDolbyVisionSceneIndexHighlightGateRolloff = 0.08;
	constexpr double kDolbyVisionSceneIndexHighlightGateMinScale = 0.35;
	constexpr double kDolbyVisionSceneIndexDarkSceneBoostReferenceNitsMdl1000 = 95.0;
	constexpr double kDolbyVisionSceneIndexDarkSceneBoostReferenceNitsMdl4000 = 175.0;
	constexpr double kDolbyVisionSceneIndexDarkSceneBoostMaxScale = 4.5;
	constexpr double kDolbyVisionSceneIndexMdl4000MetadataBoostFloorRatio = 0.75;
	constexpr double kDolbyVisionSceneIndexMetadataFallbackWeight = 0.50;
	constexpr double kDolbyVisionSceneIndexAttackTimeMs = 250.0;
	constexpr double kDolbyVisionSceneIndexDecayTimeMs = 1200.0;
	constexpr double kDolbyVisionSceneIndexFastDecayTimeMs = 450.0;
	constexpr double kDolbyVisionSceneIndexFastDecayTriggerRatio = 0.85;
	constexpr int kAutomaticInterpolationBlendMaxStep = 20;

	QJsonObject buildTransferHeadersActiveState(const QString& rootPath, const QJsonObject& currentConfig, const QJsonObject& runtimeTransferState = QJsonObject());
	QJsonObject buildRgbwLutHeadersActiveState(const QString& rootPath, const QJsonObject& currentConfig);
	QJsonObject buildLutSwitchingConfig(const QString& rootPath, const QJsonObject& currentConfig);
	QJsonObject mergeJsonObjects(const QJsonObject& base, const QJsonObject& update);
	QJsonObject getReportedVideoStreamState(int instance);
	QJsonArray normalizeStringArray(const QJsonValue& value);
	QString normalizePositiveThresholdList(const QJsonValue& value, const QList<int>& defaults, QString* error = nullptr);
	QString normalizeLutSwitchingInput(QString input);
	QString normalizeDolbyVisionBucketMetric(QString metric);
	QString detectAutomaticLutMode(const QJsonObject& state);
	QString resolveAutomaticLutSwitchingInput(const QJsonObject& state);
	double extractKodiDolbyVisionConstraintNits(const QJsonObject& kodiDv, const QString& key, const QStringList& rawLabelKeys = {});
	QString lutSwitchingFileName(const QJsonObject& config, const QString& key, const QString& fallback);
	QString lutSwitchingAbsoluteFile(const QString& directoryPath, const QString& fileName);
	QString lutSwitchingPerInputFileName(const QJsonObject& config, const QString& input, const QString& mode);
	QJsonArray stringListToJsonArray(const QStringList& values);

	bool isLoopbackAddress(QString hostname)
	{
		hostname = hostname.trimmed();
		if (hostname.compare("localhost", Qt::CaseInsensitive) == 0)
			return true;

		if (hostname.startsWith("::ffff:", Qt::CaseInsensitive))
			hostname = hostname.mid(7);

		QHostAddress address(hostname);
		if (address.protocol() == QAbstractSocket::UnknownNetworkLayerProtocol)
		{
			auto result = QHostInfo::fromName(hostname);
			if (result.error() == QHostInfo::HostInfoError::NoError)
			{
				for (const QHostAddress& candidate : result.addresses())
				{
					if (candidate.isLoopback())
						return true;
				}
			}

			return false;
		}

		return address.isLoopback();
	}

	bool isAllowedLoopbackAutomationCommand(bool noListener, const QString& peerAddress, const QString& command, const QString& subcommand)
	{
		if (!noListener || !isLoopbackAddress(peerAddress))
			return false;

		if (command == "lut-switching")
			return subcommand == "apply" || subcommand == "reload" || subcommand == "save" || subcommand == "apply-runtime";

		if (command == "current-state")
			return subcommand == "video-stream-report";

		return false;
	}

	QJsonObject buildReportedVideoStreamState(const QJsonObject& state, int instance, const QString& peerAddress)
	{
		QJsonObject result = mergeJsonObjects(getReportedVideoStreamState(instance), state);
		const QString inferredMode = detectAutomaticLutMode(result);
		if (!inferredMode.isEmpty())
			result["classifiedMode"] = inferredMode;

		const QString inferredInput = resolveAutomaticLutSwitchingInput(result);
		if (!inferredInput.isEmpty())
			result["activeInput"] = inferredInput;

		result["instance"] = instance;
		result["peerAddress"] = peerAddress;
		result["receivedAtUtc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
		return result;
	}

	QJsonObject mergeJsonObjects(const QJsonObject& base, const QJsonObject& update)
	{
		QJsonObject result = base;
		for (auto iter = update.begin(); iter != update.end(); ++iter)
		{
			if (iter.value().isNull() || iter.value().isUndefined())
			{
				result.remove(iter.key());
				continue;
			}

			if (iter.value().isObject() && result.value(iter.key()).isObject())
				result[iter.key()] = mergeJsonObjects(result.value(iter.key()).toObject(), iter.value().toObject());
			else
				result[iter.key()] = iter.value();
		}

		return result;
	}

	void storeReportedVideoStreamState(int instance, const QJsonObject& state)
	{
		QMutexLocker locker(&reportedVideoStreamStateMutex());
		reportedVideoStreamStates()[instance] = state;
	}

	QJsonObject getReportedVideoStreamState(int instance)
	{
		QMutexLocker locker(&reportedVideoStreamStateMutex());
		return reportedVideoStreamStates().value(instance);
	}

	QString formatUtcTimestampMs(qint64 timestampMs)
	{
		if (timestampMs <= 0)
			return QString();
		return QDateTime::fromMSecsSinceEpoch(timestampMs, QTimeZone::UTC).toString(Qt::ISODateWithMs);
	}

	QString stripTransferCurveComments(QString content)
	{
		content.remove(QRegularExpression(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption));
		content.remove(QRegularExpression(R"(^\s*//.*$)", QRegularExpression::MultilineOption));
		return content;
	}

	QString stripCalibrationHeaderComments(QString content)
	{
		content.remove(QRegularExpression(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption));
		content.remove(QRegularExpression(R"(^\s*//.*$)", QRegularExpression::MultilineOption));
		return content;
	}

	QString stripRgbwLutHeaderComments(QString content)
	{
		content.remove(QRegularExpression(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption));
		content.remove(QRegularExpression(R"(^\s*//.*$)", QRegularExpression::MultilineOption));
		return content;
	}

	QString stripSolverProfileComments(QString content)
	{
		content.remove(QRegularExpression(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption));
		content.remove(QRegularExpression(R"(^\s*//.*$)", QRegularExpression::MultilineOption));
		return content;
	}

	QString neutralizeProgmem(QString content)
	{
		content.replace(QRegularExpression(R"(\bPROGMEM\b)"), "/* PROGMEM */");
		return content;
	}

	QString sanitizeTransferCurveSlug(QString name)
	{
		name = name.trimmed().toLower();
		name.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
		name.remove(QRegularExpression(R"(^_+|_+$)"));
		return name.left(96);
	}

	QString sanitizeCalibrationHeaderSlug(QString name)
	{
		name = name.trimmed().toLower();
		name.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
		name.remove(QRegularExpression(R"(^_+|_+$)"));
		return name.left(96);
	}

	QString sanitizeRgbwLutHeaderSlug(QString name)
	{
		name = name.trimmed().toLower();
		name.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
		name.remove(QRegularExpression(R"(^_+|_+$)"));
		return name.left(96);
	}

	QString sanitizeSolverProfileSlug(QString name)
	{
		name = name.trimmed().toLower();
		name.replace(QRegularExpression(R"([^a-z0-9]+)"), "_");
		name.remove(QRegularExpression(R"(^_+|_+$)"));
		return name.left(96);
	}

	QString normalizeTransferCurveProfileId(QString profile, QString& error)
	{
		QString normalized = profile.trimmed();
		if (normalized.isEmpty())
		{
			error = "Transfer curve profile is required";
			return QString();
		}

		const QString lowered = normalized.toLower();
		if (lowered == "disabled" || lowered == "off" || lowered == "none")
			return "disabled";
		if (lowered == "curve3_4_new" || lowered == "3_4_new" || lowered == "3.4_new")
			return "curve3_4_new";

		if (lowered.startsWith("custom:"))
			normalized.remove(0, QString("custom:").size());

		const QString slug = sanitizeTransferCurveSlug(normalized);
		if (slug.isEmpty())
		{
			error = "Custom transfer curve profile id is invalid";
			return QString();
		}

		return QString("custom:%1").arg(slug);
	}

	QString normalizeCalibrationHeaderProfileId(QString profile, QString& error)
	{
		QString normalized = profile.trimmed();
		if (normalized.isEmpty())
		{
			error = "Calibration header profile is required";
			return QString();
		}

		const QString lowered = normalized.toLower();
		if (lowered == "disabled" || lowered == "off" || lowered == "none")
			return "disabled";

		if (lowered.startsWith("custom:"))
			normalized.remove(0, QString("custom:").size());

		const QString slug = sanitizeCalibrationHeaderSlug(normalized);
		if (slug.isEmpty())
		{
			error = "Custom calibration header profile id is invalid";
			return QString();
		}

		return QString("custom:%1").arg(slug);
	}

	QString normalizeRgbwLutProfileId(QString profile, QString& error)
	{
		QString normalized = profile.trimmed();
		if (normalized.isEmpty())
		{
			error = "RGBW LUT profile is required";
			return QString();
		}

		const QString lowered = normalized.toLower();
		if (lowered == "disabled" || lowered == "off" || lowered == "none")
			return "disabled";

		if (lowered.startsWith("custom:"))
			normalized.remove(0, QString("custom:").size());

		const QString slug = sanitizeRgbwLutHeaderSlug(normalized);
		if (slug.isEmpty())
		{
			error = "Custom RGBW LUT profile id is invalid";
			return QString();
		}

		return QString("custom:%1").arg(slug);
	}

	QString normalizeSolverProfileId(QString profile, QString& error)
	{
		QString normalized = profile.trimmed();
		if (normalized.isEmpty())
			return "builtin";

		const QString lowered = normalized.toLower();
		if (lowered == "builtin" || lowered == "built-in" || lowered == "default")
			return "builtin";

		if (lowered.startsWith("custom:"))
			normalized.remove(0, QString("custom:").size());

		const QString slug = sanitizeSolverProfileSlug(normalized);
		if (slug.isEmpty())
		{
			error = "Custom solver profile id is invalid";
			return QString();
		}

		return QString("custom:%1").arg(slug);
	}

	bool transferCurveProfileExists(const QString& rootPath, const QString& profileId)
	{
		if (profileId == "disabled" || profileId == "curve3_4_new")
			return true;
		if (!profileId.startsWith("custom:"))
			return false;

		QString slug = profileId.mid(QString("custom:").size());
		slug = sanitizeTransferCurveSlug(slug);
		if (slug.isEmpty())
			return false;

		const QString jsonPath = QDir(transferHeadersDirectoryPath(rootPath)).filePath(slug + ".json");
		return QFileInfo::exists(jsonPath);
	}

	QString normalizeOptionalTransferCurveProfileId(const QString& rootPath, QString profile, QString& error)
	{
		profile = profile.trimmed();
		if (profile.isEmpty())
			return QString();

		const QString normalized = normalizeTransferCurveProfileId(profile, error);
		if (normalized.isEmpty())
			return QString();

		if (normalized.startsWith("custom:") && !transferCurveProfileExists(rootPath, normalized))
		{
			error = QString("Transfer header profile not found: %1").arg(normalized);
			return QString();
		}

		return normalized;
	}

	int jsonPositiveInt(const QJsonValue& value, int fallbackValue)
	{
		int result = fallbackValue;
		if (value.isDouble())
			result = value.toInt(fallbackValue);
		else if (value.isString())
		{
			bool ok = false;
			const int parsed = value.toString().trimmed().toInt(&ok);
			if (ok)
				result = parsed;
		}

		return (result > 0) ? result : fallbackValue;
	}

	double jsonPositiveDouble(const QJsonValue& value, double fallbackValue = 0.0)
	{
		double result = fallbackValue;
		if (value.isDouble())
			result = value.toDouble(fallbackValue);
		else if (value.isString())
		{
			bool ok = false;
			const double parsed = value.toString().trimmed().toDouble(&ok);
			if (ok)
				result = parsed;
		}

		return (result > 0.0) ? result : fallbackValue;
	}

	double jsonBoundedDouble(const QJsonValue& value, double fallbackValue, double minValue, double maxValue)
	{
		double result = fallbackValue;
		if (value.isDouble())
			result = value.toDouble(fallbackValue);
		else if (value.isString())
		{
			bool ok = false;
			const double parsed = value.toString().trimmed().toDouble(&ok);
			if (ok)
				result = parsed;
		}

		if (!std::isfinite(result))
			return fallbackValue;

		return qBound(minValue, result, maxValue);
	}

	int jsonBoundedInt(const QJsonValue& value, int fallbackValue, int minValue, int maxValue)
	{
		int result = fallbackValue;
		if (value.isDouble())
			result = value.toInt(fallbackValue);
		else if (value.isString())
		{
			bool ok = false;
			const int parsed = value.toString().trimmed().toInt(&ok);
			if (ok)
				result = parsed;
		}

		return qBound(minValue, result, maxValue);
	}

	struct LutBucketMetric
	{
		double value { 0.0 };
		QString source;
		QString unit;
		QJsonObject details;

		bool isValid() const
		{
			return value > 0.0;
		}
	};

	struct AutomaticLutInterpolationPlan
	{
		QStringList secondaryCandidates;
		QString lowerFile;
		QString upperFile;
		int lowerBucketIndex { -1 };
		int upperBucketIndex { -1 };
		double lowerBound { 0.0 };
		double upperBound { 0.0 };
		double ratio { 0.0 };
		int blend { 0 };

		bool isActive() const
		{
			return blend > 0 && blend < 255 && !lowerFile.isEmpty() && !upperFile.isEmpty();
		}
	};

	struct AutomaticTransferInterpolationPlan
	{
		QString lowerProfile;
		QString upperProfile;
		int lowerTierIndex { -1 };
		int upperTierIndex { -1 };
		double lowerBound { 0.0 };
		double upperBound { 0.0 };
		double ratio { 0.0 };
		int blend { 0 };

		bool isActive() const
		{
			return blend > 0 && blend < 255 && !lowerProfile.isEmpty() && !upperProfile.isEmpty() && lowerProfile != upperProfile;
		}
	};

	struct AutomaticTransferSelectionPlan
	{
		QString selectedProfile;
		QString applyProfileId;
		QString tier;
		QJsonObject selectionContext;
		AutomaticTransferInterpolationPlan interpolation;
	};

	QString detectTransferAutomationCategory(const QJsonObject& state)
	{
		const QJsonObject signal = state["signal"].toObject();
		const QJsonObject hdr = state["hdr"].toObject();
		const QJsonObject dv = state["dv"].toObject();
		const QJsonObject kodiDv = state["kodiDv"].toObject();

		const QString classifiedMode = state["classifiedMode"].toString().trimmed().toLower();
		const QString dynamicRange = signal["dynamic_range"].toString().trimmed().toLower();
		const QString eotf = hdr["eotf"].toString().trimmed().toLower();

		auto hasKodiDvTelemetry = [&kodiDv]() {
			if (kodiDv.isEmpty())
				return false;
			if (jsonPositiveDouble(kodiDv["l1AvgNits"], 0.0) > 0.0 ||
				jsonPositiveDouble(kodiDv["l1MaxNits"], 0.0) > 0.0 ||
				jsonPositiveDouble(kodiDv["sourceMaxNits"], 0.0) > 0.0 ||
				jsonPositiveDouble(kodiDv["rpuMdlNits"], 0.0) > 0.0 ||
				jsonPositiveDouble(kodiDv["l6MaxLumNits"], 0.0) > 0.0 ||
				jsonPositiveDouble(kodiDv["l6MaxCllNits"], 0.0) > 0.0 ||
				jsonPositiveDouble(kodiDv["l6MaxFallNits"], 0.0) > 0.0)
			{
				return true;
			}

			const QJsonObject rawLabels = kodiDv["rawLabels"].toObject();
			return !rawLabels.isEmpty();
		};

		if (classifiedMode == "sdr" || eotf == "sdr" || dynamicRange == "sdr")
			return "sdr";

		if (classifiedMode == "lldv" || classifiedMode == "dolby-vision" || dynamicRange == "lldv" || dynamicRange == "dolby-vision")
			return "dolby-vision";

		if (eotf == "hlg" || dynamicRange == "hlg")
			return "hlg";

		if (classifiedMode == "hdr" || eotf == "hdr" || eotf == "pq" || dynamicRange == "hdr" || dynamicRange == "hdr10" || dynamicRange == "hdr10+" || dynamicRange == "pq")
			return "hdr";

		if (dv["active"].toBool(false))
			return "dolby-vision";

		if (hasKodiDvTelemetry())
			return "dolby-vision";

		return QString();
	}

	QString resolveAutomaticLutSwitchingInput(const QJsonObject& state)
	{
		const QString normalizedInput = normalizeLutSwitchingInput(state["activeInput"].toString());
		if (!normalizedInput.isEmpty())
			return normalizedInput;

		const QString category = detectTransferAutomationCategory(state);
		if (category != "dolby-vision")
			return QString();

		const QJsonObject kodiDv = state["kodiDv"].toObject();
		if (kodiDv.isEmpty())
			return QString();

		if (jsonPositiveDouble(kodiDv["l1AvgNits"], 0.0) > 0.0 ||
			jsonPositiveDouble(kodiDv["l1MaxNits"], 0.0) > 0.0 ||
			jsonPositiveDouble(kodiDv["sourceMaxNits"], 0.0) > 0.0 ||
			jsonPositiveDouble(kodiDv["rpuMdlNits"], 0.0) > 0.0 ||
			jsonPositiveDouble(kodiDv["l6MaxLumNits"], 0.0) > 0.0 ||
			jsonPositiveDouble(kodiDv["l6MaxCllNits"], 0.0) > 0.0 ||
			jsonPositiveDouble(kodiDv["l6MaxFallNits"], 0.0) > 0.0 ||
			!kodiDv["rawLabels"].toObject().isEmpty())
		{
			return "ugoos";
		}

		return QString();
	}

	QString detectAutomaticLutMode(const QJsonObject& state)
	{
		const QString category = detectTransferAutomationCategory(state);
		if (category == "sdr")
			return "sdr";
		if (category == "hlg" || category == "hdr")
			return "hdr";
		if (category == "dolby-vision")
			return "lldv";
		return QString();
	}

	QList<int> parsePositiveThresholdList(const QJsonValue& value, const QList<int>& defaults)
	{
		const QString normalized = normalizePositiveThresholdList(value, defaults);
		QList<int> parsedValues;
		for (const QString& token : normalized.split(',', Qt::SkipEmptyParts))
		{
			bool ok = false;
			const int parsed = token.trimmed().toInt(&ok);
			if (ok && parsed > 0)
				parsedValues.append(parsed);
		}
		return parsedValues.isEmpty() ? defaults : parsedValues;
	}

	QJsonArray readNormalizedStringArray(const QJsonObject& config, const QString& key)
	{
		return normalizeStringArray(config.value(key));
	}

	double roundUpToThreshold(const QList<int>& thresholds, double value)
	{
		if (value <= 0.0)
			return value;

		for (int threshold : thresholds)
		{
			if (static_cast<double>(threshold) >= value)
				return static_cast<double>(threshold);
		}

		return value;
	}

	int smoothInterpolationBlendTarget(int currentBlend, int targetBlend, int maxStep)
	{
		const int boundedCurrent = qBound(0, currentBlend, 255);
		const int boundedTarget = qBound(0, targetBlend, 255);
		const int boundedStep = qMax(1, maxStep);
		if (qAbs(boundedTarget - boundedCurrent) <= boundedStep)
			return boundedTarget;
		return boundedTarget > boundedCurrent ? (boundedCurrent + boundedStep) : (boundedCurrent - boundedStep);
	}

	QString jsonArrayValueAt(const QJsonArray& values, int index)
	{
		if (index < 0 || index >= values.size())
			return QString();
		return values.at(index).toString().trimmed();
	}

	int selectThresholdBucketIndex(const QList<int>& thresholds, double metricValue)
	{
		if (metricValue <= 0.0)
			return 0;

		for (int index = 0; index < thresholds.size(); ++index)
		{
			if (metricValue <= thresholds[index])
				return index;
		}

		return thresholds.size();
	}

	double automaticLutBucketHysteresisMargin(int threshold)
	{
		const double scaled = static_cast<double>(qMax(1, threshold)) * kAutomaticLutBucketHysteresisRatio;
		return qBound(kAutomaticLutBucketHysteresisMin, scaled, kAutomaticLutBucketHysteresisMax);
	}

	int selectThresholdBucketIndexWithHysteresis(const QList<int>& thresholds, double metricValue, const QJsonObject& previousDecision, const QString& category, const QString& family, const QString& input)
	{
		const int selectedBucket = selectThresholdBucketIndex(thresholds, metricValue);
		if (metricValue <= 0.0)
			return selectedBucket;

		if (previousDecision["category"].toString() != category)
			return selectedBucket;
		if (previousDecision["family"].toString() != family)
			return selectedBucket;
		if (previousDecision["input"].toString() != input)
			return selectedBucket;

		const int previousBucket = previousDecision["bucketIndex"].toInt(-1);
		if (previousBucket < 0 || previousBucket > thresholds.size())
			return selectedBucket;

		if (previousBucket == selectedBucket)
			return selectedBucket;

		if (previousBucket > 0)
		{
			const int lowerThreshold = thresholds[previousBucket - 1];
			if (metricValue < static_cast<double>(lowerThreshold) - automaticLutBucketHysteresisMargin(lowerThreshold))
				return selectedBucket;
		}

		if (previousBucket < thresholds.size())
		{
			const int upperThreshold = thresholds[previousBucket];
			if (metricValue > static_cast<double>(upperThreshold) + automaticLutBucketHysteresisMargin(upperThreshold))
				return selectedBucket;
		}

		return previousBucket;
	}

	QString transferTierLabel(int tierIndex, int tierCount)
	{
		const int normalizedCount = qMax(1, tierCount);
		const int normalizedIndex = qBound(0, tierIndex, normalizedCount - 1);
		return QString("rank %1/%2").arg(normalizedIndex + 1).arg(normalizedCount);
	}

	int scaledBucketTierIndex(int bucketIndex, int bucketCount, int tierCount)
	{
		if (bucketCount <= 1 || tierCount <= 1)
			return 0;

		bucketIndex = qBound(0, bucketIndex, bucketCount - 1);
		return qMin(tierCount - 1, (bucketIndex * tierCount) / bucketCount);
	}

	int scaledRankCountFromBucketCount(int bucketCount, int referenceRankCount, int referenceBucketCount)
	{
		const int normalizedBuckets = qMax(1, bucketCount);
		const int normalizedRankCount = qMax(1, referenceRankCount);
		const int normalizedReferenceBuckets = qMax(1, referenceBucketCount);
		const int scaled = qRound((static_cast<double>(normalizedBuckets) * static_cast<double>(normalizedRankCount)) / static_cast<double>(normalizedReferenceBuckets));
		return qBound(1, scaled, normalizedBuckets);
	}

	int desiredTransferTierCount(const QString& category, const QString& family, int bucketCount)
	{
		if (category == "hdr")
			return qMax(1, bucketCount);

		if (category == "dolby-vision")
		{
			if (family == "mdl4000")
				return scaledRankCountFromBucketCount(bucketCount, 3, 5);
			return scaledRankCountFromBucketCount(bucketCount, 2, 4);
		}

		return 1;
	}

	QStringList buildTransferProfileTierList(const QJsonObject& config, const QString& category, const QString& family, int tierCount)
	{
		auto readProfile = [&config](const QString& key) {
			return config[key].toString().trimmed();
		};

		auto appendUniqueNonEmpty = [](QStringList& target, const QString& value) {
			if (!value.isEmpty() && !target.contains(value))
				target.append(value);
		};

		const QString lowProfile = (category == "hdr") ? readProfile("transferHdrLowProfile") : readProfile("transferDolbyVisionLowProfile");
		const QString midProfile = (category == "hdr") ? readProfile("transferHdrMidProfile") : readProfile("transferDolbyVisionMidProfile");
		const QString highProfile = (category == "hdr") ? readProfile("transferHdrHighProfile") : readProfile("transferDolbyVisionHighProfile");

		QStringList anchors;
		if (category == "dolby-vision" && family == "mdl1000")
		{
			appendUniqueNonEmpty(anchors, lowProfile);
			appendUniqueNonEmpty(anchors, highProfile);
			if (anchors.size() < 2)
				appendUniqueNonEmpty(anchors, midProfile);
		}
		else
		{
			appendUniqueNonEmpty(anchors, lowProfile);
			appendUniqueNonEmpty(anchors, midProfile);
			appendUniqueNonEmpty(anchors, highProfile);
		}

		if (anchors.isEmpty())
			return anchors;

		const int normalizedTierCount = qMax(1, tierCount);
		QStringList profiles;
		profiles.reserve(normalizedTierCount);
		for (int index = 0; index < normalizedTierCount; ++index)
		{
			const int anchorIndex = qMin(anchors.size() - 1, (index * anchors.size()) / normalizedTierCount);
			profiles.append(anchors.at(anchorIndex));
		}

		return profiles;
	}

	QString normalizeExistingFilePath(const QString& filePath)
	{
		return QDir::cleanPath(filePath.trimmed());
	}

	void markAutomaticLutSwitchApplied(QJsonObject& decision, const QJsonObject& runtime)
	{
		const qint64 nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
		const QString normalizedCurrentFile = normalizeExistingFilePath(runtime["loadedFile"].toString());
		const QString normalizedInterpolationFile = normalizeExistingFilePath(runtime["loadedInterpolationFile"].toString());
		decision["applied"] = true;
		decision["needsApply"] = false;
		decision["reason"] = "Applied automatic LUT bucket selection";
		decision["appliedAtUtc"] = formatUtcTimestampMs(nowMs);
		if (!normalizedCurrentFile.isEmpty())
		{
			decision["currentFile"] = normalizedCurrentFile;
			decision["lastAppliedTargetFile"] = normalizedCurrentFile;
		}
		if (!normalizedInterpolationFile.isEmpty())
			decision["currentInterpolationFile"] = normalizedInterpolationFile;
		decision["currentInterpolationBlend"] = runtime["interpolationBlend"].toInt(0);
		decision["interpolationActive"] = runtime["interpolationActive"].toBool(false);
	}

	void appendUniqueCandidate(QStringList& candidates, const QString& filePath)
	{
		const QString cleaned = normalizeExistingFilePath(filePath);
		if (cleaned.isEmpty() || candidates.contains(cleaned, Qt::CaseInsensitive))
			return;
		candidates.append(cleaned);
	}

	QString resolveFirstExistingCandidate(const QStringList& candidates)
	{
		for (const QString& candidate : candidates)
		{
			if (QFileInfo::exists(candidate))
				return normalizeExistingFilePath(candidate);
		}
		return QString();
	}

	QString buildRuntimeInterpolatedTransferProfileId(const QString& lowerProfile, const QString& upperProfile, int blend)
	{
		return QStringLiteral("%1%2|%3|%4")
			.arg(QString::fromLatin1(kRuntimeInterpolatedTransferProfilePrefix))
			.arg(lowerProfile.trimmed())
			.arg(upperProfile.trimmed())
			.arg(qBound(0, blend, 255));
	}

	QList<int> jsonArrayToPositiveIntList(const QJsonValue& value)
	{
		QList<int> values;
		for (const QJsonValue& entry : value.toArray())
		{
			const int parsed = entry.toInt(0);
			if (parsed > 0)
				values.append(parsed);
		}
		return values;
	}

	void markAutomaticTransferSwitchApplied(QJsonObject& decision, const QJsonObject& runtime)
	{
		decision["applied"] = true;
		decision["needsApply"] = false;
		decision["currentProfile"] = runtime["profile"].toString();
		decision["reason"] = "Applied automatic transfer profile without device reset";
		decision["currentInterpolationActive"] = runtime["interpolationActive"].toBool(false);
		if (runtime["interpolationActive"].toBool(false))
		{
			decision["currentInterpolationLowerProfile"] = runtime["interpolationLowerProfile"].toString();
			decision["currentInterpolationUpperProfile"] = runtime["interpolationUpperProfile"].toString();
			decision["currentInterpolationBlend"] = runtime["interpolationBlend"].toInt(0);
			decision["currentInterpolationRatio"] = runtime["interpolationRatio"].toDouble(0.0);
		}
	}

	AutomaticTransferInterpolationPlan buildAdjacentTransferInterpolationPlan(const QList<int>& thresholds, int bucketCount, int tierCount, const QStringList& tierProfiles, double metricValue, bool bucketAligned)
	{
		AutomaticTransferInterpolationPlan plan;
		if (metricValue <= 0.0 || thresholds.size() < 2 || bucketCount <= 1 || tierCount <= 1 || tierProfiles.isEmpty())
			return plan;

		auto tierIndexForBucket = [bucketAligned, bucketCount, tierCount](int bucketIndex) {
			if (bucketAligned)
				return qBound(0, bucketIndex, tierCount - 1);
			return scaledBucketTierIndex(bucketIndex, bucketCount, tierCount);
		};

		auto buildPlanForBuckets = [&](int lowerBucketIndex, int upperBucketIndex, double lowerBound, double upperBound) {
			if (upperBound <= lowerBound || metricValue < lowerBound || metricValue > upperBound)
				return false;

			const int lowerTierIndex = tierIndexForBucket(lowerBucketIndex);
			const int upperTierIndex = tierIndexForBucket(upperBucketIndex);
			if (lowerTierIndex == upperTierIndex)
				return false;

			const QString lowerProfile = tierProfiles.value(lowerTierIndex).trimmed();
			const QString upperProfile = tierProfiles.value(upperTierIndex).trimmed();
			if (lowerProfile.isEmpty() || upperProfile.isEmpty() || lowerProfile == upperProfile)
				return false;

			const double ratio = qBound(0.0, (metricValue - lowerBound) / (upperBound - lowerBound), 1.0);
			const int blend = qBound(1, qRound(ratio * 255.0), 254);
			plan.lowerProfile = lowerProfile;
			plan.upperProfile = upperProfile;
			plan.lowerTierIndex = lowerTierIndex;
			plan.upperTierIndex = upperTierIndex;
			plan.lowerBound = lowerBound;
			plan.upperBound = upperBound;
			plan.ratio = ratio;
			plan.blend = blend;
			return true;
		};

		for (int lowerBucketIndex = 0; lowerBucketIndex + 1 < thresholds.size(); ++lowerBucketIndex)
		{
			const double lowerBound = static_cast<double>(thresholds[lowerBucketIndex]);
			const double upperBound = static_cast<double>(thresholds[lowerBucketIndex + 1]);
			const int upperBucketIndex = lowerBucketIndex + 1;
			if (buildPlanForBuckets(lowerBucketIndex, upperBucketIndex, lowerBound, upperBound))
				return plan;
		}

		return plan;
	}

	AutomaticLutInterpolationPlan buildAdjacentBucketInterpolationPlan(const QList<int>& thresholds, const QJsonArray& bucketFiles, const QString& directoryPath, double metricValue)
	{
		AutomaticLutInterpolationPlan plan;
		if (metricValue <= 0.0 || thresholds.size() < 2 || bucketFiles.size() < 2)
			return plan;

		auto buildPlanForBuckets = [&](int lowerBucketIndex, int upperBucketIndex, double lowerBound, double upperBound) {
			if (upperBound <= lowerBound)
				return false;
			if (metricValue < lowerBound || metricValue > upperBound)
				return false;

			const QString lowerBucketFile = jsonArrayValueAt(bucketFiles, lowerBucketIndex);
			const QString upperBucketFile = jsonArrayValueAt(bucketFiles, upperBucketIndex);
			if (lowerBucketFile.isEmpty() || upperBucketFile.isEmpty())
				return false;

			const QString resolvedLowerFile = lutSwitchingAbsoluteFile(directoryPath, lowerBucketFile);
			const QString resolvedUpperFile = lutSwitchingAbsoluteFile(directoryPath, upperBucketFile);
			if (!QFileInfo::exists(resolvedLowerFile) || !QFileInfo::exists(resolvedUpperFile) || resolvedLowerFile.compare(resolvedUpperFile, Qt::CaseInsensitive) == 0)
				return false;

			const double ratio = qBound(0.0, (metricValue - lowerBound) / (upperBound - lowerBound), 1.0);
			const int blend = qBound(1, qRound(ratio * 255.0), 254);
			plan.lowerFile = resolvedLowerFile;
			plan.upperFile = resolvedUpperFile;
			plan.lowerBucketIndex = lowerBucketIndex;
			plan.upperBucketIndex = upperBucketIndex;
			plan.lowerBound = lowerBound;
			plan.upperBound = upperBound;
			plan.ratio = ratio;
			plan.blend = blend;
			plan.secondaryCandidates = QStringList{ resolvedUpperFile };
			return true;
		};

		for (int lowerBucketIndex = 0; lowerBucketIndex + 1 < thresholds.size(); ++lowerBucketIndex)
		{
			const double lowerBound = static_cast<double>(thresholds[lowerBucketIndex]);
			const double upperBound = static_cast<double>(thresholds[lowerBucketIndex + 1]);
			const int upperBucketIndex = lowerBucketIndex + 1;
			if (buildPlanForBuckets(lowerBucketIndex, upperBucketIndex, lowerBound, upperBound))
				return plan;
		}

		return plan;
	}

	double extractPositiveDoubleFromString(const QString& value)
	{
		const QRegularExpressionMatch numericMatch = QRegularExpression(QStringLiteral("([0-9]+(?:\\.[0-9]+)?)")).match(value);
		if (!numericMatch.hasMatch())
			return 0.0;

		bool ok = false;
		const double parsed = numericMatch.captured(1).toDouble(&ok);
		return ok && parsed > 0.0 ? parsed : 0.0;
	}

	double extractKodiDolbyVisionRpuMdlNits(const QJsonObject& kodiDv, QString& source)
	{
		auto readNumericField = [&kodiDv, &source](const QString& key) {
			const double value = jsonPositiveDouble(kodiDv[key], 0.0);
			if (value > 0.0)
				source = key;
			return value;
		};

		for (const QString& key : QStringList{
			QStringLiteral("sourceMaxNits"),
			QStringLiteral("rpuMdlNits"),
			QStringLiteral("l6MaxLumNits")
		})
		{
			const double value = readNumericField(key);
			if (value > 0.0)
				return value;
		}

		const QJsonObject rawLabels = kodiDv["rawLabels"].toObject();
		for (const QString& label : QStringList{
			QStringLiteral("Player.Process(video.dovi.source.max.nits)"),
			QStringLiteral("Player.Process(video.dovi.l6.max.lum)")
		})
		{
			const double value = extractPositiveDoubleFromString(rawLabels[label].toString());
			if (value > 0.0)
			{
				source = QStringLiteral("rawLabels.%1").arg(label);
				return value;
			}
		}

		source.clear();
		return 0.0;
	}

	double extractKodiDolbyVisionConstraintNits(const QJsonObject& kodiDv, const QString& key, const QStringList& rawLabelKeys)
	{
		const double directValue = jsonPositiveDouble(kodiDv[key], 0.0);
		if (directValue > 0.0)
			return directValue;

		const QJsonObject rawLabels = kodiDv["rawLabels"].toObject();
		for (const QString& rawLabelKey : rawLabelKeys)
		{
			const double rawValue = extractPositiveDoubleFromString(rawLabels[rawLabelKey].toString());
			if (rawValue > 0.0)
				return rawValue;
		}

		return 0.0;
	}

	double extractKodiDolbyVisionSourceMaxNits(const QJsonObject& kodiDv)
	{
		const double directValue = jsonPositiveDouble(kodiDv["sourceMaxNits"], 0.0);
		if (directValue > 0.0)
			return directValue;

		const QJsonObject rawLabels = kodiDv["rawLabels"].toObject();
		return extractPositiveDoubleFromString(rawLabels[QStringLiteral("Player.Process(video.dovi.source.max.nits)")].toString());
	}

	QString detectDolbyVisionBucketFamily(const QJsonObject& state, QString& source)
	{
		const QJsonObject kodiDv = state["kodiDv"].toObject();
		const double kodiRpuMdl = extractKodiDolbyVisionRpuMdlNits(kodiDv, source);
		if (kodiRpuMdl > 0.0)
			return (kodiRpuMdl > 1000.0) ? "mdl4000" : "mdl1000";

		const int kodiL6MaxLum = jsonPositiveInt(kodiDv["l6MaxLumNits"], 0);
		if (kodiL6MaxLum > 0)
		{
			source = "kodiDv.l6MaxLumNits:fallback";
			return (kodiL6MaxLum > 1000) ? "mdl4000" : "mdl1000";
		}

		const int kodiMaxCll = jsonPositiveInt(kodiDv["l6MaxCllNits"], 0);
		if (kodiMaxCll > 0)
		{
			source = "kodiDv.l6MaxCllNits:fallback";
			return (kodiMaxCll > 1000) ? "mdl4000" : "mdl1000";
		}

		const QJsonObject hdr = state["hdr"].toObject();
		const int hdrMaxDisplayLum = jsonPositiveInt(hdr["max_display_lum_nits"], 0);
		if (hdrMaxDisplayLum > 0)
		{
			source = "hdr.max_display_lum_nits:fallback";
			return (hdrMaxDisplayLum > 1000) ? "mdl4000" : "mdl1000";
		}

		const int hdrMaxCll = jsonPositiveInt(hdr["max_cll_nits"], 0);
		if (hdrMaxCll > 0)
		{
			source = "hdr.max_cll_nits:fallback";
			return (hdrMaxCll > 1000) ? "mdl4000" : "mdl1000";
		}

		source = "default";
		return "mdl1000";
	}

	QString detectHdrBucketFamily(const QJsonObject& state, QString& source)
	{
		const QJsonObject hdr = state["hdr"].toObject();

		const int hdrMaxDisplayLum = jsonPositiveInt(hdr["max_display_lum_nits"], 0);
		if (hdrMaxDisplayLum > 0)
		{
			source = "hdr.max_display_lum_nits";
			return (hdrMaxDisplayLum > 1000) ? "mdl4000" : "mdl1000";
		}

		const int hdrMaxCll = jsonPositiveInt(hdr["max_cll_nits"], 0);
		if (hdrMaxCll > 0)
		{
			source = "hdr.max_cll_nits:fallback";
			return (hdrMaxCll > 1000) ? "mdl4000" : "mdl1000";
		}

		source = "default";
		return "mdl1000";
	}

	LutBucketMetric extractLutBucketMetric(const QJsonObject& state, const QJsonObject& config, const QJsonObject& previousDecision = QJsonObject())
	{
		auto readMetric = [](const QJsonObject& object, const QString& key, const QString& keySource, const QString& unit) {
			LutBucketMetric metric;
			metric.value = jsonPositiveDouble(object[key], 0.0);
			if (metric.value > 0.0)
			{
				metric.source = keySource;
				metric.unit = unit;
			}
			return metric;
		};

		const QString normalizedInput = resolveAutomaticLutSwitchingInput(state);
		const QString category = detectTransferAutomationCategory(state);
		if (category == "dolby-vision" && normalizedInput == "ugoos")
		{
			const QJsonObject kodiDv = state["kodiDv"].toObject();
			const QString selectedMetric = normalizeDolbyVisionBucketMetric(config["kodiDolbyVisionBucketMetric"].toString());

			if (selectedMetric == "sceneAverageIndex")
			{
				LutBucketMetric metric;
				const qint64 currentMetricTimestampMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
				const double configuredHighlightWeight = jsonBoundedDouble(config["kodiDolbyVisionSceneIndexHighlightWeight"], kDefaultDolbyVisionSceneIndexHighlightWeight, 0.0, 1.0);
				const double l1AvgNits = jsonPositiveDouble(kodiDv["l1AvgNits"], 0.0);
				const double l1MaxNits = jsonPositiveDouble(kodiDv["l1MaxNits"], 0.0);
				const bool hasAvg = l1AvgNits > 0.0;
				const bool hasMax = l1MaxNits > 0.0;

				double baseAvg = l1AvgNits;
				double baseMax = l1MaxNits;
				if (!hasAvg && hasMax)
					baseAvg = l1MaxNits;
				if (!hasMax && hasAvg)
					baseMax = l1AvgNits;

				if (baseAvg > 0.0 || baseMax > 0.0)
				{
					if (baseAvg <= 0.0)
						baseAvg = baseMax;
					if (baseMax <= 0.0)
						baseMax = baseAvg;

					QString familySource;
					const QString family = detectDolbyVisionBucketFamily(state, familySource);
					const QList<int> familyThresholds = (family == "mdl4000")
						? parsePositiveThresholdList(config["kodiDolbyVisionMdl4000Thresholds"], QList<int>{ 69, 200 })
						: parsePositiveThresholdList(config["kodiDolbyVisionMdl1000Thresholds"], QList<int>{ 69 });
					const double sourceMaxNits = extractKodiDolbyVisionSourceMaxNits(kodiDv);

					const double rawMaxCllNits = extractKodiDolbyVisionConstraintNits(
						kodiDv,
						QStringLiteral("l6MaxCllNits"),
						QStringList{ QStringLiteral("Player.Process(video.dovi.l6.max.cll)") });
					const double maxCllNits = (sourceMaxNits > 0.0 && rawMaxCllNits > sourceMaxNits)
						? sourceMaxNits
						: rawMaxCllNits;
					const double maxFallNits = extractKodiDolbyVisionConstraintNits(
						kodiDv,
						QStringLiteral("l6MaxFallNits"),
						QStringList{ QStringLiteral("Player.Process(video.dovi.l6.max.fall)") });

					const double highlightDelta = qMax(0.0, baseMax - baseAvg);
					const double highlightRatioDenominator = qMax(kDolbyVisionSceneIndexHighlightGateAverageFloorNits, baseAvg);
					const double highlightRatio = highlightDelta / highlightRatioDenominator;
					double highlightWeightScale = 1.0;
					if (highlightRatio > kDolbyVisionSceneIndexHighlightGateThreshold)
					{
						const double highlightRatioExcess = highlightRatio - kDolbyVisionSceneIndexHighlightGateThreshold;
						highlightWeightScale = qBound(
							kDolbyVisionSceneIndexHighlightGateMinScale,
							1.0 / (1.0 + (highlightRatioExcess * kDolbyVisionSceneIndexHighlightGateRolloff)),
							1.0);
					}
					double darkSceneBoostCapScale = kDolbyVisionSceneIndexDarkSceneBoostMaxScale;
					double metadataSceneCapNits = 0.0;
					const bool hasMaxCll = maxCllNits > 0.0;
					const bool hasMaxFall = maxFallNits > 0.0;
					if (hasMaxCll || hasMaxFall)
					{
						if (hasMaxCll && hasMaxFall)
						{
							const double boundedMaxFallNits = qMin(maxFallNits, maxCllNits);
							metadataSceneCapNits = boundedMaxFallNits + (configuredHighlightWeight * qMax(0.0, maxCllNits - boundedMaxFallNits));
						}
						else if (hasMaxFall)
						{
							metadataSceneCapNits = maxFallNits;
						}
						else
						{
							metadataSceneCapNits = baseAvg + (kDolbyVisionSceneIndexMetadataFallbackWeight * qMax(0.0, maxCllNits - baseAvg));
						}

						metadataSceneCapNits = roundUpToThreshold(familyThresholds, metadataSceneCapNits);

						const double metadataHighlightWeightCap = (highlightDelta > 0.0)
							? qMax(0.0, (metadataSceneCapNits - baseAvg) / highlightDelta)
							: 1.0;
						const double metadataBoostCapScale = (configuredHighlightWeight > 0.0 && highlightWeightScale > 0.0)
							? (metadataHighlightWeightCap / (configuredHighlightWeight * highlightWeightScale))
							: kDolbyVisionSceneIndexDarkSceneBoostMaxScale;
						darkSceneBoostCapScale = qMax(1.0, metadataBoostCapScale);
					}

					const double darkSceneBoostReferenceNits = (family == QStringLiteral("mdl4000"))
						? kDolbyVisionSceneIndexDarkSceneBoostReferenceNitsMdl4000
						: kDolbyVisionSceneIndexDarkSceneBoostReferenceNitsMdl1000;
					double darkSceneBoostBaseScale = darkSceneBoostReferenceNits / qMax(1.0, baseAvg);
					if (family == QStringLiteral("mdl4000") && metadataSceneCapNits > 0.0)
						darkSceneBoostBaseScale = qMax(darkSceneBoostBaseScale, darkSceneBoostCapScale * kDolbyVisionSceneIndexMdl4000MetadataBoostFloorRatio);

					const double darkSceneBoostScale = qBound(
						1.0,
						darkSceneBoostBaseScale,
						darkSceneBoostCapScale);
					const double unclampedHighlightWeight = configuredHighlightWeight * highlightWeightScale * darkSceneBoostScale;
					const double effectiveHighlightWeight = qBound(0.0, unclampedHighlightWeight, 1.0);
					double sceneIndex = baseAvg + (effectiveHighlightWeight * highlightDelta);

					QJsonObject details;
					details["mode"] = QStringLiteral("sceneAverageIndex");
					details["metricTimestampMs"] = static_cast<double>(currentMetricTimestampMs);
					details["l1AvgNits"] = baseAvg;
					details["l1MaxNits"] = baseMax;
					details["family"] = family;
					details["configuredHighlightWeight"] = configuredHighlightWeight;
					details["highlightWeight"] = effectiveHighlightWeight;
					details["unclampedHighlightWeight"] = unclampedHighlightWeight;
					details["highlightDeltaNits"] = highlightDelta;
					details["highlightRatio"] = highlightRatio;
					details["highlightRatioDenominatorNits"] = highlightRatioDenominator;
					details["highlightWeightScale"] = highlightWeightScale;
					details["darkSceneBoostScale"] = darkSceneBoostScale;
					details["darkSceneBoostBaseScale"] = darkSceneBoostBaseScale;
					details["darkSceneBoostCapScale"] = darkSceneBoostCapScale;
					details["mdl4000MetadataBoostFloorRatio"] = (family == QStringLiteral("mdl4000")) ? kDolbyVisionSceneIndexMdl4000MetadataBoostFloorRatio : 0.0;
					details["highlightGatingActive"] = (highlightWeightScale < 0.999);
					details["highlightGateAverageFloorNits"] = kDolbyVisionSceneIndexHighlightGateAverageFloorNits;
					details["highlightGateThreshold"] = kDolbyVisionSceneIndexHighlightGateThreshold;
					details["darkSceneBoostReferenceNits"] = darkSceneBoostReferenceNits;
					details["metadataDrivenBoostCap"] = (metadataSceneCapNits > 0.0);
					if (sourceMaxNits > 0.0)
						details["sourceMaxNits"] = sourceMaxNits;
					if (rawMaxCllNits > 0.0)
						details["rawL6MaxCllNits"] = rawMaxCllNits;
					if (metadataSceneCapNits > 0.0)
						details["metadataSceneCapNits"] = metadataSceneCapNits;
					details["rawSceneIndexNits"] = sceneIndex;

					if (maxCllNits > 0.0)
						details["l6MaxCllNits"] = maxCllNits;


					if (maxFallNits > 0.0)
					{
						details["l6MaxFallNits"] = maxFallNits;
					}

					const double previousFiltered = jsonPositiveDouble(previousDecision["filteredMetricValue"], 0.0);
					details["preFilterSceneIndexNits"] = sceneIndex;
					if (previousFiltered > 0.0 &&
						previousDecision["category"].toString() == QStringLiteral("dolby-vision") &&
						previousDecision["input"].toString() == normalizedInput)
					{
						const qint64 previousTimestampMs = static_cast<qint64>(jsonPositiveDouble(previousDecision["metricTimestampMs"], 0.0));
						const double elapsedMs = (previousTimestampMs > 0 && currentMetricTimestampMs > previousTimestampMs)
							? static_cast<double>(currentMetricTimestampMs - previousTimestampMs)
							: 0.0;
						const double sceneDropRatio = (previousFiltered > 0.0)
							? (sceneIndex / previousFiltered)
							: 1.0;
						const bool useFastDecay = sceneIndex < previousFiltered && sceneDropRatio <= kDolbyVisionSceneIndexFastDecayTriggerRatio;
						const double timeConstantMs = (sceneIndex > previousFiltered)
							? kDolbyVisionSceneIndexAttackTimeMs
							: (useFastDecay ? kDolbyVisionSceneIndexFastDecayTimeMs : kDolbyVisionSceneIndexDecayTimeMs);
						const double alpha = (elapsedMs > 0.0 && timeConstantMs > 0.0)
							? qBound(0.0, 1.0 - std::exp(-elapsedMs / timeConstantMs), 1.0)
							: (sceneIndex > previousFiltered ? 0.25 : (useFastDecay ? 0.35 : 0.20));
						sceneIndex = previousFiltered + ((sceneIndex - previousFiltered) * alpha);
						details["previousFilteredMetricValue"] = previousFiltered;
						details["previousMetricTimestampMs"] = static_cast<double>(previousTimestampMs);
						details["elapsedMetricMs"] = elapsedMs;
						details["sceneDropRatio"] = sceneDropRatio;
						details["fastDecayActive"] = useFastDecay;
						details["filterTimeConstantMs"] = timeConstantMs;
						details["filterAlpha"] = alpha;
					}

					const double filteredSceneIndexBeforeClamp = sceneIndex;
					sceneIndex = qMin(sceneIndex, baseMax);
					details["filteredSceneIndexBeforeClampNits"] = filteredSceneIndexBeforeClamp;
					details["filteredSceneIndexMaxCapNits"] = baseMax;
					details["filteredSceneIndexClampedToL1Max"] = (filteredSceneIndexBeforeClamp > baseMax + 0.001);

					metric.value = sceneIndex;
					metric.source = QStringLiteral("kodiDv.sceneAverageIndex");
					metric.unit = QStringLiteral("nits");
					metric.details = details;
					return metric;
				}
			}

			const QString primaryKey = (selectedMetric == "l1MaxNits") ? QStringLiteral("l1MaxNits") : QStringLiteral("l1AvgNits");
			const QString secondaryKey = (primaryKey == "l1MaxNits") ? QStringLiteral("l1AvgNits") : QStringLiteral("l1MaxNits");

			LutBucketMetric metric = readMetric(kodiDv, primaryKey, QStringLiteral("kodiDv.%1:selected").arg(primaryKey), "nits");
			if (metric.isValid())
			{
				metric.details["mode"] = selectedMetric;
				return metric;
			}

			metric = readMetric(kodiDv, secondaryKey, QStringLiteral("kodiDv.%1:fallback").arg(secondaryKey), "nits");
			if (metric.isValid())
			{
				metric.details["mode"] = selectedMetric;
				return metric;
			}

			metric = readMetric(kodiDv, "fllAvgCdM2", "kodiDv.fllAvgCdM2:fallback", "cd/m^2");
			if (metric.isValid())
			{
				metric.details["mode"] = selectedMetric;
				return metric;
			}

			metric = readMetric(kodiDv, "fllUpperCdM2", "kodiDv.fllUpperCdM2:fallback", "cd/m^2");
			if (metric.isValid())
			{
				metric.details["mode"] = selectedMetric;
				return metric;
			}
		}

		if (category == "hdr")
		{
			const QJsonObject hdr = state["hdr"].toObject();
			const double maxFallNits = jsonPositiveDouble(hdr["max_fall_nits"], 0.0);
			const double maxCllNits = jsonPositiveDouble(hdr["max_cll_nits"], 0.0);
			const double maxDisplayLumNits = jsonPositiveDouble(hdr["max_display_lum_nits"], 0.0);
			const bool hasFall = maxFallNits > 0.0;
			const bool hasCll = maxCllNits > 0.0;

			if (hasFall || hasCll)
			{
				double baseAvg = hasFall ? maxFallNits : maxCllNits;
				double baseMax = hasCll ? maxCllNits : maxFallNits;
				if (baseAvg > baseMax)
					baseAvg = baseMax;

				LutBucketMetric metric;
				const double configuredHighlightWeight = jsonBoundedDouble(config["kodiDolbyVisionSceneIndexHighlightWeight"], kDefaultDolbyVisionSceneIndexHighlightWeight, 0.0, 1.0);

				QString familySource;
				const QString family = detectHdrBucketFamily(state, familySource);
				const QList<int> familyThresholds = (family == "mdl4000")
					? parsePositiveThresholdList(config["kodiHdrMdl4000Thresholds"], QList<int>{ 250 })
					: parsePositiveThresholdList(config["kodiHdrMdl1000Thresholds"], QList<int>{ 250 });

				const double sourceMaxNits = maxDisplayLumNits;

				const double highlightDelta = qMax(0.0, baseMax - baseAvg);
				const double highlightRatioDenominator = qMax(kDolbyVisionSceneIndexHighlightGateAverageFloorNits, baseAvg);
				const double highlightRatio = highlightDelta / highlightRatioDenominator;
				double highlightWeightScale = 1.0;
				if (highlightRatio > kDolbyVisionSceneIndexHighlightGateThreshold)
				{
					const double highlightRatioExcess = highlightRatio - kDolbyVisionSceneIndexHighlightGateThreshold;
					highlightWeightScale = qBound(
						kDolbyVisionSceneIndexHighlightGateMinScale,
						1.0 / (1.0 + (highlightRatioExcess * kDolbyVisionSceneIndexHighlightGateRolloff)),
						1.0);
				}
				double darkSceneBoostCapScale = kDolbyVisionSceneIndexDarkSceneBoostMaxScale;
				double metadataSceneCapNits = 0.0;
				if (hasCll || hasFall)
				{
					if (hasCll && hasFall)
					{
						const double boundedFall = qMin(maxFallNits, maxCllNits);
						metadataSceneCapNits = boundedFall + (configuredHighlightWeight * qMax(0.0, maxCllNits - boundedFall));
					}
					else if (hasFall)
					{
						metadataSceneCapNits = maxFallNits;
					}
					else
					{
						metadataSceneCapNits = baseAvg + (kDolbyVisionSceneIndexMetadataFallbackWeight * qMax(0.0, maxCllNits - baseAvg));
					}

					metadataSceneCapNits = roundUpToThreshold(familyThresholds, metadataSceneCapNits);

					const double metadataHighlightWeightCap = (highlightDelta > 0.0)
						? qMax(0.0, (metadataSceneCapNits - baseAvg) / highlightDelta)
						: 1.0;
					const double metadataBoostCapScale = (configuredHighlightWeight > 0.0 && highlightWeightScale > 0.0)
						? (metadataHighlightWeightCap / (configuredHighlightWeight * highlightWeightScale))
						: kDolbyVisionSceneIndexDarkSceneBoostMaxScale;
					darkSceneBoostCapScale = qMax(1.0, metadataBoostCapScale);
				}

				const double darkSceneBoostReferenceNits = (family == QStringLiteral("mdl4000"))
					? kDolbyVisionSceneIndexDarkSceneBoostReferenceNitsMdl4000
					: kDolbyVisionSceneIndexDarkSceneBoostReferenceNitsMdl1000;
				double darkSceneBoostBaseScale = darkSceneBoostReferenceNits / qMax(1.0, baseAvg);
				if (family == QStringLiteral("mdl4000") && metadataSceneCapNits > 0.0)
					darkSceneBoostBaseScale = qMax(darkSceneBoostBaseScale, darkSceneBoostCapScale * kDolbyVisionSceneIndexMdl4000MetadataBoostFloorRatio);

				const double darkSceneBoostScale = qBound(
					1.0,
					darkSceneBoostBaseScale,
					darkSceneBoostCapScale);
				const double unclampedHighlightWeight = configuredHighlightWeight * highlightWeightScale * darkSceneBoostScale;
				const double effectiveHighlightWeight = qBound(0.0, unclampedHighlightWeight, 1.0);
				double sceneIndex = baseAvg + (effectiveHighlightWeight * highlightDelta);

				QJsonObject details;
				details["mode"] = QStringLiteral("sceneAverageIndex");
				details["maxFallNits"] = maxFallNits;
				details["maxCllNits"] = maxCllNits;
				if (sourceMaxNits > 0.0)
					details["sourceMaxNits"] = sourceMaxNits;
				details["baseAvgNits"] = baseAvg;
				details["baseMaxNits"] = baseMax;
				details["family"] = family;
				details["configuredHighlightWeight"] = configuredHighlightWeight;
				details["highlightWeight"] = effectiveHighlightWeight;
				details["unclampedHighlightWeight"] = unclampedHighlightWeight;
				details["highlightDeltaNits"] = highlightDelta;
				details["highlightRatio"] = highlightRatio;
				details["highlightRatioDenominatorNits"] = highlightRatioDenominator;
				details["highlightWeightScale"] = highlightWeightScale;
				details["darkSceneBoostScale"] = darkSceneBoostScale;
				details["darkSceneBoostBaseScale"] = darkSceneBoostBaseScale;
				details["darkSceneBoostCapScale"] = darkSceneBoostCapScale;
				details["highlightGatingActive"] = (highlightWeightScale < 0.999);
				if (metadataSceneCapNits > 0.0)
					details["metadataSceneCapNits"] = metadataSceneCapNits;
				details["rawSceneIndexNits"] = sceneIndex;

				sceneIndex = qMin(sceneIndex, baseMax);
				details["filteredSceneIndexNits"] = sceneIndex;

				metric.value = sceneIndex;
				metric.source = QStringLiteral("hdr.sceneAverageIndex");
				metric.unit = QStringLiteral("nits");
				metric.details = details;
				return metric;
			}
		}

		const QJsonObject hdr = state["hdr"].toObject();
		LutBucketMetric metric = readMetric(hdr, "max_cll_nits", "hdr.max_cll_nits", "nits");
		if (metric.isValid())
			return metric;

		metric = readMetric(hdr, "max_fall_nits", "hdr.max_fall_nits", "nits");
		if (metric.isValid())
			return metric;

		metric = readMetric(hdr, "max_display_lum_nits", "hdr.max_display_lum_nits:mdl_only_fallback", "nits");
		if (metric.isValid())
		{
			const int fallbackCap = jsonBoundedInt(config["kodiHdrMdlOnlyFallbackNits"], 400, 1, 10000);
			metric.details["mdlOnlyFallback"] = true;
			metric.details["mdlOnlyFallbackCapNits"] = fallbackCap;
			metric.details["rawMdlNits"] = metric.value;
			if (metric.value > fallbackCap)
				metric.value = fallbackCap;
			return metric;
		}

		return metric;
	}

	int extractTransferAutomationNits(const QJsonObject& state, QString& source)
	{
		auto extractFromObject = [&source](const QJsonObject& object, const QString& key, const QString& keySource) {
			const int value = jsonPositiveInt(object[key], 0);
			if (value > 0)
				source = keySource;
			return value;
		};

		const QString normalizedInput = resolveAutomaticLutSwitchingInput(state);
		const QString category = detectTransferAutomationCategory(state);
		if (category == "dolby-vision" && normalizedInput == "ugoos")
		{
			const QJsonObject kodiDv = state["kodiDv"].toObject();
			int nits = extractFromObject(kodiDv, "sourceMaxNits", "kodiDv.sourceMaxNits");
			if (nits > 0)
				return nits;

			nits = extractFromObject(kodiDv, "rpuMdlNits", "kodiDv.rpuMdlNits");
			if (nits > 0)
				return nits;

			nits = extractFromObject(kodiDv, "l6MaxLumNits", "kodiDv.l6MaxLumNits");
			if (nits > 0)
				return nits;

			nits = extractFromObject(kodiDv, "l6MaxCllNits", "kodiDv.l6MaxCllNits");
			if (nits > 0)
				return nits;
		}

		const QJsonObject hdr = state["hdr"].toObject();
		int nits = extractFromObject(hdr, "max_cll_nits", "hdr.max_cll_nits");
		if (nits > 0)
			return nits;

		nits = extractFromObject(hdr, "max_display_lum_nits", "hdr.max_display_lum_nits");
		if (nits > 0)
			return nits;

		nits = extractFromObject(hdr, "max_fall_nits", "hdr.max_fall_nits");
		if (nits > 0)
			return nits;

		return 0;
	}

	QJsonObject evaluateLutBucketDecision(const QString& rootPath, const QJsonObject& currentConfig, const QJsonObject& state, GrabberWrapper* grabberWrapper, QStringList& candidates, QStringList* interpolationCandidates = nullptr, int* interpolationBlend = nullptr)
	{
		const QJsonObject config = buildLutSwitchingConfig(rootPath, currentConfig);
		QJsonObject decision;
		QStringList candidatePlan;
		if (interpolationCandidates != nullptr)
			interpolationCandidates->clear();
		if (interpolationBlend != nullptr)
			*interpolationBlend = 0;
		decision["enabled"] = config["enabled"].toBool(false);
		decision["applied"] = false;
		decision["canApply"] = (grabberWrapper != nullptr);

		if (!decision["enabled"].toBool())
		{
			decision["reason"] = "LUT switching is disabled";
			return decision;
		}

		const QString mode = detectAutomaticLutMode(state);
		decision["mode"] = mode;
		if (mode.isEmpty())
		{
			decision["reason"] = "Unable to classify the reported stream for LUT bucket switching";
			return decision;
		}

		const QString normalizedInput = resolveAutomaticLutSwitchingInput(state);
		decision["input"] = normalizedInput;
		const QString directoryPath = config["directory"].toString();

		auto appendSharedFallback = [&]() {
			auto appendModeFallback = [&](const QString& fallbackMode, const QString& sharedKey, const QString& sharedDefault) {
				if (!normalizedInput.isEmpty())
				{
					const QString inputSpecific = lutSwitchingPerInputFileName(config, normalizedInput, fallbackMode);
					candidatePlan.append(inputSpecific.isEmpty()
						? QString("Per-input %1 fallback (%2) -> not configured").arg(fallbackMode.toUpper(), QString("%1%2LutFile").arg(normalizedInput, fallbackMode.left(1).toUpper() + fallbackMode.mid(1)))
						: QString("Per-input %1 fallback (%2) -> %3").arg(fallbackMode.toUpper(), QString("%1%2LutFile").arg(normalizedInput, fallbackMode.left(1).toUpper() + fallbackMode.mid(1)), lutSwitchingAbsoluteFile(directoryPath, inputSpecific)));
					if (!inputSpecific.isEmpty())
						appendUniqueCandidate(candidates, lutSwitchingAbsoluteFile(directoryPath, inputSpecific));
				}

				const QString sharedFile = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, sharedKey, sharedDefault));
				candidatePlan.append(QString("Shared %1 fallback (%2) -> %3").arg(fallbackMode.toUpper(), sharedKey, sharedFile));
				appendUniqueCandidate(candidates, sharedFile);
			};

			if (mode == "sdr")
			{
				if (config["sdrUsesHdrFile"].toBool(false))
					appendModeFallback("hdr", "hdrLutFile", "lut_lin_tables_hdr.3d");
				else
					appendModeFallback("sdr", "sdrLutFile", "lut_lin_tables_sdr.3d");
			}
			else if (mode == "hdr")
				appendModeFallback("hdr", "hdrLutFile", "lut_lin_tables_hdr.3d");
			else if (mode == "lldv")
			{
				appendModeFallback("lldv", "lldvLutFile", "lut_lin_tables_lldv.3d");
				appendModeFallback("hdr", "hdrLutFile", "lut_lin_tables_hdr.3d");
			}

			const QString activeFallback = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "activeLutFile", "lut_lin_tables.3d"));
			candidatePlan.append(QString("Active LUT fallback -> %1").arg(activeFallback));
			appendUniqueCandidate(candidates, activeFallback);
		};

		if (mode == "hdr")
		{
			const QJsonObject previousDecision = state["lutBucketDecision"].toObject();
			QString familySource;
			const QString family = detectHdrBucketFamily(state, familySource);
			const LutBucketMetric metric = extractLutBucketMetric(state, config, previousDecision);
			decision["category"] = "hdr";
			decision["family"] = family;
			if (!familySource.isEmpty())
				decision["familySource"] = familySource;
			if (metric.isValid())
			{
				decision["metricValue"] = metric.value;
				decision["filteredMetricValue"] = metric.value;
				decision["metricSource"] = metric.source;
				decision["metricUnit"] = metric.unit;
				if (!metric.details.isEmpty())
					decision["metricDetails"] = metric.details;
			}

			const QList<int> thresholds = (family == "mdl4000")
				? parsePositiveThresholdList(config["kodiHdrMdl4000Thresholds"], QList<int>{ 250 })
				: parsePositiveThresholdList(config["kodiHdrMdl1000Thresholds"], QList<int>{ 250 });
			decision["bucketCount"] = thresholds.size() + 1;
			const int bucketIndex = selectThresholdBucketIndexWithHysteresis(thresholds, metric.value, previousDecision, QStringLiteral("hdr"), family, normalizedInput);
			decision["bucketIndex"] = bucketIndex;
			QJsonArray thresholdArray;
			for (int value : thresholds)
				thresholdArray.append(value);
			decision["thresholds"] = thresholdArray;

			if (!normalizedInput.isEmpty())
			{
				const QString arrayKey = QString("%1%2").arg(normalizedInput, family == "mdl4000" ? "HdrMdl4000LutFiles" : "HdrMdl1000LutFiles");
				const QJsonArray bucketFiles = readNormalizedStringArray(config, arrayKey);
				const QString bucketFile = jsonArrayValueAt(bucketFiles, bucketIndex);
				candidatePlan.append(bucketFile.isEmpty()
					? QString("HDR %1 bucket %2/%3 (%4[%5]) -> not configured; falling back").arg(family.toUpper()).arg(bucketIndex + 1).arg(thresholds.size() + 1).arg(arrayKey).arg(bucketIndex)
					: QString("HDR %1 bucket %2/%3 (%4[%5]) -> %6").arg(family.toUpper()).arg(bucketIndex + 1).arg(thresholds.size() + 1).arg(arrayKey).arg(bucketIndex).arg(lutSwitchingAbsoluteFile(directoryPath, bucketFile)));
				if (!bucketFile.isEmpty())
				{
					appendUniqueCandidate(candidates, lutSwitchingAbsoluteFile(directoryPath, bucketFile));
					decision["bucketFile"] = bucketFile;
				}

				const AutomaticLutInterpolationPlan interpolationPlan = buildAdjacentBucketInterpolationPlan(thresholds, bucketFiles, directoryPath, metric.value);
				if (interpolationPlan.isActive())
				{
					decision["interpolationActive"] = true;
					decision["interpolationLowerBucketIndex"] = interpolationPlan.lowerBucketIndex;
					decision["interpolationUpperBucketIndex"] = interpolationPlan.upperBucketIndex;
					decision["interpolationLowerBound"] = interpolationPlan.lowerBound;
					decision["interpolationUpperBound"] = interpolationPlan.upperBound;
					decision["interpolationRatio"] = interpolationPlan.ratio;
					decision["interpolationBlend"] = interpolationPlan.blend;
					decision["interpolationLowerFile"] = interpolationPlan.lowerFile;
					decision["interpolationUpperFile"] = interpolationPlan.upperFile;
					decision["interpolationMode"] = "continuousAdjacent";
					candidates.removeAll(interpolationPlan.lowerFile);
					candidates.prepend(interpolationPlan.lowerFile);
					candidatePlan.append(QString("Continuously interpolating adjacent HDR %1 buckets %2/%3 and %4/%5 across %6..%7 nits -> %8 + %9 (blend %10)")
						.arg(family.toUpper())
						.arg(interpolationPlan.lowerBucketIndex + 1)
						.arg(thresholds.size() + 1)
						.arg(interpolationPlan.upperBucketIndex + 1)
						.arg(thresholds.size() + 1)
						.arg(interpolationPlan.lowerBound, 0, 'f', 3)
						.arg(interpolationPlan.upperBound, 0, 'f', 3)
						.arg(interpolationPlan.lowerFile)
						.arg(interpolationPlan.upperFile)
						.arg(interpolationPlan.ratio, 0, 'f', 3));
					if (interpolationCandidates != nullptr)
						*interpolationCandidates = interpolationPlan.secondaryCandidates;
					if (interpolationBlend != nullptr)
						*interpolationBlend = interpolationPlan.blend;
				}

				if (family == "mdl4000")
				{
					const QList<int> mdl1000Thresholds = parsePositiveThresholdList(config["kodiHdrMdl1000Thresholds"], QList<int>{ 250 });
					const int mdl1000BucketIndex = selectThresholdBucketIndex(mdl1000Thresholds, metric.value);
					const QJsonArray mdl1000Files = readNormalizedStringArray(config, QString("%1HdrMdl1000LutFiles").arg(normalizedInput));
					const QString mdl1000File = jsonArrayValueAt(mdl1000Files, mdl1000BucketIndex);
					candidatePlan.append(mdl1000File.isEmpty()
						? QString("HDR MDL1000 fallback bucket %1/%2 (%3HdrMdl1000LutFiles[%4]) -> not configured; falling back").arg(mdl1000BucketIndex + 1).arg(mdl1000Thresholds.size() + 1).arg(normalizedInput).arg(mdl1000BucketIndex)
						: QString("HDR MDL1000 fallback bucket %1/%2 (%3HdrMdl1000LutFiles[%4]) -> %5").arg(mdl1000BucketIndex + 1).arg(mdl1000Thresholds.size() + 1).arg(normalizedInput).arg(mdl1000BucketIndex).arg(lutSwitchingAbsoluteFile(directoryPath, mdl1000File)));
					if (!mdl1000File.isEmpty())
					{
						appendUniqueCandidate(candidates, lutSwitchingAbsoluteFile(directoryPath, mdl1000File));
						decision["fallbackBucketIndex"] = mdl1000BucketIndex;
						decision["fallbackFamily"] = "mdl1000";
						decision["fallbackReason"] = "HDR MDL4000 buckets fall back to the matching MDL1000 bucket before the shared HDR LUT";
					}
				}
			}

			appendSharedFallback();
		}
		else if (mode == "lldv")
		{
			const QJsonObject previousDecision = state["lutBucketDecision"].toObject();
			QString familySource;
			const QString family = detectDolbyVisionBucketFamily(state, familySource);
			decision["category"] = "dolby-vision";
			decision["family"] = family;
			if (!familySource.isEmpty())
				decision["familySource"] = familySource;

			const LutBucketMetric metric = extractLutBucketMetric(state, config, previousDecision);
			if (metric.isValid())
			{
				decision["metricValue"] = metric.value;
				decision["metricSource"] = metric.source;
				decision["metricUnit"] = metric.unit;
				if (!metric.details.isEmpty())
					decision["metricDetails"] = metric.details;
				if (metric.details.contains("mode"))
					decision["metricMode"] = metric.details["mode"].toString();
				if (metric.details.contains("preFilterSceneIndexNits"))
					decision["rawMetricValue"] = metric.details["preFilterSceneIndexNits"].toDouble();
				if (metric.details.contains("previousFilteredMetricValue"))
					decision["previousFilteredMetricValue"] = metric.details["previousFilteredMetricValue"].toDouble();
				decision["filteredMetricValue"] = metric.value;
			}

			const bool supportsMetadataBuckets = (normalizedInput == "ugoos");
			decision["supportsMetadataBuckets"] = supportsMetadataBuckets;
			if (supportsMetadataBuckets)
			{
				const QList<int> familyThresholds = (family == "mdl4000")
					? parsePositiveThresholdList(config["kodiDolbyVisionMdl4000Thresholds"], QList<int>{ 69, 200 })
					: parsePositiveThresholdList(config["kodiDolbyVisionMdl1000Thresholds"], QList<int>{ 69 });
				decision["bucketCount"] = familyThresholds.size() + 1;
				const int bucketIndex = selectThresholdBucketIndexWithHysteresis(familyThresholds, metric.value, previousDecision, QStringLiteral("dolby-vision"), family, normalizedInput);
				decision["bucketIndex"] = bucketIndex;
				QJsonArray thresholdArray;
				for (int value : familyThresholds)
					thresholdArray.append(value);
				decision["thresholds"] = thresholdArray;

				const QString arrayKey = QString("%1%2").arg(normalizedInput, family == "mdl4000" ? "DolbyVisionMdl4000LutFiles" : "DolbyVisionMdl1000LutFiles");
				const QJsonArray bucketFiles = readNormalizedStringArray(config, arrayKey);
				const QString bucketFile = jsonArrayValueAt(bucketFiles, bucketIndex);
				candidatePlan.append(bucketFile.isEmpty()
					? QString("%1 bucket %2/%3 (%4[%5]) -> not configured; falling back").arg(family.toUpper()).arg(bucketIndex + 1).arg(familyThresholds.size() + 1).arg(arrayKey).arg(bucketIndex)
					: QString("%1 bucket %2/%3 (%4[%5]) -> %6").arg(family.toUpper()).arg(bucketIndex + 1).arg(familyThresholds.size() + 1).arg(arrayKey).arg(bucketIndex).arg(lutSwitchingAbsoluteFile(directoryPath, bucketFile)));
				if (!bucketFile.isEmpty())
				{
					appendUniqueCandidate(candidates, lutSwitchingAbsoluteFile(directoryPath, bucketFile));
					decision["bucketFile"] = bucketFile;
				}

				const AutomaticLutInterpolationPlan interpolationPlan = buildAdjacentBucketInterpolationPlan(familyThresholds, bucketFiles, directoryPath, metric.value);
				if (interpolationPlan.isActive())
				{
					decision["interpolationActive"] = true;
					decision["interpolationLowerBucketIndex"] = interpolationPlan.lowerBucketIndex;
					decision["interpolationUpperBucketIndex"] = interpolationPlan.upperBucketIndex;
					decision["interpolationLowerBound"] = interpolationPlan.lowerBound;
					decision["interpolationUpperBound"] = interpolationPlan.upperBound;
					decision["interpolationRatio"] = interpolationPlan.ratio;
					decision["interpolationBlend"] = interpolationPlan.blend;
					decision["interpolationLowerFile"] = interpolationPlan.lowerFile;
					decision["interpolationUpperFile"] = interpolationPlan.upperFile;
					decision["interpolationMode"] = "continuousAdjacent";
					candidates.removeAll(interpolationPlan.lowerFile);
					candidates.prepend(interpolationPlan.lowerFile);
					candidatePlan.append(QString("Continuously interpolating adjacent DV buckets %1/%2 and %3/%4 across %5..%6 nits -> %7 + %8 (blend %9)")
						.arg(interpolationPlan.lowerBucketIndex + 1)
						.arg(familyThresholds.size() + 1)
						.arg(interpolationPlan.upperBucketIndex + 1)
						.arg(familyThresholds.size() + 1)
						.arg(interpolationPlan.lowerBound, 0, 'f', 3)
						.arg(interpolationPlan.upperBound, 0, 'f', 3)
						.arg(interpolationPlan.lowerFile)
						.arg(interpolationPlan.upperFile)
						.arg(interpolationPlan.ratio, 0, 'f', 3));
					if (interpolationCandidates != nullptr)
						*interpolationCandidates = interpolationPlan.secondaryCandidates;
					if (interpolationBlend != nullptr)
						*interpolationBlend = interpolationPlan.blend;
				}

				if (family == "mdl4000")
				{
					const QList<int> mdl1000Thresholds = parsePositiveThresholdList(config["kodiDolbyVisionMdl1000Thresholds"], QList<int>{ 69 });
					const int mdl1000BucketIndex = selectThresholdBucketIndex(mdl1000Thresholds, metric.value);
					const QJsonArray mdl1000Files = readNormalizedStringArray(config, QString("%1DolbyVisionMdl1000LutFiles").arg(normalizedInput));
					const QString mdl1000File = jsonArrayValueAt(mdl1000Files, mdl1000BucketIndex);
					candidatePlan.append(mdl1000File.isEmpty()
						? QString("MDL1000 fallback bucket %1/%2 (%3DolbyVisionMdl1000LutFiles[%4]) -> not configured; falling back").arg(mdl1000BucketIndex + 1).arg(mdl1000Thresholds.size() + 1).arg(normalizedInput).arg(mdl1000BucketIndex)
						: QString("MDL1000 fallback bucket %1/%2 (%3DolbyVisionMdl1000LutFiles[%4]) -> %5").arg(mdl1000BucketIndex + 1).arg(mdl1000Thresholds.size() + 1).arg(normalizedInput).arg(mdl1000BucketIndex).arg(lutSwitchingAbsoluteFile(directoryPath, mdl1000File)));
					if (!mdl1000File.isEmpty())
					{
						appendUniqueCandidate(candidates, lutSwitchingAbsoluteFile(directoryPath, mdl1000File));
						decision["fallbackBucketIndex"] = mdl1000BucketIndex;
						decision["fallbackFamily"] = "mdl1000";
						decision["fallbackReason"] = "MDL4000 buckets fall back to the matching MDL1000 bucket before the shared LLDV LUT";
					}
				}
			}
			else
			{
				decision["reason"] = "Dolby Vision metadata buckets are currently only configured for the Ugoos path";
			}

			if (!normalizedInput.isEmpty())
			{
				const QString inputSpecificLldv = lutSwitchingPerInputFileName(config, normalizedInput, "lldv");
				candidatePlan.append(inputSpecificLldv.isEmpty()
					? QString("Per-input LLDV fallback (%1LldvLutFile) -> not configured").arg(normalizedInput)
					: QString("Per-input LLDV fallback (%1LldvLutFile) -> %2").arg(normalizedInput, lutSwitchingAbsoluteFile(directoryPath, inputSpecificLldv)));
				if (!inputSpecificLldv.isEmpty())
					appendUniqueCandidate(candidates, lutSwitchingAbsoluteFile(directoryPath, inputSpecificLldv));
			}

			appendSharedFallback();
		}
		else
		{
			decision["category"] = "sdr";
			decision["reason"] = "Metadata bucket switching is not used for SDR";
			appendSharedFallback();
		}

		decision["candidateFiles"] = stringListToJsonArray(candidates);
		decision["candidatePlan"] = stringListToJsonArray(candidatePlan);
		const QString resolvedFile = resolveFirstExistingCandidate(candidates);
		const QString resolvedInterpolationFile = (interpolationCandidates != nullptr) ? resolveFirstExistingCandidate(*interpolationCandidates) : QString();
		if (!resolvedFile.isEmpty())
		{
			decision["targetFile"] = resolvedFile;
			decision["selectedFile"] = resolvedFile;
		}
		if (!resolvedInterpolationFile.isEmpty())
			decision["targetInterpolationFile"] = resolvedInterpolationFile;

		QJsonObject lutRuntime;
		QString currentLoadedFile;
		QString currentInterpolationFile;
		int currentInterpolationBlend = 0;
		if (grabberWrapper != nullptr)
		{
			lutRuntime = grabberWrapper->getLutRuntimeInfo();
			currentLoadedFile = normalizeExistingFilePath(lutRuntime["loadedFile"].toString());
			currentInterpolationFile = normalizeExistingFilePath(lutRuntime["loadedInterpolationFile"].toString());
			currentInterpolationBlend = lutRuntime["interpolationBlend"].toInt(0);
		}
		if (!currentLoadedFile.isEmpty())
			decision["currentFile"] = currentLoadedFile;
		if (!currentInterpolationFile.isEmpty())
			decision["currentInterpolationFile"] = currentInterpolationFile;
		if (currentInterpolationBlend > 0)
			decision["currentInterpolationBlend"] = currentInterpolationBlend;

		const int desiredInterpolationBlend = (interpolationBlend != nullptr) ? *interpolationBlend : 0;
		int appliedInterpolationBlend = desiredInterpolationBlend;
		const bool sameInterpolationPair = !resolvedFile.isEmpty() && !resolvedInterpolationFile.isEmpty() &&
			currentLoadedFile.compare(resolvedFile, Qt::CaseInsensitive) == 0 &&
			currentInterpolationFile.compare(resolvedInterpolationFile, Qt::CaseInsensitive) == 0;
		if (sameInterpolationPair && currentInterpolationBlend > 0 && desiredInterpolationBlend > 0 && desiredInterpolationBlend != currentInterpolationBlend)
		{
			appliedInterpolationBlend = smoothInterpolationBlendTarget(currentInterpolationBlend, desiredInterpolationBlend, kAutomaticInterpolationBlendMaxStep);
			decision["rawInterpolationBlend"] = desiredInterpolationBlend;
			decision["rawInterpolationRatio"] = static_cast<double>(desiredInterpolationBlend) / 255.0;
			decision["smoothedInterpolationBlend"] = appliedInterpolationBlend;
			decision["smoothedInterpolationRatio"] = static_cast<double>(appliedInterpolationBlend) / 255.0;
			decision["interpolationBlendSmoothingActive"] = true;
			decision["interpolationBlendMaxStep"] = kAutomaticInterpolationBlendMaxStep;
			if (interpolationBlend != nullptr)
				*interpolationBlend = appliedInterpolationBlend;
			decision["interpolationBlend"] = appliedInterpolationBlend;
			decision["interpolationRatio"] = static_cast<double>(appliedInterpolationBlend) / 255.0;
		}
		const bool desiredInterpolationActive = !resolvedInterpolationFile.isEmpty() && desiredInterpolationBlend > 0;
		const bool samePrimary = !resolvedFile.isEmpty() && currentLoadedFile.compare(resolvedFile, Qt::CaseInsensitive) == 0;
		const bool sameInterpolation = desiredInterpolationActive
			? currentInterpolationFile.compare(resolvedInterpolationFile, Qt::CaseInsensitive) == 0 && currentInterpolationBlend == appliedInterpolationBlend
			: currentInterpolationBlend == 0;
		decision["needsApply"] = decision["canApply"].toBool(false) && !resolvedFile.isEmpty() && (!samePrimary || !sameInterpolation);
		if (!decision.contains("reason"))
			decision["reason"] = resolvedFile.isEmpty() ? "No matching LUT bucket file was found; shared fallback will be used if present" : "Computed automatic LUT bucket decision";
		return decision;
	}

	AutomaticTransferSelectionPlan buildTransferAutomationSelectionPlan(const QString& rootPath, const QJsonObject& currentConfig, const QJsonObject& state, const QString& category, int nits)
	{
		AutomaticTransferSelectionPlan plan;
		const QJsonObject config = buildLutSwitchingConfig(rootPath, currentConfig);
		auto readProfile = [&config](const QString& key) {
			return config[key].toString().trimmed();
		};

		if (category == "sdr")
		{
			plan.tier = "base";
			plan.selectedProfile = readProfile("transferSdrProfile");
			plan.applyProfileId = plan.selectedProfile;
			return plan;
		}

		if (category == "hlg")
		{
			plan.tier = "base";
			plan.selectedProfile = readProfile("transferHlgProfile");
			plan.applyProfileId = plan.selectedProfile;
			return plan;
		}

		QStringList bucketCandidates;
		const QJsonObject bucketDecision = evaluateLutBucketDecision(rootPath, currentConfig, state, nullptr, bucketCandidates);
		plan.selectionContext = bucketDecision;

		const int bucketCount = qMax(1, bucketDecision["bucketCount"].toInt(1));
		const int bucketIndex = qBound(0, bucketDecision["bucketIndex"].toInt(0), bucketCount - 1);
		const QString family = bucketDecision["family"].toString();
		const int desiredTierCount = desiredTransferTierCount(category, family, bucketCount);
		QStringList tierProfiles;
		if (category == "hdr")
		{
			const QString profileKey = (family == "mdl4000") ? QStringLiteral("transferHdrMdl4000Profiles") : QStringLiteral("transferHdrMdl1000Profiles");
			const QJsonArray configuredProfiles = readNormalizedStringArray(config, profileKey);
			for (const QJsonValue& value : configuredProfiles)
				tierProfiles.append(value.toString().trimmed());
			if (tierProfiles.isEmpty())
				tierProfiles = buildTransferProfileTierList(config, category, family, desiredTierCount);
		}
		else if (category == "dolby-vision")
		{
			const QString profileKey = (family == "mdl1000") ? QStringLiteral("transferDolbyVisionMdl1000Profiles") : QStringLiteral("transferDolbyVisionMdl4000Profiles");
			const QJsonArray configuredProfiles = readNormalizedStringArray(config, profileKey);
			for (const QJsonValue& value : configuredProfiles)
				tierProfiles.append(value.toString().trimmed());
			if (tierProfiles.isEmpty())
				tierProfiles = buildTransferProfileTierList(config, category, family, desiredTierCount);
		}
		if (!tierProfiles.isEmpty())
		{
			const int tierCount = (category == "hdr") ? qMax(desiredTierCount, tierProfiles.size()) : qMax(desiredTierCount, tierProfiles.size());
			const int tierIndex = (category == "hdr") ? qBound(0, bucketIndex, tierCount - 1) : scaledBucketTierIndex(bucketIndex, bucketCount, tierCount);
			plan.tier = transferTierLabel(tierIndex, tierCount);
			plan.selectionContext["tierIndex"] = tierIndex;
			plan.selectionContext["tierCount"] = tierCount;
			plan.selectionContext["tierStrategy"] = (category == "hdr") ? "bucket-aligned" : QString("%1-ratio-scaled").arg(family.isEmpty() ? "dv" : family);
			plan.selectionContext["bucketCount"] = bucketCount;
			plan.selectionContext["bucketIndex"] = bucketIndex;
			const QString selectedProfile = tierProfiles.value(tierIndex).trimmed();
			if (!selectedProfile.isEmpty())
			{
				plan.selectedProfile = selectedProfile;
				plan.applyProfileId = selectedProfile;
				const QList<int> thresholds = jsonArrayToPositiveIntList(bucketDecision["thresholds"]);
				const double metricValue = bucketDecision["metricValue"].toDouble(0.0);
				plan.interpolation = buildAdjacentTransferInterpolationPlan(thresholds, bucketCount, tierCount, tierProfiles, metricValue, category == "hdr");
				if (plan.interpolation.isActive())
				{
					plan.applyProfileId = buildRuntimeInterpolatedTransferProfileId(plan.interpolation.lowerProfile, plan.interpolation.upperProfile, plan.interpolation.blend);
					plan.selectionContext["interpolationActive"] = true;
					plan.selectionContext["interpolationMode"] = "continuousAdjacent";
					plan.selectionContext["interpolationLowerTierIndex"] = plan.interpolation.lowerTierIndex;
					plan.selectionContext["interpolationUpperTierIndex"] = plan.interpolation.upperTierIndex;
					plan.selectionContext["interpolationLowerProfile"] = plan.interpolation.lowerProfile;
					plan.selectionContext["interpolationUpperProfile"] = plan.interpolation.upperProfile;
					plan.selectionContext["interpolationLowerBound"] = plan.interpolation.lowerBound;
					plan.selectionContext["interpolationUpperBound"] = plan.interpolation.upperBound;
					plan.selectionContext["interpolationRatio"] = plan.interpolation.ratio;
					plan.selectionContext["interpolationBlend"] = plan.interpolation.blend;
				}
				return plan;
			}

			plan.selectionContext["reason"] = QString("No transfer profile configured for %1 rank %2/%3").arg(category, QString::number(tierIndex + 1), QString::number(tierCount));
			return plan;
		}

		plan.selectionContext["tierStrategy"] = "no-transfer-profiles-configured";
		Q_UNUSED(nits);
		return plan;
	}

	struct DaytimeUpliftDecision
	{
		bool enabled = false;
		int blend = 0;
		QString profileId;
		QString reason;
		double rampPosition = 0.0;
		int startMinuteOfDay = 0;
		int endMinuteOfDay = 0;
		int rampMinutes = 0;
		int maxBlend = 0;
		int currentMinuteOfDay = 0;
	};

	DaytimeUpliftDecision computeDaytimeUpliftDecision(const QJsonObject& config, const QJsonObject& runtimeTransferState)
	{
		DaytimeUpliftDecision result;
		result.enabled = config["daytimeUpliftEnabled"].toBool(false);
		if (!result.enabled)
		{
			result.reason = "Daytime uplift is disabled";
			return result;
		}

		result.profileId = config["daytimeUpliftProfile"].toString().trimmed();
		result.maxBlend = config["daytimeUpliftMaxBlend"].toInt(128);
		result.rampMinutes = config["daytimeUpliftRampMinutes"].toInt(30);

		const int startHour = config["daytimeUpliftStartHour"].toInt(8);
		const int startMinute = config["daytimeUpliftStartMinute"].toInt(0);
		const int endHour = config["daytimeUpliftEndHour"].toInt(18);
		const int endMinute = config["daytimeUpliftEndMinute"].toInt(0);
		result.startMinuteOfDay = startHour * 60 + startMinute;
		result.endMinuteOfDay = endHour * 60 + endMinute;

		const QDateTime now = QDateTime::currentDateTime();
		result.currentMinuteOfDay = now.time().hour() * 60 + now.time().minute();

		const int ramp = qMax(1, result.rampMinutes);
		int start = result.startMinuteOfDay;
		int end = result.endMinuteOfDay;
		int current = result.currentMinuteOfDay;

		if (end <= start)
			end += 24 * 60;
		if (current < start)
			current += 24 * 60;

		if (current < start || current > end)
		{
			result.blend = 0;
			result.rampPosition = 0.0;
			result.reason = "Outside daytime window";
			return result;
		}

		const int minutesSinceStart = current - start;
		const int minutesBeforeEnd = end - current;

		double factor = 1.0;
		if (minutesSinceStart < ramp)
			factor = static_cast<double>(minutesSinceStart) / static_cast<double>(ramp);
		else if (minutesBeforeEnd < ramp)
			factor = static_cast<double>(minutesBeforeEnd) / static_cast<double>(ramp);

		factor = qBound(0.0, factor, 1.0);
		result.rampPosition = factor;
		result.blend = qBound(0, static_cast<int>(std::round(factor * result.maxBlend)), 255);

		const int currentDriverBlend = runtimeTransferState["daytimeUpliftBlend"].toInt(0);
		const int maxStep = 8;
		if (currentDriverBlend > 0 && result.blend > 0 && std::abs(result.blend - currentDriverBlend) > maxStep)
			result.blend = currentDriverBlend + ((result.blend > currentDriverBlend) ? maxStep : -maxStep);

		result.reason = result.blend > 0
			? QString("Daytime uplift active: %1/255 (ramp %2%)").arg(result.blend).arg(static_cast<int>(factor * 100.0))
			: "Daytime uplift at ramp edge (blend 0)";
		return result;
	}

	QJsonObject evaluateTransferAutomationDecision(const QString& rootPath, const QJsonObject& currentConfig, const QJsonObject& runtimeTransferState, const QJsonObject& state, QString& targetProfile)
	{
		const QJsonObject config = buildLutSwitchingConfig(rootPath, currentConfig);
		const QJsonObject activeState = buildTransferHeadersActiveState(rootPath, currentConfig, runtimeTransferState);

		QJsonObject decision;
		decision["enabled"] = config["transferAutomationEnabled"].toBool(false);
		decision["currentProfile"] = activeState["profile"].toString();
		decision["owner"] = activeState["owner"].toString();
		decision["ledDeviceType"] = activeState["ledDeviceType"].toString();
		decision["canApply"] = activeState["canSetProfile"].toBool(false);
		decision["applied"] = false;

		if (!decision["enabled"].toBool())
		{
			decision["reason"] = "Transfer automation is disabled";
			return decision;
		}

		if (!decision["canApply"].toBool())
		{
			decision["reason"] = "Transfer automation requires RawHID with transferCurveOwner set to hyperhdr";
			return decision;
		}

		const QString category = detectTransferAutomationCategory(state);
		decision["category"] = category;
		if (category.isEmpty())
		{
			decision["reason"] = "Unable to classify the reported stream for transfer automation";
			return decision;
		}

		QString nitsSource;
		const int nits = extractTransferAutomationNits(state, nitsSource);
		if (nits > 0)
			decision["nits"] = nits;
		if (!nitsSource.isEmpty())
			decision["nitsSource"] = nitsSource;

		AutomaticTransferSelectionPlan selectionPlan = buildTransferAutomationSelectionPlan(rootPath, currentConfig, state, category, nits);
		if (!selectionPlan.tier.isEmpty())
			decision["tier"] = selectionPlan.tier;
		for (auto iter = selectionPlan.selectionContext.begin(); iter != selectionPlan.selectionContext.end(); ++iter)
			decision[iter.key()] = iter.value();
		const QString rawProfile = selectionPlan.selectedProfile.trimmed();
		if (rawProfile.isEmpty())
		{
			decision["reason"] = QString("No transfer profile configured for %1").arg(category);
			return decision;
		}

		QString error;
		QString normalizedProfile = rawProfile;
		if (!selectionPlan.interpolation.isActive())
		{
			normalizedProfile = normalizeOptionalTransferCurveProfileId(rootPath, rawProfile, error);
			if (normalizedProfile.isEmpty())
			{
				decision["reason"] = error.isEmpty() ? "Configured transfer profile is invalid" : error;
				return decision;
			}
		}

		targetProfile = selectionPlan.interpolation.isActive() ? selectionPlan.applyProfileId : normalizedProfile;
		if (targetProfile.isEmpty())
		{
			decision["reason"] = error.isEmpty() ? "Configured transfer profile is invalid" : error;
			return decision;
		}

		decision["targetProfile"] = normalizedProfile;
		decision["profileExists"] = selectionPlan.interpolation.isActive() ? true : transferCurveProfileExists(rootPath, normalizedProfile);
		const bool currentInterpolationActive = runtimeTransferState["interpolationActive"].toBool(false);
		decision["currentInterpolationActive"] = currentInterpolationActive;
		if (currentInterpolationActive)
		{
			decision["currentInterpolationLowerProfile"] = runtimeTransferState["interpolationLowerProfile"].toString();
			decision["currentInterpolationUpperProfile"] = runtimeTransferState["interpolationUpperProfile"].toString();
			decision["currentInterpolationBlend"] = runtimeTransferState["interpolationBlend"].toInt(0);
			decision["currentInterpolationRatio"] = runtimeTransferState["interpolationRatio"].toDouble(0.0);
		}

		if (selectionPlan.interpolation.isActive())
		{
			const bool sameInterpolationPair = currentInterpolationActive &&
				runtimeTransferState["interpolationLowerProfile"].toString() == selectionPlan.interpolation.lowerProfile &&
				runtimeTransferState["interpolationUpperProfile"].toString() == selectionPlan.interpolation.upperProfile;
			if (sameInterpolationPair)
			{
				const int currentBlend = runtimeTransferState["interpolationBlend"].toInt(0);
				const int rawTargetBlend = selectionPlan.interpolation.blend;
				if (currentBlend > 0 && rawTargetBlend > 0 && rawTargetBlend != currentBlend)
				{
					selectionPlan.selectionContext["rawTargetInterpolationBlend"] = rawTargetBlend;
					selectionPlan.selectionContext["rawTargetInterpolationRatio"] = selectionPlan.interpolation.ratio;
					selectionPlan.interpolation.blend = smoothInterpolationBlendTarget(currentBlend, rawTargetBlend, kAutomaticInterpolationBlendMaxStep);
					selectionPlan.interpolation.ratio = static_cast<double>(selectionPlan.interpolation.blend) / 255.0;
					selectionPlan.applyProfileId = buildRuntimeInterpolatedTransferProfileId(selectionPlan.interpolation.lowerProfile, selectionPlan.interpolation.upperProfile, selectionPlan.interpolation.blend);
					selectionPlan.selectionContext["interpolationBlendSmoothingActive"] = true;
					selectionPlan.selectionContext["interpolationBlendMaxStep"] = kAutomaticInterpolationBlendMaxStep;
					selectionPlan.selectionContext["smoothedTargetInterpolationBlend"] = selectionPlan.interpolation.blend;
					selectionPlan.selectionContext["smoothedTargetInterpolationRatio"] = selectionPlan.interpolation.ratio;
					decision["rawTargetInterpolationBlend"] = rawTargetBlend;
					decision["rawTargetInterpolationRatio"] = selectionPlan.selectionContext["rawTargetInterpolationRatio"];
					decision["interpolationBlendSmoothingActive"] = true;
					decision["interpolationBlendMaxStep"] = kAutomaticInterpolationBlendMaxStep;
					decision["smoothedTargetInterpolationBlend"] = selectionPlan.interpolation.blend;
					decision["smoothedTargetInterpolationRatio"] = selectionPlan.interpolation.ratio;
				}
			}
			targetProfile = selectionPlan.applyProfileId;
			decision["interpolationActive"] = true;
			decision["targetInterpolationLowerProfile"] = selectionPlan.interpolation.lowerProfile;
			decision["targetInterpolationUpperProfile"] = selectionPlan.interpolation.upperProfile;
			decision["targetInterpolationLowerTierIndex"] = selectionPlan.interpolation.lowerTierIndex;
			decision["targetInterpolationUpperTierIndex"] = selectionPlan.interpolation.upperTierIndex;
			decision["targetInterpolationLowerBound"] = selectionPlan.interpolation.lowerBound;
			decision["targetInterpolationUpperBound"] = selectionPlan.interpolation.upperBound;
			decision["targetInterpolationRatio"] = selectionPlan.interpolation.ratio;
			decision["targetInterpolationBlend"] = selectionPlan.interpolation.blend;
			decision["needsApply"] = !currentInterpolationActive ||
				runtimeTransferState["interpolationLowerProfile"].toString() != selectionPlan.interpolation.lowerProfile ||
				runtimeTransferState["interpolationUpperProfile"].toString() != selectionPlan.interpolation.upperProfile ||
				runtimeTransferState["interpolationBlend"].toInt(-1) != selectionPlan.interpolation.blend;
			decision["reason"] = decision["needsApply"].toBool() ? "Automatic interpolated transfer change required" : "Requested interpolated transfer profile is already active";
		}
		else
		{
			decision["needsApply"] = currentInterpolationActive || (decision["currentProfile"].toString() != normalizedProfile);
			decision["reason"] = decision["needsApply"].toBool() ? "Automatic transfer change required" : "Requested transfer profile is already active";
		}
		return decision;
	}

	bool calibrationHeaderProfileExists(const QString& rootPath, const QString& profileId)
	{
		if (profileId == "disabled")
			return true;
		if (!profileId.startsWith("custom:"))
			return false;

		QString slug = profileId.mid(QString("custom:").size());
		slug = sanitizeCalibrationHeaderSlug(slug);
		if (slug.isEmpty())
			return false;

		const QString jsonPath = QDir(calibrationHeadersDirectoryPath(rootPath)).filePath(slug + ".json");
		return QFileInfo::exists(jsonPath);
	}

	bool rgbwLutProfileExists(const QString& rootPath, const QString& profileId)
	{
		if (profileId == "disabled")
			return true;
		if (!profileId.startsWith("custom:"))
			return false;

		QString slug = profileId.mid(QString("custom:").size());
		slug = sanitizeRgbwLutHeaderSlug(slug);
		if (slug.isEmpty())
			return false;

		const QDir directory(rgbwLutHeadersDirectoryPath(rootPath));
		return QFileInfo::exists(directory.filePath(slug + ".h")) || QFileInfo::exists(directory.filePath(slug + ".json"));
	}

	bool solverProfileExists(const QString& rootPath, const QString& profileId)
	{
		if (profileId == "builtin")
			return true;
		if (!profileId.startsWith("custom:"))
			return false;

		QString slug = profileId.mid(QString("custom:").size());
		slug = sanitizeSolverProfileSlug(slug);
		if (slug.isEmpty())
			return false;

		const QDir directory(solverProfilesDirectoryPath(rootPath));
		return QFileInfo::exists(directory.filePath(slug + ".h")) || QFileInfo::exists(directory.filePath(slug + ".json")) || QFileInfo(directory.filePath(slug)).isDir();
	}

	QJsonObject buildTransferHeadersActiveState(const QString& rootPath, const QJsonObject& currentConfig, const QJsonObject& runtimeTransferState)
	{
		QJsonObject state;
		const QJsonObject device = currentConfig["device"].toObject();
		const QString ledDeviceType = device["type"].toString();

		QString owner = device["transferCurveOwner"].toString("teensy").trimmed().toLower();
		if (owner.isEmpty())
			owner = "teensy";

		QString error;
		QString profile = normalizeTransferCurveProfileId(device["transferCurveProfile"].toString("curve3_4_new"), error);
		if (profile.isEmpty())
			profile = device["transferCurveProfile"].toString("curve3_4_new").trimmed();
		if (profile.isEmpty())
			profile = "curve3_4_new";

		const QString runtimeOwner = runtimeTransferState["owner"].toString().trimmed().toLower();
		const QString runtimeProfile = runtimeTransferState["profile"].toString().trimmed();
		const QString runtimeDeviceType = runtimeTransferState["ledDeviceType"].toString().trimmed();
		const QString effectiveOwner = runtimeOwner.isEmpty() ? owner : runtimeOwner;
		const QString effectiveProfile = runtimeProfile.isEmpty() ? profile : runtimeProfile;
		const QString effectiveDeviceType = runtimeDeviceType.isEmpty() ? ledDeviceType : runtimeDeviceType;

		state["ledDeviceType"] = effectiveDeviceType;
		state["owner"] = effectiveOwner;
		state["profile"] = effectiveProfile;
		state["profileExists"] = runtimeTransferState.contains("profileExists") ? runtimeTransferState["profileExists"].toBool(true) : transferCurveProfileExists(rootPath, effectiveProfile);
		state["canSetProfile"] = runtimeTransferState.contains("canSetProfile") ? runtimeTransferState["canSetProfile"].toBool(false) : ((effectiveDeviceType.compare("rawhid", Qt::CaseInsensitive) == 0) && (effectiveOwner == "hyperhdr"));
		state["activeRuntime"] = runtimeTransferState["activeRuntime"].toBool(false);
		state["interpolationActive"] = runtimeTransferState["interpolationActive"].toBool(false);
		if (runtimeTransferState["interpolationActive"].toBool(false))
		{
			state["interpolationLowerProfile"] = runtimeTransferState["interpolationLowerProfile"].toString();
			state["interpolationUpperProfile"] = runtimeTransferState["interpolationUpperProfile"].toString();
			state["interpolationBlend"] = runtimeTransferState["interpolationBlend"].toInt(0);
			state["interpolationRatio"] = runtimeTransferState["interpolationRatio"].toDouble(0.0);
		}
		return state;
	}

	QJsonObject buildCalibrationHeadersActiveState(const QString& rootPath, const QJsonObject& currentConfig)
	{
		QJsonObject state;
		const QJsonObject device = currentConfig["device"].toObject();
		const QString ledDeviceType = device["type"].toString();

		QString owner = device["calibrationHeaderOwner"].toString("teensy").trimmed().toLower();
		if (owner.isEmpty())
			owner = "teensy";

		QString error;
		QString profile = normalizeCalibrationHeaderProfileId(device["calibrationHeaderProfile"].toString("disabled"), error);
		if (profile.isEmpty())
			profile = device["calibrationHeaderProfile"].toString("disabled").trimmed();
		if (profile.isEmpty())
			profile = "disabled";

		state["ledDeviceType"] = ledDeviceType;
		state["owner"] = owner;
		state["profile"] = profile;
		state["profileExists"] = calibrationHeaderProfileExists(rootPath, profile);
		state["canSetProfile"] = (ledDeviceType.compare("rawhid", Qt::CaseInsensitive) == 0) && (owner == "hyperhdr");
		return state;
	}

	QJsonObject buildRgbwLutHeadersActiveState(const QString& rootPath, const QJsonObject& currentConfig)
	{
		QJsonObject state;
		const QJsonObject device = currentConfig["device"].toObject();
		const QString ledDeviceType = device["type"].toString();
		const QString mode = device["rgbwMode"].toString("disabled").trimmed();

		QString error;
		QString profile = normalizeRgbwLutProfileId(device["rgbwLutProfile"].toString("disabled"), error);
		if (profile.isEmpty())
			profile = device["rgbwLutProfile"].toString("disabled").trimmed();
		if (profile.isEmpty())
			profile = "disabled";

		state["ledDeviceType"] = ledDeviceType;
		state["mode"] = mode;
		state["profile"] = profile;
		state["profileExists"] = rgbwLutProfileExists(rootPath, profile);
		state["canSetProfile"] = (ledDeviceType.compare("rawhid", Qt::CaseInsensitive) == 0);
		return state;
	}

	QJsonObject buildSolverProfilesActiveState(const QString& rootPath, const QJsonObject& currentConfig)
	{
		QJsonObject state;
		const QJsonObject device = currentConfig["device"].toObject();
		const QString ledDeviceType = device["type"].toString();

		QString error;
		QString profile = normalizeSolverProfileId(device["solverProfile"].toString("builtin"), error);
		if (profile.isEmpty())
			profile = device["solverProfile"].toString("builtin").trimmed();
		if (profile.isEmpty())
			profile = "builtin";

		state["ledDeviceType"] = ledDeviceType;
		state["profile"] = profile;
		state["profileExists"] = solverProfileExists(rootPath, profile);
		state["canSetProfile"] = (ledDeviceType.compare("rawhid", Qt::CaseInsensitive) == 0);
		return state;
	}

	QJsonObject defaultLutSwitchingConfig()
	{
		QJsonObject config{
			{ "enabled", false },
			{ "directory", QString() },
			{ "activeLutFile", "lut_lin_tables.3d" },
			{ "sdrLutFile", "lut_lin_tables_sdr.3d" },
			{ "hdrLutFile", "lut_lin_tables_hdr.3d" },
			{ "lldvLutFile", "lut_lin_tables_lldv.3d" },
			{ "ugoosSdrLutFile", "lut_lin_tables_sdr_ugoos.3d" },
			{ "ugoosHdrLutFile", "lut_lin_tables_hdr_ugoos.3d" },
			{ "ugoosLldvLutFile", "lut_lin_tables_lldv_ugoos.3d" },
			{ "appletvSdrLutFile", "lut_lin_tables_sdr_appletv.3d" },
			{ "appletvHdrLutFile", "lut_lin_tables_hdr_appletv.3d" },
			{ "appletvLldvLutFile", "lut_lin_tables_lldv_appletv.3d" },
			{ "steamdeckSdrLutFile", "lut_lin_tables_sdr_steamdeck.3d" },
			{ "steamdeckHdrLutFile", "lut_lin_tables_hdr_steamdeck.3d" },
			{ "steamdeckLldvLutFile", "lut_lin_tables_lldv_steamdeck.3d" },
			{ "rx3SdrLutFile", "lut_lin_tables_sdr_rx3.3d" },
			{ "rx3HdrLutFile", "lut_lin_tables_hdr_rx3.3d" },
			{ "rx3LldvLutFile", "lut_lin_tables_lldv_rx3.3d" },
			{ "activeMode", "hdr" },
			{ "activeInput", QString() },
			{ "lastCustomFile", QString() },
			{ "sdrUsesHdrFile", false },
			{ "lutMemoryCacheEnabled", true },
			{ "transferAutomationEnabled", false },
			{ "transferSdrProfile", QString() },
			{ "transferHlgProfile", QString() },
			{ "transferHdrProfiles", QJsonArray() },
			{ "transferHdrMdl1000Profiles", QJsonArray() },
			{ "transferHdrMdl4000Profiles", QJsonArray() },
			{ "transferHdrLowProfile", QString() },
			{ "transferHdrMidProfile", QString() },
			{ "transferHdrHighProfile", QString() },
			{ "transferHdrLowNitLimit", 400 },
			{ "transferHdrMidNitLimit", 1000 },
			{ "transferDolbyVisionMdl1000Profiles", QJsonArray() },
			{ "transferDolbyVisionMdl4000Profiles", QJsonArray() },
			{ "transferDolbyVisionMdl1000RatioNumerator", 2 },
			{ "transferDolbyVisionMdl1000RatioDenominator", 4 },
			{ "transferDolbyVisionMdl4000RatioNumerator", 3 },
			{ "transferDolbyVisionMdl4000RatioDenominator", 5 },
			{ "transferDolbyVisionLowProfile", QString() },
			{ "transferDolbyVisionMidProfile", QString() },
			{ "transferDolbyVisionHighProfile", QString() },
			{ "transferDolbyVisionLowNitLimit", 400 },
			{ "transferDolbyVisionMidNitLimit", 1000 },
			{ "kodiDolbyVisionBucketMetric", "l1AvgNits" },
			{ "kodiDolbyVisionSceneIndexHighlightWeight", kDefaultDolbyVisionSceneIndexHighlightWeight },
			{ "kodiHdrThresholds", "250" },
			{ "kodiHdrMdl1000Thresholds", "250" },
			{ "kodiHdrMdl4000Thresholds", "250" },
			{ "kodiHdrMdlOnlyFallbackNits", 400 },
			{ "kodiDolbyVisionMdl1000Thresholds", "69, 120, 200" },
			{ "kodiDolbyVisionMdl4000Thresholds", "69, 120, 200, 400" },
		};

		for (const QString& input : QStringList{ "ugoos", "appletv", "steamdeck", "rx3" })
		{
			config[QString("%1HdrMetadataLutFiles").arg(input)] = QJsonArray();
			config[QString("%1HdrMdl1000LutFiles").arg(input)] = QJsonArray();
			config[QString("%1HdrMdl4000LutFiles").arg(input)] = QJsonArray();
			config[QString("%1DolbyVisionMdl1000LutFiles").arg(input)] = QJsonArray();
			config[QString("%1DolbyVisionMdl4000LutFiles").arg(input)] = QJsonArray();
		}

		return config;
	}

	QString joinPositiveThresholds(const QList<int>& values)
	{
		QStringList parts;
		for (int value : values)
			parts.append(QString::number(value));
		return parts.join(", ");
	}

	QJsonArray normalizeStringArray(const QJsonValue& value)
	{
		QJsonArray output;
		if (value.isArray())
		{
			for (const QJsonValue& item : value.toArray())
			{
				if (item.isString())
					output.append(item.toString().trimmed());
				else if (item.isNull() || item.isUndefined())
					output.append(QString());
			}
			return output;
		}

		if (value.isString())
		{
			const QString rawValue = value.toString().trimmed();
			if (!rawValue.isEmpty())
			{
				const QStringList tokens = rawValue.split(QRegularExpression(QStringLiteral(R"([\r\n,;|]+)")), Qt::KeepEmptyParts);
				for (const QString& token : tokens)
					output.append(token.trimmed());
			}
		}

		return output;
	}

	QString normalizePositiveThresholdList(const QJsonValue& value, const QList<int>& defaults, QString* error)
	{
		const QString fallback = joinPositiveThresholds(defaults);
		QList<int> parsedValues;

		auto fail = [&error, &fallback](const QString& message) {
			if (error != nullptr)
			{
				*error = message;
				return QString();
			}
			return fallback;
		};

		if (value.isArray())
		{
			for (const QJsonValue& item : value.toArray())
			{
				const int parsed = jsonPositiveInt(item, 0);
				if (parsed <= 0)
					return fail(QStringLiteral("Threshold lists must contain positive integers only"));
				parsedValues.append(parsed);
			}
		}
		else
		{
			QString rawValue;
			if (value.isString())
				rawValue = value.toString();
			else if (value.isDouble())
				rawValue = QString::number(value.toInt());

			rawValue = rawValue.trimmed();
			if (!rawValue.isEmpty())
			{
				const QStringList tokens = rawValue.split(QRegularExpression(QStringLiteral(R"([\s,;|]+)")), Qt::SkipEmptyParts);
				for (const QString& token : tokens)
				{
					bool ok = false;
					const int parsed = token.toInt(&ok);
					if (!ok || parsed <= 0)
						return fail(QStringLiteral("Threshold lists must contain positive integers only"));
					parsedValues.append(parsed);
				}
			}
		}

		if (parsedValues.isEmpty())
			parsedValues = defaults;

		for (int index = 1; index < parsedValues.size(); ++index)
		{
			if (parsedValues[index] <= parsedValues[index - 1])
				return fail(QStringLiteral("Threshold lists must be strictly increasing"));
		}

		return joinPositiveThresholds(parsedValues);
	}

	QString normalizeLutSwitchingMode(QString mode)
	{
		mode = mode.trimmed().toLower();
		if (mode == "sdr" || mode == "hdr" || mode == "lldv" || mode == "custom")
			return mode;
		return QString();
	}

	QString normalizeLutSwitchingInput(QString input)
	{
		input = input.trimmed().toLower();
		if (input == "rx0" || input == "ugoos")
			return "ugoos";
		if (input == "rx1" || input == "appletv" || input == "appletv4k")
			return "appletv";
		if (input == "rx2" || input == "steamdeck" || input == "steamdeckmoonlight")
			return "steamdeck";
		if (input == "rx3")
			return "rx3";
		return QString();
	}

	QString normalizeDolbyVisionBucketMetric(QString metric)
	{
		metric = metric.trimmed().toLower();
		if (metric == "l1maxnits" || metric == "l1-max-nits" || metric == "max" || metric == "l1max")
			return QStringLiteral("l1MaxNits");
		if (metric == "sceneaverageindex" || metric == "scene-average-index" || metric == "sceneavgindex" || metric == "sceneindex" || metric == "sceneavg")
			return QStringLiteral("sceneAverageIndex");
		return QStringLiteral("l1AvgNits");
	}

	QString lutSwitchingPerInputConfigKey(const QString& input, const QString& mode)
	{
		if (input.isEmpty() || mode.isEmpty() || mode == "custom")
			return QString();

		QString normalizedMode = mode;
		normalizedMode[0] = normalizedMode[0].toUpper();
		return QString("%1%2LutFile").arg(input, normalizedMode);
	}

	QString lutSwitchingFileName(const QJsonObject& config, const QString& key, const QString& fallback)
	{
		const QString fileName = config[key].toString(fallback).trimmed();
		return fileName.isEmpty() ? fallback : fileName;
	}

	QString lutSwitchingAbsoluteFile(const QString& directoryPath, const QString& fileName)
	{
		const QFileInfo fileInfo(fileName);
		if (fileInfo.isAbsolute())
			return QDir::cleanPath(fileInfo.absoluteFilePath());
		return QDir(directoryPath).filePath(fileName);
	}

	QString lutSwitchingPerInputFileName(const QJsonObject& config, const QString& input, const QString& mode)
	{
		const QString key = lutSwitchingPerInputConfigKey(input, mode);
		if (key.isEmpty())
			return QString();
		return config[key].toString().trimmed();
	}

	QJsonObject buildLutSwitchingConfig(const QString& rootPath, const QJsonObject& currentConfig)
	{
		QJsonObject config = defaultLutSwitchingConfig();
		const QJsonObject stored = currentConfig["lutSwitching"].toObject();
		for (auto iter = stored.begin(); iter != stored.end(); ++iter)
			config[iter.key()] = iter.value();

		QString directoryPath = config["directory"].toString().trimmed();
		if (directoryPath.isEmpty())
			directoryPath = rootPath;
		config["directory"] = QDir::cleanPath(directoryPath);

		QString activeMode = normalizeLutSwitchingMode(config["activeMode"].toString("hdr"));
		if (activeMode.isEmpty() || activeMode == "custom")
			activeMode = "hdr";
		config["activeMode"] = activeMode;
		config["lastCustomFile"] = QString();

		const QString activeInput = normalizeLutSwitchingInput(config["activeInput"].toString());
		config["activeInput"] = activeInput;

		const QStringList transferProfileKeys = {
			QStringLiteral("transferSdrProfile"),
			QStringLiteral("transferHlgProfile"),
			QStringLiteral("transferHdrProfiles"),
			QStringLiteral("transferHdrMdl1000Profiles"),
			QStringLiteral("transferHdrMdl4000Profiles"),
			QStringLiteral("transferHdrLowProfile"),
			QStringLiteral("transferHdrMidProfile"),
			QStringLiteral("transferHdrHighProfile"),
			QStringLiteral("transferDolbyVisionMdl1000Profiles"),
			QStringLiteral("transferDolbyVisionMdl4000Profiles"),
			QStringLiteral("transferDolbyVisionLowProfile"),
			QStringLiteral("transferDolbyVisionMidProfile"),
			QStringLiteral("transferDolbyVisionHighProfile")
		};
		for (const QString& key : transferProfileKeys)
		{
			if (key.endsWith(QStringLiteral("Profiles")))
				config[key] = normalizeStringArray(config[key]);
			else
				config[key] = config[key].toString().trimmed();
		}

		config["transferAutomationEnabled"] = config["transferAutomationEnabled"].toBool(false);
		config["lutMemoryCacheEnabled"] = config["lutMemoryCacheEnabled"].toBool(true);
		const int hdrLowLimit = jsonPositiveInt(config["transferHdrLowNitLimit"], 400);
		const int hdrMidLimit = qMax(hdrLowLimit + 1, jsonPositiveInt(config["transferHdrMidNitLimit"], 1000));
		config["transferHdrLowNitLimit"] = hdrLowLimit;
		config["transferHdrMidNitLimit"] = hdrMidLimit;

		const int dolbyVisionLowLimit = jsonPositiveInt(config["transferDolbyVisionLowNitLimit"], 400);
		const int dolbyVisionMidLimit = qMax(dolbyVisionLowLimit + 1, jsonPositiveInt(config["transferDolbyVisionMidNitLimit"], 1000));
		config["transferDolbyVisionLowNitLimit"] = dolbyVisionLowLimit;
		config["transferDolbyVisionMidNitLimit"] = dolbyVisionMidLimit;
		config["transferDolbyVisionMdl1000RatioNumerator"] = jsonPositiveInt(config["transferDolbyVisionMdl1000RatioNumerator"], 2);
		config["transferDolbyVisionMdl1000RatioDenominator"] = jsonPositiveInt(config["transferDolbyVisionMdl1000RatioDenominator"], 4);
		config["transferDolbyVisionMdl4000RatioNumerator"] = jsonPositiveInt(config["transferDolbyVisionMdl4000RatioNumerator"], 3);
		config["transferDolbyVisionMdl4000RatioDenominator"] = jsonPositiveInt(config["transferDolbyVisionMdl4000RatioDenominator"], 5);
		config["kodiDolbyVisionBucketMetric"] = normalizeDolbyVisionBucketMetric(config["kodiDolbyVisionBucketMetric"].toString());
		config["kodiDolbyVisionSceneIndexHighlightWeight"] = jsonBoundedDouble(config["kodiDolbyVisionSceneIndexHighlightWeight"], kDefaultDolbyVisionSceneIndexHighlightWeight, 0.0, 1.0);
		config["kodiHdrThresholds"] = normalizePositiveThresholdList(config["kodiHdrThresholds"], QList<int>{ 250 });
		config["kodiHdrMdl1000Thresholds"] = normalizePositiveThresholdList(config["kodiHdrMdl1000Thresholds"], parsePositiveThresholdList(config["kodiHdrThresholds"], QList<int>{ 250 }));
		config["kodiHdrMdl4000Thresholds"] = normalizePositiveThresholdList(config["kodiHdrMdl4000Thresholds"], parsePositiveThresholdList(config["kodiHdrThresholds"], QList<int>{ 250 }));
		config["kodiHdrMdlOnlyFallbackNits"] = jsonBoundedInt(config["kodiHdrMdlOnlyFallbackNits"], 400, 1, 10000);
		config["kodiDolbyVisionMdl1000Thresholds"] = normalizePositiveThresholdList(config["kodiDolbyVisionMdl1000Thresholds"], QList<int>{ 69, 120, 200 });
		config["kodiDolbyVisionMdl4000Thresholds"] = normalizePositiveThresholdList(config["kodiDolbyVisionMdl4000Thresholds"], QList<int>{ 69, 120, 200, 400 });
		if (readNormalizedStringArray(config, "transferHdrProfiles").isEmpty())
			config["transferHdrProfiles"] = normalizeStringArray(QJsonArray{ config["transferHdrLowProfile"], config["transferHdrMidProfile"], config["transferHdrHighProfile"] });
		if (readNormalizedStringArray(config, "transferHdrMdl1000Profiles").isEmpty())
			config["transferHdrMdl1000Profiles"] = normalizeStringArray(config["transferHdrProfiles"]);
		if (readNormalizedStringArray(config, "transferHdrMdl4000Profiles").isEmpty())
			config["transferHdrMdl4000Profiles"] = normalizeStringArray(config["transferHdrProfiles"]);
		if (readNormalizedStringArray(config, "transferDolbyVisionMdl1000Profiles").isEmpty())
			config["transferDolbyVisionMdl1000Profiles"] = normalizeStringArray(QJsonArray{ config["transferDolbyVisionLowProfile"], config["transferDolbyVisionHighProfile"] });
		if (readNormalizedStringArray(config, "transferDolbyVisionMdl4000Profiles").isEmpty())
			config["transferDolbyVisionMdl4000Profiles"] = normalizeStringArray(QJsonArray{ config["transferDolbyVisionLowProfile"], config["transferDolbyVisionMidProfile"], config["transferDolbyVisionHighProfile"] });
		for (const QString& input : QStringList{ "ugoos", "appletv", "steamdeck", "rx3" })
		{
			config[QString("%1HdrMetadataLutFiles").arg(input)] = normalizeStringArray(config[QString("%1HdrMetadataLutFiles").arg(input)]);
			if (normalizeStringArray(config[QString("%1HdrMdl1000LutFiles").arg(input)]).isEmpty())
				config[QString("%1HdrMdl1000LutFiles").arg(input)] = normalizeStringArray(config[QString("%1HdrMetadataLutFiles").arg(input)]);
			else
				config[QString("%1HdrMdl1000LutFiles").arg(input)] = normalizeStringArray(config[QString("%1HdrMdl1000LutFiles").arg(input)]);
			if (normalizeStringArray(config[QString("%1HdrMdl4000LutFiles").arg(input)]).isEmpty())
				config[QString("%1HdrMdl4000LutFiles").arg(input)] = normalizeStringArray(config[QString("%1HdrMetadataLutFiles").arg(input)]);
			else
				config[QString("%1HdrMdl4000LutFiles").arg(input)] = normalizeStringArray(config[QString("%1HdrMdl4000LutFiles").arg(input)]);
			config[QString("%1DolbyVisionMdl1000LutFiles").arg(input)] = normalizeStringArray(config[QString("%1DolbyVisionMdl1000LutFiles").arg(input)]);
			config[QString("%1DolbyVisionMdl4000LutFiles").arg(input)] = normalizeStringArray(config[QString("%1DolbyVisionMdl4000LutFiles").arg(input)]);
		}

		config["daytimeUpliftEnabled"] = config["daytimeUpliftEnabled"].toBool(false);
		config["daytimeUpliftProfile"] = config["daytimeUpliftProfile"].toString().trimmed();
		config["daytimeUpliftStartHour"] = jsonBoundedInt(config["daytimeUpliftStartHour"], 8, 0, 23);
		config["daytimeUpliftStartMinute"] = jsonBoundedInt(config["daytimeUpliftStartMinute"], 0, 0, 59);
		config["daytimeUpliftEndHour"] = jsonBoundedInt(config["daytimeUpliftEndHour"], 18, 0, 23);
		config["daytimeUpliftEndMinute"] = jsonBoundedInt(config["daytimeUpliftEndMinute"], 0, 0, 59);
		config["daytimeUpliftRampMinutes"] = jsonBoundedInt(config["daytimeUpliftRampMinutes"], 30, 0, 120);
		config["daytimeUpliftMaxBlend"] = jsonBoundedInt(config["daytimeUpliftMaxBlend"], 128, 1, 255);

		return config;
	}

	QJsonObject buildLutSwitchingPaths(const QJsonObject& config)
	{
		const QString directoryPath = config["directory"].toString();
		QJsonObject paths;
		paths["active"] = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "activeLutFile", "lut_lin_tables.3d"));
		paths["sdr"] = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "sdrLutFile", "lut_lin_tables_sdr.3d"));
		paths["hdr"] = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "hdrLutFile", "lut_lin_tables_hdr.3d"));
		paths["lldv"] = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "lldvLutFile", "lut_lin_tables_lldv.3d"));

		QJsonObject perInputPaths;
		for (const QString& input : QStringList{ "ugoos", "appletv", "steamdeck", "rx3" })
		{
			QJsonObject inputPaths;
			for (const QString& mode : QStringList{ "sdr", "hdr", "lldv" })
			{
				const QString fileName = lutSwitchingPerInputFileName(config, input, mode);
				inputPaths[mode] = fileName.isEmpty() ? QString() : lutSwitchingAbsoluteFile(directoryPath, fileName);
			}
			perInputPaths[input] = inputPaths;
		}
		paths["perInput"] = perInputPaths;
		return paths;
	}

	QJsonArray stringListToJsonArray(const QStringList& values)
	{
		QJsonArray output;
		for (const QString& value : values)
			output.append(value);
		return output;
	}

	QStringList buildLutSwitchingCandidateFiles(
		const QString& rootPath,
		const QJsonObject& currentConfig,
		const QString& requestedMode,
		const QString& requestedInput,
		const QString& requestedFile,
		QString& resolvedMode,
		QString& resolvedInput,
		QString& error)
	{
		const QJsonObject config = buildLutSwitchingConfig(rootPath, currentConfig);
		if (!config["enabled"].toBool(false))
		{
			error = "LUT switching is disabled in settings";
			return {};
		}

		const QString directoryPath = config["directory"].toString();
		const QString normalizedInput = normalizeLutSwitchingInput(requestedInput.isEmpty() ? config["activeInput"].toString() : requestedInput);
		resolvedInput = normalizedInput;
		QString primaryFile;
		const QString customFile = requestedFile.trimmed();
		if (!customFile.isEmpty())
		{
			primaryFile = lutSwitchingAbsoluteFile(directoryPath, customFile);
			resolvedMode = "custom";
		}
		else
		{
			QString mode = normalizeLutSwitchingMode(requestedMode);
			if (mode.isEmpty())
				mode = normalizeLutSwitchingMode(config["activeMode"].toString("hdr"));
			if (mode.isEmpty() || mode == "custom")
			{
				error = "A valid LUT switching mode is required";
				return {};
			}

			resolvedMode = mode;

			if (!normalizedInput.isEmpty())
			{
				const QString inputSpecific = lutSwitchingPerInputFileName(config, normalizedInput, mode);
				if (primaryFile.isEmpty() && !inputSpecific.isEmpty())
					primaryFile = lutSwitchingAbsoluteFile(directoryPath, inputSpecific);
			}

			if (primaryFile.isEmpty())
			{
				if (mode == "sdr")
				{
					if (config["sdrUsesHdrFile"].toBool(false))
						primaryFile = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "hdrLutFile", "lut_lin_tables_hdr.3d"));
					else
						primaryFile = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "sdrLutFile", "lut_lin_tables_sdr.3d"));
				}
				else if (mode == "hdr")
					primaryFile = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "hdrLutFile", "lut_lin_tables_hdr.3d"));
				else if (mode == "lldv")
					primaryFile = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "lldvLutFile", "lut_lin_tables_lldv.3d"));
			}
		}

		QStringList candidates;
		auto appendUnique = [&candidates](const QString& filePath) {
			const QString cleaned = QDir::cleanPath(filePath.trimmed());
			if (cleaned.isEmpty() || candidates.contains(cleaned, Qt::CaseInsensitive))
				return;
			candidates.append(cleaned);
		};

		appendUnique(primaryFile);
		if (!normalizedInput.isEmpty() && resolvedMode != "custom")
		{
			const QString inputSpecific = lutSwitchingPerInputFileName(config, normalizedInput, resolvedMode);
			if (!inputSpecific.isEmpty())
				appendUnique(lutSwitchingAbsoluteFile(directoryPath, inputSpecific));

			QString genericModeFallback;
			if (resolvedMode == "sdr")
			{
				if (config["sdrUsesHdrFile"].toBool(false))
					genericModeFallback = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "hdrLutFile", "lut_lin_tables_hdr.3d"));
				else
					genericModeFallback = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "sdrLutFile", "lut_lin_tables_sdr.3d"));
			}
			else if (resolvedMode == "hdr")
				genericModeFallback = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "hdrLutFile", "lut_lin_tables_hdr.3d"));
			else if (resolvedMode == "lldv")
				genericModeFallback = lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "lldvLutFile", "lut_lin_tables_lldv.3d"));

			appendUnique(genericModeFallback);
		}
		appendUnique(lutSwitchingAbsoluteFile(directoryPath, lutSwitchingFileName(config, "activeLutFile", "lut_lin_tables.3d")));
		return candidates;
	}

	QJsonObject buildLutSwitchingResponse(const QString& rootPath, const QJsonObject& currentConfig, GrabberWrapper* grabberWrapper, const QJsonObject& reportedVideoStream = QJsonObject())
	{
		const QJsonObject config = buildLutSwitchingConfig(rootPath, currentConfig);
		QJsonObject response;
		response["settings"] = config;
		response["paths"] = buildLutSwitchingPaths(config);
		QJsonObject runtime = (grabberWrapper != nullptr) ? grabberWrapper->getLutRuntimeInfo() : QJsonObject();
		const QJsonObject lutBucketDecision = reportedVideoStream["lutBucketDecision"].toObject();
		if (!lutBucketDecision.isEmpty())
			runtime["lutBucketDecision"] = lutBucketDecision;
		const QJsonObject transferAutomation = reportedVideoStream["transferAutomation"].toObject();
		if (!transferAutomation.isEmpty())
			runtime["transferAutomation"] = transferAutomation;
		const QJsonObject daytimeUplift = reportedVideoStream["daytimeUplift"].toObject();
		if (!daytimeUplift.isEmpty())
			runtime["daytimeUplift"] = daytimeUplift;
		response["runtime"] = runtime;
		return response;
	}

	QJsonObject buildStoredLutSwitchingConfig(const QString& rootPath, const QJsonObject& currentConfig, const QJsonObject& update, QString& error)
	{
		QJsonObject config = buildLutSwitchingConfig(rootPath, currentConfig);

		if (update.contains("enabled"))
			config["enabled"] = update["enabled"].toBool(config["enabled"].toBool());

		if (update.contains("directory"))
		{
			QString directory = update["directory"].toString().trimmed();
			if (directory.isEmpty())
				directory = rootPath;
			config["directory"] = QDir::cleanPath(directory);
		}

		auto applyFileOverride = [&config, &update](const QString& key) {
			if (update.contains(key))
				config[key] = update[key].toString().trimmed();
		};

		auto applyFileArrayOverride = [&config, &update](const QString& key) {
			if (update.contains(key))
				config[key] = normalizeStringArray(update[key]);
		};

		applyFileOverride("activeLutFile");
		applyFileOverride("sdrLutFile");
		applyFileOverride("hdrLutFile");
		applyFileOverride("lldvLutFile");
		applyFileOverride("ugoosSdrLutFile");
		applyFileOverride("ugoosHdrLutFile");
		applyFileOverride("ugoosLldvLutFile");
		applyFileOverride("appletvSdrLutFile");
		applyFileOverride("appletvHdrLutFile");
		applyFileOverride("appletvLldvLutFile");
		applyFileOverride("steamdeckSdrLutFile");
		applyFileOverride("steamdeckHdrLutFile");
		applyFileOverride("steamdeckLldvLutFile");
		applyFileOverride("rx3SdrLutFile");
		applyFileOverride("rx3HdrLutFile");
		applyFileOverride("rx3LldvLutFile");
		for (const QString& input : QStringList{ "ugoos", "appletv", "steamdeck", "rx3" })
		{
			applyFileArrayOverride(QString("%1HdrMetadataLutFiles").arg(input));
			applyFileArrayOverride(QString("%1HdrMdl1000LutFiles").arg(input));
			applyFileArrayOverride(QString("%1HdrMdl4000LutFiles").arg(input));
			applyFileArrayOverride(QString("%1DolbyVisionMdl1000LutFiles").arg(input));
			applyFileArrayOverride(QString("%1DolbyVisionMdl4000LutFiles").arg(input));
		}

		if (update.contains("sdrUsesHdrFile"))
			config["sdrUsesHdrFile"] = update["sdrUsesHdrFile"].toBool(config["sdrUsesHdrFile"].toBool(false));

		if (update.contains("lutMemoryCacheEnabled"))
			config["lutMemoryCacheEnabled"] = update["lutMemoryCacheEnabled"].toBool(config["lutMemoryCacheEnabled"].toBool(true));

		if (update.contains("mode"))
		{
			const QString mode = normalizeLutSwitchingMode(update["mode"].toString());
			if (mode.isEmpty())
			{
				error = "A valid LUT switching mode is required";
				return QJsonObject();
			}
			config["activeMode"] = (mode == "custom") ? config["activeMode"].toString("hdr") : mode;
		}

		if (update.contains("input"))
		{
			const QString input = normalizeLutSwitchingInput(update["input"].toString());
			if (!update["input"].toString().trimmed().isEmpty() && input.isEmpty())
			{
				error = "A valid LUT switching input is required";
				return QJsonObject();
			}
			config["activeInput"] = input;
		}

		config["lastCustomFile"] = QString();

		if (update.contains("transferAutomationEnabled"))
			config["transferAutomationEnabled"] = update["transferAutomationEnabled"].toBool(config["transferAutomationEnabled"].toBool(false));

		auto applyTransferProfileOverride = [&config, &update, &rootPath, &error](const QString& key) {
			if (!update.contains(key))
				return true;

			const QString rawProfile = update[key].toString().trimmed();
			if (rawProfile.isEmpty())
			{
				config[key] = QString();
				return true;
			}

			QString normalizeError;
			const QString normalizedProfile = normalizeOptionalTransferCurveProfileId(rootPath, rawProfile, normalizeError);
			if (normalizedProfile.isEmpty())
			{
				error = normalizeError;
				return false;
			}

			config[key] = normalizedProfile;
			return true;
		};

		auto applyTransferProfileArrayOverride = [&config, &update, &rootPath, &error](const QString& key) {
			if (!update.contains(key))
				return true;

			const QJsonArray rawValues = normalizeStringArray(update[key]);
			QJsonArray normalizedValues;
			for (const QJsonValue& item : rawValues)
			{
				const QString rawProfile = item.toString().trimmed();
				if (rawProfile.isEmpty())
				{
					normalizedValues.append(QString());
					continue;
				}

				QString normalizeError;
				const QString normalizedProfile = normalizeOptionalTransferCurveProfileId(rootPath, rawProfile, normalizeError);
				if (normalizedProfile.isEmpty())
				{
					error = normalizeError;
					return false;
				}

				normalizedValues.append(normalizedProfile);
			}

			config[key] = normalizedValues;
			return true;
		};

		if (!applyTransferProfileArrayOverride(QStringLiteral("transferHdrProfiles")) ||
			!applyTransferProfileArrayOverride(QStringLiteral("transferHdrMdl1000Profiles")) ||
			!applyTransferProfileArrayOverride(QStringLiteral("transferHdrMdl4000Profiles")) ||
			!applyTransferProfileArrayOverride(QStringLiteral("transferDolbyVisionMdl1000Profiles")) ||
			!applyTransferProfileArrayOverride(QStringLiteral("transferDolbyVisionMdl4000Profiles")))
		{
			return QJsonObject();
		}

		for (const QString& key : QStringList{
			QStringLiteral("transferSdrProfile"),
			QStringLiteral("transferHlgProfile"),
			QStringLiteral("transferHdrLowProfile"),
			QStringLiteral("transferHdrMidProfile"),
			QStringLiteral("transferHdrHighProfile"),
			QStringLiteral("transferDolbyVisionLowProfile"),
			QStringLiteral("transferDolbyVisionMidProfile"),
			QStringLiteral("transferDolbyVisionHighProfile")
		})
		{
			if (!applyTransferProfileOverride(key))
				return QJsonObject();
		}

		auto applyPositiveThreshold = [&config, &update, &error](const QString& key, int fallbackValue) {
			if (!update.contains(key))
				return true;

			const int value = jsonPositiveInt(update[key], 0);
			if (value <= 0)
			{
				error = QString("A positive value is required for %1").arg(key);
				return false;
			}

			config[key] = value;
			return true;
		};

		if (!applyPositiveThreshold("transferHdrLowNitLimit", 400) ||
			!applyPositiveThreshold("transferHdrMidNitLimit", 1000) ||
			!applyPositiveThreshold("transferDolbyVisionLowNitLimit", 400) ||
			!applyPositiveThreshold("transferDolbyVisionMidNitLimit", 1000) ||
			!applyPositiveThreshold("transferDolbyVisionMdl1000RatioNumerator", 2) ||
			!applyPositiveThreshold("transferDolbyVisionMdl1000RatioDenominator", 4) ||
			!applyPositiveThreshold("transferDolbyVisionMdl4000RatioNumerator", 3) ||
			!applyPositiveThreshold("transferDolbyVisionMdl4000RatioDenominator", 5))
		{
			return QJsonObject();
		}

		if (update.contains("kodiDolbyVisionSceneIndexHighlightWeight"))
		{
			const QJsonValue value = update["kodiDolbyVisionSceneIndexHighlightWeight"];
			double parsedValue = kDefaultDolbyVisionSceneIndexHighlightWeight;
			bool ok = false;
			if (value.isDouble())
			{
				parsedValue = value.toDouble(kDefaultDolbyVisionSceneIndexHighlightWeight);
				ok = std::isfinite(parsedValue);
			}
			else if (value.isString())
			{
				parsedValue = value.toString().trimmed().toDouble(&ok);
			}

			if (!ok || !std::isfinite(parsedValue) || parsedValue < 0.0 || parsedValue > 1.0)
			{
				error = "Dolby Vision scene average highlight weight must be between 0.0 and 1.0";
				return QJsonObject();
			}

			config["kodiDolbyVisionSceneIndexHighlightWeight"] = parsedValue;
		}

		if (jsonPositiveInt(config["transferHdrLowNitLimit"], 400) >= jsonPositiveInt(config["transferHdrMidNitLimit"], 1000))
		{
			error = "HDR low nit limit must be lower than the HDR mid nit limit";
			return QJsonObject();
		}

		if (jsonPositiveInt(config["transferDolbyVisionLowNitLimit"], 400) >= jsonPositiveInt(config["transferDolbyVisionMidNitLimit"], 1000))
		{
			error = "Dolby Vision low nit limit must be lower than the Dolby Vision mid nit limit";
			return QJsonObject();
		}

		auto applyThresholdListOverride = [&config, &update, &error](const QString& key, const QList<int>& defaults) {
			if (!update.contains(key))
				return true;

			QString normalizeError;
			const QString normalized = normalizePositiveThresholdList(update[key], defaults, &normalizeError);
			if (normalized.isEmpty())
			{
				error = QString("Invalid value for %1: %2").arg(key, normalizeError);
				return false;
			}

			config[key] = normalized;
			return true;
		};

		if (update.contains("kodiHdrMdlOnlyFallbackNits"))
			config["kodiHdrMdlOnlyFallbackNits"] = jsonBoundedInt(update["kodiHdrMdlOnlyFallbackNits"], 400, 1, 10000);

		if (!applyThresholdListOverride("kodiHdrThresholds", QList<int>{ 250 }) ||
			!applyThresholdListOverride("kodiHdrMdl1000Thresholds", QList<int>{ 250 }) ||
			!applyThresholdListOverride("kodiHdrMdl4000Thresholds", QList<int>{ 250 }) ||
			!applyThresholdListOverride("kodiDolbyVisionMdl1000Thresholds", QList<int>{ 69, 120, 200 }) ||
			!applyThresholdListOverride("kodiDolbyVisionMdl4000Thresholds", QList<int>{ 69, 120, 200, 400 }))
		{
			return QJsonObject();
		}

		if (update.contains("kodiDolbyVisionBucketMetric"))
			config["kodiDolbyVisionBucketMetric"] = normalizeDolbyVisionBucketMetric(update["kodiDolbyVisionBucketMetric"].toString());

		if (update.contains("daytimeUpliftEnabled"))
			config["daytimeUpliftEnabled"] = update["daytimeUpliftEnabled"].toBool(false);
		if (update.contains("daytimeUpliftProfile"))
			config["daytimeUpliftProfile"] = update["daytimeUpliftProfile"].toString().trimmed();
		for (const QString& key : QStringList{
			QStringLiteral("daytimeUpliftStartHour"),
			QStringLiteral("daytimeUpliftStartMinute"),
			QStringLiteral("daytimeUpliftEndHour"),
			QStringLiteral("daytimeUpliftEndMinute"),
			QStringLiteral("daytimeUpliftRampMinutes"),
			QStringLiteral("daytimeUpliftMaxBlend")
		})
		{
			if (update.contains(key))
				config[key] = update[key].toInt(config[key].toInt());
		}

		return config;
	}

	bool extractTransferCurveArrayBody(const QString& content, const QString& symbol, QString& body, QString& error)
	{
		const QRegularExpression declarationExpression(
			QString(R"(%1\s*\[\s*\d+\s*\](?:\s+PROGMEM)?\s*=\s*\{)").arg(QRegularExpression::escape(symbol)));
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

	bool parseTransferCurveArray(const QString& content, const QString& symbol, uint32_t bucketCount, QVector<uint16_t>& output, QString& error)
	{
		QString body;
		if (!extractTransferCurveArrayBody(content, symbol, body, error))
			return false;

		const QRegularExpression valueExpression(R"((?:0x[0-9a-fA-F]+)|(?:\d+))");
		QVector<QString> entries;
		entries.reserve(static_cast<qsizetype>(bucketCount));

		QRegularExpressionMatchIterator iterator = valueExpression.globalMatch(body);
		while (iterator.hasNext())
			entries.push_back(iterator.next().captured(0));

		if (entries.size() != static_cast<qsizetype>(bucketCount))
		{
			error = QString("Array '%1' contains %2 entries, expected %3")
				.arg(symbol)
				.arg(entries.size())
				.arg(bucketCount);
			return false;
		}

		output.clear();
		output.reserve(entries.size());
		for (const QString& rawEntry : entries)
		{
			bool ok = false;
			const uint value = rawEntry.trimmed().toUInt(&ok, 0);
			if (!ok || value > 65535u)
			{
				error = QString("Array '%1' contains an invalid 16-bit value: %2").arg(symbol, rawEntry.trimmed());
				return false;
			}
			output.push_back(static_cast<uint16_t>(value));
		}

		return true;
	}

	bool parseTransferCurveHeader(const QString& content, ParsedTransferCurveProfile& parsed, QString& error)
	{
		const QString cleaned = stripTransferCurveComments(content);
		const QRegularExpression bucketExpression(R"(BUCKET_COUNT\s*=\s*(\d+))");
		const QRegularExpressionMatch bucketMatch = bucketExpression.match(cleaned);
		if (!bucketMatch.hasMatch())
		{
			error = "Missing BUCKET_COUNT declaration";
			return false;
		}

		bool ok = false;
		const uint bucketCount = bucketMatch.captured(1).toUInt(&ok);
		if (!ok || bucketCount == 0u)
		{
			error = "BUCKET_COUNT must be a positive integer";
			return false;
		}

		parsed = ParsedTransferCurveProfile{};
		parsed.bucketCount = bucketCount;

		return parseTransferCurveArray(cleaned, "TARGET_R", bucketCount, parsed.red, error) &&
			parseTransferCurveArray(cleaned, "TARGET_G", bucketCount, parsed.green, error) &&
			parseTransferCurveArray(cleaned, "TARGET_B", bucketCount, parsed.blue, error) &&
			parseTransferCurveArray(cleaned, "TARGET_W", bucketCount, parsed.white, error);
	}

	bool extractCalibrationHeaderArrayBody(const QString& content, const QString& symbol, QString& body, uint32_t& declaredSize, QString& error)
	{
		const QRegularExpression declarationExpression(
			QString(R"(%1\s*\[\s*(\d+)\s*\](?:\s+PROGMEM)?\s*=\s*\{)").arg(QRegularExpression::escape(symbol)));
		const QRegularExpressionMatch declarationMatch = declarationExpression.match(content);
		if (!declarationMatch.hasMatch())
		{
			error = QString("Missing required array '%1'").arg(symbol);
			return false;
		}

		bool ok = false;
		declaredSize = declarationMatch.captured(1).toUInt(&ok);
		if (!ok || declaredSize == 0u)
		{
			error = QString("Array '%1' must declare a positive size").arg(symbol);
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

	bool parseCalibrationHeaderArray(const QString& content, const QString& symbol, uint32_t& bucketCount, QVector<uint16_t>& output, QString& error)
	{
		QString body;
		uint32_t declaredSize = 0u;
		if (!extractCalibrationHeaderArrayBody(content, symbol, body, declaredSize, error))
			return false;

		const QRegularExpression valueExpression(R"((?:0x[0-9a-fA-F]+)|(?:\d+))");
		QVector<QString> entries;
		entries.reserve(static_cast<qsizetype>(declaredSize));

		QRegularExpressionMatchIterator iterator = valueExpression.globalMatch(body);
		while (iterator.hasNext())
			entries.push_back(iterator.next().captured(0));

		if (entries.size() != static_cast<qsizetype>(declaredSize))
		{
			error = QString("Array '%1' contains %2 entries, expected %3")
				.arg(symbol)
				.arg(entries.size())
				.arg(declaredSize);
			return false;
		}

		if (bucketCount == 0u)
			bucketCount = declaredSize;
		else if (bucketCount != declaredSize)
		{
			error = QString("Array '%1' size %2 does not match expected size %3")
				.arg(symbol)
				.arg(declaredSize)
				.arg(bucketCount);
			return false;
		}

		output.clear();
		output.reserve(entries.size());
		for (const QString& rawEntry : entries)
		{
			bool ok = false;
			const uint value = rawEntry.trimmed().toUInt(&ok, 0);
			if (!ok || value > 65535u)
			{
				error = QString("Array '%1' contains an invalid 16-bit value: %2").arg(symbol, rawEntry.trimmed());
				return false;
			}
			output.push_back(static_cast<uint16_t>(value));
		}

		return true;
	}

	bool parseCalibrationHeader(const QString& content, ParsedCalibrationHeaderProfile& parsed, QString& error)
	{
		const QString cleaned = stripCalibrationHeaderComments(content);

		parsed = ParsedCalibrationHeaderProfile{};
		return parseCalibrationHeaderArray(cleaned, "LUT_R_16_TO_16", parsed.bucketCount, parsed.red, error) &&
			parseCalibrationHeaderArray(cleaned, "LUT_G_16_TO_16", parsed.bucketCount, parsed.green, error) &&
			parseCalibrationHeaderArray(cleaned, "LUT_B_16_TO_16", parsed.bucketCount, parsed.blue, error) &&
			parseCalibrationHeaderArray(cleaned, "LUT_W_16_TO_16", parsed.bucketCount, parsed.white, error);
	}

	QJsonArray transferCurveChannelToJson(const QVector<uint16_t>& values)
	{
		QJsonArray output;
		for (uint16_t value : values)
			output.append(static_cast<int>(value));
		return output;
	}

	QJsonArray rgbwLutChannelToJson(const QVector<uint16_t>& values)
	{
		QJsonArray output;
		for (uint16_t value : values)
			output.append(static_cast<int>(value));
		return output;
	}

	bool extractRgbwLutHeaderArrayBody(const QString& content, const QString& symbol, QString& body, uint32_t& declaredSize, QString& error)
	{
		const QRegularExpression declarationExpression(
			QString(R"(%1\s*\[\s*(\d+)\s*\](?:\s+PROGMEM)?\s*=\s*\{)").arg(QRegularExpression::escape(symbol)));
		const QRegularExpressionMatch declarationMatch = declarationExpression.match(content);
		if (!declarationMatch.hasMatch())
		{
			error = QString("Missing required array '%1'").arg(symbol);
			return false;
		}

		bool ok = false;
		declaredSize = declarationMatch.captured(1).toUInt(&ok);
		if (!ok || declaredSize == 0u)
		{
			error = QString("Array '%1' must declare a positive size").arg(symbol);
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

	bool parseRgbwLutHeaderArray(const QString& content, const QString& symbol, uint32_t expectedSize, QVector<uint16_t>& output, QString& error)
	{
		QString body;
		uint32_t declaredSize = 0u;
		if (!extractRgbwLutHeaderArrayBody(content, symbol, body, declaredSize, error))
			return false;

		if (declaredSize != expectedSize)
		{
			error = QString("Array '%1' size %2 does not match expected size %3").arg(symbol).arg(declaredSize).arg(expectedSize);
			return false;
		}

		const QRegularExpression valueExpression(R"((?:0x[0-9a-fA-F]+)|(?:\d+))");
		QVector<QString> entries;
		entries.reserve(static_cast<qsizetype>(declaredSize));
		QRegularExpressionMatchIterator iterator = valueExpression.globalMatch(body);
		while (iterator.hasNext())
			entries.push_back(iterator.next().captured(0));

		if (entries.size() != static_cast<qsizetype>(declaredSize))
		{
			error = QString("Array '%1' contains %2 entries, expected %3").arg(symbol).arg(entries.size()).arg(declaredSize);
			return false;
		}

		output.clear();
		output.reserve(entries.size());
		for (const QString& rawEntry : entries)
		{
			bool ok = false;
			const uint value = rawEntry.trimmed().toUInt(&ok, 0);
			if (!ok || value > 65535u)
			{
				error = QString("Array '%1' contains an invalid 16-bit value: %2").arg(symbol, rawEntry.trimmed());
				return false;
			}
			output.push_back(static_cast<uint16_t>(value));
		}

		return true;
	}

	bool extractRgbwLutHeaderUInt(const QString& content, const QString& symbol, uint32_t& value)
	{
		const QRegularExpression expression(QString(R"(\b%1\s*=\s*((?:0x[0-9a-fA-F]+)|(?:\d+))\s*;)").arg(QRegularExpression::escape(symbol)));
		const QRegularExpressionMatch match = expression.match(content);
		if (!match.hasMatch())
			return false;

		bool ok = false;
		const uint parsed = match.captured(1).toUInt(&ok, 0);
		if (!ok)
			return false;

		value = parsed;
		return true;
	}

	bool parseRgbwLutHeader(const QString& content, ParsedRgbwLutHeaderProfile& parsed, QString& error)
	{
		const QString cleaned = stripRgbwLutHeaderComments(content);
		uint32_t gridSize = 0u;
		uint32_t entryCount = 0u;
		if (!extractRgbwLutHeaderUInt(cleaned, "RGBW_LUT_GRID_SIZE", gridSize) || gridSize < 2u)
		{
			error = "Missing or invalid RGBW_LUT_GRID_SIZE declaration";
			return false;
		}
		if (!extractRgbwLutHeaderUInt(cleaned, "RGBW_LUT_ENTRY_COUNT", entryCount) || entryCount == 0u)
		{
			error = "Missing or invalid RGBW_LUT_ENTRY_COUNT declaration";
			return false;
		}

		const uint64_t expectedEntryCount = static_cast<uint64_t>(gridSize) * static_cast<uint64_t>(gridSize) * static_cast<uint64_t>(gridSize);
		if (expectedEntryCount != static_cast<uint64_t>(entryCount))
		{
			error = QString("RGBW_LUT_ENTRY_COUNT %1 does not match RGBW_LUT_GRID_SIZE^3 (%2)").arg(entryCount).arg(expectedEntryCount);
			return false;
		}

		parsed = ParsedRgbwLutHeaderProfile{};
		parsed.gridSize = gridSize;
		parsed.entryCount = entryCount;
		parsed.sourceGridSize = gridSize;
		extractRgbwLutHeaderUInt(cleaned, "RGBW_LUT_SOURCE_GRID_SIZE", parsed.sourceGridSize);
		extractRgbwLutHeaderUInt(cleaned, "RGBW_LUT_AXIS_MIN", parsed.axisMin);
		extractRgbwLutHeaderUInt(cleaned, "RGBW_LUT_AXIS_MAX", parsed.axisMax);
		uint32_t requiresInterpolation = 1u;
		if (extractRgbwLutHeaderUInt(cleaned, "RGBW_LUT_REQUIRES_3D_INTERPOLATION", requiresInterpolation))
			parsed.requires3dInterpolation = (requiresInterpolation != 0u);

		if (parsed.axisMax <= parsed.axisMin)
		{
			error = "RGBW_LUT_AXIS_MAX must be greater than RGBW_LUT_AXIS_MIN";
			return false;
		}

		return parseRgbwLutHeaderArray(cleaned, "RGBW_LUT_R", parsed.entryCount, parsed.red, error) &&
			parseRgbwLutHeaderArray(cleaned, "RGBW_LUT_G", parsed.entryCount, parsed.green, error) &&
			parseRgbwLutHeaderArray(cleaned, "RGBW_LUT_B", parsed.entryCount, parsed.blue, error) &&
			parseRgbwLutHeaderArray(cleaned, "RGBW_LUT_W", parsed.entryCount, parsed.white, error);
	}

	QJsonObject buildTransferHeadersResponse(const QString& rootPath, const LoggerName& log, const QJsonObject& currentConfig = QJsonObject())
	{
		QJsonObject response;
		const QString directoryPath = transferHeadersDirectoryPath(rootPath);
		response["path"] = directoryPath;
		response["headers"] = QJsonArray();
		if (!currentConfig.isEmpty())
			response["active"] = buildTransferHeadersActiveState(rootPath, currentConfig);

		QDir directory(directoryPath);
		if (!directory.exists())
			return response;

		QJsonArray headers;
		const QFileInfoList profileFiles = directory.entryInfoList({ "*.json" }, QDir::Files, QDir::Name);
		for (const QFileInfo& fileInfo : profileFiles)
		{
			QFile file(fileInfo.absoluteFilePath());
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				Warning(log, "Could not open transfer header profile: {:s}", fileInfo.absoluteFilePath());
				continue;
			}

			const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
			if (!document.isObject())
			{
				Warning(log, "Ignoring invalid transfer header profile: {:s}", fileInfo.absoluteFilePath());
				continue;
			}

			const QJsonObject object = document.object();
			const QString slug = object["slug"].toString(fileInfo.baseName());
			QJsonObject item;
			item["id"] = QString("custom:%1").arg(slug);
			item["slug"] = slug;
			item["name"] = object["name"].toString(slug);
			item["bucketCount"] = object["bucketCount"].toInt();
			item["modifiedDate"] = fileInfo.lastModified().toMSecsSinceEpoch();
			item["headerPath"] = QDir::cleanPath(directory.filePath(slug + ".h"));
			item["jsonPath"] = fileInfo.absoluteFilePath();
			headers.append(item);
		}

		response["headers"] = headers;
		return response;
	}

	QJsonObject buildCalibrationHeadersResponse(const QString& rootPath, const LoggerName& log, const QJsonObject& currentConfig = QJsonObject())
	{
		QJsonObject response;
		const QString directoryPath = calibrationHeadersDirectoryPath(rootPath);
		response["path"] = directoryPath;
		response["headers"] = QJsonArray();
		if (!currentConfig.isEmpty())
			response["active"] = buildCalibrationHeadersActiveState(rootPath, currentConfig);

		QDir directory(directoryPath);
		if (!directory.exists())
			return response;

		QJsonArray headers;
		const QFileInfoList profileFiles = directory.entryInfoList({ "*.json" }, QDir::Files, QDir::Name);
		for (const QFileInfo& fileInfo : profileFiles)
		{
			QFile file(fileInfo.absoluteFilePath());
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				Warning(log, "Could not open calibration header profile: {:s}", fileInfo.absoluteFilePath());
				continue;
			}

			const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
			if (!document.isObject())
			{
				Warning(log, "Ignoring invalid calibration header profile: {:s}", fileInfo.absoluteFilePath());
				continue;
			}

			const QJsonObject object = document.object();
			const QString slug = object["slug"].toString(fileInfo.baseName());
			QJsonObject item;
			item["id"] = QString("custom:%1").arg(slug);
			item["slug"] = slug;
			item["name"] = object["name"].toString(slug);
			item["bucketCount"] = object["bucketCount"].toInt();
			item["modifiedDate"] = fileInfo.lastModified().toMSecsSinceEpoch();
			item["headerPath"] = QDir::cleanPath(directory.filePath(slug + ".h"));
			item["jsonPath"] = fileInfo.absoluteFilePath();
			headers.append(item);
		}

		response["headers"] = headers;
		return response;
	}

	QJsonObject buildRgbwLutHeadersResponse(const QString& rootPath, const LoggerName& log, const QJsonObject& currentConfig = QJsonObject())
	{
		QJsonObject response;
		const QString directoryPath = rgbwLutHeadersDirectoryPath(rootPath);
		response["path"] = directoryPath;
		response["headers"] = QJsonArray();
		if (!currentConfig.isEmpty())
			response["active"] = buildRgbwLutHeadersActiveState(rootPath, currentConfig);

		QDir directory(directoryPath);
		if (!directory.exists())
			return response;

		QJsonArray headers;
		const QFileInfoList profileFiles = directory.entryInfoList({ "*.json" }, QDir::Files, QDir::Name);
		for (const QFileInfo& fileInfo : profileFiles)
		{
			QFile file(fileInfo.absoluteFilePath());
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				Warning(log, "Could not open RGBW LUT header profile: {:s}", fileInfo.absoluteFilePath());
				continue;
			}

			const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
			if (!document.isObject())
			{
				Warning(log, "Ignoring invalid RGBW LUT header profile: {:s}", fileInfo.absoluteFilePath());
				continue;
			}

			const QJsonObject object = document.object();
			const QString slug = object["slug"].toString(fileInfo.baseName());
			QJsonObject item;
			item["id"] = QString("custom:%1").arg(slug);
			item["slug"] = slug;
			item["name"] = object["name"].toString(slug);
			item["sourceGridSize"] = object["sourceGridSize"].toInt();
			item["gridSize"] = object["gridSize"].toInt();
			item["entryCount"] = object["entryCount"].toInt();
			item["axisMin"] = object["axisMin"].toInt();
			item["axisMax"] = object["axisMax"].toInt(65535);
			item["modifiedDate"] = fileInfo.lastModified().toMSecsSinceEpoch();
			item["headerPath"] = QDir::cleanPath(directory.filePath(slug + ".h"));
			item["jsonPath"] = fileInfo.absoluteFilePath();
			headers.append(item);
		}

		response["headers"] = headers;
		return response;
	}

	QJsonArray solverProfileChannelToJson(const QVector<ParsedSolverProfileEntry>& values)
	{
		QJsonArray output;
		for (const ParsedSolverProfileEntry& entry : values)
		{
			QJsonObject item;
			item["mode"] = entry.mode.isEmpty() ? QString("blend8") : entry.mode;
			item["lower_value"] = static_cast<int>(entry.lowerValue);
			item["upper_value"] = static_cast<int>(entry.upperValue);
			item["value"] = static_cast<int>(entry.value);
			item["bfi"] = static_cast<int>(entry.bfi);
			item["output_q16"] = static_cast<int>(entry.outputQ16);
			output.append(item);
		}
		return output;
	}

	bool extractSolverProfileArrayBody(const QString& content, const QString& symbol, QString& body, QString& error)
	{
		const QRegularExpression declarationExpression(
			QString(R"(%1\s*\[\s*(?:\d+)?\s*\](?:\s+PROGMEM)?\s*=\s*\{)").arg(QRegularExpression::escape(symbol)));
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

	bool parseSolverProfileHeaderLadderArray(const QString& content, const QString& symbol, QVector<ParsedSolverProfileEntry>& output, QString& error)
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
			if (!outputOk || !valueOk || !bfiOk || outputQ16 > 65535u || value > 255u || bfi > 4u)
			{
				error = QString("Array '%1' contains an invalid ladder entry").arg(symbol);
				output.clear();
				return false;
			}

			ParsedSolverProfileEntry parsedEntry;
			parsedEntry.outputQ16 = static_cast<uint16_t>(outputQ16);
			parsedEntry.value = static_cast<uint8_t>(value);
			parsedEntry.bfi = static_cast<uint8_t>(bfi);
			parsedEntry.lowerValue = static_cast<uint8_t>(value);
			parsedEntry.upperValue = static_cast<uint8_t>(value);
			parsedEntry.mode = "fill8";
			output.push_back(parsedEntry);
		}

		if (output.isEmpty())
		{
			error = QString("Array '%1' must contain at least one ladder entry").arg(symbol);
			return false;
		}

		return true;
	}

	bool parseSolverProfileHeaderBoundArray(const QString& content, const QString& symbol, QVector<ParsedSolverProfileEntry>& target, const bool lowerBound, QString& error)
	{
		QString body;
		if (!extractSolverProfileArrayBody(content, symbol, body, error))
			return false;

		const QRegularExpression valueExpression(R"((?:0x[0-9a-fA-F]+)|(?:\d+))");
		QRegularExpressionMatchIterator iterator = valueExpression.globalMatch(body);
		QVector<uint8_t> values;
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

		for (qsizetype index = 0; index < target.size(); ++index)
		{
			if (lowerBound)
				target[index].lowerValue = values[index];
			else
				target[index].upperValue = values[index];
			target[index].mode = (target[index].lowerValue < target[index].value) ? "blend8" : "fill8";
		}

		return true;
	}

	bool parseSolverProfileEntries(const QJsonArray& source, QVector<ParsedSolverProfileEntry>& output, QString& error, const QString& label)
	{
		output.clear();
		output.reserve(source.size());
		for (const QJsonValue& entryValue : source)
		{
			if (!entryValue.isObject())
			{
				error = QString("Solver profile channel '%1' must contain objects").arg(label);
				return false;
			}

			const QJsonObject entry = entryValue.toObject();
			const int outputQ16 = entry.contains("output_q16") ? entry["output_q16"].toInt(-1) : entry["outputQ16"].toInt(-1);
			const int value = entry.contains("value") ? entry["value"].toInt(-1) : entry["upper_value"].toInt(-1);
			const int bfi = entry["bfi"].toInt(-1);
			const int lowerValue = entry.contains("lower_value") ? entry["lower_value"].toInt(value) : value;
			const int upperValue = entry.contains("upper_value") ? entry["upper_value"].toInt(value) : value;
			if (outputQ16 < 0 || outputQ16 > 65535 || value < 0 || value > 255 || bfi < 0 || bfi > 4 || lowerValue < 0 || lowerValue > 255 || upperValue < 0 || upperValue > 255)
			{
				error = QString("Solver profile channel '%1' contains out-of-range values").arg(label);
				return false;
			}

			ParsedSolverProfileEntry parsed;
			parsed.outputQ16 = static_cast<uint16_t>(outputQ16);
			parsed.value = static_cast<uint8_t>(value);
			parsed.bfi = static_cast<uint8_t>(bfi);
			parsed.lowerValue = static_cast<uint8_t>(lowerValue);
			parsed.upperValue = static_cast<uint8_t>(upperValue);
			parsed.mode = entry["mode"].toString((lowerValue < value) ? "blend8" : "fill8");
			output.push_back(parsed);
		}

		std::sort(output.begin(), output.end(), [](const ParsedSolverProfileEntry& left, const ParsedSolverProfileEntry& right) {
			if (left.outputQ16 != right.outputQ16)
				return left.outputQ16 < right.outputQ16;
			if (left.bfi != right.bfi)
				return left.bfi > right.bfi;
			return left.value < right.value;
		});

		if (output.isEmpty())
		{
			error = QString("Solver profile channel '%1' must contain at least one state").arg(label);
			return false;
		}

		return true;
	}

	bool parseSolverProfileJson(const QString& content, ParsedSolverProfile& parsed, QString& error)
	{
		const QJsonDocument document = QJsonDocument::fromJson(content.toUtf8());
		if (!document.isObject())
		{
			error = "Solver profile content must be a JSON object";
			return false;
		}

		const QJsonObject object = document.object();
		const QJsonObject channels = object["channels"].toObject();
		if (channels.isEmpty())
		{
			error = "Solver profile JSON must contain a channels object";
			return false;
		}

		parsed = ParsedSolverProfile{};
		return parseSolverProfileEntries(channels["red"].toArray(), parsed.channels[1], error, "red") &&
			parseSolverProfileEntries(channels["green"].toArray(), parsed.channels[0], error, "green") &&
			parseSolverProfileEntries(channels["blue"].toArray(), parsed.channels[2], error, "blue") &&
			parseSolverProfileEntries(channels["white"].toArray(), parsed.channels[3], error, "white");
	}

	bool parseSolverProfileHeader(const QString& content, ParsedSolverProfile& parsed, QString& error)
	{
		const QString cleaned = stripSolverProfileComments(content);
		parsed = ParsedSolverProfile{};

		if (!parseSolverProfileHeaderLadderArray(cleaned, "LADDER_R", parsed.channels[1], error) ||
			!parseSolverProfileHeaderLadderArray(cleaned, "LADDER_G", parsed.channels[0], error) ||
			!parseSolverProfileHeaderLadderArray(cleaned, "LADDER_B", parsed.channels[2], error) ||
			!parseSolverProfileHeaderLadderArray(cleaned, "LADDER_W", parsed.channels[3], error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_R_LOWER", parsed.channels[1], true, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_G_LOWER", parsed.channels[0], true, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_B_LOWER", parsed.channels[2], true, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_W_LOWER", parsed.channels[3], true, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_R_UPPER", parsed.channels[1], false, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_G_UPPER", parsed.channels[0], false, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_B_UPPER", parsed.channels[2], false, error) ||
			!parseSolverProfileHeaderBoundArray(cleaned, "LADDER_W_UPPER", parsed.channels[3], false, error))
		{
			return false;
		}

		for (auto& channelEntries : parsed.channels)
		{
			std::sort(channelEntries.begin(), channelEntries.end(), [](const ParsedSolverProfileEntry& left, const ParsedSolverProfileEntry& right) {
				if (left.outputQ16 != right.outputQ16)
					return left.outputQ16 < right.outputQ16;
				if (left.bfi != right.bfi)
					return left.bfi > right.bfi;
				return left.value < right.value;
			});
		}

		return true;
	}

	bool parseSolverProfile(const QString& content, ParsedSolverProfile& parsed, QString& error, bool* sourceIsHeader = nullptr)
	{
		const QString trimmed = content.trimmed();
		if (trimmed.isEmpty())
		{
			error = "Solver profile content is required";
			return false;
		}

		if (sourceIsHeader != nullptr)
			*sourceIsHeader = false;

		if (trimmed.startsWith('{') || trimmed.startsWith('['))
			return parseSolverProfileJson(content, parsed, error);

		if (sourceIsHeader != nullptr)
			*sourceIsHeader = true;
		return parseSolverProfileHeader(content, parsed, error);
	}

	QJsonObject buildSolverProfilesResponse(const QString& rootPath, const LoggerName& log, const QJsonObject& currentConfig = QJsonObject())
	{
		QJsonObject response;
		const QString directoryPath = solverProfilesDirectoryPath(rootPath);
		response["path"] = directoryPath;
		response["profiles"] = QJsonArray();
		if (!currentConfig.isEmpty())
			response["active"] = buildSolverProfilesActiveState(rootPath, currentConfig);

		QDir directory(directoryPath);
		if (!directory.exists())
			return response;

		QJsonArray profiles;
		const QFileInfoList profileFiles = directory.entryInfoList({ "*.json" }, QDir::Files, QDir::Name);
		for (const QFileInfo& fileInfo : profileFiles)
		{
			QFile file(fileInfo.absoluteFilePath());
			if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				Warning(log, "Could not open solver profile: {:s}", fileInfo.absoluteFilePath());
				continue;
			}

			const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
			if (!document.isObject())
			{
				Warning(log, "Ignoring invalid solver profile: {:s}", fileInfo.absoluteFilePath());
				continue;
			}

			const QJsonObject object = document.object();
			const QString slug = object["slug"].toString(fileInfo.baseName());
			const QJsonObject channels = object["channels"].toObject();
			QJsonObject item;
			item["id"] = QString("custom:%1").arg(slug);
			item["slug"] = slug;
			item["name"] = object["name"].toString(slug);
			item["stateCount"] = channels["red"].toArray().size();
			item["modifiedDate"] = fileInfo.lastModified().toMSecsSinceEpoch();
			const QString headerPath = directory.filePath(slug + ".h");
			if (QFileInfo::exists(headerPath))
				item["headerPath"] = headerPath;
			item["sourceFormat"] = object["sourceFormat"].toString(QFileInfo::exists(headerPath) ? "header" : "json");
			item["jsonPath"] = fileInfo.absoluteFilePath();
			profiles.append(item);
		}

		QJsonObject builtin;
		builtin["id"] = "builtin";
		builtin["slug"] = "builtin";
		builtin["name"] = "Built-in";
		builtin["stateCount"] = 0;
		profiles.prepend(builtin);
		response["profiles"] = profiles;
		return response;
	}

	bool saveSolverProfile(const QString& rootPath, const QString& name, const QString& content, QJsonObject& response, QString& error, const LoggerName& log)
	{
		const QString trimmedName = name.trimmed();
		if (trimmedName.isEmpty())
		{
			error = "Solver profile name is required";
			return false;
		}
		if (content.trimmed().isEmpty())
		{
			error = "Solver profile content is required";
			return false;
		}

		const QString slug = sanitizeSolverProfileSlug(trimmedName);
		if (slug.isEmpty())
		{
			error = "Solver profile name must contain at least one letter or number";
			return false;
		}

		ParsedSolverProfile parsed;
		bool sourceIsHeader = false;
		if (!parseSolverProfile(content, parsed, error, &sourceIsHeader))
			return false;

		QDir rootDirectory(rootPath);
		const QString relativeDirectory = "solver_profiles";
		if (!rootDirectory.mkpath(relativeDirectory))
		{
			error = QString("Could not create solver profile directory: %1").arg(rootDirectory.filePath(relativeDirectory));
			return false;
		}

		const QString directoryPath = solverProfilesDirectoryPath(rootPath);
		const QString headerPath = QDir(directoryPath).filePath(slug + ".h");
		const QString jsonPath = QDir(directoryPath).filePath(slug + ".json");

		QJsonObject jsonProfile;
		jsonProfile["name"] = trimmedName;
		jsonProfile["slug"] = slug;
		jsonProfile["sourceFormat"] = sourceIsHeader ? "header" : "json";
		jsonProfile["channels"] = QJsonObject{
			{ "red", solverProfileChannelToJson(parsed.channels[1]) },
			{ "green", solverProfileChannelToJson(parsed.channels[0]) },
			{ "blue", solverProfileChannelToJson(parsed.channels[2]) },
			{ "white", solverProfileChannelToJson(parsed.channels[3]) },
		};

		if (sourceIsHeader)
		{
			const QString savedContent = neutralizeProgmem(content);
			if (!FileUtils::writeFile(headerPath, savedContent.toUtf8(), log))
			{
				error = QString("Could not write solver profile header: %1").arg(headerPath);
				return false;
			}
		}
		else if (QFileInfo::exists(headerPath) && !QFile::remove(headerPath))
		{
			Warning(log, "Could not remove stale solver profile header: {:s}", headerPath);
		}

		if (!FileUtils::writeFile(jsonPath, QJsonDocument(jsonProfile).toJson(QJsonDocument::Compact), log))
		{
			error = QString("Could not write solver profile: %1").arg(jsonPath);
			return false;
		}

		response = buildSolverProfilesResponse(rootPath, log);
		response["saved"] = QString("custom:%1").arg(slug);
		return true;
	}

	bool deleteSolverProfile(const QString& rootPath, const QString& profile, QJsonObject& response, QString& error, const LoggerName& log)
	{
		QString slug = profile.trimmed();
		if (slug.compare("builtin", Qt::CaseInsensitive) == 0)
		{
			error = "The built-in solver profile cannot be deleted";
			return false;
		}
		if (slug.startsWith("custom:", Qt::CaseInsensitive))
			slug.remove(0, QString("custom:").size());
		slug = sanitizeSolverProfileSlug(slug);
		if (slug.isEmpty())
		{
			error = "A custom solver profile id is required";
			return false;
		}

		const QDir directory(solverProfilesDirectoryPath(rootPath));
		const QString headerPath = directory.filePath(slug + ".h");
		const QString jsonPath = directory.filePath(slug + ".json");
		if (!QFileInfo::exists(jsonPath) && !QFileInfo::exists(headerPath))
		{
			error = QString("Solver profile not found: custom:%1").arg(slug);
			return false;
		}

		if (QFileInfo::exists(jsonPath) && !QFile::remove(jsonPath) && QFileInfo::exists(jsonPath))
		{
			error = QString("Could not remove solver profile: %1").arg(jsonPath);
			return false;
		}
		if (QFileInfo::exists(headerPath) && !QFile::remove(headerPath))
		{
			Warning(log, "Could not remove stored solver header source: {:s}", headerPath);
		}

		response = buildSolverProfilesResponse(rootPath, log);
		response["deleted"] = QString("custom:%1").arg(slug);
		return true;
	}

	bool saveTransferHeaderProfile(
		const QString& rootPath,
		const QString& name,
		const QString& content,
		QJsonObject& response,
		QString& error,
		const LoggerName& log)
	{
		const QString trimmedName = name.trimmed();
		if (trimmedName.isEmpty())
		{
			error = "Transfer header name is required";
			return false;
		}
		if (content.trimmed().isEmpty())
		{
			error = "Transfer header content is required";
			return false;
		}

		const QString slug = sanitizeTransferCurveSlug(trimmedName);
		if (slug.isEmpty())
		{
			error = "Transfer header name must contain at least one letter or number";
			return false;
		}

		ParsedTransferCurveProfile parsed;
		if (!parseTransferCurveHeader(content, parsed, error))
			return false;

		QDir rootDirectory(rootPath);
		const QString relativeDirectory = "transfer_headers";
		if (!rootDirectory.mkpath(relativeDirectory))
		{
			error = QString("Could not create transfer header directory: %1").arg(rootDirectory.filePath(relativeDirectory));
			return false;
		}

		const QString directoryPath = transferHeadersDirectoryPath(rootPath);
		const QString headerPath = QDir(directoryPath).filePath(slug + ".h");
		const QString jsonPath = QDir(directoryPath).filePath(slug + ".json");

		QJsonObject jsonProfile;
		jsonProfile["name"] = trimmedName;
		jsonProfile["slug"] = slug;
		jsonProfile["bucketCount"] = static_cast<int>(parsed.bucketCount);
		jsonProfile["channels"] = QJsonObject{
			{ "red", transferCurveChannelToJson(parsed.red) },
			{ "green", transferCurveChannelToJson(parsed.green) },
			{ "blue", transferCurveChannelToJson(parsed.blue) },
			{ "white", transferCurveChannelToJson(parsed.white) },
		};

		const QString savedContent = neutralizeProgmem(content);
		if (!FileUtils::writeFile(headerPath, savedContent.toUtf8(), log))
		{
			error = QString("Could not write transfer header file: %1").arg(headerPath);
			return false;
		}
		if (!FileUtils::writeFile(jsonPath, QJsonDocument(jsonProfile).toJson(QJsonDocument::Compact), log))
		{
			error = QString("Could not write transfer header profile: %1").arg(jsonPath);
			return false;
		}

		response = buildTransferHeadersResponse(rootPath, log);
		response["saved"] = QString("custom:%1").arg(slug);
		return true;
	}

	bool deleteTransferHeaderProfile(const QString& rootPath, const QString& profile, QJsonObject& response, QString& error, const LoggerName& log)
	{
		QString slug = profile.trimmed();
		if (slug.startsWith("custom:", Qt::CaseInsensitive))
			slug.remove(0, QString("custom:").size());
		slug = sanitizeTransferCurveSlug(slug);
		if (slug.isEmpty())
		{
			error = "A custom transfer header profile id is required";
			return false;
		}

		const QDir directory(transferHeadersDirectoryPath(rootPath));
		const QString headerPath = directory.filePath(slug + ".h");
		const QString jsonPath = directory.filePath(slug + ".json");
		if (!QFileInfo::exists(jsonPath) && !QFileInfo::exists(headerPath))
		{
			error = QString("Transfer header profile not found: custom:%1").arg(slug);
			return false;
		}

		bool removedAnything = false;
		if (QFileInfo::exists(jsonPath))
		{
			removedAnything = QFile::remove(jsonPath) || removedAnything;
			if (!removedAnything && QFileInfo::exists(jsonPath))
			{
				error = QString("Could not remove transfer header profile: %1").arg(jsonPath);
				return false;
			}
		}
		if (QFileInfo::exists(headerPath) && !QFile::remove(headerPath))
		{
			Warning(log, "Could not remove stored transfer header source: {:s}", headerPath);
		}

		response = buildTransferHeadersResponse(rootPath, log);
		response["deleted"] = QString("custom:%1").arg(slug);
		return true;
	}

	bool saveCalibrationHeaderProfile(
		const QString& rootPath,
		const QString& name,
		const QString& content,
		QJsonObject& response,
		QString& error,
		const LoggerName& log)
	{
		const QString trimmedName = name.trimmed();
		if (trimmedName.isEmpty())
		{
			error = "Calibration header name is required";
			return false;
		}
		if (content.trimmed().isEmpty())
		{
			error = "Calibration header content is required";
			return false;
		}

		const QString slug = sanitizeCalibrationHeaderSlug(trimmedName);
		if (slug.isEmpty())
		{
			error = "Calibration header name must contain at least one letter or number";
			return false;
		}

		ParsedCalibrationHeaderProfile parsed;
		if (!parseCalibrationHeader(content, parsed, error))
			return false;

		QDir rootDirectory(rootPath);
		const QString relativeDirectory = "calibration_headers";
		if (!rootDirectory.mkpath(relativeDirectory))
		{
			error = QString("Could not create calibration header directory: %1").arg(rootDirectory.filePath(relativeDirectory));
			return false;
		}

		const QString directoryPath = calibrationHeadersDirectoryPath(rootPath);
		const QString headerPath = QDir(directoryPath).filePath(slug + ".h");
		const QString jsonPath = QDir(directoryPath).filePath(slug + ".json");

		QJsonObject jsonProfile;
		jsonProfile["name"] = trimmedName;
		jsonProfile["slug"] = slug;
		jsonProfile["bucketCount"] = static_cast<int>(parsed.bucketCount);
		jsonProfile["channels"] = QJsonObject{
			{ "red", transferCurveChannelToJson(parsed.red) },
			{ "green", transferCurveChannelToJson(parsed.green) },
			{ "blue", transferCurveChannelToJson(parsed.blue) },
			{ "white", transferCurveChannelToJson(parsed.white) },
		};

		const QString savedContent = neutralizeProgmem(content);
		if (!FileUtils::writeFile(headerPath, savedContent.toUtf8(), log))
		{
			error = QString("Could not write calibration header file: %1").arg(headerPath);
			return false;
		}
		if (!FileUtils::writeFile(jsonPath, QJsonDocument(jsonProfile).toJson(QJsonDocument::Compact), log))
		{
			error = QString("Could not write calibration header profile: %1").arg(jsonPath);
			return false;
		}

		response = buildCalibrationHeadersResponse(rootPath, log);
		response["saved"] = QString("custom:%1").arg(slug);
		return true;
	}

	bool deleteCalibrationHeaderProfile(const QString& rootPath, const QString& profile, QJsonObject& response, QString& error, const LoggerName& log)
	{
		QString slug = profile.trimmed();
		if (slug.startsWith("custom:", Qt::CaseInsensitive))
			slug.remove(0, QString("custom:").size());
		slug = sanitizeCalibrationHeaderSlug(slug);
		if (slug.isEmpty())
		{
			error = "A custom calibration header profile id is required";
			return false;
		}

		const QDir directory(calibrationHeadersDirectoryPath(rootPath));
		const QString headerPath = directory.filePath(slug + ".h");
		const QString jsonPath = directory.filePath(slug + ".json");
		if (!QFileInfo::exists(jsonPath) && !QFileInfo::exists(headerPath))
		{
			error = QString("Calibration header profile not found: custom:%1").arg(slug);
			return false;
		}

		bool removedAnything = false;
		if (QFileInfo::exists(jsonPath))
		{
			removedAnything = QFile::remove(jsonPath) || removedAnything;
			if (!removedAnything && QFileInfo::exists(jsonPath))
			{
				error = QString("Could not remove calibration header profile: %1").arg(jsonPath);
				return false;
			}
		}
		if (QFileInfo::exists(headerPath) && !QFile::remove(headerPath))
		{
			Warning(log, "Could not remove stored calibration header source: {:s}", headerPath);
		}

		response = buildCalibrationHeadersResponse(rootPath, log);
		response["deleted"] = QString("custom:%1").arg(slug);
		return true;
	}

	bool saveRgbwLutHeaderProfile(
		const QString& rootPath,
		const QString& name,
		const QString& content,
		QJsonObject& response,
		QString& error,
		const LoggerName& log)
	{
		const QString trimmedName = name.trimmed();
		if (trimmedName.isEmpty())
		{
			error = "RGBW LUT header name is required";
			return false;
		}
		if (content.trimmed().isEmpty())
		{
			error = "RGBW LUT header content is required";
			return false;
		}

		const QString slug = sanitizeRgbwLutHeaderSlug(trimmedName);
		if (slug.isEmpty())
		{
			error = "RGBW LUT header name must contain at least one letter or number";
			return false;
		}

		ParsedRgbwLutHeaderProfile parsed;
		if (!parseRgbwLutHeader(content, parsed, error))
			return false;

		QDir rootDirectory(rootPath);
		const QString relativeDirectory = "rgbw_lut_headers";
		if (!rootDirectory.mkpath(relativeDirectory))
		{
			error = QString("Could not create RGBW LUT header directory: %1").arg(rootDirectory.filePath(relativeDirectory));
			return false;
		}

		const QString directoryPath = rgbwLutHeadersDirectoryPath(rootPath);
		const QString headerPath = QDir(directoryPath).filePath(slug + ".h");
		const QString jsonPath = QDir(directoryPath).filePath(slug + ".json");

		QJsonObject jsonProfile;
		jsonProfile["name"] = trimmedName;
		jsonProfile["slug"] = slug;
		jsonProfile["sourceGridSize"] = static_cast<int>(parsed.sourceGridSize);
		jsonProfile["gridSize"] = static_cast<int>(parsed.gridSize);
		jsonProfile["entryCount"] = static_cast<int>(parsed.entryCount);
		jsonProfile["axisMin"] = static_cast<int>(parsed.axisMin);
		jsonProfile["axisMax"] = static_cast<int>(parsed.axisMax);
		jsonProfile["requires3dInterpolation"] = parsed.requires3dInterpolation;
		jsonProfile["channels"] = QJsonObject{
			{ "red", rgbwLutChannelToJson(parsed.red) },
			{ "green", rgbwLutChannelToJson(parsed.green) },
			{ "blue", rgbwLutChannelToJson(parsed.blue) },
			{ "white", rgbwLutChannelToJson(parsed.white) }
		};

		const QString savedContent = neutralizeProgmem(content);
		if (!FileUtils::writeFile(headerPath, savedContent.toUtf8(), log))
		{
			error = QString("Could not write RGBW LUT header file: %1").arg(headerPath);
			return false;
		}
		if (!FileUtils::writeFile(jsonPath, QJsonDocument(jsonProfile).toJson(QJsonDocument::Compact), log))
		{
			error = QString("Could not write RGBW LUT header profile: %1").arg(jsonPath);
			return false;
		}

		response = buildRgbwLutHeadersResponse(rootPath, log);
		response["saved"] = QString("custom:%1").arg(slug);
		return true;
	}

	bool deleteRgbwLutHeaderProfile(const QString& rootPath, const QString& profile, QJsonObject& response, QString& error, const LoggerName& log)
	{
		QString slug = profile.trimmed();
		if (slug.startsWith("custom:", Qt::CaseInsensitive))
			slug.remove(0, QString("custom:").size());
		slug = sanitizeRgbwLutHeaderSlug(slug);
		if (slug.isEmpty())
		{
			error = "A custom RGBW LUT header profile id is required";
			return false;
		}

		const QDir directory(rgbwLutHeadersDirectoryPath(rootPath));
		const QString headerPath = directory.filePath(slug + ".h");
		const QString jsonPath = directory.filePath(slug + ".json");
		if (!QFileInfo::exists(jsonPath) && !QFileInfo::exists(headerPath))
		{
			error = QString("RGBW LUT header profile not found: custom:%1").arg(slug);
			return false;
		}

		bool removedAnything = false;
		if (QFileInfo::exists(jsonPath))
		{
			removedAnything = QFile::remove(jsonPath) || removedAnything;
			if (!removedAnything && QFileInfo::exists(jsonPath))
			{
				error = QString("Could not remove RGBW LUT header profile: %1").arg(jsonPath);
				return false;
			}
		}
		if (QFileInfo::exists(headerPath) && !QFile::remove(headerPath))
		{
			Warning(log, "Could not remove stored RGBW LUT header source: {:s}", headerPath);
		}

		response = buildRgbwLutHeadersResponse(rootPath, log);
		response["deleted"] = QString("custom:%1").arg(slug);
		return true;
	}
}

HyperAPI::HyperAPI(QString peerAddress, const LoggerName& log, bool localConnection, QObject* parent, bool noListener)
	: CallbackAPI(log, localConnection, parent)
{
	_noListener = noListener;
	_peerAddress = peerAddress;
	_streaming_logging_activated = false;
	_ledStreamTimer = new QTimer(this);
	_colorsStreamingInterval = 50;
	_lastSentImage = 0;

	connect(_ledStreamTimer, &QTimer::timeout, this, &HyperAPI::handleLedColorsTimer, Qt::UniqueConnection);

	Q_INIT_RESOURCE(JSONRPC_schemas);
}

void HyperAPI::handleMessage(const QString& messageString, const QString& httpAuthHeader)
{
	try
	{
		if (HyperHdrInstance::isTerminated())
			return;

		const QString ident = "JsonRpc@" + _peerAddress;
		QJsonObject message;
		// parse the message
		if (!JsonUtils::parse(ident, messageString, message, _log))
		{
			sendErrorReply("Errors during message parsing, please consult the HyperHDR Log.");
			return;
		}

		int tan = 0;
		if (message.value("tan") != QJsonValue::Undefined)
			tan = message["tan"].toInt();

		// check basic message
		if (!JsonUtils::validate(ident, message, ":schema", _log))
		{
			sendErrorReply("Errors during message validation, please consult the HyperHDR Log.", "" /*command*/, tan);
			return;
		}

		// check specific message
		const QString command = message["command"].toString();
		if (!JsonUtils::validate(ident, message, QString(":schema-%1").arg(command), _log))
		{
			sendErrorReply("Errors during specific message validation, please consult the HyperHDR Log", command, tan);
			return;
		}

		// client auth before everything else but not for http
		if (!_noListener && command == "authorize")
		{
			handleAuthorizeCommand(message, command, tan);
			return;
		}

		// check auth state
		if (!BaseAPI::isAuthorized())
		{
			bool authOk = false;
			const QString subcommand = message["subcommand"].toString().trimmed().toLower();

			if (_noListener)
			{
				QString cToken = httpAuthHeader.mid(5).trimmed();
				if (BaseAPI::isTokenAuthorized(cToken))
					authOk = true;
				else if (isAllowedLoopbackAutomationCommand(_noListener, _peerAddress, command, subcommand))
					authOk = true;
			}

			if (!authOk)
			{
				sendErrorReply("No Authorization", command, tan);
				return;
			}
		}

		bool isRunning = false;
		quint8 currentIndex = getCurrentInstanceIndex();

		SAFE_CALL_1_RET(_instanceManager.get(), IsInstanceRunning, bool, isRunning, quint8, currentIndex);
		if (_hyperhdr == nullptr || !isRunning)
		{
			sendErrorReply("Not ready", command, tan);
			return;
		}
		else
		{
			// switch over all possible commands and handle them
			if (command == "color")
				handleColorCommand(message, command, tan);
			else if (command == "image")
				handleImageCommand(message, command, tan);
			else if (command == "effect")
				handleEffectCommand(message, command, tan);
			else if (command == "sysinfo")
				handleSysInfoCommand(message, command, tan);
			else if (command == "serverinfo")
				handleServerInfoCommand(message, command, tan);
			else if (command == "clear")
				handleClearCommand(message, command, tan);
			else if (command == "adjustment")
				handleAdjustmentCommand(message, command, tan);
			else if (command == "sourceselect")
				handleSourceSelectCommand(message, command, tan);
			else if (command == "config")
				handleConfigCommand(message, command, tan);
			else if (command == "componentstate")
				handleComponentStateCommand(message, command, tan);
			else if (command == "ledcolors")
				handleLedColorsCommand(message, command, tan);
			else if (command == "logging")
				handleLoggingCommand(message, command, tan);
			else if (command == "processing")
				handleProcessingCommand(message, command, tan);
			else if (command == "videomodehdr")
				handleVideoModeHdrCommand(message, command, tan);
			else if (command == "lut-calibration")
				handleLutCalibrationCommand(message, command, tan);
			else if (command == "instance")
				handleInstanceCommand(message, command, tan);
			else if (command == "leddevice")
				handleLedDeviceCommand(message, command, tan);
			else if (command == "save-db")
				handleSaveDB(message, command, tan);
			else if (command == "load-db")
				handleLoadDB(message, command, tan);
			else if (command == "tunnel")
				handleTunnel(message, command, tan);
			else if (command == "signal-calibration")
				handleLoadSignalCalibration(message, command, tan);
			else if (command == "performance-counters")
				handlePerformanceCounters(message, command, tan);
			else if (command == "clearall")
				handleClearallCommand(message, command, tan);
			else if (command == "help")
				handleHelpCommand(message, command, tan);
			else if (command == "video-crop")
				handleCropCommand(message, command, tan);
			else if (command == "video-controls")
				handleVideoControlsCommand(message, command, tan);
			else if (command == "benchmark")
				handleBenchmarkCommand(message, command, tan);
			else if (command == "lut-install")
				handleLutInstallCommand(message, command, tan);
			else if (command == "lut-switching")
				handleLutSwitchingCommand(message, command, tan);
			else if (command == "transfer-headers")
				handleTransferHeadersCommand(message, command, tan);
			else if (command == "calibration-headers")
				handleCalibrationHeadersCommand(message, command, tan);
			else if (command == "rgbw-lut-headers")
				handleRgbwLutHeadersCommand(message, command, tan);
			else if (command == "solver-profiles")
				handleSolverProfilesCommand(message, command, tan);
			else if (command == "smoothing")
				handleSmoothingCommand(message, command, tan);
			else if (command == "current-state")
				handleCurrentStateCommand(message, command, tan);
			else if (command == "videograbber-revive")
				handleVideoGrabberReviveCommand(message, command, tan);
			// handle not implemented commands
			else
				handleNotImplemented(command, tan);
		}
	}
	catch (...)
	{
		sendErrorReply("Exception");
	}
}

void HyperAPI::initialize()
{
	// init API, REQUIRED!
	BaseAPI::init();

	// setup auth interface
	connect(this, &BaseAPI::SignalPendingTokenClientNotification, this, &HyperAPI::newPendingTokenRequest);
	connect(this, &BaseAPI::SignalTokenClientNotification, this, &HyperAPI::handleTokenResponse);

	// listen for killed instances
	connect(_instanceManager.get(), &HyperHdrManager::SignalInstanceStateChanged, this, &HyperAPI::handleInstanceStateChange);

	// pipe callbacks from subscriptions to parent
	connect(this, &CallbackAPI::SignalCallbackToClient, this, &HyperAPI::SignalCallbackJsonMessage);

	// notify hyperhdr about a jsonMessageForward
	if (_hyperhdr != nullptr)
		connect(this, &HyperAPI::SignalForwardJsonMessage, _hyperhdr.get(), &HyperHdrInstance::SignalForwardJsonMessage);
}

bool HyperAPI::handleInstanceSwitch(quint8 inst, bool /*forced*/)
{
	if (BaseAPI::setHyperhdrInstance(inst))
	{		
		Debug(_log, "Client '{:s}' switch to HyperHDR instance {:d}", (_peerAddress), inst);
		return true;
	}
	return false;
}

void HyperAPI::handleColorCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit SignalForwardJsonMessage(message);
	int priority = message["priority"].toInt();
	int duration = message["duration"].toInt(-1);
	const QString origin = message["origin"].toString("JsonRpc") + "@" + _peerAddress;

	const QJsonArray& jsonColor = message["color"].toArray();
	std::vector<uint8_t> colors;
	// TODO faster copy
	for (auto&& entry : jsonColor)
	{
		colors.emplace_back(uint8_t(entry.toInt()));
	}

	BaseAPI::setColor(priority, colors, duration, origin);
	sendSuccessReply(command, tan);
}

void HyperAPI::handleImageCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit SignalForwardJsonMessage(message);

	BaseAPI::ImageCmdData idata;
	idata.priority = message["priority"].toInt();
	idata.origin = message["origin"].toString("JsonRpc") + "@" + _peerAddress;
	idata.duration = message["duration"].toInt(-1);
	idata.width = message["imagewidth"].toInt();
	idata.height = message["imageheight"].toInt();
	idata.scale = message["scale"].toInt(-1);
	idata.format = message["format"].toString();
	idata.imgName = message["name"].toString("");
	idata.imagedata = message["imagedata"].toString();
	QString replyMsg;

	if (!BaseAPI::setImage(idata, COMP_IMAGE, replyMsg))
	{
		sendErrorReply(replyMsg, command, tan);
		return;
	}
	sendSuccessReply(command, tan);
}

void HyperAPI::handleEffectCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit SignalForwardJsonMessage(message);

	EffectCmdData dat;
	dat.priority = message["priority"].toInt();
	dat.duration = message["duration"].toInt(-1);
	dat.pythonScript = message["pythonScript"].toString();
	dat.origin = message["origin"].toString("JsonRpc") + "@" + _peerAddress;
	dat.effectName = message["effect"].toObject()["name"].toString();
	dat.data = message["imageData"].toString("").toUtf8();
	dat.args = message["effect"].toObject()["args"].toObject();

	if (BaseAPI::setEffect(dat))
		sendSuccessReply(command, tan);
	else
		sendErrorReply("Effect '" + dat.effectName + "' not found", command, tan);
}

hyperhdr::Components HyperAPI::getActiveComponent()
{
	hyperhdr::Components active;

	SAFE_CALL_0_RET(_hyperhdr.get(), getCurrentPriorityActiveComponent, hyperhdr::Components, active);

	return active;
}

void HyperAPI::handleServerInfoCommand(const QJsonObject& message, const QString& command, int tan)
{
	try
	{
		bool subscribeOnly = false;
		if (message.contains("subscribe"))
		{
			QJsonArray subsArr = message["subscribe"].toArray();
			for (const QJsonValueRef entry : subsArr)
			{
				if (entry == "performance-update" || entry == "lut-calibration-update")
					subscribeOnly = true;
			}
		}

		if (!subscribeOnly)
		{
			QJsonObject info;


			/////////////////////
			// Instance report //
			/////////////////////

			SAFE_CALL_1_RET(_hyperhdr.get(), getJsonInfo, QJsonObject, info, bool, true);

			///////////////////////////
			// Available LED devices //
			///////////////////////////

			QJsonObject ledDevices;
			QJsonArray availableLedDevices;
			for (const auto& dev : hyperhdr::leds::GET_ALL_LED_DEVICE(nullptr))
			{
				QJsonObject driver;
				driver["name"] = dev.name;
				driver["group"] = dev.group;
				availableLedDevices.append(driver);
			}

			ledDevices["available"] = availableLedDevices;
			info["ledDevices"] = ledDevices;

			///////////////////////
			// Sound Device Info //
			///////////////////////

#if defined(ENABLE_SOUNDCAPLINUX) || defined(ENABLE_SOUNDCAPWINDOWS) || defined(ENABLE_SOUNDCAPMACOS)

			QJsonObject resultSound;

			if (_soundCapture != nullptr)
				SAFE_CALL_0_RET(_soundCapture.get(), getJsonInfo, QJsonObject, resultSound);

			if (!resultSound.isEmpty())
				info["sound"] = resultSound;

#endif

			/////////////////////////
			// System Grabber Info //
			/////////////////////////

#if defined(ENABLE_DX) || defined(ENABLE_MAC_SYSTEM) || defined(ENABLE_X11) || defined(ENABLE_FRAMEBUFFER) || defined(ENABLE_AMLOGIC)

			QJsonObject resultSGrabber;

			if (_systemGrabber != nullptr && _systemGrabber->systemWrapper() != nullptr)
				SAFE_CALL_0_RET(_systemGrabber->systemWrapper(), getJsonInfo, QJsonObject, resultSGrabber);

			if (!resultSGrabber.isEmpty())
				info["systemGrabbers"] = resultSGrabber;

#endif


			//////////////////////
			//  Video grabbers  //
			//////////////////////

			QJsonObject grabbers = info["grabbers"].toObject();
			[[maybe_unused]] GrabberWrapper* grabberWrapper = (_videoGrabber != nullptr) ? _videoGrabber->grabberWrapper() : nullptr;

#if defined(ENABLE_V4L2) || defined(ENABLE_MF) || defined(ENABLE_AVF)
			if (grabberWrapper != nullptr)
				SAFE_CALL_0_RET(grabberWrapper, getJsonInfo, QJsonObject, grabbers);
#endif

			QJsonObject currentConfig;
			SAFE_CALL_0_RET(_hyperhdr.get(), getJsonConfig, QJsonObject, currentConfig);
			const QJsonObject transferHeaders = buildTransferHeadersResponse(_instanceManager->getRootPath(), _log, currentConfig);
			const QJsonObject calibrationHeaders = buildCalibrationHeadersResponse(_instanceManager->getRootPath(), _log, currentConfig);
			const QJsonObject rgbwLutHeaders = buildRgbwLutHeadersResponse(_instanceManager->getRootPath(), _log, currentConfig);
			const QJsonObject solverProfiles = buildSolverProfilesResponse(_instanceManager->getRootPath(), _log, currentConfig);
			grabbers["transfer_headers_path"] = transferHeaders["path"].toString();
			grabbers["transfer_headers"] = transferHeaders["headers"].toArray();
			grabbers["transfer_headers_active"] = transferHeaders["active"].toObject();
			grabbers["calibration_headers_path"] = calibrationHeaders["path"].toString();
			grabbers["calibration_headers"] = calibrationHeaders["headers"].toArray();
			grabbers["calibration_headers_active"] = calibrationHeaders["active"].toObject();
			grabbers["rgbw_lut_headers_path"] = rgbwLutHeaders["path"].toString();
			grabbers["rgbw_lut_headers"] = rgbwLutHeaders["headers"].toArray();
			grabbers["rgbw_lut_headers_active"] = rgbwLutHeaders["active"].toObject();
			grabbers["solver_profiles_path"] = solverProfiles["path"].toString();
			grabbers["solver_profiles"] = solverProfiles["profiles"].toArray();
			grabbers["solver_profiles_active"] = solverProfiles["active"].toObject();

			info["grabbers"] = grabbers;

			//////////////////////////////////
			//  Instances found by Bonjour  //
			//////////////////////////////////

			QJsonArray sessions;

#ifdef ENABLE_BONJOUR					
			QList<DiscoveryRecord> services;
			if (_discoveryWrapper != nullptr)
				SAFE_CALL_0_RET(_discoveryWrapper.get(), getAllServices, QList<DiscoveryRecord>, services);

			for (const auto& session : services)
			{
				QJsonObject item;
				item["name"] = session.getName();
				item["host"] = session.hostName;
				item["address"] = session.address;
				item["port"] = session.port;
				sessions.append(item);
			}
			info["sessions"] = sessions;
#endif


			///////////////////////////
			//     Instances info    //
			///////////////////////////

			QJsonArray instanceInfo;
			for (const auto& entry : BaseAPI::getAllInstanceData())
			{
				QJsonObject obj;
				obj.insert("friendly_name", entry["friendly_name"].toString());
				obj.insert("instance", entry["instance"].toInt());
				obj.insert("running", entry["running"].toBool());
				instanceInfo.append(obj);
			}
			info["instance"] = instanceInfo;
			info["currentInstance"] = getCurrentInstanceIndex();


			/////////////////
			//     MISC    //
			/////////////////

#if defined(ENABLE_PROTOBUF)
			info["hasPROTOBUF"] = 1;
#else
			info["hasPROTOBUF"] = 0;
#endif

#if defined(ENABLE_CEC)
			info["hasCEC"] = 1;
#else
			info["hasCEC"] = 0;
#endif

			info["hostname"] = QHostInfo::localHostName();
			info["lastError"] = Logger::getInstance()->getLastError();


			////////////////
			//     END    //
			////////////////

			sendSuccessDataReply(QJsonDocument(info), command, tan);
		}
		else
			sendSuccessReply(command, tan);

		// AFTER we send the info, the client might want to subscribe to future updates
		if (message.contains("subscribe"))
		{
			// check if listeners are allowed
			if (_noListener)
				return;

			CallbackAPI::subscribe(message["subscribe"].toArray());
		}
	}
	catch (...)
	{
		sendErrorReply("Exception");
	}
}

void HyperAPI::handleClearCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit SignalForwardJsonMessage(message);
	int priority = message["priority"].toInt();
	QString replyMsg;

	if (!BaseAPI::clearPriority(priority, replyMsg))
	{
		sendErrorReply(replyMsg, command, tan);
		return;
	}
	sendSuccessReply(command, tan);
}

void HyperAPI::handleClearallCommand(const QJsonObject& message, const QString& command, int tan)
{
	emit SignalForwardJsonMessage(message);
	QString replyMsg;
	BaseAPI::clearPriority(-1, replyMsg);
	sendSuccessReply(command, tan);
}

void HyperAPI::handleHelpCommand(const QJsonObject& /*message*/, const QString& command, int tan)
{
	QJsonObject req;
	req["available_commands"] = "color, image, effect, serverinfo, clear, clearall, adjustment, sourceselect, config, componentstate, ledcolors, logging, processing, sysinfo, videomodehdr, videomode, video-crop, authorize, instance, leddevice, lut-switching, transfer-headers, calibration-headers, rgbw-lut-headers, solver-profiles, transform, correction, temperature, help";
	sendSuccessDataReply(QJsonDocument(req), command, tan);
}

void HyperAPI::handleCropCommand(const QJsonObject& message, const QString& command, int tan)
{
	GrabberWrapper* grabberWrapper = (_videoGrabber != nullptr) ? _videoGrabber->grabberWrapper() : nullptr;
	const QJsonObject& adjustment = message["crop"].toObject();
	unsigned l = static_cast<unsigned>(adjustment["left"].toInt(0));
	unsigned r = static_cast<unsigned>(adjustment["right"].toInt(0));
	unsigned t = static_cast<unsigned>(adjustment["top"].toInt(0));
	unsigned b = static_cast<unsigned>(adjustment["bottom"].toInt(0));

	if (grabberWrapper != nullptr)
		QUEUE_CALL_4(grabberWrapper,setCropping, unsigned, l, unsigned, r, unsigned, t, unsigned, b);

	sendSuccessReply(command, tan);
}

void HyperAPI::handleBenchmarkCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString& subc = message["subcommand"].toString().trimmed();
	int status = message["status"].toInt();
	
	emit _instanceManager->SignalBenchmarkCapture(status, subc);	

	sendSuccessReply(command, tan);
}

void HyperAPI::lutDownloaded(QNetworkReply* reply, int hardware_brightness, int hardware_contrast, int hardware_saturation, qint64 time)
{
	QString fileName = QDir::cleanPath(_instanceManager->getRootPath() + QDir::separator() + "lut_lin_tables.3d");
	QString error = installLut(reply, fileName, hardware_brightness, hardware_contrast, hardware_saturation, time);

	if (error == nullptr)
	{
		Info(_log, "Reloading LUT...");
		BaseAPI::setVideoModeHdr(0);
		BaseAPI::setVideoModeHdr(1);

		QJsonDocument newSet;
		SAFE_CALL_1_RET(_hyperhdr.get(), getSetting, QJsonDocument, newSet, settings::type, settings::type::VIDEOGRABBER);
		QJsonObject grabber = QJsonObject(newSet.object());
		grabber["hardware_brightness"] = hardware_brightness;
		grabber["hardware_contrast"] = hardware_contrast;
		grabber["hardware_saturation"] = hardware_saturation;
		QString newConfig = QJsonDocument(grabber).toJson(QJsonDocument::Compact);
		BLOCK_CALL_2(_hyperhdr.get(), setSetting, settings::type, settings::type::VIDEOGRABBER, QString, newConfig);

		Info(_log, "New LUT has been installed as: {:s} (from: {:s})", (fileName), (reply->url().toString()));
	}
	else
	{
		Error(_log, "Error occured while installing new LUT: {:s}", (error));
	}

	QJsonObject report;
	report["status"] = (error == nullptr) ? 1 : 0;
	report["error"] = error;
	sendSuccessDataReply(QJsonDocument(report), "lut-install-update");
}

void HyperAPI::handleLutInstallCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString& address = QString("%1/lut_lin_tables.3d.zst").arg(message["subcommand"].toString().trimmed());
	int hardware_brightness = message["hardware_brightness"].toInt(0);
	int hardware_contrast = message["hardware_contrast"].toInt(0);
	int hardware_saturation = message["hardware_saturation"].toInt(0);
	qint64 time = message["now"].toInt(0);

	Debug(_log, "Request to install LUT from: {:s} (params => [{:d}, {:d}, {:d}])", (address),
		hardware_brightness, hardware_contrast, hardware_saturation);
	if (_adminAuthorized)
	{
		QNetworkAccessManager* mgr = new QNetworkAccessManager(this);
		connect(mgr, &QNetworkAccessManager::finished, this,
			[this, mgr, hardware_brightness, hardware_contrast, hardware_saturation, time](QNetworkReply* reply) {
				lutDownloaded(reply, hardware_brightness, hardware_contrast, hardware_saturation, time);
				reply->deleteLater();
				mgr->deleteLater();
			});
		QNetworkRequest request(address);
		mgr->get(request);
		sendSuccessReply(command, tan);
	}
	else
		sendErrorReply("No Authorization", command, tan);
}

void HyperAPI::handleLutSwitchingCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString subcommand = message["subcommand"].toString().trimmed().toLower();
	const QString fullCommand = command + "-" + subcommand;
	const bool allowLoopbackApply = isAllowedLoopbackAutomationCommand(_noListener, _peerAddress, command, subcommand);
	GrabberWrapper* grabberWrapper = (_videoGrabber != nullptr) ? _videoGrabber->grabberWrapper() : nullptr;
	if (grabberWrapper == nullptr)
	{
		sendErrorReply("No video grabber is available", fullCommand, tan);
		return;
	}

	const QString rootPath = _instanceManager->getRootPath();
	QJsonObject currentConfig;
	SAFE_CALL_0_RET(_hyperhdr.get(), getJsonConfig, QJsonObject, currentConfig);

	if (subcommand == "get")
	{
		sendSuccessDataReply(QJsonDocument(buildLutSwitchingResponse(rootPath, currentConfig, grabberWrapper, getReportedVideoStreamState(getCurrentInstanceIndex()))), fullCommand, tan);
		return;
	}

	if (!_adminAuthorized && !allowLoopbackApply)
	{
		sendErrorReply("No Authorization", fullCommand, tan);
		return;
	}

	if (subcommand == "apply" || subcommand == "reload" || subcommand == "save" || subcommand == "apply-runtime")
	{
		if (subcommand == "apply" || subcommand == "save" || subcommand == "apply-runtime")
		{
			QJsonObject update;
			QStringList updateKeys = {
				QStringLiteral("enabled"),
				QStringLiteral("directory"),
				QStringLiteral("activeLutFile"),
				QStringLiteral("sdrLutFile"),
				QStringLiteral("hdrLutFile"),
				QStringLiteral("lldvLutFile"),
				QStringLiteral("ugoosSdrLutFile"),
				QStringLiteral("ugoosHdrLutFile"),
				QStringLiteral("ugoosLldvLutFile"),
				QStringLiteral("appletvSdrLutFile"),
				QStringLiteral("appletvHdrLutFile"),
				QStringLiteral("appletvLldvLutFile"),
				QStringLiteral("steamdeckSdrLutFile"),
				QStringLiteral("steamdeckHdrLutFile"),
				QStringLiteral("steamdeckLldvLutFile"),
				QStringLiteral("rx3SdrLutFile"),
				QStringLiteral("rx3HdrLutFile"),
				QStringLiteral("rx3LldvLutFile"),
				QStringLiteral("sdrUsesHdrFile"),
				QStringLiteral("lutMemoryCacheEnabled"),
				QStringLiteral("transferAutomationEnabled"),
				QStringLiteral("transferSdrProfile"),
				QStringLiteral("transferHlgProfile"),
				QStringLiteral("transferHdrProfiles"),
				QStringLiteral("transferHdrMdl1000Profiles"),
				QStringLiteral("transferHdrMdl4000Profiles"),
				QStringLiteral("transferHdrLowProfile"),
				QStringLiteral("transferHdrMidProfile"),
				QStringLiteral("transferHdrHighProfile"),
				QStringLiteral("transferHdrLowNitLimit"),
				QStringLiteral("transferHdrMidNitLimit"),
				QStringLiteral("transferDolbyVisionMdl1000Profiles"),
				QStringLiteral("transferDolbyVisionMdl4000Profiles"),
				QStringLiteral("transferDolbyVisionMdl1000RatioNumerator"),
				QStringLiteral("transferDolbyVisionMdl1000RatioDenominator"),
				QStringLiteral("transferDolbyVisionMdl4000RatioNumerator"),
				QStringLiteral("transferDolbyVisionMdl4000RatioDenominator"),
				QStringLiteral("transferDolbyVisionLowProfile"),
				QStringLiteral("transferDolbyVisionMidProfile"),
				QStringLiteral("transferDolbyVisionHighProfile"),
				QStringLiteral("transferDolbyVisionLowNitLimit"),
				QStringLiteral("transferDolbyVisionMidNitLimit"),
				QStringLiteral("kodiDolbyVisionBucketMetric"),
				QStringLiteral("kodiDolbyVisionSceneIndexHighlightWeight"),
				QStringLiteral("kodiHdrThresholds"),
				QStringLiteral("kodiHdrMdl1000Thresholds"),
				QStringLiteral("kodiHdrMdl4000Thresholds"),
				QStringLiteral("kodiHdrMdlOnlyFallbackNits"),
				QStringLiteral("kodiDolbyVisionMdl1000Thresholds"),
				QStringLiteral("kodiDolbyVisionMdl4000Thresholds"),
				QStringLiteral("ugoosHdrMetadataLutFiles"),
				QStringLiteral("ugoosHdrMdl1000LutFiles"),
				QStringLiteral("ugoosHdrMdl4000LutFiles"),
				QStringLiteral("ugoosDolbyVisionMdl1000LutFiles"),
				QStringLiteral("ugoosDolbyVisionMdl4000LutFiles"),
				QStringLiteral("appletvHdrMetadataLutFiles"),
				QStringLiteral("appletvHdrMdl1000LutFiles"),
				QStringLiteral("appletvHdrMdl4000LutFiles"),
				QStringLiteral("appletvDolbyVisionMdl1000LutFiles"),
				QStringLiteral("appletvDolbyVisionMdl4000LutFiles"),
				QStringLiteral("steamdeckHdrMetadataLutFiles"),
				QStringLiteral("steamdeckHdrMdl1000LutFiles"),
				QStringLiteral("steamdeckHdrMdl4000LutFiles"),
				QStringLiteral("steamdeckDolbyVisionMdl1000LutFiles"),
				QStringLiteral("steamdeckDolbyVisionMdl4000LutFiles"),
				QStringLiteral("rx3HdrMetadataLutFiles"),
				QStringLiteral("rx3HdrMdl1000LutFiles"),
				QStringLiteral("rx3HdrMdl4000LutFiles"),
				QStringLiteral("rx3DolbyVisionMdl1000LutFiles"),
				QStringLiteral("rx3DolbyVisionMdl4000LutFiles"),
				QStringLiteral("daytimeUpliftEnabled"),
				QStringLiteral("daytimeUpliftProfile"),
				QStringLiteral("daytimeUpliftStartHour"),
				QStringLiteral("daytimeUpliftStartMinute"),
				QStringLiteral("daytimeUpliftEndHour"),
				QStringLiteral("daytimeUpliftEndMinute"),
				QStringLiteral("daytimeUpliftRampMinutes"),
				QStringLiteral("daytimeUpliftMaxBlend"),
				QStringLiteral("mode"),
				QStringLiteral("input"),
				QStringLiteral("file")
			};
			for (const QString& key : updateKeys)
			{
				if (message.contains(key))
					update[key] = message[key];
			}

			QString configError;
			const QJsonObject storedConfig = buildStoredLutSwitchingConfig(rootPath, currentConfig, update, configError);
			if (!configError.isEmpty())
			{
				sendErrorReply(configError, fullCommand, tan);
				return;
			}

			BLOCK_CALL_2(_hyperhdr.get(), setSetting, settings::type, settings::type::LUTSWITCHING, QString, QJsonDocument(storedConfig).toJson(QJsonDocument::Compact));
			currentConfig["lutSwitching"] = storedConfig;
			const bool applyRuntimeAutomation = (subcommand == "apply" || subcommand == "apply-runtime");

			QJsonObject reportedVideoStream = getReportedVideoStreamState(getCurrentInstanceIndex());
			if (!reportedVideoStream.isEmpty())
			{
				const QJsonObject lutSwitchingConfig = buildLutSwitchingConfig(rootPath, currentConfig);
				if (grabberWrapper != nullptr)
					grabberWrapper->setLutMemoryCacheEnabled(lutSwitchingConfig["lutMemoryCacheEnabled"].toBool(true));

				QJsonObject runtimeTransferState;
				SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, runtimeTransferState);
				QString targetProfile;
				QJsonObject transferAutomation = evaluateTransferAutomationDecision(rootPath, currentConfig, runtimeTransferState, reportedVideoStream, targetProfile);
				if (!transferAutomation.isEmpty())
				{
					if (applyRuntimeAutomation && transferAutomation["needsApply"].toBool(false) && !targetProfile.isEmpty())
					{
						QString runtimeError;
						SAFE_CALL_1_RET(_hyperhdr.get(), setLedDeviceRuntimeTransferCurveProfile, QString, runtimeError, QString, targetProfile);
						if (runtimeError.isEmpty())
						{
							QJsonObject updatedRuntimeTransferState;
							SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, updatedRuntimeTransferState);
							markAutomaticTransferSwitchApplied(transferAutomation, updatedRuntimeTransferState);
						}
						else
							transferAutomation["reason"] = QString("Automatic transfer profile switch failed: %1").arg(runtimeError);
					}

					reportedVideoStream["transferAutomation"] = transferAutomation;
				}

				if (applyRuntimeAutomation)
				{
					QJsonObject freshRuntimeTransferState;
					SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, freshRuntimeTransferState);
					const DaytimeUpliftDecision daytimeDecision = computeDaytimeUpliftDecision(lutSwitchingConfig, freshRuntimeTransferState);
					QJsonObject daytimeReport;
					daytimeReport["enabled"] = daytimeDecision.enabled;
					daytimeReport["blend"] = daytimeDecision.blend;
					daytimeReport["maxBlend"] = daytimeDecision.maxBlend;
					daytimeReport["rampPosition"] = daytimeDecision.rampPosition;
					daytimeReport["profile"] = daytimeDecision.profileId.isEmpty() ? QStringLiteral("curve3_4_new") : daytimeDecision.profileId;
					daytimeReport["reason"] = daytimeDecision.reason;
					daytimeReport["currentMinuteOfDay"] = daytimeDecision.currentMinuteOfDay;
					daytimeReport["startMinuteOfDay"] = daytimeDecision.startMinuteOfDay;
					daytimeReport["endMinuteOfDay"] = daytimeDecision.endMinuteOfDay;
					if (daytimeDecision.enabled)
					{
						QString daytimeError;
						SAFE_CALL_3_RET(_hyperhdr.get(), setLedDeviceDaytimeUplift, QString, daytimeError, bool, daytimeDecision.blend > 0, int, daytimeDecision.blend, QString, daytimeDecision.profileId);
						if (!daytimeError.isEmpty())
							daytimeReport["error"] = daytimeError;
						else
							daytimeReport["applied"] = true;
					}
					else
					{
						QString daytimeError;
						SAFE_CALL_3_RET(_hyperhdr.get(), setLedDeviceDaytimeUplift, QString, daytimeError, bool, false, int, 0, QString, QString());
						if (!daytimeError.isEmpty())
							daytimeReport["error"] = daytimeError;
					}
					reportedVideoStream["daytimeUplift"] = daytimeReport;
				}

				QStringList lutCandidates;
				QStringList lutInterpolationCandidates;
				int lutInterpolationBlend = 0;
				QJsonObject lutBucketDecision = evaluateLutBucketDecision(rootPath, currentConfig, reportedVideoStream, grabberWrapper, lutCandidates, &lutInterpolationCandidates, &lutInterpolationBlend);
				if (!lutBucketDecision.isEmpty())
				{
					if (applyRuntimeAutomation && lutBucketDecision["needsApply"].toBool(false) && grabberWrapper != nullptr)
					{
						QString lutError;
						if (grabberWrapper->applyLutSwitch(lutCandidates, lutInterpolationCandidates, lutInterpolationBlend, lutError))
						{
							markAutomaticLutSwitchApplied(lutBucketDecision, grabberWrapper->getLutRuntimeInfo());
						}
						else
							lutBucketDecision["reason"] = QString("Automatic LUT bucket apply failed: %1").arg(lutError);
					}

					reportedVideoStream["lutBucketDecision"] = lutBucketDecision;
				}

				storeReportedVideoStreamState(getCurrentInstanceIndex(), reportedVideoStream);
			}

			if (subcommand == "save" || subcommand == "apply-runtime" || !storedConfig["enabled"].toBool(false) || grabberWrapper == nullptr)
			{
				sendSuccessDataReply(QJsonDocument(buildLutSwitchingResponse(rootPath, currentConfig, grabberWrapper, getReportedVideoStreamState(getCurrentInstanceIndex()))), fullCommand, tan);
				return;
			}
		}

		QString resolvedMode;
		QString resolvedInput;
		QString error;
		const QString requestedMode = (subcommand == "reload") ? QString() : message["mode"].toString();
		const QString requestedInput = (subcommand == "reload") ? QString() : message["input"].toString();
		const QString requestedFile = (subcommand == "reload") ? QString() : message["file"].toString();
		const QJsonObject lutSwitchingConfig = buildLutSwitchingConfig(rootPath, currentConfig);
		grabberWrapper->setLutMemoryCacheEnabled(lutSwitchingConfig["lutMemoryCacheEnabled"].toBool(true));
		const QStringList candidates = buildLutSwitchingCandidateFiles(rootPath, currentConfig, requestedMode, requestedInput, requestedFile, resolvedMode, resolvedInput, error);
		if (!error.isEmpty())
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}

		const bool ok = (subcommand == "reload") ?
			grabberWrapper->applyLutSwitch(candidates, error) :
			grabberWrapper->applyLutSwitch(candidates, error);
		if (!ok)
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}

		QJsonObject response = buildLutSwitchingResponse(rootPath, currentConfig, grabberWrapper, getReportedVideoStreamState(getCurrentInstanceIndex()));
		response["selectedMode"] = resolvedMode;
		response["selectedInput"] = resolvedInput;
		response["candidateFiles"] = stringListToJsonArray(candidates);
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	sendErrorReply("unknown or missing subcommand", fullCommand, tan);
}

void HyperAPI::handleTransferHeadersCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString subcommand = message["subcommand"].toString().trimmed().toLower();
	const QString fullCommand = command + "-" + subcommand;

	const QString rootPath = _instanceManager->getRootPath();
	QJsonObject currentConfig;
	SAFE_CALL_0_RET(_hyperhdr.get(), getJsonConfig, QJsonObject, currentConfig);
	if (subcommand == "list")
	{
		sendSuccessDataReply(QJsonDocument(buildTransferHeadersResponse(rootPath, _log, currentConfig)), fullCommand, tan);
		return;
	}

	if (subcommand == "get-active")
	{
		QJsonObject runtimeTransferState;
		SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, runtimeTransferState);
		sendSuccessDataReply(QJsonDocument(buildTransferHeadersActiveState(rootPath, currentConfig, runtimeTransferState)), fullCommand, tan);
		return;
	}

	if (subcommand == "save")
	{
		QJsonObject response;
		QString error;
		if (!saveTransferHeaderProfile(rootPath, message["name"].toString(), message["content"].toString(), response, error, _log))
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}
		QJsonObject runtimeTransferState;
		SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, runtimeTransferState);
		response["active"] = buildTransferHeadersActiveState(rootPath, currentConfig, runtimeTransferState);

		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	if (subcommand == "delete")
	{
		QJsonObject response;
		QString error;
		if (!deleteTransferHeaderProfile(rootPath, message["profile"].toString(), response, error, _log))
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}
		QJsonObject runtimeTransferState;
		SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, runtimeTransferState);
		response["active"] = buildTransferHeadersActiveState(rootPath, currentConfig, runtimeTransferState);

		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	if (subcommand == "set-active")
	{
		if (!BaseAPI::isHyperhdrEnabled())
		{
			sendErrorReply("Saving configuration while HyperHDR is disabled isn't possible", fullCommand, tan);
			return;
		}

		QJsonObject deviceConfig = currentConfig["device"].toObject();
		const QString deviceType = deviceConfig["type"].toString().trimmed();
		if (deviceType.compare("rawhid", Qt::CaseInsensitive) != 0)
		{
			sendErrorReply("Active transfer curve switching is only supported for the RawHID LED device", fullCommand, tan);
			return;
		}

		const QString owner = deviceConfig["transferCurveOwner"].toString("teensy").trimmed().toLower();
		if (owner != "hyperhdr")
		{
			sendErrorReply("Active transfer curve switching requires transferCurveOwner to be set to 'hyperhdr'", fullCommand, tan);
			return;
		}

		QString error;
		const QString profileId = normalizeTransferCurveProfileId(message["profile"].toString(), error);
		if (profileId.isEmpty())
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}

		if (profileId.startsWith("custom:") && !transferCurveProfileExists(rootPath, profileId))
		{
			sendErrorReply(QString("Transfer header profile not found: %1").arg(profileId), fullCommand, tan);
			return;
		}

		QString runtimeError;
		SAFE_CALL_1_RET(_hyperhdr.get(), setLedDeviceRuntimeTransferCurveProfile, QString, runtimeError, QString, profileId);
		if (!runtimeError.isEmpty())
		{
			sendErrorReply(runtimeError, fullCommand, tan);
			return;
		}

		QJsonObject runtimeTransferState;
		SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, runtimeTransferState);
		QJsonObject response = buildTransferHeadersResponse(rootPath, _log, currentConfig);
		response["active"] = buildTransferHeadersActiveState(rootPath, currentConfig, runtimeTransferState);
		response["selected"] = profileId;
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	sendErrorReply("unknown or missing subcommand", fullCommand, tan);
}

void HyperAPI::handleCalibrationHeadersCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString subcommand = message["subcommand"].toString().trimmed().toLower();
	const QString fullCommand = command + "-" + subcommand;

	if (!_adminAuthorized)
	{
		sendErrorReply("No Authorization", fullCommand, tan);
		return;
	}

	const QString rootPath = _instanceManager->getRootPath();
	QJsonObject currentConfig;
	SAFE_CALL_0_RET(_hyperhdr.get(), getJsonConfig, QJsonObject, currentConfig);
	if (subcommand == "list")
	{
		sendSuccessDataReply(QJsonDocument(buildCalibrationHeadersResponse(rootPath, _log, currentConfig)), fullCommand, tan);
		return;
	}

	if (subcommand == "get-active")
	{
		sendSuccessDataReply(QJsonDocument(buildCalibrationHeadersActiveState(rootPath, currentConfig)), fullCommand, tan);
		return;
	}

	if (subcommand == "save")
	{
		QJsonObject response;
		QString error;
		if (!saveCalibrationHeaderProfile(rootPath, message["name"].toString(), message["content"].toString(), response, error, _log))
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}
		response["active"] = buildCalibrationHeadersActiveState(rootPath, currentConfig);

		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	if (subcommand == "delete")
	{
		QJsonObject response;
		QString error;
		if (!deleteCalibrationHeaderProfile(rootPath, message["profile"].toString(), response, error, _log))
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}
		response["active"] = buildCalibrationHeadersActiveState(rootPath, currentConfig);

		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	if (subcommand == "set-active")
	{
		if (!BaseAPI::isHyperhdrEnabled())
		{
			sendErrorReply("Saving configuration while HyperHDR is disabled isn't possible", fullCommand, tan);
			return;
		}

		QJsonObject updatedConfig = currentConfig;
		QJsonObject deviceConfig = updatedConfig["device"].toObject();
		const QString deviceType = deviceConfig["type"].toString().trimmed();
		if (deviceType.compare("rawhid", Qt::CaseInsensitive) != 0)
		{
			sendErrorReply("Active calibration header switching is only supported for the RawHID LED device", fullCommand, tan);
			return;
		}

		const QString owner = deviceConfig["calibrationHeaderOwner"].toString("teensy").trimmed().toLower();
		if (owner != "hyperhdr")
		{
			sendErrorReply("Active calibration header switching requires calibrationHeaderOwner to be set to 'hyperhdr'", fullCommand, tan);
			return;
		}

		QString error;
		const QString profileId = normalizeCalibrationHeaderProfileId(message["profile"].toString(), error);
		if (profileId.isEmpty())
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}

		if (profileId.startsWith("custom:") && !calibrationHeaderProfileExists(rootPath, profileId))
		{
			sendErrorReply(QString("Calibration header profile not found: %1").arg(profileId), fullCommand, tan);
			return;
		}

		deviceConfig["calibrationHeaderProfile"] = profileId;
		updatedConfig["device"] = deviceConfig;
		if (!BaseAPI::saveSettings(updatedConfig))
		{
			sendErrorReply("Save settings failed", fullCommand, tan);
			return;
		}

		QJsonObject response = buildCalibrationHeadersResponse(rootPath, _log, updatedConfig);
		response["selected"] = profileId;
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	sendErrorReply("unknown or missing subcommand", fullCommand, tan);
}

void HyperAPI::handleRgbwLutHeadersCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString subcommand = message["subcommand"].toString().trimmed().toLower();
	const QString fullCommand = command + "-" + subcommand;

	if (!_adminAuthorized)
	{
		sendErrorReply("No Authorization", fullCommand, tan);
		return;
	}

	const QString rootPath = _instanceManager->getRootPath();
	QJsonObject currentConfig;
	SAFE_CALL_0_RET(_hyperhdr.get(), getJsonConfig, QJsonObject, currentConfig);
	if (subcommand == "list")
	{
		sendSuccessDataReply(QJsonDocument(buildRgbwLutHeadersResponse(rootPath, _log, currentConfig)), fullCommand, tan);
		return;
	}

	if (subcommand == "get-active")
	{
		sendSuccessDataReply(QJsonDocument(buildRgbwLutHeadersActiveState(rootPath, currentConfig)), fullCommand, tan);
		return;
	}

	if (subcommand == "save")
	{
		QJsonObject response;
		QString error;
		if (!saveRgbwLutHeaderProfile(rootPath, message["name"].toString(), message["content"].toString(), response, error, _log))
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}
		response["active"] = buildRgbwLutHeadersActiveState(rootPath, currentConfig);
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	if (subcommand == "delete")
	{
		QJsonObject response;
		QString error;
		if (!deleteRgbwLutHeaderProfile(rootPath, message["profile"].toString(), response, error, _log))
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}
		response["active"] = buildRgbwLutHeadersActiveState(rootPath, currentConfig);
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	if (subcommand == "set-active")
	{
		if (!BaseAPI::isHyperhdrEnabled())
		{
			sendErrorReply("Saving configuration while HyperHDR is disabled isn't possible", fullCommand, tan);
			return;
		}

		QJsonObject updatedConfig = currentConfig;
		QJsonObject deviceConfig = updatedConfig["device"].toObject();
		const QString deviceType = deviceConfig["type"].toString().trimmed();
		if (deviceType.compare("rawhid", Qt::CaseInsensitive) != 0)
		{
			sendErrorReply("Active RGBW LUT header switching is only supported for the RawHID LED device", fullCommand, tan);
			return;
		}

		QString error;
		const QString profileId = normalizeRgbwLutProfileId(message["profile"].toString(), error);
		if (profileId.isEmpty())
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}

		if (profileId.startsWith("custom:") && !rgbwLutProfileExists(rootPath, profileId))
		{
			sendErrorReply(QString("RGBW LUT header profile not found: %1").arg(profileId), fullCommand, tan);
			return;
		}

		deviceConfig["rgbwLutProfile"] = profileId;
		updatedConfig["device"] = deviceConfig;
		if (!BaseAPI::saveSettings(updatedConfig))
		{
			sendErrorReply("Save settings failed", fullCommand, tan);
			return;
		}

		QJsonObject response = buildRgbwLutHeadersResponse(rootPath, _log, updatedConfig);
		response["selected"] = profileId;
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	sendErrorReply("unknown or missing subcommand", fullCommand, tan);
}

void HyperAPI::handleSolverProfilesCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString subcommand = message["subcommand"].toString().trimmed().toLower();
	const QString fullCommand = command + "-" + subcommand;

	if (!_adminAuthorized)
	{
		sendErrorReply("No Authorization", fullCommand, tan);
		return;
	}

	const QString rootPath = _instanceManager->getRootPath();
	QJsonObject currentConfig;
	SAFE_CALL_0_RET(_hyperhdr.get(), getJsonConfig, QJsonObject, currentConfig);
	if (subcommand == "list")
	{
		sendSuccessDataReply(QJsonDocument(buildSolverProfilesResponse(rootPath, _log, currentConfig)), fullCommand, tan);
		return;
	}

	if (subcommand == "get-active")
	{
		sendSuccessDataReply(QJsonDocument(buildSolverProfilesActiveState(rootPath, currentConfig)), fullCommand, tan);
		return;
	}

	if (subcommand == "save")
	{
		QJsonObject response;
		QString error;
		if (!saveSolverProfile(rootPath, message["name"].toString(), message["content"].toString(), response, error, _log))
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}
		response["active"] = buildSolverProfilesActiveState(rootPath, currentConfig);
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	if (subcommand == "delete")
	{
		QJsonObject response;
		QString error;
		if (!deleteSolverProfile(rootPath, message["profile"].toString(), response, error, _log))
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}
		response["active"] = buildSolverProfilesActiveState(rootPath, currentConfig);
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	if (subcommand == "set-active")
	{
		if (!BaseAPI::isHyperhdrEnabled())
		{
			sendErrorReply("Saving configuration while HyperHDR is disabled isn't possible", fullCommand, tan);
			return;
		}

		QJsonObject updatedConfig = currentConfig;
		QJsonObject deviceConfig = updatedConfig["device"].toObject();
		const QString deviceType = deviceConfig["type"].toString().trimmed();
		if (deviceType.compare("rawhid", Qt::CaseInsensitive) != 0)
		{
			sendErrorReply("Active solver profile switching is only supported for the RawHID LED device", fullCommand, tan);
			return;
		}

		QString error;
		const QString profileId = normalizeSolverProfileId(message["profile"].toString(), error);
		if (profileId.isEmpty())
		{
			sendErrorReply(error, fullCommand, tan);
			return;
		}

		if (profileId.startsWith("custom:") && !solverProfileExists(rootPath, profileId))
		{
			sendErrorReply(QString("Solver profile not found: %1").arg(profileId), fullCommand, tan);
			return;
		}

		deviceConfig["solverProfile"] = profileId;
		updatedConfig["device"] = deviceConfig;
		if (!BaseAPI::saveSettings(updatedConfig))
		{
			sendErrorReply("Save settings failed", fullCommand, tan);
			return;
		}

		QJsonObject response = buildSolverProfilesResponse(rootPath, _log, updatedConfig);
		response["selected"] = profileId;
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
		return;
	}

	sendErrorReply("unknown or missing subcommand", fullCommand, tan);
}

void HyperAPI::handleCurrentStateCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString subc = message["subcommand"].toString().trimmed().toLower();
	const QString fullCommand = command + "-" + subc;
	int instance = message["instance"].toInt(0);

	if (subc == "average-color")
	{
		QJsonObject avColor = BaseAPI::getAverageColor(instance);
		sendSuccessDataReply(QJsonDocument(avColor), fullCommand, tan);
	}
	else if (subc == "video-stream-get")
	{
		QJsonObject response;
		response["videoStream"] = getReportedVideoStreamState(instance);
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
	}
	else if (subc == "video-stream-report")
	{
		const bool allowLoopbackReport = isAllowedLoopbackAutomationCommand(_noListener, _peerAddress, command, subc);
		if (!_adminAuthorized && !allowLoopbackReport)
		{
			sendErrorReply("No Authorization", fullCommand, tan);
			return;
		}

		if (!message.contains("state") || !message["state"].isObject())
		{
			sendErrorReply("Missing video stream state object", fullCommand, tan);
			return;
		}

		QJsonObject currentConfig;
		SAFE_CALL_0_RET(_hyperhdr.get(), getJsonConfig, QJsonObject, currentConfig);

		QJsonObject response;
		QJsonObject videoStream = buildReportedVideoStreamState(message["state"].toObject(), instance, _peerAddress);
		GrabberWrapper* grabberWrapper = (_videoGrabber != nullptr) ? _videoGrabber->grabberWrapper() : nullptr;
		QJsonObject runtimeTransferState;
		SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, runtimeTransferState);
		QString targetProfile;
		QJsonObject transferAutomation = evaluateTransferAutomationDecision(_instanceManager->getRootPath(), currentConfig, runtimeTransferState, videoStream, targetProfile);
		if (!transferAutomation.isEmpty())
		{
			if (transferAutomation["needsApply"].toBool(false) && !targetProfile.isEmpty())
			{
				QString runtimeError;
				SAFE_CALL_1_RET(_hyperhdr.get(), setLedDeviceRuntimeTransferCurveProfile, QString, runtimeError, QString, targetProfile);
				if (runtimeError.isEmpty())
				{
					QJsonObject updatedRuntimeTransferState;
					SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, updatedRuntimeTransferState);
					markAutomaticTransferSwitchApplied(transferAutomation, updatedRuntimeTransferState);
				}
				else
					transferAutomation["reason"] = QString("Automatic transfer profile switch failed: %1").arg(runtimeError);
			}

			videoStream["transferAutomation"] = transferAutomation;
		}

		{
			QJsonObject freshRuntimeTransferState;
			SAFE_CALL_0_RET(_hyperhdr.get(), getLedDeviceRuntimeTransferCurveState, QJsonObject, freshRuntimeTransferState);
			const QJsonObject lutSwitchingConfigForDaytime = buildLutSwitchingConfig(_instanceManager->getRootPath(), currentConfig);
			const DaytimeUpliftDecision daytimeDecision = computeDaytimeUpliftDecision(lutSwitchingConfigForDaytime, freshRuntimeTransferState);
			QJsonObject daytimeReport;
			daytimeReport["enabled"] = daytimeDecision.enabled;
			daytimeReport["blend"] = daytimeDecision.blend;
			daytimeReport["maxBlend"] = daytimeDecision.maxBlend;
			daytimeReport["rampPosition"] = daytimeDecision.rampPosition;
			daytimeReport["profile"] = daytimeDecision.profileId.isEmpty() ? QStringLiteral("curve3_4_new") : daytimeDecision.profileId;
			daytimeReport["reason"] = daytimeDecision.reason;
			daytimeReport["currentMinuteOfDay"] = daytimeDecision.currentMinuteOfDay;
			daytimeReport["startMinuteOfDay"] = daytimeDecision.startMinuteOfDay;
			daytimeReport["endMinuteOfDay"] = daytimeDecision.endMinuteOfDay;
			if (daytimeDecision.enabled)
			{
				QString daytimeError;
				SAFE_CALL_3_RET(_hyperhdr.get(), setLedDeviceDaytimeUplift, QString, daytimeError, bool, daytimeDecision.blend > 0, int, daytimeDecision.blend, QString, daytimeDecision.profileId);
				if (!daytimeError.isEmpty())
					daytimeReport["error"] = daytimeError;
				else
					daytimeReport["applied"] = true;
			}
			else
			{
				QString daytimeError;
				SAFE_CALL_3_RET(_hyperhdr.get(), setLedDeviceDaytimeUplift, QString, daytimeError, bool, false, int, 0, QString, QString());
				if (!daytimeError.isEmpty())
					daytimeReport["error"] = daytimeError;
			}
			videoStream["daytimeUplift"] = daytimeReport;
		}

		QStringList lutCandidates;
		QStringList lutInterpolationCandidates;
		int lutInterpolationBlend = 0;
		const QJsonObject lutSwitchingConfig = buildLutSwitchingConfig(_instanceManager->getRootPath(), currentConfig);
		if (grabberWrapper != nullptr)
			grabberWrapper->setLutMemoryCacheEnabled(lutSwitchingConfig["lutMemoryCacheEnabled"].toBool(true));
		QJsonObject lutBucketDecision = evaluateLutBucketDecision(_instanceManager->getRootPath(), currentConfig, videoStream, grabberWrapper, lutCandidates, &lutInterpolationCandidates, &lutInterpolationBlend);
		if (!lutBucketDecision.isEmpty())
		{
			if (lutBucketDecision["needsApply"].toBool(false) && grabberWrapper != nullptr)
			{
				QString lutError;
				if (grabberWrapper->applyLutSwitch(lutCandidates, lutInterpolationCandidates, lutInterpolationBlend, lutError))
				{
					markAutomaticLutSwitchApplied(lutBucketDecision, grabberWrapper->getLutRuntimeInfo());
				}
				else
					lutBucketDecision["reason"] = QString("Automatic LUT bucket apply failed: %1").arg(lutError);
			}

			videoStream["lutBucketDecision"] = lutBucketDecision;
		}

		response["videoStream"] = videoStream;
		storeReportedVideoStreamState(instance, response["videoStream"].toObject());
		sendSuccessDataReply(QJsonDocument(response), fullCommand, tan);
	}
	else
		handleNotImplemented(command, tan);
}

void HyperAPI::handleSmoothingCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString& subc = message["subcommand"].toString().trimmed().toLower();
	int time = message["time"].toInt();

	if (subc=="all")
		QUEUE_CALL_1(_instanceManager.get(), setSmoothing, int, time)
	else
		QUEUE_CALL_1(_hyperhdr.get(), setSmoothing, int, time);

	sendSuccessReply(command, tan);
}

void HyperAPI::handleVideoControlsCommand(const QJsonObject& message, const QString& command, int tan)
{

#if defined(__APPLE__)
	sendErrorReply("Setting video controls is not supported under macOS", command, tan);
	return;
#endif

	const QJsonObject& adjustment = message["video-controls"].toObject();
	int hardware_brightness = adjustment["hardware_brightness"].toInt();
	int hardware_contrast = adjustment["hardware_contrast"].toInt();
	int hardware_saturation = adjustment["hardware_saturation"].toInt();
	int hardware_hue = adjustment["hardware_hue"].toInt();

	GrabberWrapper* grabberWrapper = (_videoGrabber != nullptr) ? _videoGrabber->grabberWrapper() : nullptr;

	if (grabberWrapper != nullptr)
	{
		QUEUE_CALL_4(grabberWrapper, setBrightnessContrastSaturationHue, int, hardware_brightness, int, hardware_contrast, int, hardware_saturation, int, hardware_hue);
	}

	sendSuccessReply(command, tan);
}

void HyperAPI::handleSourceSelectCommand(const QJsonObject& message, const QString& command, int tan)
{
	if (message.contains("auto"))
	{
		BaseAPI::setSourceAutoSelect(message["auto"].toBool(false));
	}
	else if (message.contains("priority"))
	{
		BaseAPI::setVisiblePriority(message["priority"].toInt());
	}
	else
	{
		sendErrorReply("Priority request is invalid", command, tan);
		return;
	}
	sendSuccessReply(command, tan);
}

void HyperAPI::handleSaveDB(const QJsonObject& /*message*/, const QString& command, int tan)
{
	if (_adminAuthorized)
	{
		QJsonObject backup;

		SAFE_CALL_0_RET(_instanceManager.get(), getBackup, QJsonObject, backup);

		if (!backup.empty())
			sendSuccessDataReply(QJsonDocument(backup), command, tan);
		else
			sendErrorReply("Error while generating the backup file, please consult the HyperHDR logs.", command, tan);
	}
	else
		sendErrorReply("No Authorization", command, tan);
}

void HyperAPI::handleLoadDB(const QJsonObject& message, const QString& command, int tan)
{
	if (_adminAuthorized)
	{
		QString error;

		SAFE_CALL_1_RET(_instanceManager.get(), restoreBackup, QString, error, QJsonObject, message);

		if (error.isEmpty())
		{
#ifdef __linux__
			Info(_log, "Exiting now. If HyperHDR is running as a service, systemd should restart the process.");
			HyperHdrInstance::signalTerminateTriggered();
			QTimer::singleShot(0, _instanceManager.get(), []() {QCoreApplication::exit(1); });
#else
			HyperHdrInstance::signalTerminateTriggered();
			QTimer::singleShot(0, _instanceManager.get(), []() {QCoreApplication::quit(); });
#endif
		}
		else
			sendErrorReply("Error occured while restoring the backup: " + error, command, tan);
	}
	else
		sendErrorReply("No Authorization", command, tan);
}

void HyperAPI::handlePerformanceCounters(const QJsonObject& message, const QString& command, int tan)
{
	QString subcommand = message["subcommand"].toString("");

	if (subcommand == "all")
	{
		QUEUE_CALL_1(_performanceCounters.get(), performanceInfoRequest, bool, true);
		sendSuccessReply(command, tan);
	}
	else if (subcommand == "resources")
	{
		QUEUE_CALL_1(_performanceCounters.get(), performanceInfoRequest, bool, false);
		sendSuccessReply(command, tan);
	}
	else
		sendErrorReply("Unknown subcommand", command, tan);
}

void HyperAPI::handleLoadSignalCalibration(const QJsonObject& message, const QString& command, int tan)
{
	QJsonDocument retVal;
	QString subcommand = message["subcommand"].toString("");
	QString full_command = command + "-" + subcommand;
	GrabberWrapper* grabberWrapper = (_videoGrabber != nullptr) ? _videoGrabber->grabberWrapper() : nullptr;

	if (grabberWrapper == nullptr)
	{
		sendErrorReply("No grabbers available", command, tan);
		return;
	}

	if (subcommand == "start")
	{
		if (_adminAuthorized)
		{
			SAFE_CALL_0_RET(grabberWrapper, startCalibration, QJsonDocument, retVal);
			sendSuccessDataReply(retVal, full_command, tan);
		}
		else
			sendErrorReply("No Authorization", command, tan);
	}
	else if (subcommand == "stop")
	{
		SAFE_CALL_0_RET(grabberWrapper, stopCalibration, QJsonDocument, retVal);
		sendSuccessDataReply(retVal, full_command, tan);
	}
	else if (subcommand == "get-info")
	{
		SAFE_CALL_0_RET(grabberWrapper, getCalibrationInfo, QJsonDocument, retVal);
		sendSuccessDataReply(retVal, full_command, tan);
	}
	else
		sendErrorReply("Unknown subcommand", command, tan);
}

void HyperAPI::handleConfigCommand(const QJsonObject& message, const QString& command, int tan)
{
	QString subcommand = message["subcommand"].toString("");
	QString full_command = command + "-" + subcommand;

	if (subcommand == "getschema")
	{
		handleSchemaGetCommand(message, full_command, tan);
	}
	else if (subcommand == "setconfig")
	{
		if (_adminAuthorized)
			handleConfigSetCommand(message, full_command, tan);
		else
			sendErrorReply("No Authorization", command, tan);
	}
	else if (subcommand == "getconfig")
	{
		if (_adminAuthorized)
		{
			QJsonObject getconfig;
			SAFE_CALL_0_RET(_hyperhdr.get(), getJsonConfig, QJsonObject, getconfig);
			sendSuccessDataReply(QJsonDocument(getconfig), full_command, tan);
		}
		else
			sendErrorReply("No Authorization", command, tan);
	}
	else
	{
		sendErrorReply("unknown or missing subcommand", full_command, tan);
	}
}

void HyperAPI::handleConfigSetCommand(const QJsonObject& message, const QString& command, int tan)
{
	if (message.contains("config"))
	{
		QJsonObject config = message["config"].toObject();
		if (BaseAPI::isHyperhdrEnabled())
		{
			if (BaseAPI::saveSettings(config))
			{
				sendSuccessReply(command, tan);
			}
			else
			{
				sendErrorReply("Save settings failed", command, tan);
			}
		}
		else
			sendErrorReply("Saving configuration while HyperHDR is disabled isn't possible", command, tan);
	}
}

void HyperAPI::handleSchemaGetCommand(const QJsonObject& /*message*/, const QString& command, int tan)
{
	// create result
	QJsonObject schemaJson, alldevices, properties;

	// make sure the resources are loaded (they may be left out after static linking)
	Q_INIT_RESOURCE(resource);

	// read the hyperhdr json schema from the resource
	QString schemaFile = ":/hyperhdr-schema";

	try
	{
		schemaJson = QJsonUtils::readSchema(schemaFile);
	}
	catch (const std::runtime_error& error)
	{
		throw std::runtime_error(error.what());
	}

	// collect all LED Devices
	properties = schemaJson["properties"].toObject();
	alldevices = LedDeviceWrapper::getLedDeviceSchemas();
	properties.insert("alldevices", alldevices);

	// collect all available effect schemas
	schemaJson.insert("properties", properties);

	// send the result
	sendSuccessDataReply(QJsonDocument(schemaJson), command, tan);
}

void HyperAPI::handleComponentStateCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QJsonObject& componentState = message["componentstate"].toObject();
	QString comp = componentState["component"].toString("invalid");
	bool compState = componentState["state"].toBool(true);
	QString replyMsg;

	if (!BaseAPI::setComponentState(comp, compState, replyMsg))
	{
		sendErrorReply(replyMsg, command, tan);
		return;
	}
	sendSuccessReply(command, tan);
}

void HyperAPI::handleVideoGrabberReviveCommand(const QJsonObject& message, const QString& command, int tan)
{
	Q_UNUSED(message);

	if (_videoGrabber == nullptr || _videoGrabber->grabberWrapper() == nullptr)
	{
		sendErrorReply("Video grabber is not available", command, tan);
		return;
	}

	QUEUE_CALL_0(_videoGrabber->grabberWrapper(), forceRevive);
	sendSuccessReply(command, tan);
}

void HyperAPI::handleIncomingColors(const QVector<ColorRgb>& ledValues)
{
	_currentLedValues = ledValues;

	if (_ledStreamTimer->interval() != _colorsStreamingInterval)
		_ledStreamTimer->start(_colorsStreamingInterval);
}

void HyperAPI::handleLedColorsTimer()
{
	streamLedcolorsUpdate(_currentLedValues);
}

void HyperAPI::handleLedColorsCommand(const QJsonObject& message, const QString& command, int tan)
{
	// create result
	QString subcommand = message["subcommand"].toString("");

	// max 20 Hz (50ms) interval for streaming (default: 10 Hz (100ms))
	_colorsStreamingInterval = qMax(message["interval"].toInt(100), 50);

	if (subcommand == "ledstream-start")
	{
		_streaming_leds_reply["success"] = true;
		_streaming_leds_reply["command"] = command + "-ledstream-update";
		_streaming_leds_reply["tan"] = tan;

		subscribeFor("leds-colors");

		if (!_ledStreamTimer->isActive() || _ledStreamTimer->interval() != _colorsStreamingInterval)
			_ledStreamTimer->start(_colorsStreamingInterval);

		QUEUE_CALL_0(_hyperhdr.get(), update);
	}
	else if (subcommand == "ledstream-stop")
	{
		subscribeFor("leds-colors", true);
		_ledStreamTimer->stop();
	}
	else if (subcommand == "imagestream-start")
	{
		if (BaseAPI::isAdminAuthorized())
		{
			_streaming_image_reply["success"] = true;
			_streaming_image_reply["command"] = command + "-imagestream-update";
			_streaming_image_reply["tan"] = tan;

			subscribeFor("live-video");
		}
		else
			sendErrorReply("No Authorization", command, tan);
	}
	else if (subcommand == "imagestream-stop")
	{
		subscribeFor("live-video", true);
	}
	else
	{
		return;
	}

	sendSuccessReply(command + "-" + subcommand, tan);
}

void HyperAPI::handleLoggingCommand(const QJsonObject& message, const QString& command, int tan)
{
	// create result
	QString subcommand = message["subcommand"].toString("");

	if (BaseAPI::isAdminAuthorized())
	{
		_streaming_logging_reply["success"] = true;
		_streaming_logging_reply["command"] = command;
		_streaming_logging_reply["tan"] = tan;

		if (subcommand == "start")
		{
			if (!_streaming_logging_activated)
			{
				_streaming_logging_reply["command"] = command + "-update";
				connect(Logger::getInstance(), &Logger::SignalNewLogMessage, this, &HyperAPI::incommingLogMessage, Qt::UniqueConnection);
				Debug(_log, "log streaming activated for client {:s}", _peerAddress.toStdString().c_str()); // needed to trigger log sending
			}
		}
		else if (subcommand == "stop")
		{
			if (_streaming_logging_activated)
			{
				disconnect(Logger::getInstance(), &Logger::SignalNewLogMessage, this, &HyperAPI::incommingLogMessage);
				_streaming_logging_activated = false;
				Debug(_log, "log streaming deactivated for client  {:s}", _peerAddress.toStdString().c_str());
			}
		}
		else
		{
			return;
		}

		sendSuccessReply(command + "-" + subcommand, tan);
	}
	else
	{
		sendErrorReply("No Authorization", command + "-" + subcommand, tan);
	}
}

void HyperAPI::handleProcessingCommand(const QJsonObject& message, const QString& command, int tan)
{
	BaseAPI::setLedMappingType(ImageToLedManager::mappingTypeToInt(message["mappingType"].toString("multicolor_mean")));
	sendSuccessReply(command, tan);
}

void HyperAPI::handleVideoModeHdrCommand(const QJsonObject& message, const QString& command, int tan)
{
	if (message.contains("flatbuffers_user_lut_filename"))
	{
		BaseAPI::setFlatbufferUserLUT(message["flatbuffers_user_lut_filename"].toString(""));
	}

	BaseAPI::setVideoModeHdr(message["HDR"].toInt());
	sendSuccessReply(command, tan);
}

void HyperAPI::handleLutCalibrationCommand(const QJsonObject& message, const QString& command, int tan)
{
	QString subcommand = message["subcommand"].toString("");

	if (subcommand == "capture")
	{
		bool debug = message["debug"].toBool(false);
		bool lchCorrection = message["lch_correction"].toBool(false);
		int coloredAspectMode = qBound(-1, message["colored_aspect_mode"].toInt(-1), 4);
		double nitsOverride = -1.0;
		const QJsonValue nitsOverrideValue = message["nits_override"];
		if (nitsOverrideValue.isDouble())
		{
			nitsOverride = nitsOverrideValue.toDouble(-1.0);
		}
		else if (nitsOverrideValue.isString())
		{
			bool ok = false;
			const QString trimmed = nitsOverrideValue.toString().trimmed();
			if (!trimmed.isEmpty())
			{
				nitsOverride = trimmed.toDouble(&ok);
				if (!ok)
					nitsOverride = -1.0;
			}
		}
		if (!std::isfinite(nitsOverride) || nitsOverride <= 0.0)
		{
			nitsOverride = -1.0;
		}

		QThread* lutThread = new QThread();
		LutCalibrator* lutCalibrator = new LutCalibrator(_instanceManager->getRootPath(), getActiveComponent(), debug, lchCorrection, coloredAspectMode, nitsOverride);
		lutCalibrator->moveToThread(lutThread);
		connect(lutThread, &QThread::finished, lutCalibrator, &LutCalibrator::deleteLater);
		connect(lutThread, &QThread::started, lutCalibrator, &LutCalibrator::startHandler);		
		connect(lutCalibrator, &LutCalibrator::SignalLutCalibrationUpdated, this, &CallbackAPI::lutCalibrationUpdateHandler);
		
		_lutCalibratorThread = std::unique_ptr<QThread, std::function<void(QThread*)>>(lutThread,
			[lutCalibrator](QThread* mqttThread) {
				lutCalibrator->cancelCalibrationSafe();
				THREAD_REMOVER(QString("LutCalibrator"), mqttThread, lutCalibrator);
			});
		_lutCalibratorThread->start();
	}
	else if (_lutCalibratorThread != nullptr && subcommand == "stop")
	{
		_lutCalibratorThread = nullptr;
	}
	else
	{
		sendErrorReply("The command does not have any effect", command + "-" + subcommand, tan);
		return;
	}
	
	sendSuccessReply(command, tan);
}

void HyperAPI::handleInstanceCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString& subc = message["subcommand"].toString();
	const quint8& inst = message["instance"].toInt();
	const QString& name = message["name"].toString();

	if (subc == "switchTo")
	{
		if (handleInstanceSwitch(inst))
		{
			QJsonObject obj;
			obj["instance"] = inst;
			sendSuccessDataReply(QJsonDocument(obj), command + "-" + subc, tan);
		}
		else
			sendErrorReply("Selected HyperHDR instance isn't running", command + "-" + subc, tan);
		return;
	}

	if (subc == "startInstance")
	{
		connect(this, &BaseAPI::SignalInstanceStartedClientNotification,
				this, [this, command, subc](const int& tan) { sendSuccessReply(command + "-" + subc, tan); });
		if (!BaseAPI::startInstance(inst, tan))
			sendErrorReply("Can't start HyperHDR instance index " + QString::number(inst), command + "-" + subc, tan);
		return;
	}

	if (subc == "stopInstance")
	{
		// silent fail
		BaseAPI::stopInstance(inst);
		sendSuccessReply(command + "-" + subc, tan);
		return;
	}

	if (subc == "deleteInstance")
	{
		QString replyMsg;
		if (BaseAPI::deleteInstance(inst, replyMsg))
			sendSuccessReply(command + "-" + subc, tan);
		else
			sendErrorReply(replyMsg, command + "-" + subc, tan);
		return;
	}

	// create and save name requires name
	if (name.isEmpty())
		sendErrorReply("Name string required for this command", command + "-" + subc, tan);

	if (subc == "createInstance")
	{
		QString replyMsg = BaseAPI::createInstance(name);
		if (replyMsg.isEmpty())
			sendSuccessReply(command + "-" + subc, tan);
		else
			sendErrorReply(replyMsg, command + "-" + subc, tan);
		return;
	}

	if (subc == "saveName")
	{
		QString replyMsg = BaseAPI::setInstanceName(inst, name);
		if (replyMsg.isEmpty())
			sendSuccessReply(command + "-" + subc, tan);
		else
			sendErrorReply(replyMsg, command + "-" + subc, tan);
		return;
	}
}

void HyperAPI::handleLedDeviceCommand(const QJsonObject& message, const QString& command, int tan)
{
	Debug(_log, "message: [{:s}]", QString(QJsonDocument(message).toJson(QJsonDocument::Compact)).toUtf8().constData());

	const QString& subc = message["subcommand"].toString().trimmed();
	const QString& devType = message["ledDeviceType"].toString().trimmed();

	QString full_command = command + "-" + subc;

	// TODO: Validate that device type is a valid one
/*	if ( ! valid type )
	{
		sendErrorReply("Unknown device", full_command, tan);
	}
	else
*/ {
		QJsonObject config;
		config.insert("type", devType);
		std::unique_ptr<LedDevice> ledDevice;

		if (subc == "discover")
		{
			ledDevice = std::unique_ptr<LedDevice>(hyperhdr::leds::CONSTRUCT_LED_DEVICE(config));
			const QJsonObject& params = message["params"].toObject();
			const QJsonObject devicesDiscovered = ledDevice->discover(params);

			Debug(_log, "response: [{:s}]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

			sendSuccessDataReply(QJsonDocument(devicesDiscovered), full_command, tan);
		}
		else if (subc == "getProperties")
		{
			ledDevice = std::unique_ptr<LedDevice>(hyperhdr::leds::CONSTRUCT_LED_DEVICE(config));
			const QJsonObject& params = message["params"].toObject();
			const QJsonObject deviceProperties = ledDevice->getProperties(params);

			Debug(_log, "response: [{:s}]", QString(QJsonDocument(deviceProperties).toJson(QJsonDocument::Compact)).toUtf8().constData());

			sendSuccessDataReply(QJsonDocument(deviceProperties), full_command, tan);
		}
		else if (subc == "identify")
		{
			QJsonObject params = message["params"].toObject();

			if (devType == "philipshuev2" || devType == "blink")
			{
				QUEUE_CALL_1(_hyperhdr.get(), identifyLed, QJsonObject, params);
			}
			else
			{
				ledDevice = std::unique_ptr<LedDevice>(hyperhdr::leds::CONSTRUCT_LED_DEVICE(config));
				ledDevice->identify(params);
			}

			sendSuccessReply(full_command, tan);
		}
		else if (subc == "hasLedClock")
		{
			int hasLedClock = 0;
			QJsonObject ret;
			ret["hasLedClock"] = hasLedClock;
			sendSuccessDataReply(QJsonDocument(ret), "hasLedClock-update", tan);
		}
		else
		{
			sendErrorReply("Unknown or missing subcommand", full_command, tan);
		}		
	}
}

void HyperAPI::streamLedcolorsUpdate(const QVector<ColorRgb>& ledColors)
{
	QJsonObject result;
	QJsonArray leds;

	for (const auto& color : ledColors)
	{
		leds << QJsonValue(color.red) << QJsonValue(color.green) << QJsonValue(color.blue);
	}

	result["leds"] = leds;
	_streaming_leds_reply["result"] = result;

	// send the result
	emit SignalCallbackJsonMessage(_streaming_leds_reply);
}

void HyperAPI::handlerInstanceImageUpdated(const Image<ColorRgb>& image)
{
	_liveImage = image;
	QUEUE_CALL_0(this, sendImage);
}

void HyperAPI::sendImage()
{
	qint64 _currentTime = InternalClock::now();
	int timeLimit = (_liveImage.width() > 1280) ? 60 : 30;

	if ((_liveImage.width() <= 1 || _liveImage.height() <= 1) ||
		(_lastSentImage + timeLimit > _currentTime && _lastSentImage + 100 < _currentTime))
	{
		return;
	}

	emit SignalCallbackBinaryImageMessage(_liveImage);

	_liveImage = Image<ColorRgb>();
	_lastSentImage = _currentTime;
}

void HyperAPI::incommingLogMessage(const Logger::T_LOG_MESSAGE& msg)
{
	QJsonObject result, message;
	QJsonArray messageArray;

	if (!_streaming_logging_activated)
	{
		_streaming_logging_activated = true;
		SAFE_CALL_0_RET(Logger::getInstance(), getLogMessageBuffer, QJsonArray, messageArray);
	}
	else
	{
		message["loggerName"] = msg.loggerName;
		message["function"] = msg.function;
		message["line"] = QString::number(msg.line);
		message["fileName"] = msg.fileName;
		message["message"] = msg.message;
		message["levelString"] = msg.levelString;
		message["utime"] = QString::number(msg.utime);

		messageArray.append(message);
	}

	result.insert("messages", messageArray);
	_streaming_logging_reply["result"] = result;

	// send the result
	emit SignalCallbackJsonMessage(_streaming_logging_reply);
}

void HyperAPI::newPendingTokenRequest(const QString& id, const QString& comment)
{
	QJsonObject obj;
	obj["comment"] = comment;
	obj["id"] = id;
	obj["timeout"] = 180000;

	sendSuccessDataReply(QJsonDocument(obj), "authorize-tokenRequest", 1);
}

void HyperAPI::handleTokenResponse(bool success, const QString& token, const QString& comment, const QString& id, const int& tan)
{
	const QString cmd = "authorize-requestToken";
	QJsonObject result;
	result["token"] = token;
	result["comment"] = comment;
	result["id"] = id;

	if (success)
		sendSuccessDataReply(QJsonDocument(result), cmd, tan);
	else
		sendErrorReply("Token request timeout or denied", cmd, tan);
}

void HyperAPI::handleInstanceStateChange(InstanceState state, quint8 instance, const QString& /*name*/)
{
	switch (state)
	{
	case InstanceState::STOP:
		if (getCurrentInstanceIndex() == instance)
		{
			handleInstanceSwitch();
		}
		break;
	default:
		break;
	}
}

void HyperAPI::stopDataConnections()
{
	disconnect(Logger::getInstance(), nullptr, this, nullptr);
	_streaming_logging_activated = false;
	CallbackAPI::removeSubscriptions();
	// led stream colors
	disconnect(_hyperhdr.get(), &HyperHdrInstance::SignalRawColorsChanged, this, nullptr);
	_ledStreamTimer->stop();
}

void HyperAPI::handleTunnel(const QJsonObject& message, const QString& command, int tan)
{
	const QString& subcommand = message["subcommand"].toString().trimmed();
	const QString& full_command = command + "-" + subcommand;

	if (_adminAuthorized)
	{
		const QString& ip = message["ip"].toString().trimmed();
		const QString& path = message["path"].toString().trimmed();
		const QString& data = message["data"].toString().trimmed();
		const QString& service = message["service"].toString().trimmed();

		if (service == "hue" || service == "home_assistant")
		{
			QUrl tempUrl(ip.startsWith("http") ? ip : "http://" + ip);
			if (!path.startsWith("/clip/v2") && !path.startsWith("/api"))
			{
				sendErrorReply("Invalid path", full_command, tan);
				return;
			}

			ProviderRestApi provider;

			QUrl url = QUrl(QString("%1://").arg(tempUrl.scheme()) + tempUrl.host() + ((service == "home_assistant" && tempUrl.port() >= 0) ? ":" + QString::number(tempUrl.port()) : "") + path);

			Debug(_log, "Tunnel request for: {:s}", (url.toString()));

			if (!url.isValid())
			{
				sendErrorReply("Invalid Philips Hue bridge address", full_command, tan);
				return;
			}

			if (!isLocal(url.host()))
			{
				Error(_log, "Could not resolve '{:s}' as IP local address at your HyperHDR host device.", (url.host()));
				sendErrorReply("The Philips Hue wizard supports only valid IP addresses in the LOCAL network.\nIt may be preferable to use the IP4 address instead of the host name if you are having problems with DNS resolution.", full_command, tan);
				return;
			}
			httpResponse result;
			const QJsonObject& headerObject = message["header"].toObject();

			if (!headerObject.isEmpty())
			{
				for (const auto& item : headerObject.keys())
				{
					provider.addHeader(item, headerObject[item].toString());
				}
			}

			if (subcommand != "get")
				provider.addHeader("Content-Type", "application/json");

			if (subcommand == "put")
				result = provider.put(url, data);
			else if (subcommand == "post")
				result = provider.post(url, data);
			else if (subcommand == "get")
				result = provider.get(url);
			else
			{
				sendErrorReply("Unknown command", full_command, tan);
				return;
			}

			QJsonObject reply;
			auto doc = result.getBody();
			reply["success"] = true;
			reply["command"] = full_command;
			reply["tan"] = tan;
			reply["isTunnelOk"] = !result.error();
			if (doc.isArray())
				reply["info"] = doc.array();
			else
				reply["info"] = doc.object();

			emit SignalCallbackJsonMessage(reply);
		}
		else
			sendErrorReply("Service not supported", full_command, tan);
	}
	else
		sendErrorReply("No Authorization", full_command, tan);
}

bool HyperAPI::isLocal(QString hostname)
{
	QHostAddress address(hostname);

	if (QAbstractSocket::IPv4Protocol != address.protocol() &&
		QAbstractSocket::IPv6Protocol != address.protocol())
	{
		auto result = QHostInfo::fromName(hostname);
		if (result.error() == QHostInfo::HostInfoError::NoError)
		{
			QList<QHostAddress> list = result.addresses();
			for (int x = 1; x >= 0; x--)
				foreach(const QHostAddress& l, list)
				if (l.protocol() == (x > 0) ? QAbstractSocket::IPv4Protocol : QAbstractSocket::IPv6Protocol)
				{
					address = l;
					x = 0;
					break;
				}
		}
	}

	if (QAbstractSocket::IPv4Protocol == address.protocol())
	{
		quint32 adr = address.toIPv4Address();
		uint8_t a = adr >> 24;
		uint8_t b = (adr >> 16) & 0xff;

		Debug(_log, "IP4 prefix: {:d}.{:d}", a, b);

		return ((a == 192 && b == 168) || (a == 10) || (a == 172 && b >= 16 && b <32));
	}

	else if (QAbstractSocket::IPv6Protocol == address.protocol())
	{
		auto i = address.toString();

		Debug(_log, "IP6 prefix: {:s}", (i.left(4)));

		return i.indexOf("fd") == 0 || i.indexOf("fe80") == 0;
	}

	return false;
}

void HyperAPI::handleSysInfoCommand(const QJsonObject&, const QString& command, int tan)
{
	// create result
	QJsonObject result;
	QJsonObject info;
	result["success"] = true;
	result["command"] = command;
	result["tan"] = tan;

	QJsonObject system;
	putSystemInfo(system);
	info["system"] = system;

	QJsonObject hyperhdr;
	hyperhdr["version"] = QString(HYPERHDR_VERSION);
	hyperhdr["build"] = QString(HYPERHDR_BUILD_ID);
	hyperhdr["gitremote"] = QString(HYPERHDR_GIT_REMOTE);
	hyperhdr["time"] = QString(__DATE__ " " __TIME__);
	hyperhdr["id"] = _accessManager->getID();

	bool readOnly = true;
	SAFE_CALL_0_RET(_hyperhdr.get(), getReadOnlyMode, bool, readOnly);
	hyperhdr["readOnlyMode"] = readOnly;

	info["hyperhdr"] = hyperhdr;

	// send the result
	result["info"] = info;
	emit SignalCallbackJsonMessage(result);
}

void HyperAPI::handleAdjustmentCommand(const QJsonObject& message, const QString& command, int tan)
{
	QJsonObject adjustment = message["adjustment"].toObject();

	QUEUE_CALL_1(_hyperhdr.get(), updateAdjustments, QJsonObject, adjustment);

	sendSuccessReply(command, tan);
}

void HyperAPI::handleAuthorizeCommand(const QJsonObject& message, const QString& command, int tan)
{
	const QString& subc = message["subcommand"].toString().trimmed();
	const QString& id = message["id"].toString().trimmed();
	const QString& password = message["password"].toString().trimmed();
	const QString& newPassword = message["newPassword"].toString().trimmed();
	const QString& comment = message["comment"].toString().trimmed();

	// catch test if auth is required
	if (subc == "tokenRequired")
	{
		QJsonObject req;
		req["required"] = !BaseAPI::isAuthorized();

		sendSuccessDataReply(QJsonDocument(req), command + "-" + subc, tan);
		return;
	}

	// catch test if admin auth is required
	if (subc == "adminRequired")
	{
		QJsonObject req;
		req["adminRequired"] = !BaseAPI::isAdminAuthorized();
		sendSuccessDataReply(QJsonDocument(req), command + "-" + subc, tan);
		return;
	}

	// default hyperhdr password is a security risk, replace it asap
	if (subc == "newPasswordRequired")
	{
		QJsonObject req;
		req["newPasswordRequired"] = BaseAPI::hasHyperhdrDefaultPw();
		sendSuccessDataReply(QJsonDocument(req), command + "-" + subc, tan);
		return;
	}

	// catch logout
	if (subc == "logout")
	{
		BaseAPI::logout();
		sendSuccessReply(command + "-" + subc, tan);
		return;
	}

	// change password
	if (subc == "newPassword")
	{
		// use password, newPassword
		if (BaseAPI::isAdminAuthorized())
		{
			if (BaseAPI::updateHyperhdrPassword(password, newPassword))
			{
				sendSuccessReply(command + "-" + subc, tan);
				return;
			}
			sendErrorReply("Failed to update user password", command + "-" + subc, tan);
			return;
		}
		sendErrorReply("No Authorization", command + "-" + subc, tan);
		return;
	}

	// token created from ui
	if (subc == "createToken")
	{
		// use comment
		// for user authorized sessions
		AccessManager::AuthDefinition def;
		const QString res = BaseAPI::createToken(comment, def);
		if (res.isEmpty())
		{
			QJsonObject newTok;
			newTok["comment"] = def.comment;
			newTok["id"] = def.id;
			newTok["token"] = def.token;

			sendSuccessDataReply(QJsonDocument(newTok), command + "-" + subc, tan);
			return;
		}
		sendErrorReply(res, command + "-" + subc, tan);
		return;
	}

	// rename Token
	if (subc == "renameToken")
	{
		// use id/comment
		const QString res = BaseAPI::renameToken(id, comment);
		if (res.isEmpty())
		{
			sendSuccessReply(command + "-" + subc, tan);
			return;
		}
		sendErrorReply(res, command + "-" + subc, tan);
		return;
	}

	// delete token
	if (subc == "deleteToken")
	{
		// use id
		const QString res = BaseAPI::deleteToken(id);
		if (res.isEmpty())
		{
			sendSuccessReply(command + "-" + subc, tan);
			return;
		}
		sendErrorReply(res, command + "-" + subc, tan);
		return;
	}

	// catch token request
	if (subc == "requestToken")
	{
		// use id/comment
		const bool& acc = message["accept"].toBool(true);
		if (acc)
			BaseAPI::setNewTokenRequest(comment, id, tan);
		else
			BaseAPI::cancelNewTokenRequest(comment, id);
		// client should wait for answer
		return;
	}

	// get pending token requests
	if (subc == "getPendingTokenRequests")
	{
		QVector<AccessManager::AuthDefinition> vec;
		if (BaseAPI::getPendingTokenRequests(vec))
		{
			QJsonArray arr;
			for (const auto& entry : vec)
			{
				QJsonObject obj;
				obj["comment"] = entry.comment;
				obj["id"] = entry.id;
				obj["timeout"] = int(entry.timeoutTime);
				arr.append(obj);
			}
			sendSuccessDataReply(QJsonDocument(arr), command + "-" + subc, tan);
		}
		else
		{
			sendErrorReply("No Authorization", command + "-" + subc, tan);
		}

		return;
	}

	// accept/deny token request
	if (subc == "answerRequest")
	{
		// use id
		const bool& accept = message["accept"].toBool(false);
		if (!BaseAPI::handlePendingTokenRequest(id, accept))
			sendErrorReply("No Authorization", command + "-" + subc, tan);
		return;
	}

	// get token list
	if (subc == "getTokenList")
	{
		QVector<AccessManager::AuthDefinition> defVect;
		if (BaseAPI::getTokenList(defVect))
		{
			QJsonArray tArr;
			for (const auto& entry : defVect)
			{
				QJsonObject subO;
				subO["comment"] = entry.comment;
				subO["id"] = entry.id;
				subO["last_use"] = entry.lastUse;

				tArr.append(subO);
			}
			sendSuccessDataReply(QJsonDocument(tArr), command + "-" + subc, tan);
			return;
		}
		sendErrorReply("No Authorization", command + "-" + subc, tan);
		return;
	}

	// login
	if (subc == "login")
	{
		const QString& token = message["token"].toString().trimmed();

		// catch token
		if (!token.isEmpty())
		{
			// userToken is longer
			if (token.length() > 36)
			{
				if (BaseAPI::isUserTokenAuthorized(token))
					sendSuccessReply(command + "-" + subc, tan);
				else
					sendErrorReply("No Authorization", command + "-" + subc, tan);

				return;
			}
			// usual app token is 36
			if (token.length() == 36)
			{
				if (BaseAPI::isTokenAuthorized(token))
				{
					sendSuccessReply(command + "-" + subc, tan);
				}
				else
					sendErrorReply("No Authorization", command + "-" + subc, tan);
			}
			return;
		}

		// password
		// use password
		if (password.length() >= 8)
		{
			QString userTokenRep;
			if (BaseAPI::isUserAuthorized(password) && BaseAPI::getUserToken(userTokenRep))
			{
				// Return the current valid HyperHDR user token
				QJsonObject obj;
				obj["token"] = userTokenRep;
				sendSuccessDataReply(QJsonDocument(obj), command + "-" + subc, tan);
			}
			else if (BaseAPI::isUserBlocked())
				sendErrorReply("Too many login attempts detected. Please restart HyperHDR.", command + "-" + subc, tan);
			else
				sendErrorReply("No Authorization", command + "-" + subc, tan);
		}
		else
			sendErrorReply("Password too short", command + "-" + subc, tan);
	}
}

void HyperAPI::handleNotImplemented(const QString& command, int tan)
{
	sendErrorReply("Command not implemented", command, tan);
}

void HyperAPI::sendSuccessReply(const QString& command, int tan)
{
	// create reply
	QJsonObject reply;
	reply["success"] = true;
	reply["command"] = command;
	reply["tan"] = tan;

	// send reply
	emit SignalCallbackJsonMessage(reply);
}

void HyperAPI::sendSuccessDataReply(const QJsonDocument& doc, const QString& command, int tan)
{
	QJsonObject reply;
	reply["success"] = true;
	reply["command"] = command;
	reply["tan"] = tan;
	if (doc.isArray())
		reply["info"] = doc.array();
	else
		reply["info"] = doc.object();

	emit SignalCallbackJsonMessage(reply);
}

void HyperAPI::sendErrorReply(const QString& error, const QString& command, int tan)
{
	// create reply
	QJsonObject reply;
	reply["success"] = false;
	reply["error"] = error;
	reply["command"] = command;
	reply["tan"] = tan;

	// send reply
	emit SignalCallbackJsonMessage(reply);
}
