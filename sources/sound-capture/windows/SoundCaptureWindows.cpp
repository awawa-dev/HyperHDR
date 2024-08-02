/* SoundCaptureWindows.cpp
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

#include <sound-capture/windows/SoundCaptureWindows.h>
#include <utils/Logger.h>

#include <algorithm>
#include <cmath>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define SOUNDCAPWINDOWS_BUF_LENP 10

WindowsSoundThread::WindowsSoundThread(Logger* logger, QString device, SoundCapture* parent) :
	_logger(logger),
	_device(device),
	_parent(parent)
{
}

void WindowsSoundThread::exitNow()
{
	_exitNow = true;
}

void WindowsSoundThread::run()
{
	const int periodSize = (1 << SOUNDCAPWINDOWS_BUF_LENP);
	const int bufferSizeInBytes = periodSize * 2;

	std::vector<int16_t> soundBuffer(periodSize);

	WAVEHDR	header = {};
	HWAVEIN	hWaveIn = nullptr;
	bool initialMessage = false;

	UINT deviceCount = waveInGetNumDevs(), found = 0;

	for (found = 0; found < deviceCount; found++)
	{
		WAVEINCAPSW waveCaps;
		memset(&waveCaps, 0, sizeof(waveCaps));
		waveInGetDevCapsW(found, &waveCaps, sizeof(WAVEINCAPSW));

		if (QString::fromWCharArray(waveCaps.szPname).compare(_device) == 0)
			break;
	}

	if (found >= deviceCount)
	{
		Error(_logger, "Could not find '%s' device for open", QSTRING_CSTR(_device));
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

	MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &pFormat, 0L, 0L, WAVE_FORMAT_DIRECT);

	if (result != MMSYSERR_NOERROR)
	{
		Error(_logger, "Error during opening sound device '%s'. Error code: %i", QSTRING_CSTR(_device), result);
		return;
	}

	header.lpData = (LPSTR)&soundBuffer[0];
	header.dwBufferLength = bufferSizeInBytes;
	header.dwFlags = 0L;
	header.dwLoops = 0L;

	waveInPrepareHeader(hWaveIn, &header, sizeof(WAVEHDR));
	waveInAddBuffer(hWaveIn, &header, sizeof(WAVEHDR));

	result = waveInStart(hWaveIn);
	if (result != MMSYSERR_NOERROR)
	{
		waveInClose(hWaveIn);
		Error(_logger, "Error during starting sound device '%s'. Error code: %i", QSTRING_CSTR(_device), result);
		return;
	}

	while (!_exitNow)
	{
		if (header.dwFlags & WHDR_DONE)
		{
			_parent->analyzeSpectrum(soundBuffer.data(), SOUNDCAPWINDOWS_BUF_LENP);

			header.dwFlags = 0L;
			header.dwLoops = 0L;

			waveInPrepareHeader(hWaveIn, &header, sizeof(WAVEHDR));
			waveInAddBuffer(hWaveIn, &header, sizeof(WAVEHDR));
		}
	}

	waveInStop(hWaveIn);
	waveInUnprepareHeader(hWaveIn, &header, sizeof(WAVEHDR));
	waveInClose(hWaveIn);
};


SoundCaptureWindows::SoundCaptureWindows(const QJsonDocument& effectConfig, QObject* parent)
	: SoundCapture(effectConfig, parent)
{
	_thread = nullptr;
	listDevices();
}


void SoundCaptureWindows::listDevices()
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

SoundCaptureWindows::~SoundCaptureWindows()
{
	stopDevice();
}

void SoundCaptureWindows::start()
{
	if (_isActive && !_isRunning)
	{
		QStringList deviceList = _selectedDevice.split('|');

		if (deviceList.size() == 0)
		{
			Error(_logger, "Invalid device name: %s", QSTRING_CSTR(_selectedDevice));
		}

		QString device = deviceList.at(0).trimmed();
		_normalizedName = device;
		Info(_logger, "Opening device: %s", QSTRING_CSTR(_normalizedName));

		_isRunning = true;

		_thread = new WindowsSoundThread(_logger, device, this);
		_thread->setObjectName("SoundCapturing");
		connect(_thread, &WindowsSoundThread::finished, this, [=]() {stop(); });
		_thread->start();
	}
}

void SoundCaptureWindows::stop()
{
	stopDevice();
}

void SoundCaptureWindows::stopDevice()
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
};
