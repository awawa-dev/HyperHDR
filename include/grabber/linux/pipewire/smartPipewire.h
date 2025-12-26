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
extern "C" const char* getPipewireToken();
extern "C" const char* getPipewireError();
extern "C" bool hasPipewire();
extern "C" void initPipewireDisplay(const char* restorationToken, uint32_t requestedFPS, bool enableEGL, int targetMaxSize);
extern "C" void uninitPipewireDisplay();
extern "C" PipewireImage getFramePipewire();
extern "C" void releaseFramePipewire();

