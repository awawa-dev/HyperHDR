#pragma once

#include <array>
#include <chrono>
#include <vector>

#include <led-drivers/InfiniteColorEngineRgbw.h>
#include <led-drivers/LedDevice.h>

class DriverOtherRawHid : public LedDevice
{
public:
	enum class PayloadMode
	{
		Rgb16,
		Rgb12Carry,
	};

	enum class TransferCurveOwner
	{
		Disabled,
		Teensy,
		HyperHdr,
	};

	enum class TransferCurveProfile
	{
		Disabled = 0,
		Curve3_4New = 1,
		Custom = 2,
	};

	struct CustomTransferCurveLut
	{
		uint32_t bucketCount = 0u;
		std::array<std::vector<uint16_t>, 4> channels;
	};

	enum class CalibrationHeaderOwner
	{
		Disabled,
		Teensy,
		HyperHdr,
	};

	enum class CalibrationHeaderProfile
	{
		Disabled = 0,
		Custom = 1,
	};

	enum class HostColorTransformOrder
	{
		TransferThenCalibration,
		CalibrationThenTransfer,
	};

	enum class HostRgbwLutTransferOrder
	{
		TransferBeforeLut,
		LutThenTransfer,
	};

	struct CustomCalibrationHeaderLut
	{
		uint32_t bucketCount = 0u;
		std::array<std::vector<uint16_t>, 4> channels;
	};

	struct CustomRgbwLutGrid
	{
		uint32_t sourceGridSize = 0u;
		uint32_t gridSize = 0u;
		uint32_t entryCount = 0u;
		uint32_t axisMin = 0u;
		uint32_t axisMax = 65535u;
		bool requires3dInterpolation = true;
		std::array<std::vector<uint16_t>, 4> channels;
	};

	struct SolverLadderEntry
	{
		uint16_t outputQ16 = 0u;
		uint8_t value = 0u;
		uint8_t bfi = 0u;
		uint8_t lowerValue = 0u;
		uint8_t upperValue = 0u;
	};

	struct SolverEncodedState
	{
		uint8_t value = 0u;
		uint8_t bfi = 0u;
		uint16_t outputQ16 = 0u;
	};

	struct SolverLookupTables
	{
		std::array<size_t, 4> bucketCounts{};
		std::array<std::vector<uint8_t>, 4> baselineBfi{};
		std::array<std::vector<uint16_t>, 4> baselineOutputQ16{};
	};

	using HostOwnedRgbwQ16 = InfiniteColorEngineRgbw::RgbwTargetQ16;

	struct HostPolicyProbeSample
	{
		bool valid = false;
		bool highlighted = false;
		bool hostOwnedRgbw = false;
		bool clamped = false;
		size_t ledIndex = 0u;
		std::array<uint16_t, 4> processedQ16{};
		std::array<uint16_t, 4> policyQ16{};
		std::array<uint16_t, 4> baselineQ16{};
		std::array<uint16_t, 4> emittedQ16{};
		std::array<uint8_t, 4> baselineBfi{};
		std::array<uint8_t, 4> finalBfi{};
	};

	enum class ScenePolicyMode
	{
		Disabled,
		SolverAwareV2,
		SolverAwareV3,
	};

	enum class HostRgbwMode
	{
		Disabled,
		Ice,
		ClassicMin,
		CentroidMin,
		GatedMin,
		LutHeader,
		ExactWhite,
	};

