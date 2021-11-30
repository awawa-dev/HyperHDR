#pragma once
class MFCallback : public IMFSourceReaderCallback
{
public:
	MFCallback(MFGrabber* grabber);

	STDMETHODIMP QueryInterface(REFIID iid, void** ppv);

	STDMETHODIMP_(ULONG) AddRef();

	STDMETHODIMP_(ULONG) Release();

	STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
		DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample* pSample);

	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*);

	STDMETHODIMP OnFlush(DWORD);

public:
	void WaitToQuit();

private:

	virtual ~MFCallback();

	void NotifyError(HRESULT hr);

private:
	long                _referenceCounter;
	CRITICAL_SECTION    _criticalSection;
	MFGrabber*          _grabber;
	HANDLE				_eventHandler;
	BOOL                _endOfStream;
	HRESULT             _callbackStatus;
};
