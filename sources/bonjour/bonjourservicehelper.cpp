#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <bonjour/bonjourrecord.h>
#include <bonjour/bonjourservicehelper.h>
#include <bonjour/bonjourserviceregister.h>
#include <bonjour/bonjourservicebrowser.h>
#include <utils/Logger.h>
#include <HyperhdrConfig.h>

#include <stdio.h>

#include <errno.h>
#include <signal.h>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

void BonjourServiceHelper::init(QString service)
{
	_service = service;
	has_ipv4 = 0;
	has_ipv6 = 0;
	_running = true;

	memset(&service_address_ipv4, 0, sizeof(service_address_ipv4));
	memset(&service_address_ipv6, 0, sizeof(service_address_ipv6));

#ifdef _WIN32
	WORD versionWanted = MAKEWORD(1, 1);
	WSADATA wsaData;


	if (_instaceCount++ == 0 && WSAStartup(versionWanted, &wsaData))
	{
		printf("Failed to initialize WinSock\n");
	}

	char hostname_buffer[256];
	DWORD hostname_size = (DWORD)sizeof(hostname_buffer);
	if (GetComputerNameA(hostname_buffer, &hostname_size))
		_hostname = QString::fromLocal8Bit(hostname_buffer);
	printf("Hostname: %s\n", hostname_buffer);
#else
	char hostname_buffer[256];
	size_t hostname_size = sizeof(hostname_buffer);
	if (gethostname(hostname_buffer, hostname_size) == 0)
		_hostname = QString::fromLocal8Bit(hostname_buffer);
#endif
}

BonjourServiceHelper::BonjourServiceHelper(BonjourServiceBrowser* parent, QString service) :
	QThread(parent)
{
	_port = 0;
	_register = nullptr;
	_browser = parent;
	init(service);
}

BonjourServiceHelper::BonjourServiceHelper(BonjourServiceRegister* parent, QString service, int port) :
	QThread(parent)
{
	_port = port;
	_register = parent;
	_browser = nullptr;
	init(service);
}

BonjourServiceHelper::~BonjourServiceHelper()
{
#ifdef _WIN32
	if (--_instaceCount == 0)
		WSACleanup();
#endif
}

void BonjourServiceHelper::run()
{
	if (_browser != nullptr)
	{
		mdns_query_t serviceQuery[1];
		QByteArray name = (_service + ".local").toLocal8Bit();

		serviceQuery[0].name = name.data();
		serviceQuery[0].type = MDNS_RECORDTYPE_PTR;

		serviceQuery[0].length = strlen(serviceQuery[0].name);

		send_mdns_query(serviceQuery, 1);
	}
	else if (_register != nullptr)
	{
		QByteArray hostname = (QString("%1:%2").arg(_hostname).arg(_port)).toLocal8Bit();
		QByteArray service = (_service + ".local").toLocal8Bit();
		service_mdns(hostname.data(), service.data(), _port);
	}
}


mdns_string_t BonjourServiceHelper::ipv4_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in* addr, size_t addrlen)
{
	char host[NI_MAXHOST] = { 0 };
	char service[NI_MAXSERV] = { 0 };
	int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
		service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
	int len = 0;
	if (ret == 0) {
		if (addr->sin_port != 0)
			len = snprintf(buffer, capacity, "%s:%s", host, service);
		else
			len = snprintf(buffer, capacity, "%s", host);
	}
	if (len >= (int)capacity)
		len = (int)capacity - 1;
	mdns_string_t str;
	str.str = buffer;
	str.length = len;
	return str;
}

mdns_string_t BonjourServiceHelper::ipv6_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in6* addr, size_t addrlen)
{
	char host[NI_MAXHOST] = { 0 };
	char service[NI_MAXSERV] = { 0 };
	int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
		service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
	int len = 0;
	if (ret == 0) {
		if (addr->sin6_port != 0)
			len = snprintf(buffer, capacity, "[%s]:%s", host, service);
		else
			len = snprintf(buffer, capacity, "%s", host);
	}
	if (len >= (int)capacity)
		len = (int)capacity - 1;
	mdns_string_t str;
	str.str = buffer;
	str.length = len;
	return str;
}

