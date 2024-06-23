/* Systray.h
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

#pragma once

#include <stdint.h>
#include <string>
#include <memory>
#include <functional>

#include <image/MemoryBuffer.h>

class SystrayHandler;


struct SystrayMenu
{
	MemoryBuffer<uint8_t> icon;
	std::string label;
	std::string tooltip;

	bool isDisabled = false;
	bool isChecked = false;
	int checkGroup = false;

	std::function<void(SystrayMenu*)> callback = nullptr;
	SystrayHandler* context = nullptr;

	std::unique_ptr<SystrayMenu> submenu = nullptr;
	std::unique_ptr<SystrayMenu> next = nullptr;
};

#ifdef __linux__
	typedef bool (*SystrayInitializeFun)(SystrayMenu* tray);
	typedef int (*SystrayLoopFun)(void);
	typedef void (*SystrayUpdateFun)(SystrayMenu* tray);
	typedef void (*SystrayCloseFun)(void);
#else
	bool SystrayInitialize(SystrayMenu* tray);
	int SystrayLoop();
	void SystrayUpdate(SystrayMenu* tray);
	void SystrayClose();
#endif

#ifdef _WIN32
	HWND SystrayGetWindow();
#endif


