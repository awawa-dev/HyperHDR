/* SoundCapWindows.cpp
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

#include <grabber/SoundCapWindows.h>
#include <utils/Logger.h>
#include <cmath>

#pragma comment(lib, "winmm.lib")

WAVEHDR    SoundCapWindows::_header;
HWAVEIN    SoundCapWindows::_hWaveIn;
int16_t    SoundCapWindows::_soundBuffer[(1 << SOUNDCAPWINDOWS_BUF_LENP) + 64];

SoundCapWindows::SoundCapWindows(const QJsonDocument& effectConfig, QObject* parent)
	: SoundCapture(effectConfig, parent)
{
	ListDevices();
}


void SoundCapWindows::ListDevices()
{
	UINT deviceCount = waveInGetNumDevs();

	for (UINT i = 0; i < deviceCount; i++)
	{
		WAVEINCAPSW waveCaps;
		memset(&waveCaps, 0, sizeof(waveCaps));
		waveInGetDevCapsW(i, &waveCaps, sizeof(WAVEINCAPSW));

		if (waveCaps.dwFormats & WAVE_FORMAT_2M16)
			_availableDevices.append(QString::fromWCharArray(waveCaps.szPname));
	}
}

SoundCapWindows::~SoundCapWindows()
{
	Stop();
}

void CALLBACK SoundCapWindows::soundInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	if (AnaliseSpectrum(_soundBuffer, SOUNDCAPWINDOWS_BUF_LENP))
		_resultIndex++;

	if (_isRunning)
		waveInAddBuffer(_hWaveIn, &_header, sizeof(WAVEHDR));
}

void SoundCapWindows::Start()
{
	if (_isActive && !_isRunning)
	{
		UINT deviceCount = waveInGetNumDevs(), found = 0;
		for (found = 0; found < deviceCount; found++)
		{
			WAVEINCAPSW waveCaps;
			memset(&waveCaps, 0, sizeof(waveCaps));
			waveInGetDevCapsW(found, &waveCaps, sizeof(WAVEINCAPSW));

			if (QString::fromWCharArray(waveCaps.szPname).compare(_selectedDevice) == 0)
				break;
		}

		if (found >= deviceCount)
		{
			Error(Logger::getInstance("HYPERHDR"), "Could not find '%s' device for open", QSTRING_CSTR(_selectedDevice));
			return;
		}

		WAVEFORMATEX pFormat;
		pFormat.wFormatTag = WAVE_FORMAT_PCM;
		pFormat.nChannels = 1;
		pFormat.nSamplesPerSec = 22050;
		pFormat.wBitsPerSample = 16;
		pFormat.nBlockAlign = (pFormat.nChannels * pFormat.wBitsPerSample) / 8;
		pFormat.nAvgBytesPerSec = (pFormat.nSamplesPerSec * pFormat.nChannels * pFormat.wBitsPerSample) / 8;
		pFormat.cbSize = 0;

		MMRESULT result = waveInOpen(&_hWaveIn, WAVE_MAPPER, &pFormat, (DWORD_PTR) & (soundInProc), 0L, CALLBACK_FUNCTION);

		if (result != MMSYSERR_NOERROR)
		{
			Error(Logger::getInstance("HYPERHDR"), "Error during opening sound device '%s'. Error code: %i", QSTRING_CSTR(_selectedDevice), result);
			return;
		}

		_header.lpData = (LPSTR)&_soundBuffer[0];
		_header.dwBufferLength = (1 << SOUNDCAPWINDOWS_BUF_LENP) * 2;
		_header.dwFlags = 0L;
		_header.dwLoops = 0L;
		waveInPrepareHeader(_hWaveIn, &_header, sizeof(WAVEHDR));

		waveInAddBuffer(_hWaveIn, &_header, sizeof(WAVEHDR));

		result = waveInStart(_hWaveIn);
		if (result != MMSYSERR_NOERROR)
		{
			Error(Logger::getInstance("HYPERHDR"), "Error during starting sound device '%s'. Error code: %i", QSTRING_CSTR(_selectedDevice), result);
		}
		else
			_isRunning = true;
	}
}

void SoundCapWindows::Stop()
{
	if (_isRunning)
	{
		_isRunning = false;
		waveInClose(_hWaveIn);
	}
}