	explicit DriverOtherRawHid(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(QJsonObject deviceConfig) override;
	int open() override;
	int close() override;
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	QString discoverFirst() override;
	QJsonObject discover(const QJsonObject& params) override;
	QJsonObject getRuntimeTransferCurveState() const override;
	QString setRuntimeTransferCurveProfile(QString profileId) override;
	QString setDaytimeUplift(bool enabled, int blend, const QString& profileId) override;

protected slots:
	void setInError(const QString& errorMsg) override;

private:
	bool parseHexOrDecimal(const QString& text, uint16_t& out) const;
	PayloadMode parsePayloadMode(const QString& text) const;
	TransferCurveOwner parseTransferCurveOwner(const QString& text) const;
	TransferCurveProfile parseTransferCurveProfile(const QString& text) const;
	QString parseCustomTransferCurveProfileKey(const QString& text) const;
	QString currentTransferCurveProfileId() const;
	bool isRuntimeInterpolatedTransferCurveActive() const;
	CalibrationHeaderOwner parseCalibrationHeaderOwner(const QString& text) const;
	CalibrationHeaderProfile parseCalibrationHeaderProfile(const QString& text) const;
	QString parseCustomCalibrationHeaderProfileKey(const QString& text) const;
	QString parseCustomRgbwLutProfileKey(const QString& text) const;
	QString parseSolverProfileId(const QString& text) const;
	HostColorTransformOrder parseCalibrationTransferOrder(const QString& text) const;
	HostRgbwLutTransferOrder parseRgbwLutTransferOrder(const QString& text) const;
	ScenePolicyMode parseScenePolicyMode(const QString& text) const;
	HostRgbwMode parseHostRgbwMode(const QString& text) const;
	bool isTransferCurveEnabled() const;
	bool isLutHeaderRgbwModeActive() const;
	bool shouldApplyTransferCurveOnHost() const;
	bool shouldApplyCalibrationOnHost() const;
	bool isHostOwnedRgbwEnabled() const;
	bool isIceRgbwMode() const;
	uint8_t transferCurveProfileId() const;
	uint16_t applyTransferCurveQ16(uint16_t q16, uint8_t channel) const;
	float applyTransferCurveUnit(float unit) const;
	bool loadCustomTransferCurveProfile();
	bool loadCustomTransferCurveProfileLut(const QString& profileKey, CustomTransferCurveLut& lut) const;
	bool buildRuntimeInterpolatedTransferCurveLut(const QString& lowerProfileId, const QString& upperProfileId, int blend, CustomTransferCurveLut& lut, QString& error) const;
	bool resolveDaytimeUpliftProfileLut(const QString& profileId, CustomTransferCurveLut& lut) const;
	bool isCalibrationHeaderEnabled() const;
	uint16_t applyCalibrationHeaderQ16(uint16_t q16, uint8_t channel) const;
	uint16_t applyHostChannelPipelineQ16(uint16_t q16, uint8_t channel) const;
	bool loadCustomCalibrationHeaderProfile();
	bool loadCustomRgbwLutProfile();
	void loadBuiltInSolverProfile();
	bool loadCustomSolverProfile();
	void rebuildSolverLookupTables();
	const std::vector<SolverLadderEntry>& solverLadderForChannel(uint8_t channel) const;
	SolverEncodedState solveStateForQ16(uint16_t targetQ16, uint8_t channel, uint8_t minBfi, uint8_t maxBfi, bool preferHigherBfi) const;
	SolverEncodedState solveBaselineStateForQ16(uint16_t targetQ16, uint8_t channel) const;
	float computeLegacySolvedRgbWeightedLumaUnit(uint16_t redQ16, uint16_t greenQ16, uint16_t blueQ16) const;
	float computeDirectSolvedRgbWeightedLumaUnit(uint16_t redQ16, uint16_t greenQ16, uint16_t blueQ16) const;
	float computeLegacySolvedRgbwWeightedLumaUnit(const HostOwnedRgbwQ16& rgbw) const;
	float computeDirectSolvedRgbwWeightedLumaUnit(const HostOwnedRgbwQ16& rgbw) const;
	void rebuildHostRgbwReferenceData();
	uint16_t prepareHostRgbwInputQ16(uint16_t q16, uint8_t channel) const;
	HostOwnedRgbwQ16 applyTransferCurveToHostOwnedRgbw(const HostOwnedRgbwQ16& rgbw) const;
	float computeHostRgbwGatedWhiteness(uint16_t redQ16, uint16_t greenQ16, uint16_t blueQ16) const;
	HostOwnedRgbwQ16 sampleCustomRgbwLut(uint16_t redQ16, uint16_t greenQ16, uint16_t blueQ16) const;
	HostOwnedRgbwQ16 buildHostRgbwPolicySample(uint16_t redQ16, uint16_t greenQ16, uint16_t blueQ16) const;
	void createHeader();
	size_t highlightMaskBytes() const;
	void writeColorAs16Bit(uint8_t*& writer, const ColorRgb& rgb) const;
	void writeColorAs16Bit(uint8_t*& writer, const linalg::aliases::float3& rgb) const;
	void writeColorAs12BitCarry(uint8_t*& writer, const ColorRgb& rgb) const;
	void writeColorAs12BitCarry(uint8_t*& writer, const linalg::aliases::float3& rgb) const;
	void whiteChannelExtension(uint8_t*& writer) const;
	void transferCurveExtension(uint8_t*& writer) const;
	std::array<uint16_t, 4> quantizeHostOwnedRgbwCarry(size_t ledIndex, uint16_t redQ16, uint16_t greenQ16, uint16_t blueQ16, uint16_t whiteQ16);
	bool shouldUseEmulatedRgbwPolicy() const;
	HostOwnedRgbwQ16 emulatePolicyRgbwSample(uint16_t redQ16, uint16_t greenQ16, uint16_t blueQ16) const;
	void buildHostOwnedRgbwFrame(const SharedOutputColors& nonlinearRgbColors, std::vector<HostOwnedRgbwQ16>& rgbwFrame);
	void buildPolicyRgbwFrame(const SharedOutputColors& nonlinearRgbColors, std::vector<HostOwnedRgbwQ16>& rgbwFrame) const;
	void buildPolicyRgbwFrame(const std::vector<ColorRgb>& ledValues, std::vector<HostOwnedRgbwQ16>& rgbwFrame) const;
	void computeHighlightShadowMask(const SharedOutputColors& nonlinearRgbColors, std::vector<uint8_t>& highlightMask) const;
	void computeHighlightShadowMask(const std::vector<ColorRgb>& ledValues, std::vector<uint8_t>& highlightMask) const;
	void computeHighlightShadowMask(const std::vector<HostOwnedRgbwQ16>& rgbwFrame, std::vector<uint8_t>& highlightMask) const;
	void captureHostPolicyProbeSample(
		size_t ledIndex,
		bool highlighted,
		bool hostOwnedRgbw,
		const std::array<uint16_t, 4>& processedQ16,
		const std::array<uint16_t, 4>& policyQ16,
		const std::array<uint16_t, 4>& baselineQ16,
		const std::array<uint8_t, 4>& baselineBfi,
		const std::array<uint8_t, 4>& finalBfi,
		const std::array<uint16_t, 4>& emittedQ16);
	void accumulateScenePolicyStats(uint32_t highlightedLeds);
	int writeFrameAsReports(const uint8_t* frameData, size_t frameSize);
	int drainIncomingReports();
	void handleIncomingReport(const uint8_t* report, size_t reportSize);
	void handleIncomingLogPayload(const uint8_t* payload, uint16_t payloadSize);

	QString _devicePath;
	bool _isAutoDevicePath;
	uint16_t _vid;
	uint16_t _pid;
	int _delayAfterConnect_ms;
	int _writeTimeout_ms;
	int _reportSize;
	bool _readLogChannel;
	int _logPollIntervalFrames;
	int _logReadBudget;
	int _logPollCounter;
	QString _incomingLogLine;
	int _frameDropCounter;
	int _fd;
	QString _configurationPath;
	PayloadMode _payloadMode;
	TransferCurveOwner _transferCurveOwner;
	TransferCurveProfile _transferCurveProfile;
	QString _transferCurveCustomProfileKey;
	CustomTransferCurveLut _transferCurveCustomLut;
	bool _runtimeInterpolatedTransferCurveActive;
	QString _runtimeInterpolatedTransferCurveLowerProfileId;
	QString _runtimeInterpolatedTransferCurveUpperProfileId;
	int _runtimeInterpolatedTransferCurveBlend;
	bool _daytimeUpliftEnabled = false;
	uint8_t _daytimeUpliftBlend = 0;
	QString _daytimeUpliftProfileId;
	CustomTransferCurveLut _daytimeUpliftLut;
	CalibrationHeaderOwner _calibrationHeaderOwner;
	CalibrationHeaderProfile _calibrationHeaderProfile;
	QString _calibrationHeaderCustomProfileKey;
	CustomCalibrationHeaderLut _calibrationHeaderCustomLut;
	QString _rgbwLutProfileId;
	CustomRgbwLutGrid _rgbwLutCustomGrid;
	QString _solverProfileId;
	std::array<std::vector<SolverLadderEntry>, 4> _solverLadders;
	SolverLookupTables _solverLookupTables;
	HostColorTransformOrder _hostColorTransformOrder;
	HostRgbwLutTransferOrder _hostRgbwLutTransferOrder;
	ScenePolicyMode _scenePolicyMode;
	bool _scenePolicyEnableHighlight;
	uint16_t _highlightShadowQ16Threshold;
	uint8_t _highlightShadowPercent;
	uint8_t _highlightShadowMinPeakDelta;
	uint8_t _highlightShadowUniformSpreadMax;
	uint8_t _highlightShadowTriggerMargin;
	uint8_t _highlightShadowMinSceneMedian;
	uint8_t _highlightShadowMinSceneAvg;
	uint8_t _highlightShadowDimUniformMedian;
	HostRgbwMode _hostRgbwMode;
	bool _enable_ice_rgbw;
	bool _hostRgbwApplyD65Correction;
	float _hostRgbwD65Strength;
	bool _ice_rgbw_temporal_dithering;
	float _hostRgbwGatedUvRadius;
	float _hostRgbwGatedPower;
	float _hostRgbwCentroidRedX;
	float _hostRgbwCentroidRedY;
	float _hostRgbwCentroidGreenX;
	float _hostRgbwCentroidGreenY;
	float _hostRgbwCentroidBlueX;
	float _hostRgbwCentroidBlueY;
	float _hostRgbwCentroidWhiteX;
	float _hostRgbwCentroidWhiteY;
	linalg::aliases::float3 _ice_white_temperatur;
	float _ice_white_mixer_threshold;
	float _ice_white_led_intensity;
	InfiniteColorEngineRgbw _infiniteColorEngineRgbw;
	std::vector<std::array<float, 4>> _ice_rgbw_carry_error;
	bool _hostRgbwReferenceValid;
	std::array<std::array<double, 3>, 3> _hostRgbwRgbBasis{};
	std::array<double, 3> _hostRgbwMeasuredExactVector{};
	std::array<double, 3> _hostRgbwD65ExactVector{};
	std::array<float, 3> _hostRgbwMeasuredMinVector{};
	std::array<float, 3> _hostRgbwD65MinVector{};
	std::array<double, 2> _hostRgbwMeasuredWhiteUv{};
	std::array<double, 2> _hostRgbwD65WhiteUv{};
	bool _white_channel_calibration;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;
	uint32_t _scenePolicyStatsFrames;
	uint32_t _scenePolicyStatsHighlightFrames;
	uint64_t _scenePolicyStatsHighlightedLeds;
	uint32_t _scenePolicyStatsHighlightPeakLeds;
	uint8_t _scenePolicyLastHighlightAvg;
	uint8_t _scenePolicyLastHighlightMedian;
	uint8_t _scenePolicyLastHighlightP95;
	uint8_t _scenePolicyLastHighlightSpread;
	uint8_t _scenePolicyLastHighlightGate;
	uint16_t _scenePolicyLastHighlightCandidates;
	uint8_t _scenePolicyLastHighlightRejectCode;
	HostPolicyProbeSample _hostPolicyProbeHighlighted;
	HostPolicyProbeSample _hostPolicyProbeCapped;
	const short _headerSize;

	static bool isRegistered;
};
