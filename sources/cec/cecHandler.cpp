#include <cec/cecHandler.h>
#include <utils/Logger.h>
#include <cec.h>
#include <cecloader.h>

#include <algorithm>
#include <vector>
#include <dlfcn.h>
#include <stdio.h>

#ifndef LIBCEC_OSD_NAME_SIZE
	#define LIBCEC_OSD_NAME_SIZE sizeof(CEC::libcec_configuration::strDeviceName)
#endif

void handleCecLogMessage(void* context, const CEC::cec_log_message* message);
void handleCecCommandMessage(void* context, const CEC::cec_command* command);
void handleCecKeyPress(void* context, const CEC::cec_keypress* key);

namespace
{
	CEC::ICECCallbacks			_cecCallbacks;
	CEC::libcec_configuration	_cecConfig;
	CEC::ICECAdapter*			_cecAdapter = nullptr;
}

cecHandler::cecHandler() :
	_log(Logger::getInstance("CEC"))
{	
	Info(_log, "CEC object created");

	_cecCallbacks.Clear();	
	_cecCallbacks.logMessage = handleCecLogMessage;
	_cecCallbacks.keyPress = handleCecKeyPress;
	_cecCallbacks.commandReceived = handleCecCommandMessage;

	_cecConfig.Clear();
	snprintf(_cecConfig.strDeviceName, LIBCEC_OSD_NAME_SIZE, "HyperHDR");
	_cecConfig.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_RECORDING_DEVICE);
	_cecConfig.clientVersion = CEC::LIBCEC_VERSION_CURRENT;
	_cecConfig.bActivateSource = 0;
	_cecConfig.callbacks     = &_cecCallbacks;
	_cecConfig.callbackParam = this;
}

cecHandler::~cecHandler()
{
	stop();
}

bool cecHandler::start()
{
	bool opened = false;

	if (_cecAdapter)
		return true;

	Info(_log, "Starting CEC handler.");

	_cecAdapter = LibCecInitialise(&_cecConfig);
	if(!_cecAdapter)
	{
		Error(_log, "Failed to load libCEC library. One of the CEC library component is missing");
		return false;
	}

	std::vector<CEC::cec_adapter_descriptor> adapters(20);
	int8_t size = _cecAdapter->DetectAdapters(adapters.data(), adapters.size(), nullptr, true);
	adapters.resize(size);

	if (adapters.empty())
	{
		Error(_log, "Failed to find CEC adapter");
		UnloadLibCec(_cecAdapter);
		_cecAdapter = nullptr;

		return false;
	}

	// __ARM_ARCH_ISA_A64
	
	for (const auto& adapter : adapters)
	{
		QString adapterType = "unknown";

		switch (adapter.adapterType) {
			case CEC::cec_adapter_type::ADAPTERTYPE_RPI: adapterType = "RPI"; break;
			case CEC::cec_adapter_type::ADAPTERTYPE_LINUX: adapterType = "Linux"; break;
			case CEC::cec_adapter_type::ADAPTERTYPE_UNKNOWN: adapterType = "Unknown"; break;
			case CEC::cec_adapter_type::ADAPTERTYPE_IMX: adapterType = "Imx"; break;
			case CEC::cec_adapter_type::ADAPTERTYPE_P8_DAUGHTERBOARD: adapterType = "P8 board"; break;
			case CEC::cec_adapter_type::ADAPTERTYPE_P8_EXTERNAL: adapterType = "P8 external"; break;
			case CEC::cec_adapter_type::ADAPTERTYPE_EXYNOS: adapterType = "Exynos"; break;
			case CEC::cec_adapter_type::ADAPTERTYPE_TDA995x: adapterType = "TDA995x"; break;
			case CEC::cec_adapter_type::ADAPTERTYPE_AOCEC: adapterType = "AOCEC"; break;
		}

		if (_cecAdapter->Open(adapter.strComName))
		{
			Info(_log, "%s", QSTRING_CSTR(QString("Success. CEC adapter type '%3' is ready (%1, %2)").arg(adapter.strComName).arg(adapter.strComPath).arg(adapterType)));

			opened = true;
			break;
		}
		else
		{
			Error(_log, "%s", QSTRING_CSTR(QString("Failed to open CEC adapter type '%2' (%1)").arg(adapter.strComName).arg(adapterType)));
		}	
	}
	

	if (!opened)
	{
		Error(_log, "Could not initialized any CEC device. Give up.");
		UnloadLibCec(_cecAdapter);
		_cecAdapter = nullptr;
	}

	return opened;
}

void cecHandler::stop()
{	
	if (_cecAdapter)
	{
		Info(_log, "Stopping CEC handler");
		_cecAdapter->Close();
		UnloadLibCec(_cecAdapter);
		_cecAdapter = nullptr;
	}
}

void handleCecLogMessage(void * context, const CEC::cec_log_message* message)
{	
	cecHandler* handler = static_cast<cecHandler*>(context);

	if (handler == nullptr)
		return;

	switch (message->level)
	{
		case CEC::CEC_LOG_ERROR:
			Error(handler->_log, "CEC library signaling error: %s", message->message);
			break;
		case CEC::CEC_LOG_WARNING:
			Warning(handler->_log, "CEC library warning: %s", message->message);
			break;
		default:
			break;
	}	
}

void handleCecKeyPress(void* context, const CEC::cec_keypress* key)
{
	cecHandler* handler = static_cast<cecHandler*>(context);

	if (handler == nullptr)
		return;

	Debug(handler->_log, "Key pressed: %s, (key code = %i)", _cecAdapter->ToString(key->keycode), (int)key->keycode);

	emit handler->keyPressed((int)key->keycode);
}


void handleCecCommandMessage(void * context, const CEC::cec_command* command)
{
	
	cecHandler* handler = static_cast<cecHandler*>(context);

	if (handler == nullptr || _cecAdapter == nullptr ||
		(command->opcode != CEC::CEC_OPCODE_SET_STREAM_PATH && command->opcode != CEC::CEC_OPCODE_STANDBY))
		return;	

	if (command->opcode == CEC::CEC_OPCODE_SET_STREAM_PATH)
	{
		emit handler->stateChange(true, QString(_cecAdapter->ToString(command->initiator)));
	}
	else
	{
		emit handler->stateChange(false, QString(_cecAdapter->ToString(command->initiator)));
	}
	
}
