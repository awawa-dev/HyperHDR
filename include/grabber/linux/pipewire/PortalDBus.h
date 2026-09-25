#pragma once

#include <micro-dbus/HelperDBus.h>

class PortalDBus final : public HelperDBus
{
	Q_OBJECT

public:
	explicit PortalDBus(QObject* parent = nullptr);

	bool open();

	int screenCastVersion();

	QString createSession(const QString& sessionToken, const QString& requestToken);
	QString selectSources(const QString& sessionHandle, const QString& requestToken, const QString& restoreToken);
	QString start(const QString& sessionHandle, const QString& requestToken);
	bool closeSession(const QString& sessionHandle);

signals:
	void responseReceived(const QString& path, const QVariantList& arguments, bool parseError);

private:
	void handleSignal(const QString& path, const QString& interface, const QString& member, const QVariantList& arguments, bool parseError);

	QString requestPath(DBusMessage* message, const char* operation);

	static constexpr auto DesktopService = "org.freedesktop.portal.Desktop";
	static constexpr auto DesktopPath = "/org/freedesktop/portal/desktop";
	static constexpr auto ScreenCastInterface = "org.freedesktop.portal.ScreenCast";
	static constexpr auto RequestInterface = "org.freedesktop.portal.Request";
	static constexpr auto SessionInterface = "org.freedesktop.portal.Session";
};
