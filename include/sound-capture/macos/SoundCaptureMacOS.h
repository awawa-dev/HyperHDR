#pragma once

#include <base/SoundCapture.h>

#define SOUNDCAPMACOS_BUF_LENP 10

class SoundCaptureMacOS : public SoundCapture
{
    public:
		SoundCaptureMacOS(const QJsonDocument& effectConfig, QObject* parent = nullptr);
		~SoundCaptureMacOS();

		void recordCallback(uint32_t frameBytes, uint8_t* rawAudioData);
		void start() override;
		void stop() override;

    private:
		void stopDevice();
		void listDevices();
		bool getPermission();		

		size_t     _soundBufferIndex = 0;
		int16_t    _soundBuffer[(1<<SOUNDCAPMACOS_BUF_LENP)*2] = {};
};

typedef SoundCaptureMacOS SoundGrabber;
