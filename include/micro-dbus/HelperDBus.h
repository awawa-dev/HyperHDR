#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <dbus/dbus.h>

class HelperDBus : public QObject
{
	Q_OBJECT

public:
	explicit HelperDBus(QObject* parent = nullptr);
	~HelperDBus() override;

	bool open(DBusBusType bus = DBUS_BUS_SESSION);
	bool addMatch(const QString& rule);
	bool addSignalMatch(const QString& sender, const QString& path, const QString& interface, const QString& member);

	template <typename T>
	static bool isType(const QVariant& value) {
		return HelperDBus::typeId(value) == qMetaTypeId<T>();
	}

	static inline constexpr QLatin1String DBusProperties{ "org.freedesktop.DBus.Properties" };

signals:
	void signalReceived(const QString& path, const QString& interface, const QString& member, const QVariantList& arguments, bool parseError);

protected:
	DBusMessage* makeMethodCall(const char* destination, const char* path, const char* interface, const char* method) const;
	DBusMessage* callSync(DBusMessage* message, const char* operation);

	void dispatch();
	void closeConnection();

	QString uniqueName() const;
	DBusConnection* connection() const;

	static bool appendVariant(DBusMessageIter& iter, const QVariant& value);
	static bool appendObjectPath(DBusMessageIter& iter, const QString& path);
	static bool appendVariantMap(DBusMessageIter& iter, const QVariantMap& map);
	static bool readMessage(DBusMessage* message, QVariantList& arguments);

	bool send(DBusMessage* message) const;
	bool emitSignal(const char* path, const char* interface, const char* name) const;
	bool replyVoid(DBusMessage* request) const;
	bool replyError(DBusMessage* request, const char* name, const char* text) const;
	void replyString(DBusMessage* request, const char* value) const;

	template <typename Fn>
	void replyWith(DBusMessage* request, const char* operation, Fn&& fn) const
	{
		DBusMessage* reply = dbus_message_new_method_return(request);
		if (!reply)
			return;
		DBusMessageIter iter;
		dbus_message_iter_init_append(reply, &iter);
		if (!fn(iter))
		{
			dbus_message_unref(reply);
			replyError(request, DBUS_ERROR_FAILED, operation);
			return;
		}
		send(reply);
	}

	template <typename Fn>
	static bool dbusContainer(DBusMessageIter& parent, int type, const char* signature, Fn&& fn)
	{
		DBusMessageIter child{};
		if (!dbus_message_iter_open_container(&parent, type, signature, &child)) return false;
		if (fn(child) && dbus_message_iter_close_container(&parent, &child)) return true;
		dbus_message_iter_abandon_container_if_open(&parent, &child);
		return false;
	}

	bool registerObjectPath(const char* path, DBusObjectPathMessageFunction messageFunction, void* userData);
	QVariant getProperty(const QString& destination, const QString& path, const QString& interface, const QString& property, const char* operation);

private:
	static dbus_bool_t addWatch(DBusWatch* watch, void* data);
	static void removeWatch(DBusWatch* watch, void* data);
	static void toggleWatch(DBusWatch* watch, void* data);
	static void freeWatchData(void* data);

	static DBusHandlerResult filter(DBusConnection*, DBusMessage*, void*);

	static bool appendMapEntry(DBusMessageIter& map, const QByteArray& key, const QVariant& value);

	static bool readValue(DBusMessageIter& iter, QVariant& value);
	static bool readArray(DBusMessageIter& iter, QVariant& value);
	static bool readMap(DBusMessageIter& iter, QVariant& value);
	static bool readStruct(DBusMessageIter& iter, QVariant& value);

	static int typeId(const QVariant& value);

	void handleSignal(DBusMessage* message);
	void logError(const char* operation, DBusError& error) const;

	DBusConnection* _connection = nullptr;
};
