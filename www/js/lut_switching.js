$(document).ready(function () {
	performTranslation();
	const perInputIds = ["ugoos", "appletv", "steamdeck", "rx3"];
	const perModeIds = ["sdr", "hdr", "lldv"];
	const metadataBucketGroups = [
		{
			id: "hdrMdl1000",
			configSuffix: "HdrMdl1000LutFiles",
			thresholdSelector: "#lut_switching_kodi_hdr_mdl1000_thresholds",
			defaultThresholds: ["250"],
			title: "HDR MDL1000 buckets",
			helpText: "Blank HDR MDL1000 buckets fall back to the shared HDR LUT.",
			inputs: ["ugoos", "appletv", "steamdeck", "rx3"],
			summarySelector: "#lut_switching_kodi_hdr_mdl1000_threshold_summary"
		},
		{
			id: "hdrMdl4000",
			configSuffix: "HdrMdl4000LutFiles",
			thresholdSelector: "#lut_switching_kodi_hdr_mdl4000_thresholds",
			defaultThresholds: ["250"],
			title: "HDR MDL4000 buckets",
			helpText: "Blank HDR MDL4000 buckets fall back to the shared HDR LUT.",
			inputs: ["ugoos", "appletv", "steamdeck", "rx3"],
			summarySelector: "#lut_switching_kodi_hdr_mdl4000_threshold_summary"
		},
		{
			id: "dvMdl1000",
			configSuffix: "DolbyVisionMdl1000LutFiles",
			thresholdSelector: "#lut_switching_kodi_dv_mdl1000_thresholds",
			defaultThresholds: ["69", "120", "200"],
			title: "Dolby Vision L1/FLL MDL1000 buckets",
			helpText: "Blank MDL1000 buckets fall back to the shared LLDV LUT.",
			inputs: ["ugoos"],
			summarySelector: "#lut_switching_kodi_dv_mdl1000_threshold_summary"
		},
		{
			id: "dvMdl4000",
			configSuffix: "DolbyVisionMdl4000LutFiles",
			thresholdSelector: "#lut_switching_kodi_dv_mdl4000_thresholds",
			defaultThresholds: ["69", "120", "200", "400"],
			title: "Dolby Vision L1/FLL MDL4000 buckets",
			helpText: "Blank MDL4000 buckets fall back to the matching MDL1000 bucket, then to the shared LLDV LUT.",
			inputs: ["ugoos"],
			summarySelector: "#lut_switching_kodi_dv_mdl4000_threshold_summary"
		}
	];
	const transferProfileGroups = [
		{
			id: "hdrMdl1000Transfer",
			configKey: "transferHdrMdl1000Profiles",
			title: "HDR MDL1000 transfer profiles",
			helpText: "One transfer profile per HDR MDL1000 bucket.",
			mode: "hdr-buckets",
			thresholdSelector: "#lut_switching_kodi_hdr_mdl1000_thresholds",
			defaultThresholds: ["250"]
		},
		{
			id: "hdrMdl4000Transfer",
			configKey: "transferHdrMdl4000Profiles",
			title: "HDR MDL4000 transfer profiles",
			helpText: "One transfer profile per HDR MDL4000 bucket.",
			mode: "hdr-buckets",
			thresholdSelector: "#lut_switching_kodi_hdr_mdl4000_thresholds",
			defaultThresholds: ["250"]
		},
		{
			id: "dvMdl1000Transfer",
			configKey: "transferDolbyVisionMdl1000Profiles",
			title: "Dolby Vision MDL1000 transfer ranks",
			helpText: "These ranks scale from the active MDL1000 bucket count using the configured ratio.",
			mode: "ranks",
			thresholdSelector: "#lut_switching_kodi_dv_mdl1000_thresholds",
			ratioNumeratorKey: "transferDolbyVisionMdl1000RatioNumerator",
			ratioDenominatorKey: "transferDolbyVisionMdl1000RatioDenominator",
			ratioNumeratorSelector: "#lut_switching_dv_mdl1000_ratio_numerator",
			ratioDenominatorSelector: "#lut_switching_dv_mdl1000_ratio_denominator",
			defaultRatioNumerator: 2,
			defaultRatioDenominator: 4
		},
		{
			id: "dvMdl4000Transfer",
			configKey: "transferDolbyVisionMdl4000Profiles",
			title: "Dolby Vision MDL4000 transfer ranks",
			helpText: "These ranks scale from the active MDL4000 bucket count using the configured ratio.",
			mode: "ranks",
			thresholdSelector: "#lut_switching_kodi_dv_mdl4000_thresholds",
			ratioNumeratorKey: "transferDolbyVisionMdl4000RatioNumerator",
			ratioDenominatorKey: "transferDolbyVisionMdl4000RatioDenominator",
			ratioNumeratorSelector: "#lut_switching_dv_mdl4000_ratio_numerator",
			ratioDenominatorSelector: "#lut_switching_dv_mdl4000_ratio_denominator",
			defaultRatioNumerator: 3,
			defaultRatioDenominator: 5
		}
	];
	const LIVE_STREAM_REFRESH_MS = 500;
	const LIVE_RUNTIME_REFRESH_MS = 1500;
	let streamRefreshInFlight = false;
	let runtimeRefreshInFlight = false;
	let lutSwitchingFormState = {};

	function cloneStringArray(value)
	{
		return normalizeStringArray(value).slice();
	}

	function cloneLutSwitchingSettings(settings = {})
	{
		const clone = Object.assign({}, settings || {});
		metadataBucketGroups.forEach(function (group) {
			perInputIds.forEach(function (inputId) {
				clone[metadataConfigKey(inputId, group)] = cloneStringArray(settings[metadataConfigKey(inputId, group)]);
			});
		});

		transferProfileGroups.forEach(function (group) {
			clone[group.configKey] = cloneStringArray(settings[group.configKey]);
		});

		return clone;
	}

	function ensureFormStateArray(key)
	{
		if (!Array.isArray(lutSwitchingFormState[key]))
			lutSwitchingFormState[key] = cloneStringArray(lutSwitchingFormState[key]);
		return lutSwitchingFormState[key];
	}

	function syncMetadataBucketValuesToState()
	{
		$(".lut-switching-metadata-input").each(function () {
			const input = $(this);
			const key = input.data("configKey");
			const index = Number(input.data("bucketIndex"));
			if (!key || !Number.isFinite(index))
				return;

			const values = ensureFormStateArray(key);
			values[index] = input.val().trim();
		});
	}

	function syncTransferProfileValuesToState()
	{
		$(".lut-switching-transfer-input").each(function () {
			const input = $(this);
			const key = input.data("configKey");
			const index = Number(input.data("profileIndex"));
			if (!key || !Number.isFinite(index))
				return;

			const values = ensureFormStateArray(key);
			values[index] = input.val().trim();
		});
	}

	function boolLabel(value)
	{
		return value ? $.i18n("lut_switching_yes") : $.i18n("lut_switching_no");
	}

	function valueOrFallback(value, fallback = "-")
	{
		if (value === null || typeof value === "undefined")
			return fallback;

		if (typeof value === "string")
		{
			const trimmed = value.trim();
			return trimmed.length > 0 ? trimmed : fallback;
		}

		return value;
	}

	function joinNonEmpty(values, fallback = "-")
	{
		const parts = (values || []).filter(function (value) {
			return typeof value === "string" && value.trim().length > 0;
		});
		return parts.length > 0 ? parts.join(" / ") : fallback;
	}

	function parsePositiveInt(value, fallback)
	{
		const parsed = parseInt(value, 10);
		return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
	}

	function parseBoundedFloat(value, fallback, min, max)
	{
		const parsed = parseFloat(value);
		if (!Number.isFinite(parsed))
			return fallback;
		return Math.min(max, Math.max(min, parsed));
	}

	function scaleRankCountFromBuckets(bucketCount, ratioNumerator, ratioDenominator)
	{
		const normalizedBuckets = Math.max(1, parsePositiveInt(bucketCount, 1));
		const normalizedNumerator = Math.max(1, parsePositiveInt(ratioNumerator, 1));
		const normalizedDenominator = Math.max(1, parsePositiveInt(ratioDenominator, normalizedBuckets));
		const scaled = Math.round((normalizedBuckets * normalizedNumerator) / normalizedDenominator);
		return Math.max(1, Math.min(normalizedBuckets, scaled));
	}

	function getConfiguredTransferRatioValue(group, part)
	{
		if (!group || group.mode !== "ranks")
			return 1;

		const selector = part === "numerator" ? group.ratioNumeratorSelector : group.ratioDenominatorSelector;
		const stateKey = part === "numerator" ? group.ratioNumeratorKey : group.ratioDenominatorKey;
		const fallback = part === "numerator" ? group.defaultRatioNumerator : group.defaultRatioDenominator;

		if (selector)
		{
			const element = $(selector);
			if (element.length > 0)
				return parsePositiveInt(element.val(), fallback);
		}

		return parsePositiveInt(lutSwitchingFormState[stateKey], fallback);
	}

	function getDerivedTransferRankCount(group)
	{
		if (!group || group.mode !== "ranks" || !group.thresholdSelector)
			return 1;

		const thresholds = parseThresholdList($(group.thresholdSelector).val(), group.defaultThresholds || []);
		const bucketCount = thresholds.length + 1;
		return scaleRankCountFromBuckets(
			bucketCount,
			getConfiguredTransferRatioValue(group, "numerator"),
			getConfiguredTransferRatioValue(group, "denominator")
		);
	}

	function formatNumber(value, suffix = "")
	{
		if (value === null || typeof value === "undefined" || value === "")
			return "-";

		const numeric = Number(value);
		if (!Number.isFinite(numeric))
			return valueOrFallback(value);

		const formatted = Number.isInteger(numeric) ? String(numeric) : numeric.toFixed(1);
		return suffix ? formatted + " " + suffix : formatted;
	}

	function formatKodiTriplet(values, suffix)
	{
		const parts = values.map(function (value) {
			return value === null || typeof value === "undefined" ? "-" : formatNumber(value);
		});
		const joined = parts.join(" | ");
		return suffix ? joined + " " + suffix : joined;
	}

	function formatHdrInfo(hdr)
	{
		if (!hdr || Object.keys(hdr).length === 0)
			return "-";

		const parts = [];
		if (hdr.eotf)
			parts.push("EOTF: " + hdr.eotf);
		if (hdr.max_display_lum_nits)
			parts.push("Mastering max: " + hdr.max_display_lum_nits + " nits");
		if (hdr.max_cll_nits)
			parts.push("MaxCLL: " + hdr.max_cll_nits + " nits");
		if (hdr.max_fall_nits)
			parts.push("MaxFALL: " + hdr.max_fall_nits + " nits");
		return parts.length > 0 ? parts.join(", ") : "-";
	}

	function formatDvSinkSummary(dvSink, label)
	{
		if (!dvSink || Object.keys(dvSink).length === 0)
			return "";

		const parts = [];
		if (label)
			parts.push(label + ":");
		if (dvSink.interface)
			parts.push(dvSink.interface);
		if (dvSink.luminance && dvSink.luminance.maxNits)
			parts.push("max " + dvSink.luminance.maxNits + " nits");
		if (dvSink.backlightControl && dvSink.backlightControl.defaultNits)
			parts.push("default " + dvSink.backlightControl.defaultNits + " nits");
		return parts.join(" ");
	}

	function formatDvInfo(state)
	{
		const dv = state.dv || {};
		if (!dv || Object.keys(dv).length === 0)
			return "-";

		const parts = [];
		if (typeof dv.active !== "undefined")
			parts.push(boolLabel(dv.active));
		if (dv.kind)
			parts.push(dv.kind);
		if (dv.interface && dv.interface !== "none")
			parts.push(dv.interface);

		const sinkParts = [];
		const tx0Summary = formatDvSinkSummary(state.dvSinkTx0, "TX0");
		const tx1Summary = formatDvSinkSummary(state.dvSinkTx1, "TX1");
		if (tx0Summary)
			sinkParts.push(tx0Summary);
		if (tx1Summary)
			sinkParts.push(tx1Summary);
		if (sinkParts.length > 0)
			parts.push(sinkParts.join(" | "));

		return parts.length > 0 ? parts.join(" / ") : "-";
	}

	function formatTransferAutomation(automation)
	{
		if (!automation || Object.keys(automation).length === 0)
			return "-";

		const parts = [];
		if (automation.category)
		{
			let categoryLabel = automation.category;
			if (automation.tier)
				categoryLabel += " " + automation.tier;
			if (Number.isFinite(Number(automation.tierIndex)) && Number.isFinite(Number(automation.tierCount)))
				categoryLabel += " [" + (Number(automation.tierIndex) + 1) + "/" + Number(automation.tierCount) + "]";
			parts.push(categoryLabel);
		}
		if (automation.tierStrategy)
			parts.push(automation.tierStrategy);
		if (Number.isFinite(Number(automation.bucketIndex)) && Number.isFinite(Number(automation.bucketCount)))
			parts.push("bucket " + (Number(automation.bucketIndex) + 1) + "/" + Number(automation.bucketCount));
		if (automation.interpolationActive === true)
		{
			const lowerProfile = valueOrFallback(automation.targetInterpolationLowerProfile || automation.interpolationLowerProfile);
			const upperProfile = valueOrFallback(automation.targetInterpolationUpperProfile || automation.interpolationUpperProfile);
			const lowerTierIndex = Number(automation.targetInterpolationLowerTierIndex ?? automation.interpolationLowerTierIndex);
			const upperTierIndex = Number(automation.targetInterpolationUpperTierIndex ?? automation.interpolationUpperTierIndex);
			let interpolationLabel = "transfer soft zone";
			if (Number.isFinite(lowerTierIndex) && Number.isFinite(upperTierIndex) && Number.isFinite(Number(automation.tierCount)))
				interpolationLabel += " rank " + (lowerTierIndex + 1) + " -> " + (upperTierIndex + 1);
			if (lowerProfile !== "-" || upperProfile !== "-")
				interpolationLabel += " " + lowerProfile + " + " + upperProfile;
			parts.push(interpolationLabel);
			if (Number.isFinite(Number(automation.targetInterpolationLowerBound)) && Number.isFinite(Number(automation.targetInterpolationUpperBound)))
				parts.push("window " + Number(automation.targetInterpolationLowerBound).toFixed(3) + ".." + Number(automation.targetInterpolationUpperBound).toFixed(3) + " nits");
			if (Number.isFinite(Number(automation.targetInterpolationRatio)))
				parts.push("target blend " + Number(automation.targetInterpolationRatio).toFixed(3));
		}
		if (automation.targetProfile)
			parts.push("target " + automation.targetProfile);
		if (automation.currentProfile)
			parts.push("current " + automation.currentProfile);
		if (automation.currentInterpolationActive === true)
		{
			const currentLower = valueOrFallback(automation.currentInterpolationLowerProfile);
			const currentUpper = valueOrFallback(automation.currentInterpolationUpperProfile);
			let currentInterpolationLabel = "current transfer blend";
			if (currentLower !== "-" || currentUpper !== "-")
				currentInterpolationLabel += " " + currentLower + " + " + currentUpper;
			if (Number.isFinite(Number(automation.currentInterpolationBlend)))
				currentInterpolationLabel += " (" + Number(automation.currentInterpolationBlend) + "/255)";
			parts.push(currentInterpolationLabel);
		}
		if (automation.applied === true)
			parts.push($.i18n("lut_switching_transfer_state_applied"));
		else if (automation.needsApply === false && automation.targetProfile)
			parts.push($.i18n("lut_switching_transfer_state_unchanged"));
		else if (automation.reason)
			parts.push(automation.reason);

		return parts.length > 0 ? parts.join(" / ") : "-";
	}

	function formatMetricSourceLabel(sourceText)
	{
		const source = valueOrFallback(sourceText, "").trim();
		if (!source)
			return "";

		const labels = {
			"hdr.max_cll_nits": "HDR static metadata: MaxCLL",
			"hdr.max_cll_nits:fallback": "HDR family fallback: MaxCLL",
			"hdr.max_display_lum_nits": "HDR static metadata: mastering max luminance",
			"hdr.max_fall_nits": "HDR static metadata: MaxFALL",
			"kodiDv.sourceMaxNits": "Kodi DV source metadata: source max nits",
			"kodiDv.rpuMdlNits": "Kodi DV source metadata: source max nits",
			"kodiDv.l6MaxLumNits": "Kodi DV source metadata: L6 mastering max",
			"kodiDv.l6MaxCllNits": "Kodi DV source metadata: L6 MaxCLL",
			"kodiDv.fllAvgCdM2": "Kodi DV runtime metadata: FLL avg",
			"kodiDv.fllAvgCdM2:fallback": "Kodi DV runtime metadata fallback: FLL avg",
			"kodiDv.fllUpperCdM2": "Kodi DV runtime metadata: FLL upper",
			"kodiDv.fllUpperCdM2:fallback": "Kodi DV runtime metadata fallback: FLL upper",
			"kodiDv.l1AvgNits": "Kodi DV runtime metadata: L1 avg",
			"kodiDv.l1AvgNits:selected": "Kodi DV runtime metadata: selected L1 avg",
			"kodiDv.l1AvgNits:fallback": "Kodi DV runtime metadata fallback: L1 avg",
			"kodiDv.l1MaxNits": "Kodi DV runtime metadata: L1 max",
			"kodiDv.l1MaxNits:selected": "Kodi DV runtime metadata: selected L1 max",
			"kodiDv.l1MaxNits:fallback": "Kodi DV runtime metadata fallback: L1 max",
			"kodiDv.sceneAverageIndex": "Kodi DV runtime metadata: scene average index",
			"hdr.sceneAverageIndex": "HDR static metadata: scene average index"
		};

		return labels[source] || source;
	}

	function formatTransferNits(automation)
	{
		if (!automation || Object.keys(automation).length === 0)
			return "-";

		if (automation.nits)
		{
			const sourceText = automation.nitsSource ? String(automation.nitsSource) : "";
			const unit = sourceText.includes("CdM2") ? "cd/m^2" : "nits";
			const sourceLabel = formatMetricSourceLabel(sourceText);
			const parts = [formatNumber(automation.nits, unit) + (sourceLabel ? " (" + sourceLabel + ")" : "")];
			const metricDetails = automation.metricDetails || {};
			if (metricDetails.mode === "sceneAverageIndex")
			{
				const configuredWeight = Number(metricDetails.configuredHighlightWeight);
				const effectiveWeight = Number(metricDetails.highlightWeight);
				if (Number.isFinite(configuredWeight) && Number.isFinite(effectiveWeight))
				{
					let weightLabel = "k " + configuredWeight.toFixed(2);
					if (Math.abs(effectiveWeight - configuredWeight) > 0.0005)
						weightLabel += " -> " + effectiveWeight.toFixed(2);
					parts.push(weightLabel);
				}
				if (metricDetails.highlightGatingActive === true && Number.isFinite(Number(metricDetails.highlightRatio)))
					parts.push("highlight gated ratio " + Number(metricDetails.highlightRatio).toFixed(2));
			}
			return parts.join(" / ");
		}

		if (automation.nitsSource)
			return formatMetricSourceLabel(automation.nitsSource);

		return automation.reason || "-";
	}

	function formatLutBucketDecision(decision)
	{
		if (!decision || Object.keys(decision).length === 0)
			return "-";

		const parts = [];
		if (decision.mode)
			parts.push(String(decision.mode).toUpperCase());
		if (decision.family)
			parts.push(decision.family);
		if (Number.isFinite(Number(decision.bucketIndex)) && Number.isFinite(Number(decision.bucketCount)))
			parts.push("bucket " + (Number(decision.bucketIndex) + 1) + "/" + Number(decision.bucketCount));
		if (decision.interpolationActive && decision.targetFile && decision.targetInterpolationFile)
		{
			const blendRatio = typeof decision.interpolationBlend !== "undefined"
				? Math.round((Number(decision.interpolationBlend) / 255) * 100)
				: (typeof decision.interpolationRatio !== "undefined" ? Math.round(Number(decision.interpolationRatio) * 100) : null);
			const interpolationLabel = decision.interpolationMode === "softZone" ? "target soft zone blend " : "target blend ";
			parts.push(interpolationLabel + decision.targetFile + " + " + decision.targetInterpolationFile + (blendRatio !== null ? " (" + blendRatio + "%)" : ""));
		}
		else if (decision.targetFile)
			parts.push("target " + decision.targetFile);
		if (decision.currentFile)
			parts.push("current " + decision.currentFile);
		if (decision.currentInterpolationFile)
		{
			const currentBlendRatio = typeof decision.currentInterpolationBlend !== "undefined"
				? Math.round((Number(decision.currentInterpolationBlend) / 255) * 100)
				: null;
			parts.push("current blend " + decision.currentInterpolationFile + (currentBlendRatio !== null ? " (" + currentBlendRatio + "%)" : ""));
		}
		if (decision.applied === true)
			parts.push("Applied");
		else if (decision.needsApply === false && decision.targetFile)
			parts.push("Aligned");
		else if (decision.reason)
			parts.push(decision.reason);

		return parts.length > 0 ? parts.join(" / ") : "-";
	}

	function formatLutBucketNits(decision)
	{
		if (!decision || Object.keys(decision).length === 0)
			return "-";

		const parts = [];
		if (typeof decision.metricValue !== "undefined" && decision.metricValue !== null)
			parts.push(formatNumber(decision.metricValue, decision.metricUnit || "nits"));
		else if (decision.nits)
			parts.push(formatNumber(decision.nits, "nits"));

		if (decision.metricSource)
			parts.push(formatMetricSourceLabel(decision.metricSource));
		else if (decision.nitsSource)
			parts.push(formatMetricSourceLabel(decision.nitsSource));
		const metricDetails = decision.metricDetails || {};
		if (metricDetails.mode === "sceneAverageIndex")
		{
			const configuredWeight = Number(metricDetails.configuredHighlightWeight);
			const effectiveWeight = Number(metricDetails.highlightWeight);
			if (Number.isFinite(configuredWeight) && Number.isFinite(effectiveWeight))
			{
				let weightLabel = "k " + configuredWeight.toFixed(2);
				if (Math.abs(effectiveWeight - configuredWeight) > 0.0005)
					weightLabel += " -> " + effectiveWeight.toFixed(2);
				parts.push(weightLabel);
			}
			if (metricDetails.highlightGatingActive === true && Number.isFinite(Number(metricDetails.highlightRatio)))
				parts.push("highlight gated ratio " + Number(metricDetails.highlightRatio).toFixed(2));
		}
		if (typeof decision.interpolationLowerBound !== "undefined" && typeof decision.interpolationUpperBound !== "undefined")
			parts.push((decision.interpolationMode === "softZone" ? "soft zone " : "blend window ") + formatNumber(decision.interpolationLowerBound, decision.metricUnit || "nits") + " to " + formatNumber(decision.interpolationUpperBound, decision.metricUnit || "nits"));
		if (decision.thresholds)
			parts.push("thresholds " + (Array.isArray(decision.thresholds) ? decision.thresholds.join(", ") : String(decision.thresholds)));
		if (decision.reason && parts.length === 0)
			parts.push(decision.reason);

		return parts.length > 0 ? parts.join(" / ") : "-";
	}

	function normalizeStringArray(value)
	{
		if (Array.isArray(value))
		{
			return value.map(function (item) {
				return typeof item === "string" ? item.trim() : "";
			});
		}

		if (typeof value === "string")
			return value.split(/[\r\n,;|]+/).map(function (item) { return item.trim(); });

		return [];
	}

	function parseThresholdList(value, defaults)
	{
		const raw = typeof value === "string" ? value : "";
		const tokens = raw.split(/[\s,;|]+/).filter(function (token) {
			return token.trim().length > 0;
		});
		return tokens.length > 0 ? tokens : defaults.slice();
	}

	function metadataConfigKey(inputId, group)
	{
		return inputId + group.configSuffix;
	}

	function groupAppliesToInput(group, inputId)
	{
		return (group.inputs || []).includes(inputId);
	}

	function metadataInputId(inputId, groupId, bucketIndex)
	{
		return "lut_switching_metadata_" + inputId + "_" + groupId + "_" + bucketIndex;
	}

	function getInputDisplayLabel(inputId)
	{
		const option = $("#lut_switching_input option[value='" + inputId + "']");
		return option.length > 0 ? option.text().trim() : inputId;
	}

	function collectMetadataBucketValuesFromDom()
	{
		const values = {};
		$(".lut-switching-metadata-input").each(function () {
			const input = $(this);
			const key = input.data("configKey");
			const index = Number(input.data("bucketIndex"));
			if (!key || !Number.isFinite(index))
				return;

			if (!Array.isArray(values[key]))
				values[key] = [];

			values[key][index] = input.val().trim();
		});
		return values;
	}

	function buildMetadataBucketLabel(thresholds, bucketIndex)
	{
		if (thresholds.length === 0)
			return "Bucket " + (bucketIndex + 1);

		if (bucketIndex === 0)
			return "Bucket 1: <= " + thresholds[0] + " cd/m^2";

		if (bucketIndex === thresholds.length)
			return "Bucket " + (bucketIndex + 1) + ": > " + thresholds[thresholds.length - 1] + " cd/m^2";

		return "Bucket " + (bucketIndex + 1) + ": > " + thresholds[bucketIndex - 1] + " and <= " + thresholds[bucketIndex] + " cd/m^2";
	}

	function buildTransferRankLabel(rankIndex, rankCount)
	{
		return "Rank " + (rankIndex + 1) + "/" + rankCount;
	}

	function transferProfileInputId(groupId, itemIndex)
	{
		return "lut_switching_transfer_profile_" + groupId + "_" + itemIndex;
	}

	function collectTransferProfileValuesFromDom()
	{
		const values = {};
		$(".lut-switching-transfer-input").each(function () {
			const input = $(this);
			const key = input.data("configKey");
			const index = Number(input.data("profileIndex"));
			if (!key || !Number.isFinite(index))
				return;

			if (!Array.isArray(values[key]))
				values[key] = [];

			values[key][index] = input.val().trim();
		});
		return values;
	}

	function renderTransferProfileEditors(settings = {})
	{
		const container = $("#lut_switching_transfer_bucket_sections");
		syncTransferProfileValuesToState();
		container.empty();

		transferProfileGroups.forEach(function (group) {
			const card = $("<div>").addClass("col-12");
			const box = $("<div>").addClass("border rounded p-3 h-100");
			box.append($("<div>").addClass("fw-semibold mb-2").text(group.title));
			if (group.helpText)
				box.append($("<div>").addClass("small text-muted mb-3").text(group.helpText));

			const sourceValues = Array.isArray(lutSwitchingFormState[group.configKey]) ? lutSwitchingFormState[group.configKey] : normalizeStringArray(settings[group.configKey]);
			let labels = [];

			if (group.mode === "hdr-buckets")
			{
				const thresholds = parseThresholdList($(group.thresholdSelector).val(), group.defaultThresholds || ["250"]);
				for (let index = 0; index <= thresholds.length; index += 1)
					labels.push(buildMetadataBucketLabel(thresholds, index));
			}
			else
			{
				const count = getDerivedTransferRankCount(group);
				if (sourceValues.length > count)
				{
					sourceValues.length = count;
					lutSwitchingFormState[group.configKey] = sourceValues.slice();
				}
				for (let index = 0; index < count; index += 1)
					labels.push(buildTransferRankLabel(index, count));
			}

			labels.forEach(function (label, index) {
				const inputId = transferProfileInputId(group.id, index);
				const wrapper = $("<div>").addClass(index === labels.length - 1 ? "mb-0" : "mb-2");
				wrapper.append($("<label>").addClass("form-label form-label-sm").attr("for", inputId).text(label));
				wrapper.append(
					$("<input>")
						.addClass("form-control form-control-sm lut-switching-transfer-input")
						.attr("id", inputId)
						.attr("type", "text")
						.attr("list", "lut_switching_transfer_profile_options")
						.attr("placeholder", "Leave blank to disable this rank")
						.attr("data-config-key", group.configKey)
						.attr("data-profile-index", index)
						.val(sourceValues[index] || "")
				);
				box.append(wrapper);
			});

			card.append(box);
			container.append(card);
		});
	}

	function renderMetadataBucketEditors(settings = {})
	{
		const container = $("#lut_switching_metadata_bucket_sections");
		syncMetadataBucketValuesToState();
		container.empty();

		metadataBucketGroups.forEach(function (group) {
			const thresholds = parseThresholdList($(group.thresholdSelector).val(), group.defaultThresholds);
			group.thresholds = thresholds;
		});
		updateThresholdSummaries();

		perInputIds.forEach(function (inputId) {
			const column = $("<div>").addClass("col-12");
			const card = $("<div>").addClass("border rounded p-3 h-100");
			card.append($("<div>").addClass("fw-semibold mb-3").text(getInputDisplayLabel(inputId)));
			let visibleGroupIndex = 0;

			metadataBucketGroups.forEach(function (group) {
				if (!groupAppliesToInput(group, inputId))
					return;

				const configKey = metadataConfigKey(inputId, group);
				const sourceValues = Array.isArray(lutSwitchingFormState[configKey]) ? lutSwitchingFormState[configKey] : normalizeStringArray(settings[configKey]);
				const section = $("<div>").addClass(visibleGroupIndex === 0 ? "mb-3" : "mb-0");
				section.append($("<div>").addClass("small text-muted mb-2").text(group.title));
				if (group.helpText)
					section.append($("<div>").addClass("small text-muted mb-2").text(group.helpText));

				group.thresholds.forEach(function (_threshold, index) {
					const bucketId = metadataInputId(inputId, group.id, index);
					const wrapper = $("<div>").addClass("mb-2");
					wrapper.append($("<label>").addClass("form-label form-label-sm").attr("for", bucketId).text(buildMetadataBucketLabel(group.thresholds, index)));
					wrapper.append(
						$("<input>")
							.addClass("form-control form-control-sm lut-switching-metadata-input")
							.attr("id", bucketId)
							.attr("type", "text")
							.attr("placeholder", "Leave blank to fall back")
							.attr("data-config-key", configKey)
							.attr("data-bucket-index", index)
							.val(sourceValues[index] || "")
					);
					section.append(wrapper);
				});

				const finalBucketIndex = group.thresholds.length;
				const finalBucketId = metadataInputId(inputId, group.id, finalBucketIndex);
				const finalWrapper = $("<div>").addClass("mb-2");
				finalWrapper.append($("<label>").addClass("form-label form-label-sm").attr("for", finalBucketId).text(buildMetadataBucketLabel(group.thresholds, finalBucketIndex)));
				finalWrapper.append(
					$("<input>")
						.addClass("form-control form-control-sm lut-switching-metadata-input")
						.attr("id", finalBucketId)
						.attr("type", "text")
						.attr("placeholder", "Leave blank to fall back")
						.attr("data-config-key", configKey)
						.attr("data-bucket-index", finalBucketIndex)
						.val(sourceValues[finalBucketIndex] || "")
				);
				section.append(finalWrapper);

				card.append(section);
				visibleGroupIndex += 1;
			});

			column.append(card);
			container.append(column);
		});
	}

	function updateThresholdSummaries()
	{
		metadataBucketGroups.forEach(function (group) {
			const thresholds = parseThresholdList($(group.thresholdSelector).val(), group.defaultThresholds);
			const bucketCount = thresholds.length + 1;
			if (group.summarySelector)
				$(group.summarySelector).text("Thresholds: " + thresholds.join(", ") + " | Buckets: " + bucketCount);
		});
	}

	function populateTransferProfileOptions(headers)
	{
		const dataList = $("#lut_switching_transfer_profile_options");
		const options = ["disabled", "curve3_4_new"];

		(headers || []).forEach(function (header) {
			if (header && header.id)
				options.push(header.id);
		});

		dataList.empty();
		Array.from(new Set(options)).forEach(function (value) {
			dataList.append($("<option>").attr("value", value));
		});
	}

	function fillVideoStream(state)
	{
		const empty = $("#lut_switching_stream_empty");
		const content = $("#lut_switching_stream_content");
		const kodiDv = state ? (state.kodiDv || null) : null;

		if (!state || Object.keys(state).length === 0)
		{
			empty.show();
			content.hide();
			fillKodiDvState(null);
			$("#lut_switching_stream_snapshot").text("{}");
			return;
		}

		empty.hide();
		content.show();

		const signal = state.signal || {};
		const avi = state.avi || {};
		const hdr = state.hdr || {};
		const dv = state.dv || {};
		const input = joinNonEmpty([state.activeInput, state.activeDevice]);
		const transferAutomation = state.transferAutomation || {};
		const lutBucketDecision = state.lutBucketDecision || {};

		$("#lut_switching_stream_received").text(valueOrFallback(state.receivedAtUtc));
		$("#lut_switching_stream_reported_by").text(joinNonEmpty([state.reportedBy, state.peerAddress]));
		$("#lut_switching_stream_mode").text(valueOrFallback(state.classifiedMode));
		$("#lut_switching_stream_input").text(input);
		$("#lut_switching_stream_dynamic_range").text(valueOrFallback(signal.dynamic_range));
		$("#lut_switching_stream_chroma").text(valueOrFallback(signal.chroma_subsampling));
		$("#lut_switching_stream_bit_depth").text(signal.bit_depth ? signal.bit_depth + "-bit" : "-");
		$("#lut_switching_stream_pixel_clock").text(signal.pixel_clock_mhz ? signal.pixel_clock_mhz + " MHz" : "-");
		$("#lut_switching_stream_colorspace").text(joinNonEmpty([signal.colorspace_text, avi.colorimetry, avi.quantization]));
		$("#lut_switching_stream_avi").text(joinNonEmpty([avi.colorspace, avi.colorimetry, avi.quantization]));
		$("#lut_switching_stream_hdr").text(formatHdrInfo(hdr));
		$("#lut_switching_stream_dv").text(formatDvInfo(state));
		$("#lut_switching_stream_transfer").text(formatTransferAutomation(transferAutomation));
		$("#lut_switching_stream_transfer_nits").text(formatTransferNits(transferAutomation));
		$("#lut_switching_stream_lut_bucket").text(formatLutBucketDecision(lutBucketDecision));
		$("#lut_switching_stream_lut_bucket_nits").text(formatLutBucketNits(lutBucketDecision));
		fillKodiDvState(kodiDv);
		$("#lut_switching_stream_raw_status").text(valueOrFallback(state.rawStatus));
		$("#lut_switching_stream_snapshot").text(JSON.stringify(state, null, 2));
	}

	function fillKodiDvState(kodiDv)
	{
		const empty = $("#lut_switching_kodi_dv_empty");
		const content = $("#lut_switching_kodi_dv_content");

		if (!kodiDv || Object.keys(kodiDv).length === 0)
		{
			empty.show();
			content.hide();
			$("#lut_switching_kodi_dv_snapshot").text("{}");
			return;
		}

		empty.hide();
		content.show();
		$("#lut_switching_kodi_dv_status").text(kodiDv.available === true ? "Available" : "Waiting for active Kodi DV metadata");
		$("#lut_switching_kodi_dv_host").text(joinNonEmpty([kodiDv.host ? String(kodiDv.host) : "", kodiDv.port ? String(kodiDv.port) : ""], "-"));
		$("#lut_switching_kodi_dv_transport").text(valueOrFallback(kodiDv.transport));
		$("#lut_switching_kodi_dv_fll_source").text(valueOrFallback(kodiDv.fllSource));
		$("#lut_switching_kodi_dv_l1").text(formatKodiTriplet([kodiDv.l1MinNits, kodiDv.l1MaxNits, kodiDv.l1AvgNits], "nits"));
		$("#lut_switching_kodi_dv_fll").text(formatKodiTriplet([kodiDv.fllLowerCdM2, kodiDv.fllUpperCdM2, kodiDv.fllAvgCdM2], "cd/m^2"));
		$("#lut_switching_kodi_dv_l6").text(joinNonEmpty([
			kodiDv.l6MaxCllNits !== null && typeof kodiDv.l6MaxCllNits !== "undefined" ? "MaxCLL " + formatNumber(kodiDv.l6MaxCllNits, "nits") : "",
			kodiDv.l6MaxFallNits !== null && typeof kodiDv.l6MaxFallNits !== "undefined" ? "MaxFALL " + formatNumber(kodiDv.l6MaxFallNits, "nits") : ""
		]));
		$("#lut_switching_kodi_dv_error").text(valueOrFallback(kodiDv.error));
		$("#lut_switching_kodi_dv_snapshot").text(JSON.stringify(kodiDv.rawLabels || kodiDv, null, 2));
	}

	function fillRuntime(runtime)
	{
		const loadedFile = runtime.loadedFile || $.i18n("lut_switching_runtime_missing");
		$("#lut_switching_runtime_loaded").text(loadedFile);
		$("#lut_switching_runtime_crc").text(runtime.lutFastCRC || "-");
		$("#lut_switching_runtime_compressed").text(boolLabel(runtime.loadedCompressed === true));
		$("#lut_switching_runtime_buffer").text(boolLabel(runtime.lutBufferInit === true));

		const list = $("#lut_switching_runtime_candidates");
		list.empty();
		const candidatePlan = (((runtime.lutBucketDecision || {}).candidatePlan) || []);
		if (candidatePlan.length > 0)
		{
			candidatePlan.forEach(function (entry) {
				list.append($("<li>").text(entry));
			});
		}
		else
		{
			(runtime.candidateFiles || []).forEach(function (filePath) {
				list.append($("<li>").text(filePath));
			});
		}
		if (candidatePlan.length === 0 && (runtime.candidateFiles || []).length === 0)
			list.append($("<li>").text("-"));

		const daytime = runtime.daytimeUplift || {};
		if (daytime.enabled)
		{
			const parts = [];
			parts.push("Blend: " + (daytime.blend || 0) + "/" + (daytime.maxBlend || 255));
			parts.push("Ramp: " + Math.round((daytime.rampPosition || 0) * 100) + "%");
			const formatMinute = function (m) {
				const h = Math.floor(m / 60) % 24;
				const mm = m % 60;
				return ("" + h).padStart(2, "0") + ":" + ("" + mm).padStart(2, "0");
			};
			parts.push("Window: " + formatMinute(daytime.startMinuteOfDay || 0) + " – " + formatMinute(daytime.endMinuteOfDay || 0));
			parts.push("Now: " + formatMinute(daytime.currentMinuteOfDay || 0));
			parts.push("Profile: " + (daytime.profile || "curve3_4_new"));
			if (daytime.error) parts.push("Error: " + daytime.error);
			$("#lut_switching_daytime_uplift_status").text(parts.join(" · "));
		}
		else
		{
			$("#lut_switching_daytime_uplift_status").text("Daytime uplift disabled");
		}
	}

	function fillForm(settings)
	{
		lutSwitchingFormState = cloneLutSwitchingSettings(settings);
		$("#lut_switching_enabled").prop("checked", settings.enabled === true);
		$("#lut_switching_directory").val(settings.directory || "");
		$("#lut_switching_active_file").val(settings.activeLutFile || "");
		$("#lut_switching_sdr_file").val(settings.sdrLutFile || "");
		$("#lut_switching_hdr_file").val(settings.hdrLutFile || "");
		$("#lut_switching_lldv_file").val(settings.lldvLutFile || "");
		perInputIds.forEach(function (inputId) {
			perModeIds.forEach(function (modeId) {
				const key = inputId + modeId.charAt(0).toUpperCase() + modeId.slice(1) + "LutFile";
				$("#lut_switching_" + inputId + "_" + modeId + "_file").val(settings[key] || "");
			});
		});
		$("#lut_switching_sdr_uses_hdr").prop("checked", settings.sdrUsesHdrFile === true);
		$("#lut_switching_memory_cache_enabled").prop("checked", settings.lutMemoryCacheEnabled !== false);
		$("#lut_switching_transfer_enabled").prop("checked", settings.transferAutomationEnabled === true);
		$("#lut_switching_transfer_sdr_profile").val(settings.transferSdrProfile || "");
		$("#lut_switching_transfer_hlg_profile").val(settings.transferHlgProfile || "");
		$("#lut_switching_dv_mdl1000_ratio_numerator").val(parsePositiveInt(settings.transferDolbyVisionMdl1000RatioNumerator, 2));
		$("#lut_switching_dv_mdl1000_ratio_denominator").val(parsePositiveInt(settings.transferDolbyVisionMdl1000RatioDenominator, 4));
		$("#lut_switching_dv_mdl4000_ratio_numerator").val(parsePositiveInt(settings.transferDolbyVisionMdl4000RatioNumerator, 3));
		$("#lut_switching_dv_mdl4000_ratio_denominator").val(parsePositiveInt(settings.transferDolbyVisionMdl4000RatioDenominator, 5));
		$("#lut_switching_kodi_hdr_mdl1000_thresholds").val(settings.kodiHdrMdl1000Thresholds || settings.kodiHdrThresholds || "250");
		$("#lut_switching_kodi_hdr_mdl4000_thresholds").val(settings.kodiHdrMdl4000Thresholds || settings.kodiHdrThresholds || "250");
		$("#lut_switching_kodi_hdr_mdl_only_fallback_nits").val(parsePositiveInt(settings.kodiHdrMdlOnlyFallbackNits, 400));
		$("#lut_switching_kodi_dv_bucket_metric").val(settings.kodiDolbyVisionBucketMetric || "l1AvgNits");
		$("#lut_switching_kodi_dv_scene_index_highlight_weight").val(parseBoundedFloat(settings.kodiDolbyVisionSceneIndexHighlightWeight, 0.25, 0, 1).toFixed(2));
		$("#lut_switching_kodi_dv_mdl1000_thresholds").val(settings.kodiDolbyVisionMdl1000Thresholds || "69, 120, 200");
		$("#lut_switching_kodi_dv_mdl4000_thresholds").val(settings.kodiDolbyVisionMdl4000Thresholds || "69, 120, 200, 400");
		$("#lut_switching_daytime_uplift_enabled").prop("checked", settings.daytimeUpliftEnabled === true);
		$("#lut_switching_daytime_uplift_profile").val(settings.daytimeUpliftProfile || "");
		$("#lut_switching_daytime_uplift_max_blend").val(parsePositiveInt(settings.daytimeUpliftMaxBlend, 128));
		$("#lut_switching_daytime_uplift_ramp_minutes").val(parsePositiveInt(settings.daytimeUpliftRampMinutes, 30));
		$("#lut_switching_daytime_uplift_start_hour").val(settings.daytimeUpliftStartHour != null ? settings.daytimeUpliftStartHour : 8);
		$("#lut_switching_daytime_uplift_start_minute").val(settings.daytimeUpliftStartMinute != null ? settings.daytimeUpliftStartMinute : 0);
		$("#lut_switching_daytime_uplift_end_hour").val(settings.daytimeUpliftEndHour != null ? settings.daytimeUpliftEndHour : 18);
		$("#lut_switching_daytime_uplift_end_minute").val(settings.daytimeUpliftEndMinute != null ? settings.daytimeUpliftEndMinute : 0);
		$("#lut_switching_mode").val(settings.activeMode || "hdr");
		$("#lut_switching_input").val(settings.activeInput || "");
		$("#lut_switching_custom_file").val(settings.lastCustomFile || "");
		renderTransferProfileEditors(settings);
		renderMetadataBucketEditors(settings);
	}

	async function loadTransferProfiles()
	{
		const response = await requestTransferHeadersList();
		if (!response || response.success !== true)
		{
			populateTransferProfileOptions([]);
			return;
		}

		const info = response.info || {};
		populateTransferProfileOptions(info.headers || []);
	}

	async function refreshVideoStream()
	{
		const response = await requestCurrentStateVideoStream(window.currentHyperHdrInstance || 0);
		if (!response || response.success !== true)
		{
			fillVideoStream(null);
			return;
		}

		const info = response.info || {};
		fillVideoStream(info.videoStream || null);
	}

	async function refreshRuntime()
	{
		const response = await requestLutSwitchingGet();
		if (!response || response.success !== true)
			return;

		const info = response.info || {};
		fillRuntime(info.runtime || {});
	}

	async function refreshState()
	{
		const response = await requestLutSwitchingGet();
		if (!response || response.success !== true)
		{
			showInfoDialog("error", $.i18n("main_menu_grabber_lut_switching"), "Failed to load LUT switching state");
			return;
		}

		await loadTransferProfiles();

		const info = response.info || {};
		fillForm(info.settings || {});
		fillRuntime(info.runtime || {});
		await refreshVideoStream();
	}

	function collectPayload(options = {})
	{
		const includeExplicitSelection = options.includeExplicitSelection !== false;
		const customFile = $("#lut_switching_custom_file").val().trim();
		syncMetadataBucketValuesToState();
		syncTransferProfileValuesToState();
		const payload = {
			enabled: $("#lut_switching_enabled").is(":checked"),
			directory: $("#lut_switching_directory").val().trim(),
			activeLutFile: $("#lut_switching_active_file").val().trim(),
			sdrLutFile: $("#lut_switching_sdr_file").val().trim(),
			hdrLutFile: $("#lut_switching_hdr_file").val().trim(),
			lldvLutFile: $("#lut_switching_lldv_file").val().trim(),
			sdrUsesHdrFile: $("#lut_switching_sdr_uses_hdr").is(":checked"),
			lutMemoryCacheEnabled: $("#lut_switching_memory_cache_enabled").is(":checked"),
			transferAutomationEnabled: $("#lut_switching_transfer_enabled").is(":checked"),
			transferSdrProfile: $("#lut_switching_transfer_sdr_profile").val().trim(),
			transferHlgProfile: $("#lut_switching_transfer_hlg_profile").val().trim(),
			transferHdrMdl1000Profiles: cloneStringArray(lutSwitchingFormState.transferHdrMdl1000Profiles),
			transferHdrMdl4000Profiles: cloneStringArray(lutSwitchingFormState.transferHdrMdl4000Profiles),
			transferDolbyVisionMdl1000Profiles: cloneStringArray(lutSwitchingFormState.transferDolbyVisionMdl1000Profiles),
			transferDolbyVisionMdl4000Profiles: cloneStringArray(lutSwitchingFormState.transferDolbyVisionMdl4000Profiles),
			transferDolbyVisionMdl1000RatioNumerator: parsePositiveInt($("#lut_switching_dv_mdl1000_ratio_numerator").val(), 2),
			transferDolbyVisionMdl1000RatioDenominator: parsePositiveInt($("#lut_switching_dv_mdl1000_ratio_denominator").val(), 4),
			transferDolbyVisionMdl4000RatioNumerator: parsePositiveInt($("#lut_switching_dv_mdl4000_ratio_numerator").val(), 3),
			transferDolbyVisionMdl4000RatioDenominator: parsePositiveInt($("#lut_switching_dv_mdl4000_ratio_denominator").val(), 5),
			kodiDolbyVisionBucketMetric: $("#lut_switching_kodi_dv_bucket_metric").val(),
			kodiDolbyVisionSceneIndexHighlightWeight: parseBoundedFloat($("#lut_switching_kodi_dv_scene_index_highlight_weight").val(), 0.25, 0, 1),
			kodiHdrThresholds: $("#lut_switching_kodi_hdr_mdl1000_thresholds").val().trim(),
			kodiHdrMdl1000Thresholds: $("#lut_switching_kodi_hdr_mdl1000_thresholds").val().trim(),
			kodiHdrMdl4000Thresholds: $("#lut_switching_kodi_hdr_mdl4000_thresholds").val().trim(),
			kodiHdrMdlOnlyFallbackNits: parseInt($("#lut_switching_kodi_hdr_mdl_only_fallback_nits").val(), 10) || 400,
			kodiDolbyVisionMdl1000Thresholds: $("#lut_switching_kodi_dv_mdl1000_thresholds").val().trim(),
			kodiDolbyVisionMdl4000Thresholds: $("#lut_switching_kodi_dv_mdl4000_thresholds").val().trim(),
			daytimeUpliftEnabled: $("#lut_switching_daytime_uplift_enabled").is(":checked"),
			daytimeUpliftProfile: $("#lut_switching_daytime_uplift_profile").val().trim(),
			daytimeUpliftMaxBlend: parseInt($("#lut_switching_daytime_uplift_max_blend").val(), 10) || 128,
			daytimeUpliftRampMinutes: parseInt($("#lut_switching_daytime_uplift_ramp_minutes").val(), 10) || 30,
			daytimeUpliftStartHour: parseInt($("#lut_switching_daytime_uplift_start_hour").val(), 10) || 0,
			daytimeUpliftStartMinute: parseInt($("#lut_switching_daytime_uplift_start_minute").val(), 10) || 0,
			daytimeUpliftEndHour: parseInt($("#lut_switching_daytime_uplift_end_hour").val(), 10) || 0,
			daytimeUpliftEndMinute: parseInt($("#lut_switching_daytime_uplift_end_minute").val(), 10) || 0
		};

		if (includeExplicitSelection)
		{
			payload.mode = $("#lut_switching_mode").val();
			payload.input = $("#lut_switching_input").val();
		}

		perInputIds.forEach(function (inputId) {
			perModeIds.forEach(function (modeId) {
				const key = inputId + modeId.charAt(0).toUpperCase() + modeId.slice(1) + "LutFile";
				payload[key] = $("#lut_switching_" + inputId + "_" + modeId + "_file").val().trim();
			});
			metadataBucketGroups.forEach(function (group) {
				const key = metadataConfigKey(inputId, group);
				payload[key] = cloneStringArray(lutSwitchingFormState[key]);
			});
		});

		if (includeExplicitSelection && customFile.length > 0)
			payload.file = customFile;

		return payload;
	}

	async function refreshAfterAction(response, errorText, successText)
	{
		if (!response || response.success !== true)
		{
			showInfoDialog("error", $.i18n("main_menu_grabber_lut_switching"), errorText);
			return false;
		}

		toaster("success", $.i18n("main_menu_grabber_lut_switching"), successText, 0);
		const info = response.info || {};
		fillForm(info.settings || {});
		fillRuntime(info.runtime || {});
		await refreshVideoStream();
		return true;
	}

	$("#lut_switching_save_apply").off().on("click", async function () {
		const response = await requestLutSwitchingApply(collectPayload());
		await refreshAfterAction(response, "Failed to save and apply LUT switching state", $.i18n("lut_switching_saved"));
	});

	$("#lut_switching_save_runtime_sections").off().on("click", async function () {
		const response = await requestLutSwitchingSave(collectPayload({ includeExplicitSelection: false }));
		await refreshAfterAction(response, "Failed to save transfer and LUT bucket settings", "Transfer and LUT bucket settings saved");
	});

	$("#lut_switching_apply_runtime_decisions").off().on("click", async function () {
		const response = await requestLutSwitchingApplyRuntime(collectPayload({ includeExplicitSelection: false }));
		await refreshAfterAction(response, "Failed to apply runtime transfer and LUT bucket decisions", "Runtime transfer and LUT bucket decisions applied");
	});

	$("#lut_switching_reload").off().on("click", async function () {
		const response = await requestLutSwitchingReload();
		if (!response || response.success !== true)
		{
			showInfoDialog("error", $.i18n("main_menu_grabber_lut_switching"), "Failed to reload LUT state");
			return;
		}

		toaster("success", $.i18n("main_menu_grabber_lut_switching"), $.i18n("lut_switching_reloaded"), 0);
		const info = response.info || {};
		fillForm(info.settings || {});
		fillRuntime(info.runtime || {});
		await refreshVideoStream();
	});

	$("#lut_switching_stream_refresh").off().on("click", async function () {
		await refreshRuntime();
		await refreshVideoStream();
	});

	$("#lut_switching_kodi_hdr_mdl1000_thresholds, #lut_switching_kodi_hdr_mdl4000_thresholds, #lut_switching_kodi_dv_mdl1000_thresholds, #lut_switching_kodi_dv_mdl4000_thresholds")
		.off("input.metadataBuckets")
		.on("input.metadataBuckets", function () {
			renderTransferProfileEditors();
			renderMetadataBucketEditors();
		});

	$(".lut-switching-transfer-ratio")
		.off("input.transferRatio")
		.on("input.transferRatio", function () {
			renderTransferProfileEditors();
		});

	function stopLiveRefresh()
	{
		if (window.__lutSwitchingStreamRefreshHandle)
			window.clearInterval(window.__lutSwitchingStreamRefreshHandle);
		if (window.__lutSwitchingRuntimeRefreshHandle)
			window.clearInterval(window.__lutSwitchingRuntimeRefreshHandle);
		window.__lutSwitchingStreamRefreshHandle = null;
		window.__lutSwitchingRuntimeRefreshHandle = null;
	}

	function startLiveRefresh()
	{
		stopLiveRefresh();

		window.__lutSwitchingStreamRefreshHandle = window.setInterval(async function () {
			if (streamRefreshInFlight)
				return;

			streamRefreshInFlight = true;
			try
			{
				await refreshVideoStream();
			}
			finally
			{
				streamRefreshInFlight = false;
			}
		}, LIVE_STREAM_REFRESH_MS);

		window.__lutSwitchingRuntimeRefreshHandle = window.setInterval(async function () {
			if (runtimeRefreshInFlight)
				return;

			runtimeRefreshInFlight = true;
			try
			{
				await refreshRuntime();
			}
			finally
			{
				runtimeRefreshInFlight = false;
			}
		}, LIVE_RUNTIME_REFRESH_MS);
	}

	$(window).off("beforeunload.lutSwitching").on("beforeunload.lutSwitching", function () {
		stopLiveRefresh();
	});

	refreshState().then(function () {
		startLiveRefresh();
	});
});