/* SystrayMacOS.cpp
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

#include <Cocoa/Cocoa.h>
#include <systray/Systray.h>
#include <iostream>

@interface MenuDelegate: NSObject
- (void)clicked:(NSMenuItem*)sender;
@end
@implementation MenuDelegate{}
    - (void)clicked:(NSMenuItem*)sender
    {
        id representedObject = sender.representedObject;
        struct SystrayMenu* pTrayMenu = (struct SystrayMenu*)[representedObject pointerValue];
        if (pTrayMenu != NULL && pTrayMenu->callback != NULL)
        {
            pTrayMenu->callback(pTrayMenu);
        }
    }   
@end

namespace
{
    MenuDelegate* menuDelegate;
    NSApplication* app;
    NSStatusBar* statusBar;
    NSStatusItem* statusItem;
}

static NSMenu* nativeMenu(struct SystrayMenu* m) {
    NSMenu* menu = [[NSMenu alloc] init];
    [menu setAutoenablesItems:FALSE];

    for (; m != NULL && !m->label.empty(); m = m->next.get())
    {
        std::string labelText = " " + m->label;
        labelText.erase(std::remove(labelText.begin(), labelText.end(), '&'), labelText.end());

        if (m->label == "-")
        {
            [menu addItem:[NSMenuItem separatorItem]];
        }
        else
        {
            NSString* labelNS = [NSString stringWithUTF8String: labelText.c_str()];
            NSMenuItem* menuItem = [[NSMenuItem alloc] init];
            [menuItem setTitle:labelNS];
            [menuItem setEnabled:m->isDisabled == 0 ? TRUE : FALSE];
            [menuItem setState:m->isChecked == 1 ? TRUE : FALSE];
            [menuItem setRepresentedObject:[NSValue valueWithPointer:m]];

            if (m->icon.size())
            {
                auto* data = [NSData dataWithBytes: m->icon.data()
                                             length: m->icon.size()];

                NSImage* image =  [[NSImage alloc] initWithData:data];
                [menuItem setImage: image ];
            }

            if (m->callback != NULL)
            {
                [menuItem setTarget:menuDelegate];
                [menuItem setAction:@selector(clicked:)];
            }
            [menu addItem:menuItem];
            if (m->submenu != NULL)
            {
                [menu setSubmenu:nativeMenu(m->submenu.get()) forItem:menuItem];
            }
        }
    }
    return menu;
}

bool SystrayInitialize(SystrayMenu* tray)
{
    menuDelegate = [[MenuDelegate alloc] init];
    app = [NSApplication sharedApplication];
    statusBar = [NSStatusBar systemStatusBar];
    statusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];

    if (tray != nullptr)
    {
        SystrayUpdate(tray);
    }

    return true;
}

int SystrayLoop()
{
    NSDate* until = [NSDate distantPast];
    NSEvent* event = [app nextEventMatchingMask:ULONG_MAX untilDate:until
                                         inMode:[NSString stringWithUTF8String:"kCFRunLoopDefaultMode"] dequeue:TRUE];
    if (event)
    {
        [app sendEvent:event];
    }
    return 0;
}

void SystrayUpdate(SystrayMenu* tray)
{
    double iconHeight = [[NSStatusBar systemStatusBar] thickness];

    auto* data = [NSData dataWithBytes:tray->icon.data()
                                 length:tray->icon.size()];

    NSImage* image =  [[NSImage alloc] initWithData:data];

    if (iconHeight < image.size.height)
    {
        std::cout << "Resizing the main icon: " << image.size.width << "x" << image.size.height << " to height=" << iconHeight << "\n";
        double width = image.size.width * (iconHeight / image.size.height);
        [image setSize:NSMakeSize(width, iconHeight)];
    }

    statusItem.button.image = image;
    if (!tray->tooltip.empty())
    {
        statusItem.button.toolTip = [NSString stringWithUTF8String:tray->tooltip.c_str()];
    }
    [statusItem setMenu:nativeMenu(tray->submenu.get())];
}

void SystrayClose()
{
}