mdns_string_t BonjourServiceHelper::ip_address_to_string(char* buffer, size_t capacity, const struct sockaddr* addr, size_t addrlen)
{
	if (addr->sa_family == AF_INET6)
		return ipv6_address_to_string(buffer, capacity, (const struct sockaddr_in6*)addr, addrlen);
	return ipv4_address_to_string(buffer, capacity, (const struct sockaddr_in*)addr, addrlen);
}

int BonjourServiceHelper::open_client_sockets(int* sockets, int max_sockets, int port)
{
	// When sending, each socket can only send to one network interface
	// Thus we need to open one socket for each interface and address family
	int num_sockets = 0;

#ifdef _WIN32

	IP_ADAPTER_ADDRESSES* adapter_address = 0;
	ULONG address_size = 8000;
	unsigned int ret;
	unsigned int num_retries = 4;
	do {
		adapter_address = (IP_ADAPTER_ADDRESSES*)malloc(address_size);
		ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, 0,
			adapter_address, &address_size);
		if (ret == ERROR_BUFFER_OVERFLOW) {
			free(adapter_address);
			adapter_address = 0;
			address_size *= 2;
		}
		else {
			break;
		}
	} while (num_retries-- > 0);

	if (!adapter_address || (ret != NO_ERROR)) {
		free(adapter_address);
		printf("Failed to get network adapter addresses\n");
		return num_sockets;
	}

	int first_ipv4 = 1;
	int first_ipv6 = 1;
	for (PIP_ADAPTER_ADDRESSES adapter = adapter_address; adapter; adapter = adapter->Next) {
		if (adapter->TunnelType == TUNNEL_TYPE_TEREDO)
			continue;
		if (adapter->OperStatus != IfOperStatusUp)
			continue;

		for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast;
			unicast = unicast->Next) {
			if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
				struct sockaddr_in* saddr = (struct sockaddr_in*)unicast->Address.lpSockaddr;
				if ((saddr->sin_addr.S_un.S_un_b.s_b1 != 127) ||
					(saddr->sin_addr.S_un.S_un_b.s_b2 != 0) ||
					(saddr->sin_addr.S_un.S_un_b.s_b3 != 0) ||
					(saddr->sin_addr.S_un.S_un_b.s_b4 != 1)) {
					int log_addr = 0;
					if (first_ipv4) {
						service_address_ipv4 = *saddr;
						first_ipv4 = 0;
						log_addr = 1;
					}
					has_ipv4 = 1;
					if (num_sockets < max_sockets) {
						saddr->sin_port = htons((unsigned short)port);
						int sock = mdns_socket_open_ipv4(saddr);
						if (sock >= 0) {
							sockets[num_sockets++] = sock;
							log_addr = 1;
						}
						else {
							log_addr = 0;
						}
					}
					if (log_addr) {
						char buffer[128];
						mdns_string_t addr = ipv4_address_to_string(buffer, sizeof(buffer), saddr,
							sizeof(struct sockaddr_in));
						printf("Local IPv4 address: %.*s\n", MDNS_STRING_FORMAT(addr));
					}
				}
			}
			else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) {
				struct sockaddr_in6* saddr = (struct sockaddr_in6*)unicast->Address.lpSockaddr;
				// Ignore link-local addresses
				if (saddr->sin6_scope_id)
					continue;
				static const unsigned char localhost[] = { 0, 0, 0, 0, 0, 0, 0, 0,
														  0, 0, 0, 0, 0, 0, 0, 1 };
				static const unsigned char localhost_mapped[] = { 0, 0, 0,    0,    0,    0, 0, 0,
																 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1 };
				if ((unicast->DadState == NldsPreferred) &&
					memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
					memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16)) {
					int log_addr = 0;
					if (first_ipv6) {
						service_address_ipv6 = *saddr;
						first_ipv6 = 0;
						log_addr = 1;
					}
					has_ipv6 = 1;
					if (num_sockets < max_sockets) {
						saddr->sin6_port = htons((unsigned short)port);
						int sock = mdns_socket_open_ipv6(saddr);
						if (sock >= 0) {
							sockets[num_sockets++] = sock;
							log_addr = 1;
						}
						else {
							log_addr = 0;
						}
					}
					if (log_addr) {
						char buffer[128];
						mdns_string_t addr = ipv6_address_to_string(buffer, sizeof(buffer), saddr,
							sizeof(struct sockaddr_in6));
						printf("Local IPv6 address: %.*s\n", MDNS_STRING_FORMAT(addr));
					}
				}
			}
		}
	}

	free(adapter_address);

