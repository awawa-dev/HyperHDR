#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QList>
	#include <QRectF>
	#include <QJsonDocument>
	#include <QSemaphore>
	#include <cstdint>
	#include <vector>
#endif

#include <image/Image.h>
#include <image/ColorRgb.h>
#include <utils/Logger.h>
#include <utils/Components.h>

#include <utils/FrameDecoder.h>

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

	virtual int  getHdrToneMappingEnabled() = 0;
	virtual void setHdrToneMappingEnabled(int mode) = 0;
	virtual int  getFpsSoftwareDecimation() = 0;
	virtual void setFpsSoftwareDecimation(int decimation) = 0;
	virtual int  getActualFps() = 0;
	virtual QString getSignature() = 0;

signals:
	void SignalSaveCalibration(QString saveData);

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
		void buildPoints(QString _signature, int _width, int _height);		
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
