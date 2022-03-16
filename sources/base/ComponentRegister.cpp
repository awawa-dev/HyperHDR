#include <base/ComponentRegister.h>
#include <iostream>

#include <base/HyperHdrInstance.h>
#include <HyperhdrConfig.h>

using namespace hyperhdr;

ComponentRegister::ComponentRegister(HyperHdrInstance* hyperhdr)
	: _hyperhdr(hyperhdr)
	, _log(Logger::getInstance(QString("COMPONENTREG%1").arg(hyperhdr->getInstanceIndex())))
{
	// init all comps to false
	QVector<hyperhdr::Components> vect;
	vect << COMP_ALL << COMP_HDR << COMP_SMOOTHING << COMP_BLACKBORDER << COMP_FORWARDER;

#if defined(ENABLE_BOBLIGHT)
	vect << COMP_BOBLIGHTSERVER;
#endif

	vect << COMP_VIDEOGRABBER << COMP_SYSTEMGRABBER << COMP_LEDDEVICE;
	for (auto e : vect)
	{
		_componentStates.emplace(e, (e == COMP_ALL));
	}

	connect(_hyperhdr, &HyperHdrInstance::compStateChangeRequest, this, &ComponentRegister::handleCompStateChangeRequest);
}

ComponentRegister::~ComponentRegister()
{
}

int ComponentRegister::isComponentEnabled(hyperhdr::Components comp) const
{
	return (_componentStates.count(comp)) ? _componentStates.at(comp) : -1;
}

void ComponentRegister::setNewComponentState(hyperhdr::Components comp, bool activated)
{
	if (_componentStates[comp] != activated)
	{
		Info(_log, "%s: %s", componentToString(comp), (activated ? "enabled" : "disabled"));
		_componentStates[comp] = activated;
		// emit component has changed state
		emit updatedComponentState(comp, activated);
	}
}

void ComponentRegister::handleCompStateChangeRequest(hyperhdr::Components comps, bool activated)
{
	if (comps == COMP_ALL && !_inProgress)
	{
		_inProgress = true;
		if (!activated && _prevComponentStates.empty())
		{
			Debug(_log, "Disable Hyperhdr, store current component states");
			for (const auto& comp : _componentStates)
			{
				// save state
				_prevComponentStates.emplace(comp.first, comp.second);
				// disable if enabled
				if (comp.second)
				{
					emit _hyperhdr->compStateChangeRequest(comp.first, false);
				}
			}
			setNewComponentState(COMP_ALL, false);
		}
		else
		{
			if (activated && !_prevComponentStates.empty())
			{
				Debug(_log, "Enable Hyperhdr, recover previous component states");
				for (const auto& comp : _prevComponentStates)
				{
					// if comp was enabled, enable again
					if (comp.second)
					{
						emit _hyperhdr->compStateChangeRequest(comp.first, true);
					}
				}
				_prevComponentStates.clear();
				setNewComponentState(COMP_ALL, true);
			}
		}
		_inProgress = false;
	}
}