#else

	struct ifaddrs* ifaddr = 0;
	struct ifaddrs* ifa = 0;

	if (getifaddrs(&ifaddr) < 0)
		printf("Unable to get interface addresses\n");

	int first_ipv4 = 1;
	int first_ipv6 = 1;
	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr)
			continue;
		if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_MULTICAST))
			continue;
		if ((ifa->ifa_flags & IFF_LOOPBACK) || (ifa->ifa_flags & IFF_POINTOPOINT))
			continue;

		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct sockaddr_in* saddr = (struct sockaddr_in*)ifa->ifa_addr;
			if (saddr->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
				int log_addr = 0;
				if (first_ipv4) {
					service_address_ipv4 = *saddr;
					first_ipv4 = 0;
					log_addr = 1;
				}
				has_ipv4 = 1;
				if (num_sockets < max_sockets) {
					saddr->sin_port = htons(port);
					int sock = mdns_socket_open_ipv4(saddr);
					if (sock >= 0) {
						sockets[num_sockets++] = sock;
						log_addr = 1;
					}
					else {
						log_addr = 0;
					}
				}
				if (log_addr) {
					char buffer[128];
					mdns_string_t addr = ipv4_address_to_string(buffer, sizeof(buffer), saddr,
						sizeof(struct sockaddr_in));
					printf("Local IPv4 address: %.*s\n", MDNS_STRING_FORMAT(addr));
				}
			}
		}
		else if (ifa->ifa_addr->sa_family == AF_INET6) {
			struct sockaddr_in6* saddr = (struct sockaddr_in6*)ifa->ifa_addr;
			// Ignore link-local addresses
			if (saddr->sin6_scope_id)
				continue;
			static const unsigned char localhost[] = { 0, 0, 0, 0, 0, 0, 0, 0,
													  0, 0, 0, 0, 0, 0, 0, 1 };
			static const unsigned char localhost_mapped[] = { 0, 0, 0,    0,    0,    0, 0, 0,
															 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1 };
			if (memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
				memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16)) {
				int log_addr = 0;
				if (first_ipv6) {
					service_address_ipv6 = *saddr;
					first_ipv6 = 0;
					log_addr = 1;
				}
				has_ipv6 = 1;
				if (num_sockets < max_sockets) {
					saddr->sin6_port = htons(port);
					int sock = mdns_socket_open_ipv6(saddr);
					if (sock >= 0) {
						sockets[num_sockets++] = sock;
						log_addr = 1;
					}
					else {
						log_addr = 0;
					}
				}
				if (log_addr) {
					char buffer[128];
					mdns_string_t addr = ipv6_address_to_string(buffer, sizeof(buffer), saddr,
						sizeof(struct sockaddr_in6));
					printf("Local IPv6 address: %.*s\n", MDNS_STRING_FORMAT(addr));
				}
			}
		}
	}

	freeifaddrs(ifaddr);

#endif

	return num_sockets;
}

