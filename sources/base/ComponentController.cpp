#ifndef PCH_ENABLED	
	#include <iostream>
#endif

#include <HyperhdrConfig.h>
#include <base/ComponentController.h>
#include <base/HyperHdrInstance.h>

using namespace hyperhdr;

ComponentController::ComponentController(HyperHdrInstance* hyperhdr, bool disableOnStartup) :
	_log(Logger::getInstance(QString("COMPONENTCTRL%1").arg(hyperhdr->getInstanceIndex()))),
	_disableOnStartup(disableOnStartup)
{
	// init all comps to false
	QVector<hyperhdr::Components> vect;
	vect << COMP_ALL << COMP_HDR << COMP_SMOOTHING << COMP_BLACKBORDER << COMP_FORWARDER;

	vect << COMP_VIDEOGRABBER << COMP_SYSTEMGRABBER << COMP_LEDDEVICE;
	for (auto e : vect)
	{
		_componentStates.emplace(e, (e == COMP_ALL));
	}

	connect(this, &ComponentController::SignalRequestComponent, hyperhdr, &HyperHdrInstance::SignalRequestComponent);
	connect(hyperhdr, &HyperHdrInstance::SignalRequestComponent, this, &ComponentController::handleCompStateChangeRequest);
	Debug(_log, "ComponentController is initialized. Components are %s", (_disableOnStartup) ? "DISABLED" : "ENABLED");
}

ComponentController::~ComponentController()
{
	Debug(_log, "ComponentController is released");
}

void ComponentController::handleCompStateChangeRequest(hyperhdr::Components comps, bool activated)
{
	if (comps == COMP_ALL)
	{
		if (!activated && _prevComponentStates.empty())
		{
			bool disableLeds = _disableOnStartup && !isComponentEnabled(COMP_ALL) && _prevComponentStates.empty();

			Debug(_log, "Disabling HyperHDR instance: saving current component states first");
			_componentStates[COMP_ALL] = false;

			for (int i = 0; i < 2; i++)
				for (const auto& comp : _componentStates)
					if (comp.first != COMP_ALL &&
						((i == 0 && comp.first == COMP_LEDDEVICE) || (i == 1 && comp.first != COMP_LEDDEVICE)))
					{
						if (disableLeds && comp.first == COMP_LEDDEVICE)
							_prevComponentStates.emplace(comp.first, true);
						else
							_prevComponentStates.emplace(comp.first, comp.second);

						if (comp.second)
						{
							emit SignalRequestComponent(comp.first, false);
						}
					}

			Debug(_log, "Disabling HyperHDR instance: preparations completed");
			emit SignalComponentStateChanged(COMP_ALL, false);
		}
		else
		{
			if (activated && !_prevComponentStates.empty())
			{
				Debug(_log, "Enabling HyperHDR instance: recovering previuosly saved component states");
				for (const auto& comp : _prevComponentStates)
					if (comp.first != COMP_ALL)
					{
						if (comp.second)
						{
							emit SignalRequestComponent(comp.first, true);
						}
					}
				_prevComponentStates.clear();
				setNewComponentState(COMP_ALL, true);
			}
		}
	}
}

int ComponentController::isComponentEnabled(hyperhdr::Components comp) const
{
	return (_componentStates.count(comp)) ? _componentStates.at(comp) : -1;
}

const std::map<hyperhdr::Components, bool>& ComponentController::getComponentsState() const
{
	return _componentStates;
}

void ComponentController::setNewComponentState(hyperhdr::Components comp, bool activated)
{
	if (_componentStates[comp] != activated)
	{
		Info(_log, "%s: %s", componentToString(comp), (activated ? "enabled" : "disabled"));
		_componentStates[comp] = activated;
		// emit component has changed state
		emit SignalComponentStateChanged(comp, activated);
	}
}

void ComponentController::turnGrabbers(bool activated)
{
	if (_prevGrabbers.empty() && !activated)
	{
		_prevGrabbers.emplace(COMP_SYSTEMGRABBER, isComponentEnabled(COMP_SYSTEMGRABBER));
		_prevGrabbers.emplace(COMP_VIDEOGRABBER, isComponentEnabled(COMP_VIDEOGRABBER));
		for (const auto& comp : _prevGrabbers)
			if (comp.second)
			{
				emit SignalRequestComponent(comp.first, false);
			}
	}
	else if (!_prevGrabbers.empty() && activated)
	{
		for (const auto& comp : _prevGrabbers)
			if (comp.second)
			{
				emit SignalRequestComponent(comp.first, true);
			}
		_prevGrabbers.clear();
	}
}
