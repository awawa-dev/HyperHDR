#include <utils/Logger.h>
#include <cec/WrapperCEC.h>
#include <cec/cecHandler.h>
#include <algorithm>


WrapperCEC::WrapperCEC():
	_cecHandler(nullptr),
	_log(Logger::getInstance("CEC"))
{
}

WrapperCEC::~WrapperCEC()
{
	enable(false);
}

void WrapperCEC::sourceRequestHandler(hyperhdr::Components component, int hyperhdrInd, bool listen)
{
	if (component == hyperhdr::Components::COMP_CEC)
	{
		bool found = (std::find(_cecClients.begin(), _cecClients.end(), hyperhdrInd) != _cecClients.end());

		if (listen && !found)
			_cecClients.push_back(hyperhdrInd);
		else if (!listen && found)
			_cecClients.remove(hyperhdrInd);
		else
			return;

		if (_cecClients.empty())
			enable(false);
		else
			enable(true);
	}
}

void WrapperCEC::enable(bool enabled)
{
	if (enabled)
	{
		if (_cecHandler == nullptr)
		{
			Info(_log, "Opening libCEC library.");

			_cecHandler = new cecHandler();
			connect(_cecHandler, &cecHandler::stateChange, this, &WrapperCEC::SignalStateChange);
			connect(_cecHandler, &cecHandler::keyPressed, this, &WrapperCEC::SignalKeyPressed);
			if (_cecHandler->start())
				Info(_log, "Success: libCEC library loaded.");
			else
			{
				Error(_log, "Could not open libCEC library");
				enable(false);
			}
		}
	}
	else
	{
		if (_cecHandler != nullptr)
		{
			Info(_log, "Disconnecting from libCEC library");

			disconnect(_cecHandler, &cecHandler::stateChange, this, &WrapperCEC::SignalStateChange);
			disconnect(_cecHandler, &cecHandler::keyPressed, this, &WrapperCEC::SignalKeyPressed);
			
			_cecHandler->stop();
			Info(_log, "Cleaning up libCEC");
			delete _cecHandler;
			_cecHandler = nullptr;
		}
	}
}
