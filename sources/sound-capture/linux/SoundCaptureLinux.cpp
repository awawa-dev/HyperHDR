/* SoundCaptureLinux.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#include <QSemaphore>
#include <QString>
#include <QTimer>

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <dlfcn.h>

#include <sound-capture/linux/SoundCaptureLinux.h>
#include <utils/InternalClock.h>

#define ALSALIB "libasound.so.2"

#define GET_FUN_HOOK(x) { \
	this->x = nullptr; \
	this->x = (x ## _fun)dlsym(this->_library, #x); \
	if (this->x == nullptr) \
		throw std::runtime_error(#x); \
}

#define SOUNDCAPLINUX_BUF_LENP 10

AlsaWorkerThread::AlsaWorkerThread(Logger* logger, QString device, SoundCapture* parent) :
	_logger(logger),
	_device(device),
	_parent(parent)
{
}

void AlsaWorkerThread::exitNow()
{
	_exitNow = true;
}

void AlsaWorkerThread::closeAlsaLib()
{
	if (_library)
	{
		dlclose(_library);
		_library = nullptr;
	}
}

bool AlsaWorkerThread::initAlsaLib()
{
	try
	{
		closeAlsaLib();

		if ((_library = dlopen(ALSALIB, RTLD_NOW)) == nullptr)
		{
			throw std::runtime_error("Could not open libasound.so.2");
		}

		GET_FUN_HOOK(snd_pcm_open);
		GET_FUN_HOOK(snd_pcm_hw_params_malloc);
		GET_FUN_HOOK(snd_pcm_hw_params_any);
		GET_FUN_HOOK(snd_pcm_hw_params_set_access);
		GET_FUN_HOOK(snd_pcm_hw_params_set_format);
		GET_FUN_HOOK(snd_pcm_hw_params_set_rate_near);
		GET_FUN_HOOK(snd_pcm_hw_params_set_channels);
		GET_FUN_HOOK(snd_pcm_hw_params_set_period_size_near);
		GET_FUN_HOOK(snd_pcm_hw_params_set_buffer_size_near);
		GET_FUN_HOOK(snd_pcm_hw_params);
		GET_FUN_HOOK(snd_pcm_state);
		GET_FUN_HOOK(snd_pcm_start);
		GET_FUN_HOOK(snd_pcm_hw_params_free);
		GET_FUN_HOOK(snd_pcm_wait);
		GET_FUN_HOOK(snd_pcm_readi);
		GET_FUN_HOOK(snd_pcm_prepare);
		GET_FUN_HOOK(snd_pcm_drop);
		GET_FUN_HOOK(snd_pcm_hw_free);
		GET_FUN_HOOK(snd_pcm_close);
		GET_FUN_HOOK(snd_config_update_free_global);
		GET_FUN_HOOK(snd_strerror);
		GET_FUN_HOOK(snd_pcm_avail_update);
	}
	catch (std::exception& ex)
	{
		closeAlsaLib();
		Error(_logger, "Loading Alsa failed: '%s'", ex.what());
		return false;
	}

	return true;
}

void AlsaWorkerThread::run(){
	snd_pcm_sframes_t workingSizeSample = (1 << SOUNDCAPLINUX_BUF_LENP);
	snd_pcm_uframes_t periodSize = workingSizeSample;
	snd_pcm_uframes_t bufferSizeInBytes = periodSize * 2;
	bool			initialMessage = false;
	snd_pcm_t*		handle;
	int				status = 0;
	bool    		error = false;
	unsigned int 	exactRate = 22050;

	if (!initAlsaLib())
		return;

	if ((status = this->snd_pcm_open(&handle, QSTRING_CSTR(_device), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		Error(_logger, "Cannot open input sound device '%s'. Error: '%s'", QSTRING_CSTR(_device), this->snd_strerror(status));

		// clean-up
		closeAlsaLib();
		return;
	}

	snd_pcm_hw_params_t* selected_hw_params = nullptr;

	if ((status = this->snd_pcm_hw_params_malloc(&selected_hw_params)) < 0) {
		Error(_logger, "Cannot allocate hardware parameter buffer: '%s'", this->snd_strerror(status));
		this->snd_pcm_close(handle);

		// clean-up
		closeAlsaLib();
		return;
	}

	try
	{
		if ((status = this->snd_pcm_hw_params_any(handle, selected_hw_params)) < 0) {
			Error(_logger, "Cannot set snd_pcm_hw_params_any: '%s'", this->snd_strerror(status));
			throw 1;
		}

		if ((status = this->snd_pcm_hw_params_set_access(handle, selected_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
			Error(_logger, "Cannot set snd_pcm_hw_params_set_access: '%s'", this->snd_strerror(status));
			throw 2;
		}

		if ((status = this->snd_pcm_hw_params_set_format(handle, selected_hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
			Error(_logger, "Cannot set snd_pcm_hw_params_set_format: '%s'", this->snd_strerror(status));
			throw 3;
		}

		if ((status = this->snd_pcm_hw_params_set_rate_near(handle, selected_hw_params, &exactRate, 0)) < 0) {
			Error(_logger, "Cannot set snd_pcm_hw_params_set_rate_near: '%s'", this->snd_strerror(status));
			throw 4;
		}
		else if (exactRate != 22050)
		{
			Error(_logger, "Cannot set rate to 22050");
			throw 5;
		}

		if ((status = this->snd_pcm_hw_params_set_channels(handle, selected_hw_params, 1)) < 0) {
			Error(_logger, "Cannot set snd_pcm_hw_params_set_channels: '%s'", this->snd_strerror(status));
			throw 6;
		}

		if ((status = this->snd_pcm_hw_params_set_period_size_near(handle, selected_hw_params, &periodSize, 0)) < 0)
		{
			Error(_logger, "Cannot set snd_pcm_hw_params_set_period_size_near: '%s'", this->snd_strerror(status));
			throw 7;
		}
		else
			Info(_logger, "Sound period size = %lu", (unsigned long)periodSize);

		bufferSizeInBytes = periodSize * 2;	

		if ((status = this->snd_pcm_hw_params_set_buffer_size_near(handle, selected_hw_params, &bufferSizeInBytes)) < 0)
		{
			Error(_logger, "Cannot set snd_pcm_hw_params_set_buffer_size_near: '%s'", this->snd_strerror(status));
			throw 8;
		}
		else
			Info(_logger, "Sound buffer size = %lu", (unsigned long)bufferSizeInBytes);		

		if ((status = this->snd_pcm_hw_params(handle, selected_hw_params)) < 0) {
			Error(_logger, "Cannot set snd_pcm_hw_params: '%s'", this->snd_strerror(status));
			throw 9;
		}

		if ((status = this->snd_pcm_state(handle)) != SND_PCM_STATE_PREPARED) {
			Error(_logger, "Preparing device failed: '%s'", this->snd_strerror(status));
			throw 10;
		}

		if ((status = this->snd_pcm_start(handle)) < 0) {
			Error(_logger, "ALSA start failed: '%s'", this->snd_strerror(status));
			throw 11;
		}

	}
	catch (...)
	{
		error = true;
	}

	this->snd_pcm_hw_params_free(selected_hw_params);

	std::vector<int16_t> soundBuffer(workingSizeSample, 0);

	while (!error && !_exitNow)
	{
		snd_pcm_sframes_t retVal = this->snd_pcm_wait(handle, 100);

		if (retVal > 0 && !_exitNow)
		{
			retVal = this->snd_pcm_readi(handle, soundBuffer.data(), soundBuffer.size());
			if (retVal >= workingSizeSample && !_exitNow)
			{
				_parent->analyzeSpectrum(soundBuffer.data(), SOUNDCAPLINUX_BUF_LENP);

				if (!initialMessage)
					Debug(_logger, "Got new audio frame: %lli bytes. Remains: %lli bytes", workingSizeSample, this->snd_pcm_avail_update(handle));
				initialMessage = true;
			}
			else if (!_exitNow)
				printf("Underrun has occured. The sound frame is skipped\n");
		}

		if (retVal == -EPIPE && !_exitNow)
		{
			retVal = this->snd_pcm_prepare(handle);
			if (retVal < 0)
			{
				Error(_logger, "Could not recover from EPIPE: %s. Sound capturing thread is exiting now.", this->snd_strerror(retVal));
				break;
			}

			snd_pcm_state_t state = this->snd_pcm_state(handle);
			Warning(_logger, "EPIPE detected. Current state: %i", state);

			if ((status = this->snd_pcm_start(handle)) < 0) {
				Error(_logger, "ALSA restart after EPIPE failed: '%s'", this->snd_strerror(status));
				break;
			}
		}
		else if (retVal < 0)
		{
			Error(_logger, "Critical error detected: %s. Sound capturing thread is exiting now.", this->snd_strerror(retVal));
			break;
		}
	}

	this->snd_pcm_drop(handle);
	this->snd_pcm_hw_free(handle);
	this->snd_pcm_close(handle);
	this->snd_config_update_free_global();

	// clean-up
	closeAlsaLib();
};


SoundCaptureLinux::SoundCaptureLinux(const QJsonDocument& effectConfig, QObject* parent)
                                        : SoundCapture(effectConfig, parent)
{
	_thread = nullptr;
	listDevices();
}

void SoundCaptureLinux::closeAlsaLib()
{
	if (_library)
	{
		dlclose(_library);
		_library = nullptr;
	}
}

bool SoundCaptureLinux::initAlsaLib()
{
	try
	{
		closeAlsaLib();

		if ((_library = dlopen(ALSALIB, RTLD_NOW)) == nullptr)
		{
			throw std::runtime_error("Could not open libasound.so.2");
		}

		GET_FUN_HOOK(snd_device_name_hint);
		GET_FUN_HOOK(snd_device_name_free_hint);
		GET_FUN_HOOK(snd_device_name_get_hint);
		GET_FUN_HOOK(snd_config_update_free_global);
	}
	catch (std::exception& ex)
	{
		closeAlsaLib();
		Error(_logger, "Loading Alsa failed: '%s'", ex.what());
		return false;
	}

	return true;
}

void SoundCaptureLinux::listDevices()
{
	char** devices = nullptr;
	char** device = nullptr;

	if (!initAlsaLib())
		return;

	int status = this->snd_device_name_hint(-1, "pcm", (void***)&devices);
	if (status != 0)
	{
		Error(_logger, "Could not find sound devices for enumerating (%d)", status);

		// clean-up
		closeAlsaLib();
		return;
	}

	device = devices;
	while (*device != NULL)
	{
		char *name = this->snd_device_name_get_hint(*device, "NAME");

		if (name != nullptr)
		{
			char* desc = this->snd_device_name_get_hint(*device, "DESC");
			char* ioid = this->snd_device_name_get_hint(*device, "IOID");

			if (ioid == NULL || std::strcmp(ioid, "Output") != 0)
			{
				_availableDevices.append(QString(name) + " | " + QString(desc) + " | " + QString(ioid));
			}

			if (name != nullptr) free(name);
			if (desc != nullptr) free(desc);
			if (ioid != nullptr) free(ioid);
		}
		device++;
	}
	this->snd_device_name_free_hint((void**)devices);
	this->snd_config_update_free_global();

	// clean-up
	closeAlsaLib();
}

SoundCaptureLinux::~SoundCaptureLinux()
{
	stopDevice();
}

void SoundCaptureLinux::start()
{
	if (_isActive && !_isRunning)
	{	
		QStringList deviceList = _selectedDevice.split('|');
		
		if (deviceList.size() == 0)
		{
			Error(_logger, "Invalid device name: %s", QSTRING_CSTR(_selectedDevice));
		}
		
		QString device = deviceList.at(0).trimmed();
		_normalizedName = _selectedDevice;
		auto it = std::remove_if(_normalizedName.begin(), _normalizedName.end(), [](const QChar& c) { return !c.isLetterOrNumber() && c!=' ' && c!=':' && c!='=' && c!='|' && c!='.'; });
		_normalizedName.chop(std::distance(it, _normalizedName.end()));
		Info(_logger, "Opening device: %s", QSTRING_CSTR(_normalizedName));		

		_isRunning = true;

		_thread = new AlsaWorkerThread(_logger, device, this);
		_thread->setObjectName("SoundCapturing");
		connect(_thread, &AlsaWorkerThread::finished, this, [=]() {stop(); });		
		_thread->start();		
	}
}

void SoundCaptureLinux::stop()
{
	stopDevice();
}

void SoundCaptureLinux::stopDevice()
{
	if (!_isRunning)
		return;

	Info(_logger, "Closing hardware sound driver: '%s'", QSTRING_CSTR(_normalizedName));

	_isRunning = false;

	if (_thread != nullptr)
	{
		_thread->disconnect(this);
		_thread->exitNow();
		_thread->quit();
		_thread->wait();
		delete _thread;
		_thread = nullptr;
	}

	Info(_logger, "Hardware sound driver is closed");
}

