/* SystrayWindows.cpp
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

#include <windows.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")
#include <shellapi.h>
#include <systray/Systray.h>
#include <list>

#define WM_TRAY_CALLBACK_MESSAGE (WM_USER + 1)
#define WC_TRAY_CLASS_NAME "TRAY"
#define ID_TRAY_FIRST 1000

namespace
{
	static WNDCLASSEX wc{};
	static NOTIFYICONDATA systrayIcon{};
	static HWND window = nullptr;
	static HMENU menu = nullptr;
	static UINT WM_TASKBAR_RESTARTED = 0;
	std::list<HICON> icons;
	std::list<HBITMAP> bitmaps;
	ULONG_PTR gdiToken = 0;
}

HWND SystrayGetWindow()
{
	return window;
}

static LRESULT CALLBACK _tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam)
{
	switch (msg)
	{
		case WM_CLOSE:
			DestroyWindow(hwnd);
			hwnd = nullptr;
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_TRAY_CALLBACK_MESSAGE:
			if (lparam == WM_LBUTTONUP || lparam == WM_RBUTTONUP)
			{
				POINT p;
				GetCursorPos(&p);
				SetForegroundWindow(hwnd);
				WORD cmd = TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON |
												 TPM_RETURNCMD | TPM_NONOTIFY,
											p.x, p.y, 0, hwnd, NULL);
				SendMessage(hwnd, WM_COMMAND, cmd, 0);
				return 0;
			}
			break;

		case WM_COMMAND:
			if (wparam >= ID_TRAY_FIRST)
			{
				MENUITEMINFO item = {
					.cbSize = sizeof(MENUITEMINFO),
					.fMask = MIIM_ID | MIIM_DATA
				};
				if (GetMenuItemInfo(menu, static_cast<UINT>(wparam), FALSE, &item))
				{
					SystrayMenu* menu = reinterpret_cast<SystrayMenu*>(item.dwItemData);
					if (menu != nullptr && menu->callback != nullptr)
					{
						menu->callback(menu);
					}
				}
				return 0;
			}
			break;
	}

	if (msg == WM_TASKBAR_RESTARTED)
	{
		Shell_NotifyIcon(NIM_ADD, &systrayIcon);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HMENU _tray_menu(SystrayMenu *m, UINT *id)
{
	HMENU hmenu = CreatePopupMenu();

	for (; m != nullptr && !m->label.empty(); m = m->next.get(), (*id)++)
	{
		if (m->label == "-")
		{
			InsertMenu(hmenu, *id, MF_SEPARATOR, TRUE, "");
		}
		else
		{
			MENUITEMINFO item;
			memset(&item, 0, sizeof(item));
			item.cbSize = sizeof(MENUITEMINFO);
			item.fMask = MIIM_ID | MIIM_STATE | MIIM_DATA | MIIM_STRING;
			item.fType = 0;
			item.fState = 0;
			if (m->submenu != NULL)
			{
				item.fMask = item.fMask | MIIM_SUBMENU;
				item.hSubMenu = _tray_menu(m->submenu.get(), id);
			}
			if (m->isDisabled)
			{
				item.fState |= MFS_DISABLED;
			}
			if (m->isChecked)
			{
				item.fState |= MFS_CHECKED;
			}
			item.wID = *id;
			item.dwTypeData = const_cast<char*>(m->label.c_str());
			item.dwItemData = (ULONG_PTR)m;
			if (m->icon.size())
			{
				Gdiplus::Bitmap* pBitmap = NULL;
				IStream* pStream = NULL;

				HRESULT hResult = ::CreateStreamOnHGlobal(NULL, TRUE, &pStream);
				if (hResult == S_OK && pStream)
				{
					hResult = pStream->Write(m->icon.data(), ULONG(m->icon.size()), NULL);
					if (hResult == S_OK)
					{
						pBitmap = Gdiplus::Bitmap::FromStream(pStream);

						HBITMAP hbmp;
						if (pBitmap != nullptr && pBitmap->GetHBITMAP((Gdiplus::ARGB)Gdiplus::Color::Black, &hbmp) == Gdiplus::Ok)
						{
							item.fMask |= MIIM_BITMAP;
							item.hbmpItem = hbmp;
							bitmaps.push_back(hbmp);
							delete pBitmap;
						}						
					}
					pStream->Release();					
				}				
			}

			InsertMenuItem(hmenu, *id, TRUE, &item);
		}
	}
	return hmenu;
}

bool SystrayInitialize(SystrayMenu *tray)
{
	if (!gdiToken)
	{
		Gdiplus::GdiplusStartupInput si;
		si.GdiplusVersion = 1;
		si.DebugEventCallback = nullptr;
		si.SuppressBackgroundThread = false;
		si.SuppressExternalCodecs = true;
		if (Gdiplus::GdiplusStartup(&gdiToken, &si, nullptr) != Gdiplus::Ok)
		{
			return false;
		}
	}

	WM_TASKBAR_RESTARTED = RegisterWindowMessage("TaskbarCreated");

	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = _tray_wnd_proc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = WC_TRAY_CLASS_NAME;
	if (!RegisterClassEx(&wc))
	{
		return false;
	}

	window = CreateWindowEx(0, WC_TRAY_CLASS_NAME, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (window == nullptr)
	{
		UnregisterClass(WC_TRAY_CLASS_NAME, GetModuleHandle(NULL));
		return false;
	}
	UpdateWindow(window);

	memset(&systrayIcon, 0, sizeof(systrayIcon));
	systrayIcon.cbSize = sizeof(NOTIFYICONDATA);
	systrayIcon.hWnd = window;
	systrayIcon.uID = 0;
	systrayIcon.uFlags = NIF_ICON | NIF_MESSAGE;
	systrayIcon.uCallbackMessage = WM_TRAY_CALLBACK_MESSAGE;
	Shell_NotifyIcon(NIM_ADD, &systrayIcon);

	if (tray != nullptr)
		SystrayUpdate(tray);

	return true;
}

int SystrayLoop()
{
	MSG msg;
	int limit = 10;

	while (PeekMessage(&msg, window, 0, 0, PM_REMOVE) && limit-- > 0)
	{
		if (msg.message == WM_QUIT)
		{
			return -1;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

static void cleanup()
{
	if (menu != nullptr)
	{
		DestroyMenu(menu);
	}
	menu = nullptr;

	for (auto icon : icons)
	{
		DestroyIcon(icon);
	}
	icons.clear();

	for (auto bitmap : bitmaps)
	{
		DeleteObject(bitmap);
	}
	bitmaps.clear();
}

void SystrayUpdate(SystrayMenu *tray)
{
	HICON icon = CreateIconFromResourceEx(tray->icon.data(), static_cast<DWORD>(tray->icon.size()), 1, 0x30000, 32, 32, LR_DEFAULTCOLOR);
	systrayIcon.hIcon = icon;

	if (!tray->tooltip.empty())
	{
		tray->tooltip.copy(systrayIcon.szTip, sizeof(systrayIcon.szTip), 0);
		systrayIcon.uFlags |= NIF_TIP;
	}
	else
		systrayIcon.uFlags &= ~NIF_TIP;

	Shell_NotifyIcon(NIM_MODIFY, &systrayIcon);

	cleanup();

	icons.push_back(icon);
	
	UINT id = ID_TRAY_FIRST;

	menu = _tray_menu(tray->submenu.get(), &id);
	SendMessage(window, WM_INITMENUPOPUP, (WPARAM)menu, 0);	
}

void SystrayClose()
{
	if (systrayIcon.hIcon != nullptr)
	{
		Shell_NotifyIcon(NIM_DELETE, &systrayIcon);
		systrayIcon.hIcon = nullptr;
	}

	cleanup();

	if (window != nullptr)
	{
		PostQuitMessage(0);
		UnregisterClass(WC_TRAY_CLASS_NAME, GetModuleHandle(NULL));
	}

	if (gdiToken)
	{
		Gdiplus::GdiplusShutdown(gdiToken);
		gdiToken = 0;
	}
}
