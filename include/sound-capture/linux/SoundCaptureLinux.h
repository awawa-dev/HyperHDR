#pragma once

#ifndef PCH_ENABLED
	#include <atomic>
#endif

#include <base/SoundCapture.h>

#include <alsa/asoundlib.h>


typedef int (*snd_pcm_open_fun)(snd_pcm_t** pcm, const char* name, snd_pcm_stream_t stream, int mode);
typedef int (*snd_pcm_hw_params_malloc_fun)(snd_pcm_hw_params_t** ptr);
typedef int (*snd_pcm_hw_params_any_fun)(snd_pcm_t* pcm, snd_pcm_hw_params_t* params);
typedef int (*snd_pcm_hw_params_set_access_fun)(snd_pcm_t* pcm, snd_pcm_hw_params_t* params, snd_pcm_access_t _access);
typedef int (*snd_pcm_hw_params_set_format_fun)(snd_pcm_t* pcm, snd_pcm_hw_params_t* params, snd_pcm_format_t val);
typedef int (*snd_pcm_hw_params_set_rate_near_fun)(snd_pcm_t* pcm, snd_pcm_hw_params_t* params, unsigned int* val, int* dir);
typedef int (*snd_pcm_hw_params_set_channels_fun)(snd_pcm_t* pcm, snd_pcm_hw_params_t* params, unsigned int val);
typedef int (*snd_pcm_hw_params_set_period_size_near_fun)(snd_pcm_t* pcm, snd_pcm_hw_params_t* params, snd_pcm_uframes_t* val, int* dir);
typedef int (*snd_pcm_hw_params_set_buffer_size_near_fun)(snd_pcm_t* pcm, snd_pcm_hw_params_t* params, snd_pcm_uframes_t* val);
typedef int (*snd_pcm_hw_params_fun)(snd_pcm_t* pcm, snd_pcm_hw_params_t* params);
typedef snd_pcm_state_t (*snd_pcm_state_fun)(snd_pcm_t* pcm);
typedef int (*snd_pcm_start_fun)(snd_pcm_t* pcm);
typedef void (*snd_pcm_hw_params_free_fun)(snd_pcm_hw_params_t* obj);
typedef int (*snd_pcm_wait_fun)(snd_pcm_t* pcm, int timeout);
typedef snd_pcm_sframes_t (*snd_pcm_readi_fun)(snd_pcm_t* pcm, void* buffer, snd_pcm_uframes_t size);
typedef int (*snd_pcm_prepare_fun)(snd_pcm_t* pcm);
typedef int (*snd_pcm_drop_fun)(snd_pcm_t* pcm);
typedef int (*snd_pcm_hw_free_fun)(snd_pcm_t* pcm);
typedef int (*snd_pcm_close_fun)(snd_pcm_t* pcm);
typedef int (*snd_device_name_hint_fun)(int card, const char* iface, void*** hints);
typedef int	(*snd_device_name_free_hint_fun)(void** hints);
typedef char* (*snd_device_name_get_hint_fun)(const void* hint, const char* id);
typedef int (*snd_config_update_free_global_fun)(void);
typedef const char* (*snd_strerror_fun)(int errnum);
typedef snd_pcm_sframes_t (*snd_pcm_avail_update_fun)(snd_pcm_t* pcm);

class AlsaWorkerThread : public QThread
{
	Q_OBJECT
		
		std::atomic<bool> _exitNow{ false };
		Logger* _logger;
		QString _device;
		SoundCapture* _parent;

		void run() override;

	public:
		AlsaWorkerThread(Logger* logger, QString device, SoundCapture* parent);
		void exitNow();

	private:
		bool initAlsaLib();
		void closeAlsaLib();

		void* _library = nullptr;
		snd_pcm_open_fun							snd_pcm_open = nullptr;
		snd_pcm_hw_params_malloc_fun				snd_pcm_hw_params_malloc = nullptr;
		snd_pcm_hw_params_any_fun					snd_pcm_hw_params_any = nullptr;
		snd_pcm_hw_params_set_access_fun			snd_pcm_hw_params_set_access = nullptr;
		snd_pcm_hw_params_set_format_fun			snd_pcm_hw_params_set_format = nullptr;
		snd_pcm_hw_params_set_rate_near_fun			snd_pcm_hw_params_set_rate_near = nullptr;
		snd_pcm_hw_params_set_channels_fun			snd_pcm_hw_params_set_channels = nullptr;
		snd_pcm_hw_params_set_period_size_near_fun	snd_pcm_hw_params_set_period_size_near = nullptr;
		snd_pcm_hw_params_set_buffer_size_near_fun	snd_pcm_hw_params_set_buffer_size_near = nullptr;
		snd_pcm_hw_params_fun						snd_pcm_hw_params = nullptr;
		snd_pcm_state_fun							snd_pcm_state = nullptr;
		snd_pcm_start_fun							snd_pcm_start = nullptr;
		snd_pcm_hw_params_free_fun					snd_pcm_hw_params_free = nullptr;
		snd_pcm_wait_fun							snd_pcm_wait = nullptr;
		snd_pcm_readi_fun							snd_pcm_readi = nullptr;
		snd_pcm_prepare_fun							snd_pcm_prepare = nullptr;
		snd_pcm_drop_fun							snd_pcm_drop = nullptr;
		snd_pcm_hw_free_fun							snd_pcm_hw_free = nullptr;
		snd_pcm_close_fun							snd_pcm_close = nullptr;
		snd_config_update_free_global_fun			snd_config_update_free_global = nullptr;
		snd_strerror_fun							snd_strerror = nullptr;
		snd_pcm_avail_update_fun					snd_pcm_avail_update = nullptr;
};

class SoundCaptureLinux : public SoundCapture
{
	Q_OBJECT

	public:
		SoundCaptureLinux(const QJsonDocument& effectConfig, QObject* parent = nullptr);
		~SoundCaptureLinux();
		void start() override;
		void stop() override;
	private:
		bool initAlsaLib();
		void closeAlsaLib();
		void stopDevice();
		void listDevices();
		AlsaWorkerThread* _thread;

		void* _library = nullptr;
		snd_device_name_hint_fun					snd_device_name_hint = nullptr;
		snd_device_name_free_hint_fun				snd_device_name_free_hint = nullptr;
		snd_device_name_get_hint_fun				snd_device_name_get_hint = nullptr;
		snd_config_update_free_global_fun			snd_config_update_free_global = nullptr;
};

typedef SoundCaptureLinux SoundGrabber;
