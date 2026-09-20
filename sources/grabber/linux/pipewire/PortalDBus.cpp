/* PortalDBus.cpp
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

#include <grabber/linux/pipewire/PortalDBus.h>

#include <QDebug>

PortalDBus::PortalDBus(QObject* parent)
	: HelperDBus(parent)
{
	connect(this, &HelperDBus::signalReceived, this, &PortalDBus::handleSignal);
}

bool PortalDBus::open()
{
	if (!HelperDBus::open())
		return false;

	QString sender = uniqueName();
	if (sender.isEmpty())
	{
		qWarning() << "PortalDBus: session bus did not provide a unique name";
		closeConnection();
		return false;
	}

	sender.replace('.', '_');
	if (sender.startsWith(':'))
		sender.remove(0, 1);

	const QString rule = QStringLiteral(
		"type='signal',interface='%1',member='Response',"
		"path_namespace='/org/freedesktop/portal/desktop/request/%2'")
		.arg(QString::fromLatin1(RequestInterface), sender);

	if (!addMatch(rule))
	{
		qWarning() << "PortalDBus: could not subscribe to Request.Response";
		closeConnection();
		return false;
	}

	return true;
}

int PortalDBus::screenCastVersion()
{
	DBusMessage* message = makeMethodCall(DesktopService, DesktopPath, PropertiesInterface, "Get");
	if (!message)
		return -1;

	DBusMessageIter args;
	dbus_message_iter_init_append(message, &args);

	const char* interface = ScreenCastInterface;
	const char* property = "version";

	if (!dbus_message_iter_append_basic( &args, DBUS_TYPE_STRING, &interface) ||
		!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &property))
	{
		dbus_message_unref(message);
		qWarning() << "PortalDBus: could not encode ScreenCast.version";
		return -1;
	}

	DBusMessage* reply = callSync(message, "ScreenCast.version");
	if (!reply)
		return -1;

	QVariantList values;
	const bool valid = readMessage(reply, values) &&
		values.size() == 1 &&
		values.first().canConvert<quint32>();

	const int version = valid ? values.first().toUInt() : -1;
	dbus_message_unref(reply);

	if (!valid)
		qWarning() << "PortalDBus: invalid ScreenCast.version reply";

	return version;
}

QString PortalDBus::createSession(const QString& sessionToken, const QString& requestToken)
{
	DBusMessage* message = makeMethodCall(
		DesktopService, DesktopPath, ScreenCastInterface, "CreateSession");
	if (!message)
		return {};

	DBusMessageIter args;
	dbus_message_iter_init_append(message, &args);

	const QVariantMap options{
		{QStringLiteral("session_handle_token"), sessionToken},
		{QStringLiteral("handle_token"), requestToken}
	};

	if (!appendVariantMap(args, options))
	{
		dbus_message_unref(message);
		return {};
	}

	return requestPath(message, "CreateSession");
}

QString PortalDBus::selectSources(const QString& sessionHandle, const QString& requestToken, const QString& restoreToken)
{
	DBusMessage* message = makeMethodCall(DesktopService, DesktopPath, ScreenCastInterface, "SelectSources");

	if (!message)
		return {};

	DBusMessageIter args;
	dbus_message_iter_init_append(message, &args);

	if (!appendObjectPath(args, sessionHandle))
	{
		dbus_message_unref(message);
		return {};
	}

	QVariantMap options{
		{QStringLiteral("multiple"), false},
		{QStringLiteral("types"), quint32(1)},
		{QStringLiteral("cursor_mode"), quint32(1)},
		{QStringLiteral("handle_token"), requestToken},
		{QStringLiteral("persist_mode"), quint32(2)}
	};

	if (!restoreToken.isEmpty())
		options.insert(QStringLiteral("restore_token"), restoreToken);

	if (!appendVariantMap(args, options))
	{
		dbus_message_unref(message);
		return {};
	}

	return requestPath(message, "SelectSources");
}

QString PortalDBus::start(const QString& sessionHandle, const QString& requestToken)
{
	DBusMessage* message = makeMethodCall(DesktopService, DesktopPath, ScreenCastInterface, "Start");

	if (!message)
		return {};

	DBusMessageIter args;
	dbus_message_iter_init_append(message, &args);

	const char* parentWindow = "";
	const QVariantMap options{
		{QStringLiteral("handle_token"), requestToken}
	};

	if (!appendObjectPath(args, sessionHandle) ||
		!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parentWindow) ||
		!appendVariantMap(args, options))
	{
		dbus_message_unref(message);
		return {};
	}

	return requestPath(message, "Start");
}

bool PortalDBus::closeSession(const QString& sessionHandle)
{
	const QByteArray path = sessionHandle.toUtf8();
	DBusMessage* message = makeMethodCall(DesktopService, path.constData(), SessionInterface, "Close");

	if (!message)
		return false;

	DBusMessage* reply = callSync(message, "Session.Close");
	if (!reply)
		return false;

	dbus_message_unref(reply);
	return true;
}

void PortalDBus::handleSignal(const QString& path, const QString& interface, const QString& member, const QVariantList& arguments, bool parseError)
{
	if (interface != QLatin1String(RequestInterface) || member != QLatin1String("Response"))
	{
		return;
	}

	emit responseReceived(path, arguments, parseError);
}

QString PortalDBus::requestPath(DBusMessage* message, const char* operation)
{
	DBusMessage* reply = callSync(message, operation);
	if (!reply)
		return {};

	QVariantList values;
	const bool valid = readMessage(reply, values) &&
		values.size() == 1 &&
		values.first().canConvert<QString>();

	const QString path = valid ? values.first().toString() : QString{};
	dbus_message_unref(reply);

	if (!valid || path.isEmpty())
	{
		qWarning().nospace() << "PortalDBus: invalid Request path returned by " << operation;
		return {};
	}

	dispatch();
	return path;
}
