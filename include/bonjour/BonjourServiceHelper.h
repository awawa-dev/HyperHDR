#pragma once

#ifndef PCH_ENABLED
	#include <atomic>
	#include <QThread>
	#include <QHash>
#endif

#include <mdns.h>

class BonjourServiceHelper;

class BonjourServiceRegister;

class BonjourServiceBrowser;

class BonjourServiceHelper : public QThread
{
	Q_OBJECT

private:
	int open_client_sockets(int* sockets, int max_sockets, int port);
	int open_service_sockets(int* sockets, int max_sockets);
	int service_mdns(QString hostname, QString serviceName, int service_port);

	static int serviceCallback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
		uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
		size_t size, size_t name_offset, size_t name_length, size_t record_offset,
		size_t record_length, void* user_data);


	static QString ip_address_to_string(const struct sockaddr_in* addr, size_t addrlen);
	static QString ip_address_to_string(const struct sockaddr_in6* addr, size_t addrlen);

	struct sockaddr_in service_address_ipv4;
	struct sockaddr_in6 service_address_ipv6;

	int has_ipv4;
	int has_ipv6;
	static int _instanceCount;

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
		BonjourServiceHelper* parent;
	} service_t;

public:
	BonjourServiceHelper(BonjourServiceRegister* parent, QString service, int port);

	~BonjourServiceHelper();

	void run() override;

	void prepareToExit() { _running = false; }

	BonjourServiceRegister* _register;
	QString			_service;
	QString			_hostname;
	int				_port;
	std::atomic<bool> _running;
	std::atomic<unsigned long> _scanService;
};

