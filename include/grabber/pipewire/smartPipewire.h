#pragma once

#include <cstdint>

struct PipewireImage
{
	int		version;
	bool	isError = false;
	int		width, height, stride;
	bool	isOrderRgb;
	uint8_t* data = nullptr;
};
extern "C" const char* getPipewireToken();
extern "C" const char* getPipewireError();
extern "C" bool hasPipewire();
extern "C" void initPipewireDisplay(const char* restorationToken, uint32_t requestedFPS);
extern "C" void uninitPipewireDisplay();
extern "C" PipewireImage getFramePipewire();
extern "C" void releaseFramePipewire();