int BonjourServiceHelper::send_mdns_query(mdns_query_t* query, size_t count)
{
	int sockets[32];
	int query_id[32];
	int num_sockets = open_client_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]), 0);
	if (num_sockets <= 0) {
		printf("Failed to open any client sockets\n");
		return -1;
	}
	printf("Opened %d socket%s for mDNS query\n", num_sockets, num_sockets ? "s" : "");

	size_t capacity = 2048;
	void* buffer = malloc(capacity);
	void* user_data = _browser;

	printf("Sending mDNS query");
	for (size_t iq = 0; iq < count; ++iq) {
		const char* record_name = "PTR";
		if (query[iq].type == MDNS_RECORDTYPE_SRV)
			record_name = "SRV";
		else if (query[iq].type == MDNS_RECORDTYPE_A)
			record_name = "A";
		else if (query[iq].type == MDNS_RECORDTYPE_AAAA)
			record_name = "AAAA";
		else
			query[iq].type = MDNS_RECORDTYPE_PTR;
		printf(" : %s %s", query[iq].name, record_name);
	}
	printf("\n");
	for (int isock = 0; isock < num_sockets; ++isock) {
		query_id[isock] =
			mdns_multiquery_send(sockets[isock], query, count, buffer, capacity, 0);
		if (query_id[isock] < 0)
			printf("Failed to send mDNS query: %s\n", strerror(errno));
	}

	// This is a simple implementation that loops for 5 seconds or as long as we get replies
	int res;
	printf("Reading mDNS query replies\n");
	int records = 0;
	do {
		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		int nfds = 0;
		fd_set readfs;
		FD_ZERO(&readfs);
		for (int isock = 0; isock < num_sockets; ++isock) {
			if (sockets[isock] >= nfds)
				nfds = sockets[isock] + 1;
			FD_SET(sockets[isock], &readfs);
		}

		res = select(nfds, &readfs, 0, 0, &timeout);
		if (res > 0) {
			for (int isock = 0; isock < num_sockets; ++isock) {
				if (FD_ISSET(sockets[isock], &readfs)) {
					int rec = mdns_query_recv(sockets[isock], buffer, capacity, callbackBrowser,
						user_data, query_id[isock]);
					if (rec > 0)
						records += rec;
				}
				FD_SET(sockets[isock], &readfs);
			}
		}
	} while (res > 0);

	printf("Read %d records\n", records);

	free(buffer);

	for (int isock = 0; isock < num_sockets; ++isock)
		mdns_socket_close(sockets[isock]);
	printf("Closed socket%s\n", num_sockets ? "s" : "");

	return 0;
}

int BonjourServiceHelper::callbackBrowser(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
	uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
	size_t size, size_t name_offset, size_t name_length, size_t record_offset,
	size_t record_length, void* user_data)
{
	(void)sizeof(sock);
	(void)sizeof(query_id);
	(void)sizeof(name_length);
	(void)sizeof(user_data);

	char entrybuffer[256];
	char namebuffer[256];

	mdns_string_t entrystr =
		mdns_string_extract(data, size, &name_offset, entrybuffer, sizeof(entrybuffer));

	if (rtype == MDNS_RECORDTYPE_SRV) {
		mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length,
			namebuffer, sizeof(namebuffer));

		if (user_data != nullptr)
		{
			BonjourServiceBrowser* receive = (BonjourServiceBrowser*)user_data;
			emit receive->resolve(QString::fromLocal8Bit(srv.name.str).left(srv.name.length), srv.port);
		}
	}
	else if (rtype == MDNS_RECORDTYPE_A) {
		struct sockaddr_in addr;
		mdns_record_parse_a(data, size, record_offset, record_length, &addr);
		mdns_string_t addrstr =
			ipv4_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));

		if (user_data != nullptr)
		{
			BonjourServiceBrowser* receive = (BonjourServiceBrowser*)user_data;
			emit receive->resolveIp(QString::fromLocal8Bit(entrystr.str).left(entrystr.length),
				QString::fromLocal8Bit(addrstr.str).left(addrstr.length));
		}
	}
	return 0;
}

