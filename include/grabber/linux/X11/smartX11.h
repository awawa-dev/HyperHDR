// only pure C here

struct x11Display
{
	int index;
	int x,y;
	char screenName[50];
};

struct x11Displays
{
	int count;
	struct x11Display *display;
};

struct x11Handle
{
	int   index;
	void* handle;
	void* image;
	int width;
	int height;
	int size;
};

extern "C" struct x11Displays* enumerateX11Displays();
extern "C" void releaseX11Displays(struct x11Displays* buffer);
extern "C" x11Handle* initX11Display(int display);
extern "C" void uninitX11Display(x11Handle * retVal);
extern "C" unsigned char* getFrame(x11Handle * retVal);
extern "C" void releaseFrame(x11Handle * retVal);

