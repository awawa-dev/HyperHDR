/* InternalClock.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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


#include <utils/InternalClock.h>
#include <QMetaObject>
#include <QPointer>

#ifdef _WIN32
	#include <Windows.h>
	using namespace std::chrono_literals;
#else
	#include <time.h>
#endif


const std::chrono::time_point<std::chrono::steady_clock> InternalClock::start = std::chrono::steady_clock::now();
const std::chrono::time_point<std::chrono::high_resolution_clock> InternalClock::startPrecise = std::chrono::high_resolution_clock::now();

long long int InternalClock::now()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}


long long int InternalClock::nowPrecise()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startPrecise).count();
}

bool InternalClock::isPreciseSteady()
{
	return std::chrono::high_resolution_clock::is_steady;
}

HighResolutionScheduler::~HighResolutionScheduler()
{
	stop();
}

void HighResolutionScheduler::start(QObject* receiver, std::function<void()> task, std::chrono::microseconds period)
{
	stop();

#ifdef _WIN32
	if (timer = CreateWaitableTimerEx(
		NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS); timer == nullptr)
		return;
#endif

	running = true;
	worker = std::thread([this, receiver = QPointer<QObject>(receiver), task, period]() {
		using clock = std::chrono::steady_clock;
		auto start_time = clock::now();
		while (running && receiver)
		{
			QMetaObject::invokeMethod(receiver, [task]() { task(); }, Qt::QueuedConnection);
			start_time += period;
			sleepUntil(start_time);
		}
	});

#ifdef _WIN32
	if (period < 10ms)
	{
		SetThreadPriority(worker.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
	}
#endif

}

void HighResolutionScheduler::stop()
{
	running = false;
	if (worker.joinable())
	{
		worker.join();
	}
#ifdef _WIN32
	if (timer)
	{
		CloseHandle(timer);
		timer = nullptr;
	}
#endif
}

void HighResolutionScheduler::sleepUntil(std::chrono::steady_clock::time_point tp)
{
	using hundred_nanoseconds = std::chrono::duration<long long, std::ratio<1, 10000000>>;

#ifdef _WIN32
	auto now = std::chrono::steady_clock::now();
	auto diff = std::chrono::duration_cast<hundred_nanoseconds>(tp - now).count();

	if (diff > 0)
	{
		LARGE_INTEGER li;
		li.QuadPart = -diff;
		SetWaitableTimerEx(timer, &li, 0, NULL, NULL, NULL, 0);
		WaitForSingleObject(timer, INFINITE);
	}
#elif defined(__APPLE__)
	auto now = std::chrono::steady_clock::now();
	if (tp <= now) return;
	auto duration = tp - now;
	auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
	timespec ts;
	ts.tv_sec = ns / 1000000000;
	ts.tv_nsec = ns % 1000000000;
	nanosleep(&ts, nullptr);
#else
	timespec ts;
	auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp).time_since_epoch().count();
	ts.tv_sec = ns / 1000000000;
	ts.tv_nsec = ns % 1000000000;
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
#endif
}

/*
scheduler.start(workerObj,
	[workerObj]() { workerObj->doWorkLambda(); },
	std::chrono::milliseconds(6));*/
