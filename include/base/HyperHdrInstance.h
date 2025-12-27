#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QStringList>
	#include <QSize>
	#include <QJsonObject>
	#include <QJsonValue>
	#include <QJsonArray>
	#include <QMap>
	#include <QTime>
	#include <QThread>
	#include <QVector>

	#include <list>
	#include <memory>
	#include <atomic>
#endif

#include <image/ColorRgb.h>
#include <image/Image.h>
#include <utils/settings.h>
#include <utils/Components.h>
#include <base/LedString.h>
#include <effects/EffectDefinition.h>
#include <effects/ActiveEffectDefinition.h>
#include <led-drivers/LedDevice.h>
#include <lut-calibrator/LutCalibrator.h>
#include <infinite-color-engine/SharedOutputColors.h>

class Muxer;
class ComponentController;
class ImageToLedManager;
class NetworkForwarder;
class CoreInfiniteEngine;
class EffectEngine;
class InstanceConfig;
class VideoControl;
class SystemControl;
class RawUdpServer;
class LedDeviceWrapper;
class Logger;
class HyperHdrManager;
class SoundCapture;
class GrabberHelper;


class HyperHdrInstance : public QObject
{
	Q_OBJECT

public:
	HyperHdrInstance(quint8 instance, bool disableOnstartup, QString name);
	~HyperHdrInstance();

	quint8 getInstanceIndex() const { return _instIndex; }
	QSize getLedGridSize() const { return _ledGridSize; }
	bool getScanParameters(size_t led, double& hscanBegin, double& hscanEnd, double& vscanBegin, double& vscanEnd) const;
	unsigned addEffectConfig(unsigned id, int settlingTime_ms = 200, double ledUpdateFrequency_hz = 25.0, bool pause = false);

	static void signalTerminateTriggered();
	static bool isTerminated();
	static int getTotalRunningCount();

public slots:
	bool clear(int priority, bool forceClearAll = false);
	QJsonObject getAverageColor();
	bool hasPriority(int priority);
	hyperhdr::Components getComponentForPriority(int priority);
	hyperhdr::Components getCurrentPriorityActiveComponent();
	int getCurrentPriority() const;
	std::list<EffectDefinition> getEffects() const;
	int getLedCount() const;
	bool getReadOnlyMode() const;
	QJsonDocument getSetting(settings::type type) const;
	int hasLedClock();
	void identifyLed(const QJsonObject& params);
	int isComponentEnabled(hyperhdr::Components comp) const;
	QJsonObject getJsonConfig() const;
	QJsonObject getJsonInfo(bool full);
	void registerInput(int priority, hyperhdr::Components component, const QString& origin = "System", const QString& owner = "", unsigned smooth_cfg = 0);
	void saveCalibration(QString saveData);
	bool saveSettings(const QJsonObject& config, bool correct = false);
	void setSignalStateByCEC(bool enable);
	bool setInputLeds(int priority, const QVector<ColorRgb>& ledColors, int timeout_ms = -1, bool clearEffect = true);
	void setInputImage(int priority, const Image<ColorRgb>& image, int64_t timeout_ms = -1, bool clearEffect = true);
	void signalSetGlobalImageHandler(int priority, const Image<ColorRgb>& image, int timeout_ms, hyperhdr::Components origin, QString clientDescription);
	void setColor(int priority, const QVector<ColorRgb>& ledColors, int timeout_ms = -1, const QString& origin = "System", bool clearEffects = true);
	void signalSetGlobalColorHandler(int priority, const QVector<ColorRgb>& ledColor, int timeout_ms, hyperhdr::Components origin, QString clientDescription);
	bool setInputInactive(quint8 priority);
	void setLedMappingType(int mappingType);
	void setNewComponentState(hyperhdr::Components component, bool state);
	void setSetting(settings::type type, QString config);
	void setSourceAutoSelect(bool state);
	void setSmoothing(int time);
	bool setVisiblePriority(int priority);
	bool sourceAutoSelectEnabled() const;
	void start();
	void turnGrabbers(bool active);
	void update();
	void updateAdjustments(const QJsonObject& config);


	int setEffect(const QString& effectName, int priority, int timeout = -1, const QString& origin = "System");

signals:
	void SignalComponentStateChanged(hyperhdr::Components comp, bool state);
	void SignalPrioritiesChanged();
	void SignalVisiblePriorityChanged(quint8 priority);
	void SignalVisibleComponentChanged(hyperhdr::Components comp);
	void SignalInstancePauseChanged(int instance, bool isEnabled);
	void SignalRequestComponent(hyperhdr::Components component, bool enabled);
	void SignalImageToLedsMappingChanged(int mappingType);
	void SignalInstanceImageUpdated(const Image<ColorRgb>& ret);
	void SignalForwardJsonMessage(QJsonObject);
	void SignalInstanceSettingsChanged(settings::type type, QJsonDocument data);
	void SignalAdjustmentUpdated(QJsonArray newConfig);
	void SignalFinalOutputColorsReady(SharedOutputColors nonlinearRgbColors);
	void SignalSmoothingClockTick();
	void SignalSmoothingRestarted(int suggestedInterval);
	void SignalRawColorsChanged(QVector<ColorRgb> ledValues);
	void SignalInstanceJustStarted();
	void SignalColorIsSet(ColorRgb color, int duration);

private slots:
	void handleVisibleComponentChanged(hyperhdr::Components comp);
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void handlePriorityChangedLedDevice(const quint8& priority);

private:
	void updateResult(std::vector<linalg::aliases::float3>&& _ledBuffer);

	const quint8	_instIndex;
	QTime			_bootEffect;
	LedString		_ledString;

	std::unique_ptr<InstanceConfig> _instanceConfig;
	std::unique_ptr<ComponentController> _componentController;
	std::unique_ptr<ImageToLedManager> _imageProcessor;
	std::unique_ptr<Muxer> _muxer;
	std::unique_ptr<LedDeviceWrapper> _ledDeviceWrapper;
	std::unique_ptr<CoreInfiniteEngine> _infinite;
	std::unique_ptr<EffectEngine> _effectEngine;
	std::unique_ptr<VideoControl> _videoControl;
	std::unique_ptr<SystemControl> _systemControl;
	std::unique_ptr<RawUdpServer> _rawUdpServer;

	LoggerName			_log;
	int					_hwLedCount;
	QSize				_ledGridSize;

	QVector<ColorRgb>	_currentLedColors;
	QString					_name;

	bool					_disableOnStartup;

	static std::atomic<bool>	_signalTerminate;
	static std::atomic<int>		_totalRunningCount;

	struct {
		qint64		token = 0;
		qint64		statBegin = 0;
		uint32_t	total = 0;
	} _computeStats;
};
