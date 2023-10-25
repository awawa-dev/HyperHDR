#pragma once

#include <cstdint>

struct PipewireImage
{
	int		version;
	bool	isError;
	int		width, height, stride;
	bool	isOrderRgb;
	uint8_t* data;
};

typedef int (*pipewire_callback_func)(const PipewireImage& frame);

extern "C" const char* getPipewireToken();
extern "C" const char* getPipewireError();
extern "C" bool hasPipewire();
extern "C" void initPipewireDisplay(const char* restorationToken, uint32_t requestedFPS, pipewire_callback_func callback);
extern "C" void uniniPipewireDisplay();