int BonjourServiceHelper::open_service_sockets(int* sockets, int max_sockets)
{
	// When recieving, each socket can recieve data from all network interfaces
	// Thus we only need to open one socket for each address family
	int num_sockets = 0;

	// Call the client socket function to enumerate and get local addresses,
	// but not open the actual sockets
	open_client_sockets(0, 0, 0);

	if (num_sockets < max_sockets) {
		struct sockaddr_in sock_addr;
		memset(&sock_addr, 0, sizeof(struct sockaddr_in));
		sock_addr.sin_family = AF_INET;
#ifdef _WIN32
		sock_addr.sin_addr = in4addr_any;
#else
		sock_addr.sin_addr.s_addr = INADDR_ANY;
#endif
		sock_addr.sin_port = htons(MDNS_PORT);
#ifdef __APPLE__
		sock_addr.sin_len = sizeof(struct sockaddr_in);
#endif
		int sock = mdns_socket_open_ipv4(&sock_addr);
		if (sock >= 0)
			sockets[num_sockets++] = sock;
	}

	if (num_sockets < max_sockets) {
		struct sockaddr_in6 sock_addr;
		memset(&sock_addr, 0, sizeof(struct sockaddr_in6));
		sock_addr.sin6_family = AF_INET6;
		sock_addr.sin6_addr = in6addr_any;
		sock_addr.sin6_port = htons(MDNS_PORT);
#ifdef __APPLE__
		sock_addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
		int sock = mdns_socket_open_ipv6(&sock_addr);
		if (sock >= 0)
			sockets[num_sockets++] = sock;
	}

	return num_sockets;
}

