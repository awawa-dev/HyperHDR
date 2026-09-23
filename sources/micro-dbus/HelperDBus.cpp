/* HelperDBus.cpp
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

#include <micro-dbus/HelperDBus.h>

#include <QDebug>
#include <QSocketNotifier>

 int HelperDBus::typeId(const QVariant& value) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return value.typeId();
#else
	return value.userType();
#endif
}

HelperDBus::HelperDBus(QObject* parent) : QObject(parent) {}

HelperDBus::~HelperDBus()
{
	closeConnection();
}

bool HelperDBus::open(DBusBusType bus)
{
	if (_connection) return true;

	DBusError error;
	dbus_error_init(&error);

	if (!(_connection = dbus_bus_get_private(bus, &error))) {
		logError("Could not connect to session bus", error);
		return false;
	}

	dbus_error_free(&error);
	dbus_connection_set_exit_on_disconnect(_connection, FALSE);

	if (dbus_connection_set_watch_functions(_connection, &HelperDBus::addWatch, &HelperDBus::removeWatch, &HelperDBus::toggleWatch, this, nullptr)) {
		if (dbus_connection_add_filter(_connection, &HelperDBus::filter, this, nullptr)) {
			return true;
		}
		qWarning() << "HelperDBus: could not install D-Bus filter";
		dbus_connection_set_watch_functions(_connection, nullptr, nullptr, nullptr, nullptr, nullptr);
	}
	else {
		qWarning() << "HelperDBus: could not install D-Bus watch functions";
	}

	dbus_connection_close(_connection);
	dbus_connection_unref(_connection);
	_connection = nullptr;
	return false;
}

bool HelperDBus::addMatch(const QString& rule)
{
	if (!_connection) return false;

	DBusError error;
	dbus_error_init(&error);

	dbus_bus_add_match(_connection, rule.toUtf8().constData(), &error);
	if (dbus_error_is_set(&error)) {
		logError("Could not add D-Bus match", error);
		return false;
	}

	dbus_error_free(&error);
	return true;
}

bool HelperDBus::addSignalMatch(const QString& sender, const QString& path, const QString& interface, const QString& member)
{
	return addMatch(QString("type='signal',sender='%1',path='%2',interface='%3',member='%4'").arg(sender).arg(path).arg(interface).arg(member));
}

DBusMessage* HelperDBus::makeMethodCall(const char* destination, const char* path, const char* interface, const char* method) const
{
	if (auto* message = dbus_message_new_method_call(destination, path, interface, method))
		return message;

	qWarning().nospace() << "HelperDBus: could not allocate message for " << method;
	return nullptr;
}

DBusMessage* HelperDBus::callSync(DBusMessage* message, const char* operation)
{
	if (!message || !_connection) {
		if (message) dbus_message_unref(message);
		return nullptr;
	}

	DBusError error;
	dbus_error_init(&error);

	DBusMessage* reply = dbus_connection_send_with_reply_and_block(_connection, message, DBUS_TIMEOUT_USE_DEFAULT, &error);
	dbus_message_unref(message);

	if (!reply) {
		logError(operation, error);
		return nullptr;
	}

	dbus_error_free(&error);

	return reply;
}

QString HelperDBus::uniqueName() const
{
	if (!_connection) return {};
	const char* name = dbus_bus_get_unique_name(_connection);
	return name ? QString::fromUtf8(name) : QString{};
}

DBusConnection* HelperDBus::connection() const
{
	return _connection;
}

void HelperDBus::dispatch()
{
	while (_connection && dbus_connection_get_dispatch_status(_connection) == DBUS_DISPATCH_DATA_REMAINS) {
		dbus_connection_dispatch(_connection);
	}
}

void HelperDBus::closeConnection()
{
	if (!_connection) return;
	dbus_connection_remove_filter(_connection, &HelperDBus::filter, this);
	dbus_connection_set_watch_functions(_connection, nullptr, nullptr, nullptr, nullptr, nullptr);
	dbus_connection_close(_connection);
	dbus_connection_unref(_connection);
	_connection = nullptr;
}

bool HelperDBus::appendObjectPath(DBusMessageIter& iter, const QString& path)
{
	const QByteArray utf8 = path.toUtf8();
	const char* value = utf8.constData();
	return dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &value) == TRUE;
}

bool HelperDBus::appendVariantMap(DBusMessageIter& iter, const QVariantMap& map)
{
	DBusMessageIter array;
	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &array)) return false;

	for (auto it = map.cbegin(); it != map.cend(); ++it) {
		if (!appendMapEntry(array, it.key().toUtf8(), it.value())) {
			dbus_message_iter_abandon_container_if_open(&iter, &array);
			return false;
		}
	}
	return dbus_message_iter_close_container(&iter, &array) == TRUE;
}

bool HelperDBus::appendMapEntry(DBusMessageIter& map, const QByteArray& keyBytes, const QVariant& value)
{
	DBusMessageIter entry, variant;
	if (!dbus_message_iter_open_container(&map, DBUS_TYPE_DICT_ENTRY, nullptr, &entry)) return false;

	auto abandonEntry = [&]() { dbus_message_iter_abandon_container_if_open(&map, &entry); return false; };

	const char* key = keyBytes.constData();
	if (!dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key)) return abandonEntry();

	const char* signature = nullptr;
	switch (typeId(value)) {
		case QMetaType::QString:     signature = "s"; break;
		case QMetaType::Bool:        signature = "b"; break;
		case QMetaType::UInt:        signature = "u"; break;
		case QMetaType::Int:         signature = "i"; break;
		case QMetaType::QVariantMap: signature = "a{sv}"; break;
		default:                     return abandonEntry();
	}

	if (!dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, signature, &variant))
		return abandonEntry();

	if (!appendVariant(variant, value) || !dbus_message_iter_close_container(&entry, &variant)) {
		dbus_message_iter_abandon_container_if_open(&entry, &variant);
		return abandonEntry();
	}

	if (!dbus_message_iter_close_container(&map, &entry))
		return abandonEntry();

	return true;
}

bool HelperDBus::appendVariant(DBusMessageIter& iter, const QVariant& value)
{
	switch (typeId(value)) {
		case QMetaType::QString: {
			const QByteArray utf8 = value.toString().toUtf8();
			const char* data = utf8.constData();
			return dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &data) == TRUE;
		}
		case QMetaType::Bool: {
			const dbus_bool_t data = value.toBool() ? TRUE : FALSE;
			return dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &data) == TRUE;
		}
		case QMetaType::UInt: {
			const dbus_uint32_t data = value.toUInt();
			return dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &data) == TRUE;
		}
		case QMetaType::ULongLong: {
			const dbus_uint64_t data = value.toULongLong();
			return dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &data) == TRUE;
		}
		case QMetaType::Int: {
			const dbus_int32_t data = value.toInt();
			return dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &data) == TRUE;
		}
		case QMetaType::QVariantMap:
			return appendVariantMap(iter, value.toMap());
		default:
			return false;
	}
}

bool HelperDBus::readMessage(DBusMessage* message, QVariantList& arguments)
{
	if (!message) return false;

	DBusMessageIter iter;
	if (!dbus_message_iter_init(message, &iter)) return true;

	bool ok = true;
	do {
		QVariant value;
		ok &= readValue(iter, value);
		arguments.append(value);
	} while (dbus_message_iter_next(&iter));

	return ok;
}

bool HelperDBus::readValue(DBusMessageIter& iter, QVariant& value)
{
	auto iterType = dbus_message_iter_get_arg_type(&iter);
	switch (iterType) {
		case DBUS_TYPE_STRING:
		case DBUS_TYPE_OBJECT_PATH: {
			const char* text = nullptr;
			dbus_message_iter_get_basic(&iter, &text);
			if (!text) return false;
			return value = QString::fromUtf8(text), true;
		}
		case DBUS_TYPE_BOOLEAN: {
			dbus_bool_t boolean = FALSE;
			dbus_message_iter_get_basic(&iter, &boolean);
			return value = (boolean != FALSE), true;
		}
		case DBUS_TYPE_UINT32: {
			dbus_uint32_t number = 0;
			dbus_message_iter_get_basic(&iter, &number);
			return value = static_cast<quint32>(number), true;
		}
		case DBUS_TYPE_UINT64: {
			dbus_uint64_t number = 0;
			dbus_message_iter_get_basic(&iter, &number);
			return value = static_cast<quint64>(number), true;
		}
		case DBUS_TYPE_INT32: {
			dbus_int32_t number = 0;
			dbus_message_iter_get_basic(&iter, &number);
			return value = static_cast<qint32>(number), true;
		}
		case DBUS_TYPE_VARIANT: {
			DBusMessageIter nested;
			dbus_message_iter_recurse(&iter, &nested);

			if (dbus_message_iter_get_arg_type(&nested) == DBUS_TYPE_INVALID) return false;
			return readValue(nested, value);
		}
		case DBUS_TYPE_ARRAY:  return readArray(iter, value);
		case DBUS_TYPE_STRUCT: return readStruct(iter, value);
		default:
			//qWarning().nospace() << "HelperDBus: unsupported D-Bus type = '" << iterType << "' received by readValue";
			return true;
	}
}

bool HelperDBus::readArray(DBusMessageIter& iter, QVariant& value)
{
	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) return false;

	if (dbus_message_iter_get_element_type(&iter) == DBUS_TYPE_DICT_ENTRY) return readMap(iter, value);

	DBusMessageIter items;
	dbus_message_iter_recurse(&iter, &items);

	QVariantList list;
	bool ok = true;
	for (; dbus_message_iter_get_arg_type(&items) != DBUS_TYPE_INVALID; dbus_message_iter_next(&items)) {
		QVariant item;
		ok &= readValue(items, item);
		list.append(item);
	}

	value = list;
	return ok;
}

bool HelperDBus::readMap(DBusMessageIter& iter, QVariant& value)
{
	DBusMessageIter entries;
	dbus_message_iter_recurse(&iter, &entries);

	QVariantMap map;
	bool ok = true;

	while (dbus_message_iter_get_arg_type(&entries) != DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		dbus_message_iter_recurse(&entries, &entry);

		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING) {
			//qWarning().nospace() << "HelperDBus: unsupported D-Bus type = '" << dbus_message_iter_get_arg_type(&entry) << "' received by readMap";
			value = QVariant{};
			return true;
		}

		const char* key = nullptr;
		dbus_message_iter_get_basic(&entry, &key);
		if (!key || !dbus_message_iter_next(&entry)) {
			ok = false;
			dbus_message_iter_next(&entries);
			continue;
		}

		QVariant item;
		ok &= readValue(entry, item);
		map.insert(QString::fromUtf8(key), item);

		dbus_message_iter_next(&entries);
	}

	value = map;
	return ok;
}

bool HelperDBus::readStruct(DBusMessageIter& iter, QVariant& value)
{
	DBusMessageIter members;
	dbus_message_iter_recurse(&iter, &members);

	QVariantMap map;
	int index = 0;
	bool ok = true;
	for (; dbus_message_iter_get_arg_type(&members) != DBUS_TYPE_INVALID; dbus_message_iter_next(&members)) {
		QVariant item;
		ok &= readValue(members, item);
		map.insert(QString::number(index++), item);
	}

	value = map;
	return ok;
}

void HelperDBus::handleSignal(DBusMessage* message)
{	
	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL) return;

	const char* interface = dbus_message_get_interface(message);
	const char* member = dbus_message_get_member(message);
	if (!interface || !member) return;

	QVariantList arguments;
	const bool parseError = !readMessage(message, arguments);

	const char* path = dbus_message_get_path(message);

	emit signalReceived(QString::fromUtf8(path ? path : ""),
		QString::fromUtf8(interface),
		QString::fromUtf8(member),
		arguments,
		parseError);
}

DBusHandlerResult HelperDBus::filter(DBusConnection*, DBusMessage* message, void* data)
{
	if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_SIGNAL) {
		static_cast<HelperDBus*>(data)->handleSignal(message);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void HelperDBus::logError(const char* operation, DBusError& error) const
{
	qWarning().nospace() << "HelperDBus: " << operation << ": " << (error.name ? error.name : "D-Bus error") << ": " << (error.message ? error.message : "");
	dbus_error_free(&error);
}

dbus_bool_t HelperDBus::addWatch(DBusWatch* watch, void* data)
{
	auto* self = static_cast<HelperDBus*>(data);

	if (!(dbus_watch_get_flags(watch) & DBUS_WATCH_READABLE))
	{
		return TRUE;
	}

	auto* notifier = new QSocketNotifier(dbus_watch_get_unix_fd(watch), QSocketNotifier::Read, self);

	notifier->setEnabled(dbus_watch_get_enabled(watch));

	auto handler = [self, watch]() {
		if (!dbus_watch_handle(watch, DBUS_WATCH_READABLE))
			qWarning() << "HelperDBus: dbus_watch_handle() failed";
		self->dispatch();
	};

	#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)	
		connect(notifier, &QSocketNotifier::activated, self, handler);
	#else
		connect(notifier, []<class T>(void (QSocketNotifier::*signal)(QSocketDescriptor, QSocketNotifier::Type, T)) { return signal;}(&QSocketNotifier::activated), self, handler);
	#endif

	dbus_watch_set_data(watch, notifier, &HelperDBus::freeWatchData);
	return TRUE;
}

void HelperDBus::removeWatch(DBusWatch* watch, void*)
{
	if (auto* notifier = static_cast<QSocketNotifier*>(dbus_watch_get_data(watch)))
		notifier->setEnabled(false);
	
	dbus_watch_set_data(watch, nullptr, nullptr);
}

void HelperDBus::toggleWatch(DBusWatch* watch, void*)
{
	if (auto* notifier = static_cast<QSocketNotifier*>(dbus_watch_get_data(watch)))
		notifier->setEnabled(dbus_watch_get_enabled(watch) && (dbus_watch_get_flags(watch) & DBUS_WATCH_READABLE));
}

void HelperDBus::freeWatchData(void* data)
{
	delete static_cast<QSocketNotifier*>(data);
}

bool HelperDBus::send(DBusMessage* message) const
{
	if (!message || !_connection)
	{
		if (message) dbus_message_unref(message);
		return false;
	}

	const bool ok = dbus_connection_send(_connection, message, nullptr) == TRUE;
	dbus_message_unref(message);
	if (ok)
		dbus_connection_flush(_connection);
	return ok;
}

bool HelperDBus::emitSignal(const char* path, const char* interface, const char* name) const
{
	return send(dbus_message_new_signal(path, interface, name));
}

bool HelperDBus::replyVoid(DBusMessage* request) const
{
	return send(dbus_message_new_method_return(request));
}

bool HelperDBus::replyError(DBusMessage* request, const char* name, const char* text) const
{
	return send(dbus_message_new_error(request, name, text ? text : ""));
}

void HelperDBus::replyString(DBusMessage* request, const char* value) const
{
	replyWith(request, "Introspect", [&](auto& iter) {
		return appendVariant(iter, QString::fromUtf8(value ? value : ""));
		});
}

bool HelperDBus::registerObjectPath(const char* path, DBusObjectPathMessageFunction messageFunction, void* userData)
{
	if (!_connection || !path || !messageFunction)
		return false;

	DBusObjectPathVTable vtable{};
	vtable.message_function = messageFunction;

	DBusError error;
	dbus_error_init(&error);
	const bool ok = dbus_connection_try_register_object_path(
		_connection, path, &vtable, userData, &error) == TRUE;
	if (!ok)
	{
		if (dbus_error_is_set(&error))
			logError("Could not register D-Bus object path", error);
		else
			dbus_error_free(&error);
		return false;
	}

	dbus_error_free(&error);
	return true;
}

QVariant HelperDBus::getProperty(const QString& destination, const QString& path, const QString& interface, const QString& property, const char* operation)
{
	const QByteArray destinationUtf8 = destination.toUtf8();
	const QByteArray pathUtf8 = path.toUtf8();
	const QByteArray interfaceUtf8 = interface.toUtf8();
	const QByteArray propertyUtf8 = property.toUtf8();

	DBusMessage* message = makeMethodCall(
		destinationUtf8.constData(),
		pathUtf8.constData(),
		HelperDBus::DBusProperties.latin1(),
		"Get");
	if (!message)
		return {};

	DBusMessageIter args;
	dbus_message_iter_init_append(message, &args);
	const char* interfacePtr = interfaceUtf8.constData();
	const char* propertyPtr = propertyUtf8.constData();
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &interfacePtr) ||
		!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &propertyPtr))
	{
		dbus_message_unref(message);
		return {};
	}

	DBusMessage* reply = callSync(message, operation);
	if (!reply)
		return {};

	QVariantList values;
	const bool ok = readMessage(reply, values) && values.size() == 1;
	const QVariant value = ok ? values.first() : QVariant{};
	dbus_message_unref(reply);
	return value;
}
