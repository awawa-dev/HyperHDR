#pragma once
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include <alsa/asoundlib.h>

#include <QSemaphore>
#include <QTimer>

#include <base/SoundCapture.h>

#define SOUNDCAPLINUX_BUF_LENP 10
class SoundCapLinux : public SoundCapture
{
    friend class HyperHdrDaemon;
    public:

    private:
			SoundCapLinux(const QJsonDocument& effectConfig, QObject* parent = nullptr);
			~SoundCapLinux();
			void ListDevices();
			void    Start() override;
			void    Stop() override;

			snd_pcm_t*				_handle;
			snd_async_handler_t*	_pcmCallback;

	private:
			static void RecordCallback(snd_async_handler_t* audioHandler);

			static int16_t    _soundBuffer[(1<<SOUNDCAPLINUX_BUF_LENP)*2];
};