int BonjourServiceHelper::service_mdns(const char* hostname, const char* service_name, int service_port)
{
	int sockets[32];
	int num_sockets = open_service_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]));
	if (num_sockets <= 0) {
		printf("Failed to open any client sockets\n");
		return -1;
	}
	printf("Opened %d socket%s for mDNS service\n", num_sockets, num_sockets ? "s" : "");

	size_t service_name_length = strlen(service_name);
	if (!service_name_length) {
		printf("Invalid service name\n");
		return -1;
	}

	char* service_name_buffer = (char*)malloc(service_name_length + 2);
	memcpy(service_name_buffer, service_name, service_name_length);
	if (service_name_buffer[service_name_length - 1] != '.')
		service_name_buffer[service_name_length++] = '.';
	service_name_buffer[service_name_length] = 0;
	service_name = service_name_buffer;

	printf("Service mDNS: %s:%d\n", service_name, service_port);
	printf("Hostname: %s\n", hostname);

	size_t capacity = 2048;
	void* buffer = malloc(capacity);

	mdns_string_t service_string = { service_name, strlen(service_name) };
	mdns_string_t hostname_string = { hostname, strlen(hostname) };

	// Build the service instance "<hostname>.<_service-name>._tcp.local." string
	char service_instance_buffer[256] = { 0 };
	snprintf(service_instance_buffer, sizeof(service_instance_buffer) - 1, "%.*s.%.*s",
		MDNS_STRING_FORMAT(hostname_string), MDNS_STRING_FORMAT(service_string));
	mdns_string_t service_instance_string = { service_instance_buffer, strlen(service_instance_buffer) };

	// Build the "<hostname>.local." string
	char qualified_hostname_buffer[256] = { 0 };
	snprintf(qualified_hostname_buffer, sizeof(qualified_hostname_buffer) - 1, "%.*s.local.",
		MDNS_STRING_FORMAT(hostname_string));
	mdns_string_t hostname_qualified_string = { qualified_hostname_buffer, strlen(qualified_hostname_buffer) };

	service_t service = { 0 };
	service.service = service_string;
	service.hostname = hostname_string;
	service.service_instance = service_instance_string;
	service.hostname_qualified = hostname_qualified_string;
	service.address_ipv4 = service_address_ipv4;
	service.address_ipv6 = service_address_ipv6;
	service.port = service_port;

	// Setup our mDNS records

	// PTR record reverse mapping "<_service-name>._tcp.local." to
	// "<hostname>.<_service-name>._tcp.local."
	service.record_ptr.name = service.service,
	service.record_ptr.type = MDNS_RECORDTYPE_PTR;
	service.record_ptr.data.ptr.name = service.service_instance;
	service.record_ptr.rclass = 0;
	service.record_ptr.ttl = 0;

	// SRV record mapping "<hostname>.<_service-name>._tcp.local." to
	// "<hostname>.local." with port. Set weight & priority to 0.
	service.record_srv.name = service.service_instance;
	service.record_srv.type = MDNS_RECORDTYPE_SRV;
	service.record_srv.data.srv.name = service.hostname_qualified;
	service.record_srv.data.srv.port = service.port;
	service.record_srv.data.srv.priority = 0;
	service.record_srv.data.srv.weight = 0;
	service.record_srv.rclass = 0;
	service.record_srv.ttl = 0;

	// A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
	service.record_a.name = service.hostname_qualified;
	service.record_a.type = MDNS_RECORDTYPE_A;
	service.record_a.data.a.addr = service.address_ipv4;
	service.record_a.rclass = 0;
	service.record_a.ttl = 0;

	service.record_aaaa.name = service.hostname_qualified;
	service.record_aaaa.type = MDNS_RECORDTYPE_AAAA;
	service.record_aaaa.data.aaaa.addr = service.address_ipv6;
	service.record_aaaa.rclass = 0;
	service.record_aaaa.ttl = 0;

	// Add two test TXT records for our service instance name, will be coalesced into
	// one record with both key-value pair strings by the library
	service.txt_record[0].name = service.service_instance;
	service.txt_record[0].type = MDNS_RECORDTYPE_TXT;
	service.txt_record[0].data.txt.key = {MDNS_STRING_CONST("version")};
	service.txt_record[0].data.txt.value = {MDNS_STRING_CONST(HYPERHDR_VERSION)};
	service.txt_record[0].rclass = 0;
	service.txt_record[0].ttl = 0;

	// Send an announcement on startup of service
	{
		printf("Sending announce\n");
		mdns_record_t additional[5] = { 0 };
		size_t additional_count = 0;
		additional[additional_count++] = service.record_srv;
		if (service.address_ipv4.sin_family == AF_INET)
			additional[additional_count++] = service.record_a;
		if (service.address_ipv6.sin6_family == AF_INET6)
			additional[additional_count++] = service.record_aaaa;
		additional[additional_count++] = service.txt_record[0];

		for (int isock = 0; isock < num_sockets; ++isock)
			mdns_announce_multicast(sockets[isock], buffer, capacity, service.record_ptr, 0, 0,
				additional, additional_count);
	}

	// This is a crude implementation that checks for incoming queries
	while (_running) {
		int nfds = 0;
		fd_set readfs;
		FD_ZERO(&readfs);
		for (int isock = 0; isock < num_sockets; ++isock) {
			if (sockets[isock] >= nfds)
				nfds = sockets[isock] + 1;
			FD_SET(sockets[isock], &readfs);
		}

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;

		if (select(nfds, &readfs, 0, 0, &timeout) >= 0) {
			for (int isock = 0; isock < num_sockets; ++isock) {
				if (FD_ISSET(sockets[isock], &readfs)) {
					mdns_socket_listen(sockets[isock], buffer, capacity, serviceCallback,
						&service);
				}
				FD_SET(sockets[isock], &readfs);
			}
		}
		else {
			break;
		}
	}

	// Send a goodbye on end of service
	{
		printf("Sending goodbye\n");
		mdns_record_t additional[5] = { 0 };
		size_t additional_count = 0;
		additional[additional_count++] = service.record_srv;
		if (service.address_ipv4.sin_family == AF_INET)
			additional[additional_count++] = service.record_a;
		if (service.address_ipv6.sin6_family == AF_INET6)
			additional[additional_count++] = service.record_aaaa;
		additional[additional_count++] = service.txt_record[0];

		for (int isock = 0; isock < num_sockets; ++isock)
			mdns_goodbye_multicast(sockets[isock], buffer, capacity, service.record_ptr, 0, 0,
				additional, additional_count);
	}

	free(buffer);
	free(service_name_buffer);

	for (int isock = 0; isock < num_sockets; ++isock)
		mdns_socket_close(sockets[isock]);
	printf("Closed socket%s\n", num_sockets ? "s" : "");

	return 0;
}

