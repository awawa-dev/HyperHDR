/* MFCallback.cpp
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

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <mferror.h>
#pragma push_macro("Info")
	#undef Info
	#include <strmif.h>
#pragma pop_macro("Info")


#include <grabber/windows/MF/MFGrabber.h>
#include <grabber/windows/MF/MFCallback.h>

MFCallback::MFCallback(MFGrabber* grabber) :
	_referenceCounter(1), _grabber(grabber), _endOfStream(FALSE), _callbackStatus(S_OK)
{
	_eventHandler = CreateEvent(NULL, FALSE, FALSE, NULL);

	InitializeCriticalSection(&_criticalSection);
}

STDMETHODIMP MFCallback::QueryInterface(REFIID iid, void** ppv)
{
	static const QITAB qit[] =
	{
		QITABENT(MFCallback, IMFSourceReaderCallback),
		{ 0 },
	};
	return QISearch(this, qit, iid, ppv);
}

STDMETHODIMP_(ULONG) MFCallback::AddRef()
{
	return InterlockedIncrement(&_referenceCounter);
}

STDMETHODIMP_(ULONG) MFCallback::Release()
{
	ULONG uCount = InterlockedDecrement(&_referenceCounter);
	if (uCount == 0)
	{
		delete this;
	}
	return uCount;
}

STDMETHODIMP MFCallback::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
	DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample* pSample)
{

	EnterCriticalSection(&_criticalSection);

	if (SUCCEEDED(hrStatus))
	{
		QString error = "";
		bool frameSend = false;

		if (pSample)
		{
			IMFMediaBuffer* buffer;

			hrStatus = pSample->ConvertToContiguousBuffer(&buffer);
			if (SUCCEEDED(hrStatus))
			{

				BYTE* data = nullptr;
				DWORD maxLength = 0, currentLength = 0;

				hrStatus = buffer->Lock(&data, &maxLength, &currentLength);
				if (SUCCEEDED(hrStatus))
				{
					frameSend = true;

					_grabber->receive_image(data, currentLength, error);

					buffer->Unlock();
				}
				else
					error = QString("Buffer's lock failed (%1)").arg(hrStatus);

				buffer->Release();
			}
			else
				error = QString("Sample's convertToContiguousBuffer failed (%1)").arg(hrStatus);

		}
		else
			error = "Warning: receive empty sample";

		if (!frameSend)
			_grabber->receive_image(NULL, 0, error);
	}
	else
	{
		NotifyError(hrStatus);
	}

	if (MF_SOURCE_READERF_ENDOFSTREAM & dwStreamFlags)
	{
		_endOfStream = TRUE;
	}
	_callbackStatus = hrStatus;

	LeaveCriticalSection(&_criticalSection);
	return S_OK;
}


STDMETHODIMP MFCallback::OnEvent(DWORD, IMFMediaEvent*)
{
	return S_OK;
}

STDMETHODIMP MFCallback::OnFlush(DWORD)
{
	return S_OK;
}

void MFCallback::WaitToQuit()
{
	if (_eventHandler != NULL)
	{
		WaitForSingleObject(_eventHandler, 500);
	}
}

MFCallback::~MFCallback()
{
	if (_eventHandler != NULL)
	{
		CloseHandle(_eventHandler);
		_eventHandler = NULL;
	}
	DeleteCriticalSection(&_criticalSection);
}

void MFCallback::NotifyError(HRESULT hr)
{
	wprintf(L"Source Reader error: 0x%X\n", hr);
}
