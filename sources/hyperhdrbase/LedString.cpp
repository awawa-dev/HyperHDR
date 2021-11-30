// STL includes
#include <cstring>
#include <iostream>

#include <hyperhdrbase/LedString.h>

std::vector<Led>& LedString::leds()
{
	return mLeds;
}

const std::vector<Led>& LedString::leds() const
{
	return mLeds;
}


ColorOrder LedString::createColorOrder(const QJsonObject& deviceConfig)
{
	return stringToColorOrder(deviceConfig["colorOrder"].toString("rgb"));
}

LedString LedString::createLedString(const QJsonArray& ledConfigArray, const ColorOrder deviceOrder)
{
	LedString ledString;
	const QString deviceOrderStr = colorOrderToString(deviceOrder);

	for (signed i = 0; i < ledConfigArray.size(); ++i)
	{
		const QJsonObject& ledConfig = ledConfigArray[i].toObject();
		Led led;

		led.minX_frac = qMax(0.0, qMin(1.0, ledConfig["hmin"].toDouble()));
		led.maxX_frac = qMax(0.0, qMin(1.0, ledConfig["hmax"].toDouble()));
		led.minY_frac = qMax(0.0, qMin(1.0, ledConfig["vmin"].toDouble()));
		led.maxY_frac = qMax(0.0, qMin(1.0, ledConfig["vmax"].toDouble()));
		led.group = ledConfig["group"].toInt(0);
		// Fix if the user swapped min and max
		if (led.minX_frac > led.maxX_frac)
		{
			std::swap(led.minX_frac, led.maxX_frac);
		}
		if (led.minY_frac > led.maxY_frac)
		{
			std::swap(led.minY_frac, led.maxY_frac);
		}

		// Get the order of the rgb channels for this led (default is device order)
		ledString.colorOrder = stringToColorOrder(ledConfig["colorOrder"].toString(deviceOrderStr));
		ledString.leds().push_back(led);
	}
	return ledString;
}

QSize LedString::getLedLayoutGridSize(const QJsonArray& ledConfigArray)
{
	std::vector<int> midPointsX;
	std::vector<int> midPointsY;

	for (signed i = 0; i < ledConfigArray.size(); ++i)
	{
		const QJsonObject& ledConfig = ledConfigArray[i].toObject();

		double minX_frac = qMax(0.0, qMin(1.0, ledConfig["hmin"].toDouble()));
		double maxX_frac = qMax(0.0, qMin(1.0, ledConfig["hmax"].toDouble()));
		double minY_frac = qMax(0.0, qMin(1.0, ledConfig["vmin"].toDouble()));
		double maxY_frac = qMax(0.0, qMin(1.0, ledConfig["vmax"].toDouble()));
		// Fix if the user swapped min and max
		if (minX_frac > maxX_frac)
		{
			std::swap(minX_frac, maxX_frac);
		}
		if (minY_frac > maxY_frac)
		{
			std::swap(minY_frac, maxY_frac);
		}

		// calculate mid point and make grid calculation
		midPointsX.push_back(int(1000.0 * (minX_frac + maxX_frac) / 2.0));
		midPointsY.push_back(int(1000.0 * (minY_frac + maxY_frac) / 2.0));

	}

	// remove duplicates
	std::sort(midPointsX.begin(), midPointsX.end());
	midPointsX.erase(std::unique(midPointsX.begin(), midPointsX.end()), midPointsX.end());
	std::sort(midPointsY.begin(), midPointsY.end());
	midPointsY.erase(std::unique(midPointsY.begin(), midPointsY.end()), midPointsY.end());

	QSize gridSize(static_cast<int>(midPointsX.size()), static_cast<int>(midPointsY.size()));

	// Correct the grid in case it is malformed in width vs height
	// Expected is at least 50% of width <-> height
	if ((gridSize.width() / gridSize.height()) > 2)
		gridSize.setHeight(qMax(1, gridSize.width() / 2));
	else if ((gridSize.width() / gridSize.height()) < 0.5)
		gridSize.setWidth(qMax(1, gridSize.height() / 2));

	// Limit to 80px for performance reasons
	const int pl = 80;
	if (gridSize.width() > pl || gridSize.height() > pl)
	{
		gridSize.scale(pl, pl, Qt::KeepAspectRatio);
	}

	return gridSize;
}
