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

namespace PipewirePortal
{
	constexpr int ScreenID_ScreenCast = 1;
	constexpr int ScreenID_RemoteDesktop = 2;
	constexpr int MinRemoteDesktopPortalVersion = 2;
}

extern "C" const char* getPipewireToken();
extern "C" const char* getPipewireError();
extern "C" bool hasPipewire();
extern "C" bool hasPipewireRemoteDesktop();
extern "C" void initPipewireDisplay(const char* restorationToken, uint32_t requestedFPS, bool enableEGL, int targetMaxSize, int selectedDisplay);
extern "C" void uninitPipewireDisplay();
extern "C" PipewireImage getFramePipewire();
extern "C" void releaseFramePipewire();
extern "C" bool isRestartNeeded();
