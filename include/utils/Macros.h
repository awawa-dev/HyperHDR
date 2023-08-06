#pragma once

/* Macros.h
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
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
	if (target->thread() != this->thread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result)); \
	else \
		result = target->method(); \
}

#define SAFE_CALL_1_RET(target, method, returnType, result, p1type, p1value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != this->thread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value)); \
	else \
		result = target->method(p1value); \
}

#define SAFE_CALL_2_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != this->thread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		result = target->method(p1value, p2value); \
}

#define SAFE_CALL_3_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != this->thread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		result = target->method(p1value, p2value, p3value); \
}

#define SAFE_CALL_4_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value , ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != this->thread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		result = target->method(p1value, p2value, p3value, p4value); \
}

#define SAFE_CALL_0(target, method, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection); \
	else \
		target->method(); \
}

#define SAFE_CALL_1(target, method, p1type, p1value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value)); \
	else \
		target->method(p1value); \
}

#define SAFE_CALL_2(target, method, p1type, p1value, p2type, p2value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		target->method(p1value, p2value); \
}

#define SAFE_CALL_3(target, method, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		target->method(p1value, p2value, p3value); \
}

#define SAFE_CALL_4(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value , ...) \
{ \
	SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		target->method(p1value, p2value, p3value, p4value); \
}



