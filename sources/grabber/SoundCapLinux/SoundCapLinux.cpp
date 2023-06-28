/* SoundCapLinux.cpp
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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

#include <grabber/SoundCapLinux.h>
#include <utils/Logger.h>
#include <cmath>
#include <QString>
#include <stdexcept>
#include <iostream>
#include <unistd.h>

int16_t    SoundCapLinux::_soundBuffer[(1<<SOUNDCAPLINUX_BUF_LENP)*2];

SoundCapLinux::SoundCapLinux(const QJsonDocument& effectConfig, QObject* parent)
                                        : SoundCapture(effectConfig, parent),
                                        _handle(NULL),
										_pcmCallback(NULL)
{
        ListDevices();
}


void SoundCapLinux::ListDevices()
{
    char** devices;
    char** device;

    int status = snd_device_name_hint(-1, "pcm", (void***)&devices);
    if (status != 0) {
        Error(Logger::getInstance("HYPERHDR"), "Could not find sound devices for enumerating (%d)", status);
        return;
    }

    device = devices;
    while (*device != NULL)
	{
        char *name = snd_device_name_get_hint(*device, "NAME");

		if (name != nullptr)
		{
			char* desc = snd_device_name_get_hint(*device, "DESC");
			char* ioid = snd_device_name_get_hint(*device, "IOID");
			_availableDevices.append(QString(name) + " | " + QString(desc) + " | " + QString(ioid));

			if (name != nullptr) free(name);
			if (desc != nullptr) free(desc);
			if (ioid != nullptr) free(ioid);
		}
        device++;
    }
    snd_device_name_free_hint((void**)devices);
}

SoundCapLinux::~SoundCapLinux()
{
        Stop();
}

void SoundCapLinux::RecordCallback(snd_async_handler_t *audioHandler)
{
	try
	{
		if (_isRunning && audioHandler != NULL)
		{
			snd_pcm_sframes_t periodSize = (1 << SOUNDCAPLINUX_BUF_LENP);
			snd_pcm_t* shandle = snd_async_handler_get_pcm(audioHandler);
			if (shandle != NULL)
			{
				snd_pcm_sframes_t incoming = snd_pcm_avail_update(shandle);

				if (incoming >= periodSize) {
					int total = snd_pcm_readi(shandle, _soundBuffer, periodSize);
					if (total == periodSize) {
						if (AnaliseSpectrum(_soundBuffer, SOUNDCAPLINUX_BUF_LENP))
							_resultIndex++;
					}
				}
			}
		}
	}
	catch(...)
	{
	}
}

void SoundCapLinux::Start()
{
	if (_isActive && !_isRunning)
	{
		int				status;
		bool    		error = false;
		unsigned int 	exactRate = 22050;

		snd_pcm_uframes_t periodSize = (1 << SOUNDCAPLINUX_BUF_LENP) * 2;
		snd_pcm_uframes_t bufferSize = periodSize * 2;
		snd_pcm_hw_params_t *hw_params;

		QStringList deviceList = _selectedDevice.split('|');

		if (deviceList.size() == 0)
		{
			Error(Logger::getInstance("HYPERHDR"), "Invalid device name: %s", QSTRING_CSTR(_selectedDevice));
		}

		QString device = deviceList.at(0).trimmed();
		Info(Logger::getInstance("HYPERHDR"), "Opening device: %s", QSTRING_CSTR(device));


		if ((status = snd_pcm_open (&_handle, QSTRING_CSTR(device), SND_PCM_STREAM_CAPTURE, 0)) < 0) {
			Error(Logger::getInstance("HYPERHDR"), "Cannot open input sound device '%s'. Error: '%s'",  QSTRING_CSTR(device), snd_strerror (status));
			return;
		}

		if ((status = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
				Error(Logger::getInstance("HYPERHDR"), "Cannot allocate hardware parameter buffer: '%s'", snd_strerror (status));
				snd_pcm_close(_handle);
				return;
		}

		try
		{
			if ((status = snd_pcm_hw_params_any (_handle, hw_params)) < 0) {
				Error(Logger::getInstance("HYPERHDR"), "Cannot set snd_pcm_hw_params_any: '%s'", snd_strerror (status));
				throw 1;
			}

			if ((status = snd_pcm_hw_params_set_access (_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
				Error(Logger::getInstance("HYPERHDR"), "Cannot set snd_pcm_hw_params_set_access: '%s'", snd_strerror (status));
				throw 2;
			}

			if ((status = snd_pcm_hw_params_set_format (_handle, hw_params,  SND_PCM_FORMAT_S16_LE )) < 0) {
				Error(Logger::getInstance("HYPERHDR"), "Cannot set snd_pcm_hw_params_set_format: '%s'", snd_strerror (status));
				throw 3;
			}

			if ((status = snd_pcm_hw_params_set_rate_near (_handle, hw_params, &exactRate, 0)) < 0) {
				Error(Logger::getInstance("HYPERHDR"), "Cannot set snd_pcm_hw_params_set_rate_near: '%s'", snd_strerror (status));
				throw 4;
			}
			else if (exactRate != 22050)
			{
				Error(Logger::getInstance("HYPERHDR"), "Cannot set rate to 22050");
				throw 5;
			}

			if ((status = snd_pcm_hw_params_set_channels (_handle, hw_params, 1)) < 0) {
				Error(Logger::getInstance("HYPERHDR"), "Cannot set snd_pcm_hw_params_set_channels: '%s'",snd_strerror (status));
				throw 6;
			}

			if( (status = snd_pcm_hw_params_set_period_size_near(_handle, hw_params, &periodSize, 0)) < 0 )
			{
				Error(Logger::getInstance("HYPERHDR"), "Cannot set snd_pcm_hw_params_set_period_size_near: '%s'", snd_strerror(status) );
				throw 7;
			}
			else 
        		Info(Logger::getInstance("HYPERHDR"), "Sound period size = %lu", (unsigned long)periodSize);

			if( (status = snd_pcm_hw_params_set_buffer_size_near(_handle, hw_params, &bufferSize)) < 0 )
			{
				Error(Logger::getInstance("HYPERHDR"), "Cannot set snd_pcm_hw_params_set_buffer_size_near: '%s'", snd_strerror(status) );
				throw 8;
			}
			else 
        		Info(Logger::getInstance("HYPERHDR"), "Sound buffer size = %lu", (unsigned long)bufferSize);


			if ((status = snd_pcm_hw_params (_handle, hw_params)) < 0) {
				Error(Logger::getInstance("HYPERHDR"),  "Cannot set snd_pcm_hw_params: '%s'", snd_strerror (status));
				throw 9;
			}

			if ((status = snd_pcm_prepare (_handle)) < 0) {
				Error(Logger::getInstance("HYPERHDR"),  "Cannot prepare device for use: '%s'", snd_strerror (status));
				throw 10;
			}

			if ((status = snd_async_add_pcm_handler(&_pcmCallback, _handle, RecordCallback, NULL)) < 0) {
				Error(Logger::getInstance("HYPERHDR"),  "Registering record callback error, probably you chose wrong or virtual audio capture device: '%s'", snd_strerror(status));
				throw 11;
			}

			if (snd_pcm_state(_handle) == SND_PCM_STATE_PREPARED) {
        		if ((status = snd_pcm_start(_handle)) < 0) {
	        		Error(Logger::getInstance("HYPERHDR"),  "Start failed: '%s'", snd_strerror(status));
					snd_async_del_handler(_pcmCallback);
					_pcmCallback = NULL;
					throw 12;
        		}
			}
			else{
				Error(Logger::getInstance("HYPERHDR"),  "Preparing device failed: '%s'", snd_strerror(status));
				snd_async_del_handler(_pcmCallback);
				_pcmCallback = NULL;
				throw 13;
			}

		}
		catch(...)
		{
			error = true;
		}

		snd_pcm_hw_params_free (hw_params);

		if (!error)
		{
			_isRunning = true;
		}
		else
		{
			snd_pcm_close(_handle);
			_handle = NULL;
		}
	}
}

void SoundCapLinux::Stop()
{
	if (!_isRunning)
		return;

	Info(Logger::getInstance("HYPERHDR"),  "Disconnecting from sound driver: '%s'", QSTRING_CSTR(_selectedDevice));


	_isRunning = false;

	if (_pcmCallback != NULL)
	{
		snd_pcm_drop(_handle);
		snd_async_del_handler(_pcmCallback);
		_pcmCallback = NULL;
	}

	if (_handle != NULL)
	{
		snd_pcm_close(_handle);
		_handle = NULL;
	}
}
