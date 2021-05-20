#pragma once
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <mferror.h>
#include <strmif.h>

#include <grabber/MFGrabber.h>
#include <grabber/MFCallback.h>

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
}

void MFCallback::NotifyError(HRESULT hr)
{
	wprintf(L"Source Reader error: 0x%X\n", hr);
}
