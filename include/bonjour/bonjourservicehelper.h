#pragma once

#include <QThread>
#include <QHostInfo>
#include <QHash>
#include <mdns.h>

class BonjourServiceHelper;

class BonjourServiceRegister;

class BonjourServiceBrowser;
class Logger;

class BonjourServiceHelper : public QThread
{
	Q_OBJECT

private:
	void init(QString service);

	int open_client_sockets(int* sockets, int max_sockets, int port);
	int open_service_sockets(int* sockets, int max_sockets);
	int send_mdns_query(mdns_query_t* query, size_t count);
	int service_mdns(const char* hostname, const char* service_name, int service_port);

	static int callbackBrowser(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
		uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
		size_t size, size_t name_offset, size_t name_length, size_t record_offset,
		size_t record_length, void* user_data);

	static int serviceCallback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
		uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
		size_t size, size_t name_offset, size_t name_length, size_t record_offset,
		size_t record_length, void* user_data);


	static mdns_string_t ipv4_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in* addr, size_t addrlen);
	static mdns_string_t ipv6_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in6* addr, size_t addrlen);
	static mdns_string_t ip_address_to_string(char* buffer, size_t capacity, const struct sockaddr* addr, size_t addrlen);

	struct sockaddr_in service_address_ipv4;
	struct sockaddr_in6 service_address_ipv6;

	int has_ipv4;
	int has_ipv6;
	static int _instaceCount;

	typedef struct {
		mdns_string_t service;
		mdns_string_t hostname;
		mdns_string_t service_instance;
		mdns_string_t hostname_qualified;
		struct sockaddr_in address_ipv4;
		struct sockaddr_in6 address_ipv6;
		int port;
		mdns_record_t record_ptr;
		mdns_record_t record_srv;
		mdns_record_t record_a;
		mdns_record_t record_aaaa;
		mdns_record_t txt_record[1];
	} service_t;

public:
	BonjourServiceHelper(BonjourServiceBrowser* parent, QString service);
	BonjourServiceHelper(BonjourServiceRegister* parent, QString service, int port);

	~BonjourServiceHelper();

	void run() override;

	void prepareToExit() { _running = false; }

	BonjourServiceBrowser* _browser;
	BonjourServiceRegister* _register;
	QString _service;
	QString _hostname;
	bool _running;
	int _port;
protected:
	Logger*	_log;
};

