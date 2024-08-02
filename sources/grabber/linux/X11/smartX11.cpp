/* smartX11.cpp
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

#include <grabber/linux/X11/smartX11.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdio.h>

struct x11Displays* enumerateX11Displays()
{
	Display* myDisplay = XOpenDisplay(nullptr);

	if (myDisplay == nullptr)
		return nullptr;

	auto dispCount = ScreenCount(myDisplay);

	if (dispCount <= 0)
	{
		XCloseDisplay(myDisplay);
		return nullptr;
	}

	struct x11Displays* retVal = (struct x11Displays*) malloc(sizeof(struct x11Displays));
	retVal->count = dispCount;
	retVal->display = (struct x11Display*) malloc(dispCount * sizeof(struct x11Display));

	for (int i = 0; i < dispCount; i++) {

		Screen* myScreen = ScreenOfDisplay(myDisplay, i);
		((retVal->display)[i]).index = i;
		((retVal->display)[i]).x = myScreen->width;
		((retVal->display)[i]).y = myScreen->height;
		snprintf(((retVal->display)[i]).screenName, 50, "Display nr: %d", i);
	}

	XCloseDisplay(myDisplay);

	return retVal;
}

void releaseX11Displays(struct x11Displays* buffer)
{
	if (buffer == nullptr || buffer->display == nullptr)
		return;

	free(buffer->display);

	buffer->display = 0;

	buffer->display = nullptr;

	free(buffer);
}

static int x11errorHandler(Display* d, XErrorEvent* e)
{
	return 0;
}

static XErrorHandler oldHandler = nullptr;

x11Handle* initX11Display(int display)
{	
	Display* maindisplay = XOpenDisplay(NULL);

	if (maindisplay == nullptr)
		return nullptr;

	struct x11Handle* retVal = (struct x11Handle*)malloc(sizeof(struct x11Handle));

	retVal->handle = (void*)maindisplay;	
	retVal->index = display;
	retVal->image = nullptr;
	retVal->width = 0;
	retVal->height = 0;
	retVal->size = 0;

	oldHandler = XSetErrorHandler(x11errorHandler);

	return retVal;
}

void releaseFrame(x11Handle* retVal)
{
	if (retVal == nullptr)
		return;

	if (retVal->image != nullptr)
		XDestroyImage((XImage*)retVal->image);

	retVal->image = nullptr;
}

void uninitX11Display(x11Handle* retVal)
{
	if (oldHandler != nullptr)
	{
		XSetErrorHandler(oldHandler);
		oldHandler = nullptr;
	}

	if (retVal == nullptr)
		return;

	releaseFrame(retVal);

	if (retVal->handle != nullptr)
		XCloseDisplay((Display*)retVal->handle);

	retVal->handle = nullptr;

	free(retVal);	
}

unsigned char* getFrame(x11Handle* retVal)
{
	if (retVal == nullptr || retVal->handle == nullptr)
		return nullptr;

	releaseFrame(retVal);

	Window window = RootWindow((Display*)retVal->handle, retVal->index);
	XWindowAttributes attr = {};

	XGetWindowAttributes((Display*)retVal->handle, window, &attr);

	XImage *img = XGetImage((Display*)retVal->handle, window, 0, 0, attr.width, attr.height, AllPlanes, ZPixmap);
	retVal->image = (void*)img;

	if (retVal->image != nullptr)
	{
		retVal->width = img->width;
		retVal->height = img->height;
		retVal->size = img->bytes_per_line * img->height;

		return (unsigned char*)img->data;
	}
	else
		return nullptr;
}


