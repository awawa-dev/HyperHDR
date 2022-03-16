#pragma once

#include <base/SoundCapture.h>
#include <windows.h>
#include "mmsystem.h"

#define SOUNDCAPWINDOWS_BUF_LENP 10
class SoundCapWindows : public SoundCapture
{
	friend class HyperHdrDaemon;
public:

private:
	SoundCapWindows(const QJsonDocument& effectConfig, QObject* parent = nullptr);
	~SoundCapWindows();
	void ListDevices();
	void    Start() override;
	void    Stop() override;

	static void CALLBACK soundInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
	static int16_t    _bufferIndex;
	static int16_t    _soundBuffer[(1 << SOUNDCAPWINDOWS_BUF_LENP) + 64];
	static WAVEHDR    _header;
	static HWAVEIN    _hWaveIn;
};

