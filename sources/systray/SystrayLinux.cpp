/* SystrayLinux.cpp
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

#include <systray/Systray.h>
#include <micro-dbus/HelperDBus.h>

#include <QCoreApplication>
#include <QVariantMap>
#include <QString>

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>

namespace
{
	constexpr char SniPath[] = "/StatusNotifierItem";
	constexpr char MenuPath[] = "/StatusNotifierItem/menu";
	constexpr char SniIface[] = "org.kde.StatusNotifierItem";
	constexpr char MenuIface[] = "com.canonical.dbusmenu";
	constexpr char PropsIface[] = "org.freedesktop.DBus.Properties";
	constexpr char IntroIface[] = "org.freedesktop.DBus.Introspectable";
	constexpr char BusIface[] = "org.freedesktop.DBus";
	constexpr char WatcherService[] = "org.kde.StatusNotifierWatcher";
	constexpr char WatcherPath[] = "/StatusNotifierWatcher";

	constexpr char SniIntrospect[] = R"xml(
<node>
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect"><arg type="s" direction="out"/></method>
  </interface>
  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get"><arg type="s" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="out"/></method>
    <method name="GetAll"><arg type="s" direction="in"/><arg type="a{sv}" direction="out"/></method>
  </interface>
  <interface name="org.kde.StatusNotifierItem">
    <property name="Category" type="s" access="read"/>
    <property name="Id" type="s" access="read"/>
    <property name="Title" type="s" access="read"/>
    <property name="Status" type="s" access="read"/>
    <property name="WindowId" type="i" access="read"/>
    <property name="IconThemePath" type="s" access="read"/>
    <property name="Menu" type="o" access="read"/>
    <property name="ItemIsMenu" type="b" access="read"/>
    <property name="IconName" type="s" access="read"/>
    <method name="ContextMenu"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="Activate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="SecondaryActivate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="Scroll"><arg type="i" direction="in"/><arg type="s" direction="in"/></method>
    <signal name="NewIcon"/>
  </interface>
</node>
)xml";

	constexpr char MenuIntrospect[] = R"xml(
<node>
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect"><arg type="s" direction="out"/></method>
  </interface>
  <interface name="com.canonical.dbusmenu">
    <method name="GetLayout"><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="as" direction="in"/>
      <arg type="u" direction="out"/><arg type="(ia{sv}av)" direction="out"/></method>
    <method name="GetGroupProperties"><arg type="ai" direction="in"/><arg type="as" direction="in"/><arg type="a(ia{sv})" direction="out"/></method>
    <method name="Event"><arg type="i" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="in"/><arg type="u" direction="in"/></method>
    <method name="AboutToShow"><arg type="i" direction="in"/><arg type="b" direction="out"/></method>
    <signal name="LayoutUpdated"><arg type="u"/><arg type="i"/></signal>
  </interface>
</node>
)xml";

	struct Property { const char* name; const char* signature; };
	constexpr Property SniProperties[] = {
		{"Category", "s"}, {"Id", "s"}, {"Title", "s"}, {"Status", "s"},
		{"WindowId", "i"}, {"IconThemePath", "s"}, {"Menu", "o"},
		{"ItemIsMenu", "b"}, {"IconName", "s"}
	};

	std::string_view view(const char* value) { return value ? std::string_view(value) : std::string_view{}; }

	class SystraySni final : public HelperDBus
	{
	public:
		bool start()
		{
			if (!QCoreApplication::instance() || !open(DBUS_BUS_SESSION)) return false;
			_serviceName = "org.freedesktop.StatusNotifierItem-" + std::to_string(getpid()) + "-1";

			DBusError error;
			dbus_error_init(&error);
			const int nameResult = dbus_bus_request_name(connection(), _serviceName.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
			if (dbus_error_is_set(&error) ||
				(nameResult != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER && nameResult != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER))
			{				
				dbus_error_free(&error);
				closeConnection();
				return false;
			}
			dbus_error_free(&error);
			dispatch();

			for (const char* path : { SniPath, MenuPath })
			{
				if (!registerObjectPath(path, &SystraySni::messageFilter, this))
				{
					closeConnection();
					return false;
				}
			}

			addMatch(QStringLiteral(
				"type='signal',sender='org.freedesktop.DBus',path='/org/freedesktop/DBus',"
				"interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org.kde.StatusNotifierWatcher'"));
			connect(this, &HelperDBus::signalReceived, this,
				[this](const QString&, const QString& iface, const QString& member,
					const QVariantList& args, bool parseError) {
						if (!parseError && iface == QLatin1String(BusIface) &&
							member == QLatin1String("NameOwnerChanged") && args.size() >= 3 &&
							!args[2].toString().isEmpty())
							registerWithWatcher();
				}, Qt::QueuedConnection);
			registerWithWatcher();
			return true;
		}

		void update(SystrayMenu* tray)
		{
			_tray = tray;
			rebuildIndex();
			++_revision;
			emitSignal(SniPath, SniIface, "NewIcon");
			emitLayoutUpdated();
		}

		bool isDarkMode()
		{
			if (auto* msg = makeMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "ReadOne"); msg)
			{
				DBusMessageIter it;
				dbus_message_iter_init_append(msg, &it);

				const char* ns = "org.freedesktop.appearance";
				const char* key = "color-scheme";

				dbus_message_iter_append_basic(&it, DBUS_TYPE_STRING, &ns);
				dbus_message_iter_append_basic(&it, DBUS_TYPE_STRING, &key);

				if (auto* reply = callSync(msg, "ReadOne"); reply)
				{
					QVariantList args;
					const bool isValid = readMessage(reply, args) && !args.isEmpty() && isType<uint>(args.value(0));
					dbus_message_unref(reply);
					if (isValid)
					{						
						return args.value(0).toUInt() == 1;
					}
				}
			}

			return false;
		}

	private:
		void emitLayoutUpdated()
		{
			DBusMessage* signal = dbus_message_new_signal(MenuPath, MenuIface, "LayoutUpdated");
			if (!signal) return;
			DBusMessageIter iter;
			dbus_message_iter_init_append(signal, &iter);
			if (!appendVariant(iter, _revision) || !appendVariant(iter, 0)) { dbus_message_unref(signal); return; }
			send(signal);
		}

		bool watcherPresent() const
		{
			DBusError error;
			dbus_error_init(&error);
			const bool present = dbus_bus_name_has_owner(connection(), WatcherService, &error) == TRUE;
			dbus_error_free(&error);
			return present;
		}

		void registerWithWatcher()
		{
			if (!watcherPresent()) return;
			DBusMessage* message = makeMethodCall(
				WatcherService, WatcherPath, WatcherService, "RegisterStatusNotifierItem");
			if (!message)
				return;

			const char* service = _serviceName.c_str();
			if (!dbus_message_append_args(
				message, DBUS_TYPE_STRING, &service, DBUS_TYPE_INVALID))
			{
				dbus_message_unref(message);
				return;
			}
			if (DBusMessage* reply = callSync(message, "RegisterStatusNotifierItem"))
				dbus_message_unref(reply);
			dispatch();
		}

		void rebuildIndex()
		{
			_items.clear();
			if (_tray && _tray->submenu)
				indexList(_tray->submenu.get());
		}

		void indexList(SystrayMenu* item)
		{
			for (; item && !item->label.empty(); item = item->next.get())
			{
				_items.push_back(item);
				if (item->submenu)
					indexList(item->submenu.get());
			}
		}

		SystrayMenu* itemById(int id) const
		{
			return id > 0 && id <= static_cast<int>(_items.size())
				? _items[static_cast<size_t>(id) - 1]
				: nullptr;
		}

		int idOf(const SystrayMenu* item) const
		{
			for (size_t i = 0; i < _items.size(); ++i)
				if (_items[i] == item)
					return static_cast<int>(i) + 1;
			return -1;
		}

		SystrayMenu* firstChild(int id) const
		{
			if (id == 0)
				return _tray && _tray->submenu ? _tray->submenu.get() : nullptr;
			if (auto* item = itemById(id))
				return item->submenu.get();
			return nullptr;
		}

		const Property* findProperty(std::string_view name) const
		{
			for (const auto& property : SniProperties)
				if (name == property.name)
					return &property;
			return nullptr;
		}

		bool appendSniValue(DBusMessageIter& iter, std::string_view name) const
		{
			if (name == "Category") return appendVariant(iter, QStringLiteral("ApplicationStatus"));
			if (name == "Id" || name == "Title") return appendVariant(iter, QStringLiteral("HyperHDR"));
			if (name == "Status") return appendVariant(iter, QStringLiteral("Active"));
			if (name == "WindowId") return appendVariant(iter, 0);
			if (name == "IconThemePath") return appendVariant(iter, QStringLiteral(""));
			if (name == "Menu") return appendObjectPath(iter, QString::fromLatin1(MenuPath));
			if (name == "ItemIsMenu") return appendVariant(iter, true);
			if (name == "IconName") return appendVariant(iter, _tray ? QString::fromStdString(_tray->iconName) : QString());
			return false;
		}

		void handleSniProperties(DBusMessage* message, bool isGet)
		{
			if (!dbus_message_has_signature(message, isGet ? "ss" : "s"))
			{
				replyError(message, DBUS_ERROR_INVALID_ARGS, isGet ? "Get" : "GetAll");
				return;
			}

			DBusMessageIter args;
			dbus_message_iter_init(message, &args);

			const char* ifacePtr = nullptr;
			dbus_message_iter_get_basic(&args, &ifacePtr);
			const std::string_view iface = view(ifacePtr);

			if (iface != SniIface)
			{
				replyError(message, DBUS_ERROR_UNKNOWN_INTERFACE, iface.data());
				return;
			}

			if (isGet)
			{
				dbus_message_iter_next(&args);
				const char* namePtr = nullptr;
				dbus_message_iter_get_basic(&args, &namePtr);
				const std::string_view name = view(namePtr);

				const auto* property = findProperty(name);
				if (!property)
				{
					replyError(message, DBUS_ERROR_UNKNOWN_PROPERTY, name.data());
					return;
				}

				replyWith(message, "Get", [&](auto& iter) {
					return dbusContainer(iter, DBUS_TYPE_VARIANT, property->signature,
						[&](auto& value) { return appendSniValue(value, property->name); });
					});
				return;
			}

			replyWith(message, "GetAll", [&](auto& iter) {
				return dbusContainer(iter, DBUS_TYPE_ARRAY, "{sv}", [&](auto& dict) {
					for (const auto& property : SniProperties)
					{
						if (!dbusContainer(dict, DBUS_TYPE_DICT_ENTRY, nullptr, [&](auto& entry) {
							return appendVariant(entry, QString::fromUtf8(property.name)) &&
								dbusContainer(entry, DBUS_TYPE_VARIANT, property.signature,
									[&](auto& value) { return appendSniValue(value, property.name); });
							}))
							return false;
					}
					return true;
					});
				});
		}

		QVariantMap getMenuProperties(SystrayMenu* item) const
		{
			QVariantMap map;
			if (!item)
			{
				map[QStringLiteral("children-display")] = QStringLiteral("submenu");
				return map;
			}
			if (item->label == "-")
			{
				map[QStringLiteral("type")] = QStringLiteral("separator");
				return map;
			}

			QString label = QString::fromStdString(item->label);
			label.remove(QLatin1Char('&'));
			map[QStringLiteral("label")] = label;

			if (item->isDisabled)
				map[QStringLiteral("enabled")] = false;
			if (item->checkGroup)
				map[QStringLiteral("toggle-type")] = QStringLiteral("radio");
			else if (item->isChecked)
				map[QStringLiteral("toggle-type")] = QStringLiteral("checkmark");
			if (item->checkGroup || item->isChecked)
				map[QStringLiteral("toggle-state")] = item->isChecked ? 1 : 0;
			if (!item->iconName.empty())
				map[QStringLiteral("icon-name")] = QString::fromStdString(item->iconName);
			if (item->submenu)
				map[QStringLiteral("children-display")] = QStringLiteral("submenu");

			return map;
		}

		bool appendLayout(DBusMessageIter& out, int id, int depth) const
		{
			if (id != 0 && !itemById(id))
				return false;

			return dbusContainer(out, DBUS_TYPE_STRUCT, nullptr, [&](auto& node) {
				if (!appendVariant(node, id) ||
					!appendVariantMap(node, getMenuProperties(id == 0 ? nullptr : itemById(id))))
					return false;

				return dbusContainer(node, DBUS_TYPE_ARRAY, "v", [&](auto& children) {
					if (depth == 0) return true;

					for (auto* child = firstChild(id);
						child && !child->label.empty();
						child = child->next.get())
					{
						const int childId = idOf(child);
						if (childId < 0 ||
							!dbusContainer(children, DBUS_TYPE_VARIANT, "(ia{sv}av)", [&](auto& v) {
								return appendLayout(v, childId, depth < 0 ? -1 : depth - 1);
								}))
							return false;
					}
					return true;
					});
				});
		}

		void handleGetLayout(DBusMessage* message)
		{
			if (!dbus_message_has_signature(message, "iias"))
			{
				replyError(message, DBUS_ERROR_INVALID_ARGS, "GetLayout");
				return;
			}

			DBusMessageIter args;
			dbus_message_iter_init(message, &args);
			dbus_int32_t parentId = 0, depth = -1;
			dbus_message_iter_get_basic(&args, &parentId);
			dbus_message_iter_next(&args);
			dbus_message_iter_get_basic(&args, &depth);

			if (parentId < 0 || (parentId != 0 && !itemById(parentId)))
			{
				replyError(message, DBUS_ERROR_INVALID_ARGS, "GetLayout");
				return;
			}

			replyWith(message, "GetLayout", [&](auto& iter) {
				return appendVariant(iter, _revision) && appendLayout(iter, parentId, depth);
				});
		}

		void handleGetGroupProperties(DBusMessage* message)
		{
			if (!dbus_message_has_signature(message, "aias"))
			{
				replyError(message, DBUS_ERROR_INVALID_ARGS, "GetGroupProperties");
				return;
			}

			DBusMessageIter args, ids;
			dbus_message_iter_init(message, &args);
			dbus_message_iter_recurse(&args, &ids);

			replyWith(message, "GetGroupProperties", [&](auto& iter) {
				return dbusContainer(iter, DBUS_TYPE_ARRAY, "(ia{sv})", [&](auto& array) {
					for (; dbus_message_iter_get_arg_type(&ids) != DBUS_TYPE_INVALID;
						dbus_message_iter_next(&ids))
					{
						dbus_int32_t id = 0;
						dbus_message_iter_get_basic(&ids, &id);
						if (id < 0 || (id != 0 && !itemById(id)))
							continue;

						if (!dbusContainer(array, DBUS_TYPE_STRUCT, nullptr, [&](auto& node) {
							return appendVariant(node, id) &&
								appendVariantMap(node, getMenuProperties(id == 0 ? nullptr : itemById(id)));
							}))
							return false;
					}
					return true;
					});
				});
		}

		void handleMenuEvent(DBusMessage* message)
		{
			if (!dbus_message_has_signature(message, "isvu"))
			{
				replyError(message, DBUS_ERROR_INVALID_ARGS, "Event");
				return;
			}

			DBusMessageIter args;
			dbus_message_iter_init(message, &args);
			dbus_int32_t id = 0;
			const char* eventId = nullptr;
			dbus_message_iter_get_basic(&args, &id);
			dbus_message_iter_next(&args);
			dbus_message_iter_get_basic(&args, &eventId);

			if (view(eventId) == "clicked")
			{
				if (auto* item = itemById(id); item && !item->isDisabled && item->callback)
					item->callback(item);
			}

			replyVoid(message);
		}

		DBusHandlerResult handleSni(DBusMessage* message)
		{
			if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
				return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

			const std::string_view iface = view(dbus_message_get_interface(message));
			const std::string_view method = view(dbus_message_get_member(message));

			if (iface == IntroIface && method == "Introspect")
			{
				replyString(message, SniIntrospect);
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			if (iface == PropsIface)
				return handleSniPropertiesCall(message, method);
			if (iface == SniIface)
				return handleSniMethod(message, method);

			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		DBusHandlerResult handleSniPropertiesCall(DBusMessage* message, std::string_view method)
		{
			if (method == "Get")
			{
				handleSniProperties(message, true);
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			if (method == "GetAll")
			{
				handleSniProperties(message, false);
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		DBusHandlerResult handleSniMethod(DBusMessage* message, std::string_view method)
		{
			if (method == "ContextMenu" || method == "Activate" || method == "SecondaryActivate")
			{
				if (dbus_message_has_signature(message, "ii")) replyVoid(message);
				else replyError(message, DBUS_ERROR_INVALID_ARGS, method.data());
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			if (method == "Scroll")
			{
				if (dbus_message_has_signature(message, "is")) replyVoid(message);
				else replyError(message, DBUS_ERROR_INVALID_ARGS, "Scroll");
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		DBusHandlerResult handleMenu(DBusMessage* message)
		{
			if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
				return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

			const std::string_view iface = view(dbus_message_get_interface(message));
			const std::string_view method = view(dbus_message_get_member(message));

			if (iface == IntroIface && method == "Introspect")
			{
				replyString(message, MenuIntrospect);
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			if (iface == MenuIface)
				return handleMenuMethod(message, method);

			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		DBusHandlerResult handleMenuMethod(DBusMessage* message, std::string_view method)
		{
			if (method == "GetLayout")
			{
				handleGetLayout(message);
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			if (method == "GetGroupProperties")
			{
				handleGetGroupProperties(message);
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			if (method == "Event")
			{
				handleMenuEvent(message);
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			if (method == "AboutToShow")
			{
				if (dbus_message_has_signature(message, "i"))
					replyWith(message, "AboutToShow", [](auto& iter) { return appendVariant(iter, false); });
				else
					replyError(message, DBUS_ERROR_INVALID_ARGS, "AboutToShow");
				return DBUS_HANDLER_RESULT_HANDLED;
			}
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		static DBusHandlerResult messageFilter(DBusConnection*, DBusMessage* message, void* data)
		{
			auto* self = static_cast<SystraySni*>(data);
			const std::string_view path = view(dbus_message_get_path(message));

			if (path == SniPath)
				return self->handleSni(message);
			if (path == MenuPath)
				return self->handleMenu(message);

			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		SystrayMenu* _tray = nullptr;
		std::string _serviceName;
		dbus_uint32_t _revision = 0;
		std::vector<SystrayMenu*> _items;
	};

	std::unique_ptr<SystraySni> g_sni;
}

extern "C"
{
	bool SystrayInitialize(SystrayMenu* tray)
	{
		if (!g_sni) {
			g_sni = std::make_unique<SystraySni>();
			if (!g_sni->start()) {
				g_sni.reset();
				return false;
			}
			else if (tray) {
				g_sni->update(tray);
			}
		}
		return true;
	}

	void SystrayUpdate(SystrayMenu* tray) {
		if (g_sni && tray) {
			g_sni->update(tray);
		}
	}

	int SystrayLoop() {
		return 0;
	}

	void SystrayClose() {
		g_sni.reset();
	}

	bool SystrayDarkmode() {
		return (g_sni) ? g_sni->isDarkMode() : false;
	}
}
