#pragma once

#ifndef PCH_ENABLED
	#include <map>
	#include <QObject>

	#include <utils/Logger.h>
	#include <utils/Components.h>
#endif

class HyperHdrInstance;

class ComponentController : public QObject
{
	Q_OBJECT

public:
    ComponentController(HyperHdrInstance* hyperhdr, bool disableOnStartup);
    virtual ~ComponentController();

	int isComponentEnabled(hyperhdr::Components comp) const;
	const std::map<hyperhdr::Components, bool>& getComponentsState() const;

signals:
	void SignalComponentStateChanged(hyperhdr::Components comp, bool state);
	void SignalRequestComponent(hyperhdr::Components component, bool enabled);

public slots:
	void setNewComponentState(hyperhdr::Components comp, bool activated);

private slots:
	void handleCompStateChangeRequest(hyperhdr::Components comps, bool activated);

private:
	Logger* _log;
	std::map<hyperhdr::Components, bool> _componentStates;
	std::map<hyperhdr::Components, bool> _prevComponentStates;
	bool _disableOnStartup;
};
