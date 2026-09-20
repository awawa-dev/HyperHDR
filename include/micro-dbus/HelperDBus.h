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

signals:
	void signalReceived(const QString& path, const QString& interface, const QString& member, const QVariantList& arguments, bool parseError);

protected:
	DBusMessage* makeMethodCall(const char* destination, const char* path, const char* interface, const char* method) const;
	DBusMessage* callSync(DBusMessage* message, const char* operation);

	void dispatch();
	void closeConnection();

	QString uniqueName() const;

	static bool appendObjectPath(DBusMessageIter& iter, const QString& path);
	static bool appendVariantMap(DBusMessageIter& iter, const QVariantMap& map);
	static bool readMessage(DBusMessage* message, QVariantList& arguments);

private:
	static dbus_bool_t addWatch(DBusWatch* watch, void* data);
	static void removeWatch(DBusWatch* watch, void* data);
	static void toggleWatch(DBusWatch* watch, void* data);
	static void freeWatchData(void* data);

	static DBusHandlerResult filter(DBusConnection*, DBusMessage*, void*);

	static bool appendVariant(DBusMessageIter& iter, const QVariant& value);
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
