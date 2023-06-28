#pragma once
#include <stdio.h>

#include <QSemaphore>
#include <QTimer>

#include <base/SoundCapture.h>

#define SOUNDCAPMACOS_BUF_LENP 10
class SoundCapMacOS : public SoundCapture
{
    friend class HyperHdrDaemon;
    public:
			void RecordCallback(uint32_t frameBytes, uint8_t* rawVideoData);
    private:
			SoundCapMacOS(const QJsonDocument& effectConfig, QObject* parent = nullptr);
			~SoundCapMacOS();
			void ListDevices();
			void Start() override;
			void Stop() override;
			bool getPermission();

			static size_t     _soundBufferIndex;
			static int16_t    _soundBuffer[(1<<SOUNDCAPMACOS_BUF_LENP)*2];
};
