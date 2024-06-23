#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QJsonObject>
	#include <QJsonArray>
	#include <QSize>

	#include <ctime>
	#include <vector>
#endif

#include <image/ColorRgb.h>

namespace Json { class Value; }

class LedString
{

public:
	enum class ColorOrder
	{
		ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_BRG, ORDER_GBR, ORDER_BGR
	};

	static QString colorOrderToString(ColorOrder colorOrder);
	static ColorOrder stringToColorOrder(const QString& order);

	struct Led
	{
		double minX_frac;
		double maxX_frac;
		double minY_frac;
		double maxY_frac;

		bool disabled;
		int group;
	};

	std::vector<Led>& leds();
	const std::vector<Led>& leds() const;
	ColorOrder colorOrder = ColorOrder::ORDER_RGB;

	bool hasDisabled = false;

	static ColorOrder createColorOrder(const QJsonObject& deviceConfig);
	static LedString createLedString(const QJsonArray& ledConfigArray, const ColorOrder deviceOrder);
	static QSize getLedLayoutGridSize(const QJsonArray& ledConfigArray);

private:
	std::vector<Led> mLeds;
};