int BonjourServiceHelper::serviceCallback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
	uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
	size_t size, size_t name_offset, size_t name_length, size_t record_offset,
	size_t record_length, void* user_data)
{
	(void)sizeof(ttl);
	if (entry != MDNS_ENTRYTYPE_QUESTION)
		return 0;
	char namebuffer[256];
	char sendbuffer[1024];

	const char dns_sd[] = "_services._dns-sd._udp.local.";
	const service_t* service = (const service_t*)user_data;

	size_t offset = name_offset;
	mdns_string_t name = mdns_string_extract(data, size, &offset, namebuffer, sizeof(namebuffer));

	const char* record_name = 0;
	if (rtype == MDNS_RECORDTYPE_PTR)
		record_name = "PTR";
	else if (rtype == MDNS_RECORDTYPE_SRV)
		record_name = "SRV";
	else if (rtype == MDNS_RECORDTYPE_A)
		record_name = "A";
	else if (rtype == MDNS_RECORDTYPE_AAAA)
		record_name = "AAAA";
	else if (rtype == MDNS_RECORDTYPE_TXT)
		record_name = "TXT";
	else if (rtype == MDNS_RECORDTYPE_ANY)
		record_name = "ANY";

	if (record_name == 0)
		return 0;

	if ((name.length == (sizeof(dns_sd) - 1)) &&
		(strncmp(name.str, dns_sd, sizeof(dns_sd) - 1) == 0)) {
		if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype == MDNS_RECORDTYPE_ANY)) {
			// The PTR query was for the DNS-SD domain, send answer with a PTR record for the
			// service name we advertise, typically on the "<_service-name>._tcp.local." format

			// Answer PTR record reverse mapping "<_service-name>._tcp.local." to
			// "<hostname>.<_service-name>._tcp.local."
			mdns_record_t answer;
			answer.name = name;
			answer.type = MDNS_RECORDTYPE_PTR;
			answer.data.ptr.name = service->service;

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0, 0,
					0);
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0, 0,
					0);
			}
		}
	}
	else if ((name.length == service->service.length) &&
		(strncmp(name.str, service->service.str, name.length) == 0)) {
		if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype == MDNS_RECORDTYPE_ANY)) {
			// The PTR query was for our service (usually "<_service-name._tcp.local"), answer a PTR
			// record reverse mapping the queried service name to our service instance name
			// (typically on the "<hostname>.<_service-name>._tcp.local." format), and add
			// additional records containing the SRV record mapping the service instance name to our
			// qualified hostname (typically "<hostname>.local.") and port, as well as any IPv4/IPv6
			// address for the hostname as A/AAAA records, and two test TXT records

			// Answer PTR record reverse mapping "<_service-name>._tcp.local." to
			// "<hostname>.<_service-name>._tcp.local."
			mdns_record_t answer = service->record_ptr;

			mdns_record_t additional[5] = { 0 };
			size_t additional_count = 0;

			// SRV record mapping "<hostname>.<_service-name>._tcp.local." to
			// "<hostname>.local." with port. Set weight & priority to 0.
			additional[additional_count++] = service->record_srv;

			// A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
			if (service->address_ipv4.sin_family == AF_INET)
				additional[additional_count++] = service->record_a;
			if (service->address_ipv6.sin6_family == AF_INET6)
				additional[additional_count++] = service->record_aaaa;

			// Add two test TXT records for our service instance name, will be coalesced into
			// one record with both key-value pair strings by the library
			additional[additional_count++] = service->txt_record[0];

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0,
					additional, additional_count);
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
					additional, additional_count);
			}
		}
	}
	else if ((name.length == service->service_instance.length) &&
		(strncmp(name.str, service->service_instance.str, name.length) == 0)) {
		if ((rtype == MDNS_RECORDTYPE_SRV) || (rtype == MDNS_RECORDTYPE_ANY)) {
			// The SRV query was for our service instance (usually
			// "<hostname>.<_service-name._tcp.local"), answer a SRV record mapping the service
			// instance name to our qualified hostname (typically "<hostname>.local.") and port, as
			// well as any IPv4/IPv6 address for the hostname as A/AAAA records, and two test TXT
			// records

			// Answer PTR record reverse mapping "<_service-name>._tcp.local." to
			// "<hostname>.<_service-name>._tcp.local."
			mdns_record_t answer = service->record_srv;

			mdns_record_t additional[5] = { 0 };
			size_t additional_count = 0;

			// A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
			if (service->address_ipv4.sin_family == AF_INET)
				additional[additional_count++] = service->record_a;
			if (service->address_ipv6.sin6_family == AF_INET6)
				additional[additional_count++] = service->record_aaaa;

			// Add two test TXT records for our service instance name, will be coalesced into
			// one record with both key-value pair strings by the library
			additional[additional_count++] = service->txt_record[0];

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0,
					additional, additional_count);
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
					additional, additional_count);
			}
		}
	}
	else if ((name.length == service->hostname_qualified.length) &&
		(strncmp(name.str, service->hostname_qualified.str, name.length) == 0)) {
		if (((rtype == MDNS_RECORDTYPE_A) || (rtype == MDNS_RECORDTYPE_ANY)) &&
			(service->address_ipv4.sin_family == AF_INET)) {
			// The A query was for our qualified hostname (typically "<hostname>.local.") and we
			// have an IPv4 address, answer with an A record mappiing the hostname to an IPv4
			// address, as well as any IPv6 address for the hostname, and two test TXT records

			// Answer A records mapping "<hostname>.local." to IPv4 address
			mdns_record_t answer = service->record_a;

			mdns_record_t additional[5] = { 0 };
			size_t additional_count = 0;

			// AAAA record mapping "<hostname>.local." to IPv6 addresses
			if (service->address_ipv6.sin6_family == AF_INET6)
				additional[additional_count++] = service->record_aaaa;

			// Add two test TXT records for our service instance name, will be coalesced into
			// one record with both key-value pair strings by the library
			additional[additional_count++] = service->txt_record[0];

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0,
					additional, additional_count);
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
					additional, additional_count);
			}
		}
		else if (((rtype == MDNS_RECORDTYPE_AAAA) || (rtype == MDNS_RECORDTYPE_ANY)) &&
			(service->address_ipv6.sin6_family == AF_INET6)) {
			// The AAAA query was for our qualified hostname (typically "<hostname>.local.") and we
			// have an IPv6 address, answer with an AAAA record mappiing the hostname to an IPv6
			// address, as well as any IPv4 address for the hostname, and two test TXT records

			// Answer AAAA records mapping "<hostname>.local." to IPv6 address
			mdns_record_t answer = service->record_aaaa;

			mdns_record_t additional[5] = { 0 };
			size_t additional_count = 0;

			// A record mapping "<hostname>.local." to IPv4 addresses
			if (service->address_ipv4.sin_family == AF_INET)
				additional[additional_count++] = service->record_a;

			// Add two test TXT records for our service instance name, will be coalesced into
			// one record with both key-value pair strings by the library
			additional[additional_count++] = service->txt_record[0];

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);			

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0,
					additional, additional_count);
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
					additional, additional_count);
			}
		}
	}
	return 0;
}
