#pragma once

#ifndef PCH_ENABLED
	#include <QJsonObject>
#endif

#include <utils/Logger.h>
#include <utils/settings.h>
#include <utils/Components.h>
#include <blackborder/BlackBorderDetector.h>

class HyperHdrInstance;

namespace hyperhdr
{
	class BlackBorderProcessor : public QObject
	{
		Q_OBJECT

	public:
		BlackBorderProcessor(HyperHdrInstance* hyperhdr, QObject* parent);
		~BlackBorderProcessor() override;

		BlackBorder getCurrentBorder() const;
		bool enabled() const;

		void setEnabled(bool enable);
		void setHardDisable(bool disable);
		bool process(const Image<ColorRgb>& image);

	signals:
		void setNewComponentState(hyperhdr::Components component, bool state);

	private slots:
		void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
		void handleCompStateChangeRequest(hyperhdr::Components component, bool enable);

	private:
		bool updateBorder(const BlackBorder& newDetectedBorder);
		bool _enabled;
		unsigned _unknownSwitchCnt;
		unsigned _borderSwitchCnt;
		unsigned _maxInconsistentCnt;
		unsigned _blurRemoveCnt;
		QString _detectionMode;
		std::unique_ptr<BlackBorderDetector> _borderDetector;
		BlackBorder _currentBorder;
		BlackBorder _previousDetectedBorder;
		unsigned _consistentCnt;
		unsigned _inconsistentCnt;
		double _oldThreshold;
		bool _hardDisabled;
		bool _userEnabled;
	};
}
