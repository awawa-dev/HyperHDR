#pragma once

/* Macros.h
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
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

#ifndef PCH_ENABLED
	#include <QThread>
	#include <type_traits>
	#include <concepts>
	#include <tuple>
#endif

#if defined(QT_DEBUG) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	#include <QMetaType>
	#include <QDebug>
	#define QT_METATYPE_CHECK_INTERNAL(ptype, method, n) \
		if (!QMetaType::fromName(#ptype).isValid()) qCritical("HyperHDR Safety Warning: Type '%s' used in method '%s' (arg %d) is NOT registered! Queued calls will fail.", #ptype, #method, n);
#else
	#define QT_METATYPE_CHECK_INTERNAL(ptype, method, n)
#endif

namespace hyperhdr::macros {
	inline void SAFE_CALL_TEST_FUN() {};

	template <size_t I, typename MethodPtr> struct SafeSlotArg;
	template <size_t I, typename R, typename C, typename... Args>
	struct SafeSlotArg<I, R(C::*)(Args...)> {
		using T = std::remove_cvref_t<std::tuple_element_t<I, std::tuple<Args...>>>;
	};
	template <size_t I, typename R, typename C, typename... Args>
	struct SafeSlotArg<I, R(C::*)(Args...) const> {
		using T = std::remove_cvref_t<std::tuple_element_t<I, std::tuple<Args...>>>;
	};

	template <typename MethodPtr> struct SafeSlotRet;
	template <typename R, typename C, typename... Args>
	struct SafeSlotRet<R(C::*)(Args...)> {
		using Type = std::remove_cvref_t<R>;
	};
	template <typename R, typename C, typename... Args>
	struct SafeSlotRet<R(C::*)(Args...) const> {
		using Type = std::remove_cvref_t<R>;
	};
}

#define SCHK_ARG(n, target, method, ptype, pvalue) \
{ \
	static_assert(std::same_as<typename hyperhdr::macros::SafeSlotArg<n, decltype(&std::remove_pointer_t<decltype(target)>::method)>::T, ptype>, "Parameter " #n " type mismatch for Qt slot: " #method); \
	static_assert(std::convertible_to<std::remove_cvref_t<decltype(pvalue)>, ptype>, "The value passed as parameter " #n " does not match the declared type: " #ptype); \
	QT_METATYPE_CHECK_INTERNAL(ptype, method, n) \
}

#define SCHK_RET(target, method, rtype) \
    static_assert(std::same_as<typename hyperhdr::macros::SafeSlotRet<decltype(&std::remove_pointer_t<decltype(target)>::method)>::Type, rtype>, "Return type mismatch for Qt slot: " #method)

#define SAFE_CALL_0_RET(target, method, returnType, result, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_RET(target, method, returnType); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result)); \
	else \
		result = target->method(); \
}

#define SAFE_CALL_1_RET(target, method, returnType, result, p1type, p1value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_RET(target, method, returnType); \
	SCHK_ARG(0, target, method, p1type, p1value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value)); \
	else \
		result = target->method(p1value); \
}

#define SAFE_CALL_2_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_RET(target, method, returnType); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		result = target->method(p1value, p2value); \
}

#define SAFE_CALL_3_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_RET(target, method, returnType); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		result = target->method(p1value, p2value, p3value); \
}

#define SAFE_CALL_4_RET(target, method, returnType, result, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_RET(target, method, returnType); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); SCHK_ARG(3, target, method, p4type, p4value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_RETURN_ARG(returnType, result), Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		result = target->method(p1value, p2value, p3value, p4value); \
}

#define QUEUE_CALL_0(target, method, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection); \
	else \
		target->method(); \
}

#define QUEUE_CALL_1(target, method, p1type, p1value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value)); \
	else \
		target->method(p1value); \
}

#define QUEUE_CALL_2(target, method, p1type, p1value, p2type, p2value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		target->method(p1value, p2value); \
}

#define QUEUE_CALL_3(target, method, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		target->method(p1value, p2value, p3value); \
}

#define QUEUE_CALL_4(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); SCHK_ARG(3, target, method, p4type, p4value); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		target->method(p1value, p2value, p3value, p4value); \
}

#define QUEUE_CALL_5(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value, p5type, p5value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); SCHK_ARG(3, target, method, p4type, p4value); SCHK_ARG(4, target, method, p5type, p5value); \
	if (true) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value), Q_ARG(p5type, p5value)); \
	else \
		target->method(p1value, p2value, p3value, p4value, p5value); \
}

#define BLOCK_CALL_0(target, method, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection); \
	else \
		target->method(); \
}

#define BLOCK_CALL_1(target, method, p1type, p1value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value)); \
	else \
		target->method(p1value); \
}

#define BLOCK_CALL_2(target, method, p1type, p1value, p2type, p2value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		target->method(p1value, p2value); \
}

#define BLOCK_CALL_3(target, method, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		target->method(p1value, p2value, p3value); \
}

#define BLOCK_CALL_4(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); SCHK_ARG(3, target, method, p4type, p4value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		target->method(p1value, p2value, p3value, p4value); \
}

#define BLOCK_CALL_5(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value, p5type, p5value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); SCHK_ARG(3, target, method, p4type, p4value); SCHK_ARG(4, target, method, p5type, p5value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::BlockingQueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value), Q_ARG(p5type, p5value)); \
	else \
		target->method(p1value, p2value, p3value, p4value, p5value); \
}

#define AUTO_CALL_0(target, method, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection); \
	else \
		target->method(); \
}

#define AUTO_CALL_1(target, method, p1type, p1value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value)); \
	else \
		target->method(p1value); \
}

#define AUTO_CALL_2(target, method, p1type, p1value, p2type, p2value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value)); \
	else \
		target->method(p1value, p2value); \
}

#define AUTO_CALL_3(target, method, p1type, p1value, p2type, p2value, p3type, p3value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value)); \
	else \
		target->method(p1value, p2value, p3value); \
}

#define AUTO_CALL_4(target, method, p1type, p1value, p2type, p2value, p3type, p3value, p4type, p4value, ...) \
{ \
	hyperhdr::macros::SAFE_CALL_TEST_FUN(__VA_ARGS__); \
	SCHK_ARG(0, target, method, p1type, p1value); SCHK_ARG(1, target, method, p2type, p2value); SCHK_ARG(2, target, method, p3type, p3value); SCHK_ARG(3, target, method, p4type, p4value); \
	if (target->thread() != QThread::currentThread()) \
		QMetaObject::invokeMethod(target, #method, Qt::QueuedConnection, Q_ARG(p1type, p1value), Q_ARG(p2type, p2value), Q_ARG(p3type, p3value), Q_ARG(p4type, p4value)); \
	else \
		target->method(p1value, p2value, p3value, p4value); \
}

namespace hyperhdr {
	void THREAD_REMOVER(QString message, QThread* parentThread, QObject* client);
	void THREAD_MULTI_REMOVER(QString message, QThread* parentThread, std::vector<QObject*> clients);
	void SMARTPOINTER_MESSAGE(QString message);
};
