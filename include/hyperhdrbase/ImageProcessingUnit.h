#pragma once

#include <QString>
#include <QQueue>

class ImageProcessingUnit : public QObject
{
	Q_OBJECT

public:
	ImageProcessingUnit(HyperHdrInstance* hyperhdr);
	~ImageProcessingUnit();

signals:
	void dataReadySignal(std::vector<ColorRgb> result);
	void processImageSignal();
	void queueImageSignal(const Image<ColorRgb>& image);
	void clearQueueImageSignal();

private slots:
	void queueImage(const Image<ColorRgb>& image);
	void clearQueueImage();
	void processImage();

private:
	/// HyperHDR instance pointer
	HyperHdrInstance*		_hyperhdr;
	QQueue<Image<ColorRgb>> _buffer;
};
