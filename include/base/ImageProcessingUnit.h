#pragma once

class ImageProcessingUnit : public QObject
{
	Q_OBJECT

public:
	ImageProcessingUnit(HyperHdrInstance* hyperhdr);
	~ImageProcessingUnit();

signals:
	void dataReadySignal(std::vector<ColorRgb> result);
	void processImageSignal();
	void queueImageSignal(int priority, const Image<ColorRgb>& image);
	void clearQueueImageSignal();

private slots:
	void queueImage(int priority, const Image<ColorRgb>& image);
	void clearQueueImage();
	void processImage();

private:
	int	_priority;
	HyperHdrInstance*	_hyperhdr;
	Image<ColorRgb>		_frameBuffer;
};
