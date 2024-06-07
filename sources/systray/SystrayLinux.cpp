/* SystrayLinux.cpp
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

#include <string.h>
#include <stddef.h>
#include <algorithm>
#include <iostream>

#include <systray/Systray.h>
#include <libayatana-appindicator/app-indicator.h>

#define TRAY_APPINDICATOR_ID "hyper-tray-id"

namespace
{
	AppIndicator* indicator = nullptr;
}

static void _tray_menu_cb(GtkMenuItem* item, gpointer data)
{
	SystrayMenu* menu = reinterpret_cast<SystrayMenu*>(data);
	if (menu != nullptr && menu->callback != nullptr)
	{
		menu->callback(menu);
	}
}

static GtkMenuShell* _tray_menu(SystrayMenu *m)
{
	GtkMenuShell* menu = reinterpret_cast<GtkMenuShell*>(gtk_menu_new());

	for (; m != nullptr && !m->label.empty(); m = m->next.get())
	{
		GtkWidget* item = nullptr;
		std::string label = m->label;

		label.erase(std::remove(label.begin(), label.end(), '&'), label.end());

		if (m->label == "-")
		{
			item = gtk_separator_menu_item_new();
		}
		else
		{
			if (m->submenu != nullptr)
			{
				item = gtk_menu_item_new_with_label(const_cast<char*>(label.c_str()));
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
					GTK_WIDGET(_tray_menu(m->submenu.get())));
			}
			else if (m->isChecked)
			{
				item = gtk_check_menu_item_new_with_label(const_cast<char*>(label.c_str()));
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), m->isChecked);
			}
			else if (!m->tooltip.empty())
			{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
				item = gtk_image_menu_item_new_with_mnemonic(const_cast<char*>(label.c_str()));
				GtkWidget* icon = gtk_image_new_from_file(const_cast<char*>(m->tooltip.c_str()));
				gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(item), TRUE);
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), icon);
G_GNUC_END_IGNORE_DEPRECATIONS
			}
			else
			{
				item = gtk_menu_item_new_with_label(const_cast<char*>(label.c_str()));
			}

			gtk_widget_set_sensitive(item, !m->isDisabled);
			if (m->callback != nullptr)
			{
				g_signal_connect(item, "activate", G_CALLBACK(_tray_menu_cb), m);
			}
		}

		if (item != nullptr)
		{
			gtk_widget_show(item);
			gtk_menu_shell_append(menu, item);
		}
	}
	return menu;
}

extern "C"
{
	void SystrayUpdate(SystrayMenu* tray)
	{
		app_indicator_set_icon_full(indicator, const_cast<char*>(tray->tooltip.c_str()), "HyperHDR");
		app_indicator_set_menu(indicator, GTK_MENU(_tray_menu(tray->submenu.get())));
	}

	bool SystrayInitialize(SystrayMenu* tray)
	{
		if (gtk_init_check(0, NULL) == FALSE)
		{
			return false;
		}

		indicator = app_indicator_new(TRAY_APPINDICATOR_ID, "", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
		if (indicator == nullptr)
		{
			return false;
		}

		app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

		if (tray != nullptr)
			SystrayUpdate(tray);

		return true;
	}

	int SystrayLoop()
	{
		int limit = 10;

		while (gtk_events_pending() && limit-- > 0)
		{
			if (gtk_main_iteration_do(false))
			{
				return -1;
			}
		}
		return 0;
	}

	void SystrayClose()
	{
	}
}

