struct PipewireImage
{
	int		version;
	bool	isError;
	int		width, height;
	bool	isOrderRgb;
	unsigned char* data;
};
extern "C" const char* getPipewireToken();
extern "C" const char* getPipewireError();
extern "C" bool hasPipewire();
extern "C" void initPipewireDisplay(const char* restorationToken);
extern "C" void uniniPipewireDisplay();
extern "C" PipewireImage getFramePipewire();
extern "C" void releaseFramePipewire();

