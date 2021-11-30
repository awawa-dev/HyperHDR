#pragma once

#include <QObject>
#include <QList>
#include <QRectF>
#include <cstdint>
#include <vector>

#include <utils/ColorRgb.h>
#include <utils/Image.h>
#include <utils/FrameDecoder.h>
#include <utils/Logger.h>
#include <utils/Components.h>
#include <QJsonDocument>
#include <QSemaphore>

class DetectionAutomatic : public QObject
{
	Q_OBJECT
public:
	DetectionAutomatic();	

	struct calibrationPoint
	{
		bool init;
		bool lost;
		int x, y;
		int r, g, b;
		calibrationPoint();
		calibrationPoint(int _x, int _y);
	};

	bool checkSignalDetectionAutomatic(Image<ColorRgb>& image);

	bool getDetectionAutoSignal();

	bool isCalibrating();

	void setAutoSignalParams(bool _saveResources, int errorTolerance, int modelTolerance, int sleepTime, int wakeTime);

	void setAutomaticCalibrationData(QString signature, int quality, int width, int height,
		std::vector<DetectionAutomatic::calibrationPoint> sdrVec, std::vector<DetectionAutomatic::calibrationPoint> hdrVec);

public slots:
	QJsonDocument startCalibration();

	QJsonDocument stopCalibration();

	QJsonDocument getCalibrationInfo();

private:
	Logger* _log;	

	enum class calibrationPhase { WAITING_FOR_SDR, CALIBRATING_SDR, WAITING_FOR_HDR, CALIBRATING_HDR };

	struct calibration
	{
		bool		isSuccess;
		bool		isRunning;

		int			sdrStat;
		int			hdrStat;
		int			backupHDR;
		int			decimationFPS;
		qint64		phaseStartTime;
		qint64		phaseEndTime;
		qint64		endTime;
		int			currentSDRframe;
		int			currentHDRframe;
		int			width;
		int			height;
		QString		signature;
		int			tolerance;
		int			model;
		int			quality;
		QString		status;

		calibrationPhase currentPhase;

		std::vector<calibrationPoint> sdrPoint;
		std::vector<calibrationPoint> hdrPoint;

		calibration();
		void reset();
		void buildPoints(int _width, int _height);
		QString getSignature();
	} calibrationData, checkData;

	void calibrateFrame(Image<ColorRgb>& image);
	void saveResult();
	bool checkSignal(Image<ColorRgb>& image);
	void resetStats();

	bool _saveResources;
	int _errorTolerance;
	int _modelTolerance;
	int _sleepTime;
	int _wakeTime;
	bool _noSignal;
	qint64 _offSignalTime;
	qint64 _onSignalTime;
	int _backupDecimation;
};
