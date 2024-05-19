#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <QSize>
#endif

#include <QBuffer>
#include <utils/ColorRgb.h>
#include <utils/Image.h>

class HyperImage : public QObject
{
	Q_OBJECT

public:
	HyperImage();
	HyperImage(QSize size);

	int width() const;
	int height() const;

	Image<ColorRgb> renderImage() const;

	void resize(int sizeX, int sizeY);
	void setPen(const ColorRgb& color);
	void drawLine(int ax, int ay, int bx, int by);
	void drawPoint(int x, int y);
	void fill(const ColorRgb& color);
	void fillRect(int ax, int ay, int bx, int by, const ColorRgb& color);	

	static void svg2png(QString filename, int width, int height, QBuffer& buffer);
	static ColorRgb ColorRgbfromString(QString colorName);
};
