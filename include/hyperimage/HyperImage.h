#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
#endif

#include <QBuffer>
#include <utils/ColorRgb.h>

class HyperImage : public QObject
{
	Q_OBJECT

public:
	static void svg2png(QString filename, int width, int height, QBuffer& buffer);
	static ColorRgb ColorRgbfromString(QString colorName);
};
