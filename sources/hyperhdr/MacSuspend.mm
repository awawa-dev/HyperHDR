/* MacSuspend.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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



#include <algorithm>
#include <cstdint>
#include <limits>
#include <cmath>
#include <cassert>
#include <stdlib.h>
#include "LinuxSuspend.h"
#include <utils/Components.h>
#include <utils/JsonUtils.h>
#include <utils/Image.h>
#include <base/HyperHdrIManager.h>
#include <iostream>
#import <Cocoa/Cocoa.h>


@interface MacSuspendHandler : NSObject {
}

-(id)init;
-(void)goingSleep: (NSNotification*)note;
-(void)goingWake: (NSNotification*)note;
@end


@implementation MacSuspendHandler

- (id)init
{
	self = [super init];

	[[[NSWorkspace sharedWorkspace]notificationCenter] addObserver:self
		selector : @selector(goingSleep:)
		name: NSWorkspaceWillSleepNotification object : NULL];

	[[[NSWorkspace sharedWorkspace]notificationCenter] addObserver:self
		selector : @selector(goingWake:)
		name: NSWorkspaceDidWakeNotification object : NULL];

	return self;
}

- (void)goingSleep: (NSNotification*)note
{
	QMetaObject::invokeMethod(HyperHdrIManager::getInstance(), "hibernate", Q_ARG(bool, false));
}


- (void)goingWake: (NSNotification*)note
{
	QMetaObject::invokeMethod(HyperHdrIManager::getInstance(), "hibernate", Q_ARG(bool, true));
}

@end

MacSuspendHandler* _macSuspendHandlerInstance = nullptr;

SuspendHandler::SuspendHandler()
{
	_macSuspendHandlerInstance = [MacSuspendHandler new];
}

