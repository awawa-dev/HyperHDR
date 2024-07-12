#pragma once

#ifndef PCH_ENABLED
	#include <QThread>
#endif

/* Macros.h
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

inline void SAFE_CALL_TEST_FUN() {};

#define SAFE_CALL_0_RET(target, method, returnType, result, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result)); \
	else \
		result = target->method(); \
}

#define SAFE_CALL_1_RET(target, method, returnType, result, p1type, p1value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value)); \
	else \
		result = target->method(p1value); \
}

#define SAFE_CALL_2_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		result = target->method(p1value, p2value); \
}

#define SAFE_CALL_3_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		result = target->method(p1value, p2value, p3value); \
}

#define SAFE_CALL_4_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value , ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		result = target->method(p1value, p2value, p3value, p4value); \
}

#define QUEUE_CALL_0(target, method, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection); \
	else \
		target->method(); \
}

#define QUEUE_CALL_1(target, method, p1type, p1value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value)); \
	else \
		target->method((p1type) p1value); \
}

#define QUEUE_CALL_2(target, method, p1type, p1value, p2type, p2value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		target->method((p1type) p1value, (p2type) p2value); \
}

#define QUEUE_CALL_3(target, method, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		target->method((p1type) p1value, (p2type) p2value, (p3type) p3value); \
}

#define QUEUE_CALL_4(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value , ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		target->method((p1type) p1value, (p2type) p2value, (p3type) p3value, (p4type) p4value); \
}

#define QUEUE_CALL_5(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value , p5type, p5value , ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value), Q_ARG(p5type, p5value)); \
	else \
		target->method((p1type) p1value, (p2type) p2value, (p3type) p3value, (p4type) p4value, (p5type) p5value); \
}

#define BLOCK_CALL_0(target, method, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection); \
	else \
		target->method(); \
}

#define BLOCK_CALL_1(target, method, p1type, p1value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value)); \
	else \
		target->method((p1type) p1value); \
}

#define BLOCK_CALL_2(target, method, p1type, p1value, p2type, p2value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		target->method((p1type) p1value, (p2type) p2value); \
}

#define BLOCK_CALL_3(target, method, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		target->method((p1type) p1value, (p2type) p2value, (p3type) p3value); \
}

#define BLOCK_CALL_4(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value , ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		target->method((p1type) p1value, (p2type) p2value, (p3type) p3value, (p4type) p4value); \
}

#define BLOCK_CALL_5(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value, p5type, p5value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value), Q_ARG(p5type, p5value)); \
	else \
		target->method((p1type) p1value, (p2type) p2value, (p3type) p3value, (p4type) p4value, (p5type) p5value); \
}

#define AUTO_CALL_0(target, method, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection); \
	else \
		target->method(); \
}

#define AUTO_CALL_1(target, method, p1type, p1value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value)); \
	else \
		target->method(p1value); \
}

#define AUTO_CALL_2(target, method, p1type, p1value, p2type, p2value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		target->method(p1value, p2value); \
}

#define AUTO_CALL_3(target, method, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		target->method(p1value, p2value, p3value); \
}

#define AUTO_CALL_4(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value , ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		target->method(p1value, p2value, p3value, p4value); \
}

#define QSTRING_CSTR(str) str.toLocal8Bit().constData()

namespace hyperhdr {
	void THREAD_REMOVER(QString message, QThread* parentThread, QObject* client);
	void THREAD_MULTI_REMOVER(QString message, QThread* parentThread, std::vector<QObject*> clients);
	void SMARTPOINTER_MESSAGE(QString message);
};
