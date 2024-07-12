/* SuspendHandlerMacOS.cpp
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



#include <algorithm>
#include <cstdint>
#include <limits>
#include <cmath>
#include <cassert>
#include <stdlib.h>
#include <suspend-handler/SuspendHandlerMacOS.h>
#include <utils/Components.h>
#include <image/Image.h>
#include <base/HyperHdrManager.h>
#include <iostream>
#include <QThread>
#import <Cocoa/Cocoa.h>

@interface MacSuspendHandler : NSObject {
}

-(id)init;
-(void)goingSleep: (NSNotification*)note;
-(void)goingWake: (NSNotification*)note;
@end


namespace {
	MacSuspendHandler* _macSuspendHandlerInstance = nil;
	SuspendHandler* _suspendHandler = nullptr;
}


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
	if (_suspendHandler != nullptr)
		emit _suspendHandler->SignalHibernate(false, hyperhdr::SystemComponent::SUSPEND);
}


- (void)goingWake: (NSNotification*)note
{
	if (_suspendHandler != nullptr)
		emit _suspendHandler->SignalHibernate(true, hyperhdr::SystemComponent::SUSPEND);
}

@end

SuspendHandler::SuspendHandler(bool sessionLocker)
{
	_macSuspendHandlerInstance = [MacSuspendHandler new];
	_suspendHandler = this;
}

SuspendHandler::~SuspendHandler()
{
	_macSuspendHandlerInstance = nil;
	_suspendHandler = nullptr;
}

