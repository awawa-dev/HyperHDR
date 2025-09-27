#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <bonjour/DiscoveryRecord.h>
#include <bonjour/BonjourServiceHelper.h>
#include <bonjour/BonjourServiceRegister.h>
#include <mdns.h>

#include <utils/Logger.h>
#include <HyperhdrConfig.h>

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

int BonjourServiceHelper::_instanceCount = 0;

BonjourServiceHelper::BonjourServiceHelper(BonjourServiceRegister* parent, QString service, int port) :
	QThread(parent)
{
	_port = port;
	_register = parent;
	_service = service;
	has_ipv4 = 0;
	has_ipv6 = 0;
	_scanService = (1 << DiscoveryRecord::Service::HyperHDR) |
		(1 << DiscoveryRecord::Service::WLED) |
		(1 << DiscoveryRecord::Service::PhilipsHue) |
		(1 << DiscoveryRecord::Service::HomeAssistant);

	_running = true;

	memset(&service_address_ipv4, 0, sizeof(service_address_ipv4));
	memset(&service_address_ipv6, 0, sizeof(service_address_ipv6));

#ifdef _WIN32
	WORD versionWanted = MAKEWORD(1, 1);
	WSADATA wsaData;


	if (_instanceCount++ == 0 && WSAStartup(versionWanted, &wsaData))
	{
		printf("Failed to initialize WinSock\n");
	}

	char hostname_buffer[256];
	DWORD hostname_size = (DWORD)sizeof(hostname_buffer);
	if (GetComputerNameA(hostname_buffer, &hostname_size))
		_hostname = QString::fromLocal8Bit(hostname_buffer);
#else
	char hostname_buffer[256];
	size_t hostname_size = sizeof(hostname_buffer);
	if (gethostname(hostname_buffer, hostname_size) == 0)
		_hostname = QString::fromLocal8Bit(hostname_buffer);
#endif
}

BonjourServiceHelper::~BonjourServiceHelper()
{
#ifdef _WIN32
	if (--_instanceCount == 0)
		WSACleanup();
#endif
}

void BonjourServiceHelper::run()
{
	if (_register != nullptr)
	{
		service_mdns(_hostname, (_service + ".local."), _port);
	}
}


QString BonjourServiceHelper::ip_address_to_string(const struct sockaddr_in* addr, size_t addrlen)
{
	QString result;
	char host[NI_MAXHOST] = {};
	char service[NI_MAXSERV] = {};
	int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
		service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
	if (ret == 0) {
		if (addr->sin_port != 0)
			result = QString("%1:%2").arg(QString::fromLocal8Bit(host)).arg(QString::fromLocal8Bit(service));
		else
			result = QString("%1").arg(QString::fromLocal8Bit(host));
	}

	return result;
}

QString BonjourServiceHelper::ip_address_to_string(const struct sockaddr_in6* addr, size_t addrlen)
{
	QString result;
	char host[NI_MAXHOST] = {};
	char service[NI_MAXSERV] = {};
	int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
		service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
	if (ret == 0) {
		if (addr->sin6_port != 0)
			result = QString("[%1]:%2").arg(QString::fromLocal8Bit(host)).arg(QString::fromLocal8Bit(service));
		else
			result = QString("%1").arg(QString::fromLocal8Bit(host));
	}

	return result;
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
						QString log = ip_address_to_string(saddr, sizeof(struct sockaddr_in));
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
						QString log = ip_address_to_string(saddr, sizeof(struct sockaddr_in6));
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
					QString addr = ip_address_to_string(saddr, sizeof(struct sockaddr_in));
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
					QString addr = ip_address_to_string(saddr, sizeof(struct sockaddr_in6));
				}
			}
		}
	}

	freeifaddrs(ifaddr);

#endif

	return num_sockets;
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

int BonjourServiceHelper::service_mdns(QString hostname, QString serviceName, int service_port)
{
	Logger* _log = Logger::getInstance("SERVICE_mDNS");
	int sockets[32];
	int num_sockets = open_service_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]));

	if (num_sockets <= 0)
	{
		Error(_log, "Failed to open any client sockets");
		return -1;
	}

	Info(_log, "Starting the network discovery thread");

	QByteArray mainBuffer(2048, 0);
	const QByteArray& serviceNameBuf = serviceName.toLocal8Bit();
	const QByteArray& hostnameBuf = hostname.toLocal8Bit();

	mdns_string_t service_string = { serviceNameBuf.data(), strlen(serviceNameBuf.data()) };
	mdns_string_t hostname_string = { hostnameBuf.data(), strlen(hostnameBuf.data()) };

	// Build the service instance "<hostname>.<_service-name>._tcp.local." string	
	const QByteArray& serviceInstanceBuf = QString::asprintf("%.*s:%i.%.*s", MDNS_STRING_FORMAT(hostname_string), service_port, MDNS_STRING_FORMAT(service_string)).toLocal8Bit();
	mdns_string_t service_instance_string = { serviceInstanceBuf.data(), strlen(serviceInstanceBuf.data()) };

	// Build the "<hostname>.local." string
	const QByteArray& qualifiedHostnameBuf = QString::asprintf("%.*s.local.", MDNS_STRING_FORMAT(hostname_string)).toLocal8Bit();
	mdns_string_t hostname_qualified_string = { qualifiedHostnameBuf.data(), strlen(qualifiedHostnameBuf.data()) };

	service_t service = {};

	service.parent = this;

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
	service.record_ptr = {};
	service.record_ptr.name = service.service;
	service.record_ptr.type = MDNS_RECORDTYPE_PTR;
	service.record_ptr.data.ptr.name = service.service_instance;
	service.record_ptr.rclass = 0;
	service.record_ptr.ttl = 0;

	// SRV record mapping "<hostname>.<_service-name>._tcp.local." to
	// "<hostname>.local." with port. Set weight & priority to 0.
	service.record_srv = {};
	service.record_srv.name = service.service_instance;
	service.record_srv.type = MDNS_RECORDTYPE_SRV;
	service.record_srv.data.srv.name = service.hostname_qualified;
	service.record_srv.data.srv.port = service.port;
	service.record_srv.data.srv.priority = 0;
	service.record_srv.data.srv.weight = 0;
	service.record_srv.rclass = 0;
	service.record_srv.ttl = 0;

	// A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
	service.record_a = {};
	service.record_a.name = service.hostname_qualified;
	service.record_a.type = MDNS_RECORDTYPE_A;
	service.record_a.data.a.addr = service.address_ipv4;
	service.record_a.rclass = 0;
	service.record_a.ttl = 0;

	service.record_aaaa = {};
	service.record_aaaa.name = service.hostname_qualified;
	service.record_aaaa.type = MDNS_RECORDTYPE_AAAA;
	service.record_aaaa.data.aaaa.addr = service.address_ipv6;
	service.record_aaaa.rclass = 0;
	service.record_aaaa.ttl = 0;

	// Add two test TXT records for our service instance name, will be coalesced into
	// one record with both key-value pair strings by the library
	service.txt_record[0] = {};
	service.txt_record[0].name = service.service_instance;
	service.txt_record[0].type = MDNS_RECORDTYPE_TXT;
	service.txt_record[0].data.txt.key = { MDNS_STRING_CONST("version") };
	service.txt_record[0].data.txt.value = { MDNS_STRING_CONST(HYPERHDR_VERSION) };
	service.txt_record[0].rclass = 0;
	service.txt_record[0].ttl = 0;

	// Send an announcement on startup of service
	std::vector<mdns_record_t> serviceMessage;

	serviceMessage.push_back(service.record_srv);
	if (service.address_ipv4.sin_family == AF_INET)
		serviceMessage.push_back(service.record_a);
	if (service.address_ipv6.sin6_family == AF_INET6)
		serviceMessage.push_back(service.record_aaaa);
	serviceMessage.push_back(service.txt_record[0]);

	for (int i = 0; i < num_sockets; i++)
		mdns_announce_multicast(sockets[i], mainBuffer.data(), mainBuffer.length(), service.record_ptr, 0, 0,
			serviceMessage.data(), serviceMessage.size());


	// This is a crude implementation that checks for incoming queries
	while (_running) {
		int nfds = 0;
		fd_set readfs;

		FD_ZERO(&readfs);
		for (int i = 0; i < num_sockets; i++)
		{
			if (sockets[i] >= nfds)
				nfds = sockets[i] + 1;
			FD_SET(sockets[i], &readfs);
		}

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;

		if (select(nfds, &readfs, 0, 0, &timeout) >= 0)
		{
			for (int i = 0; i < num_sockets; i++)
			{
				if (FD_ISSET(sockets[i], &readfs))
				{
					mdns_socket_listen(sockets[i], mainBuffer.data(), mainBuffer.length(), serviceCallback, &service);
				}
				FD_SET(sockets[i], &readfs);
			}
		}
		else
		{
			Error(_log, "Error while monitoring network socket descriptors");
			break;
		}

		if (_scanService != 0)
		{
			for (auto scanner :{ DiscoveryRecord::Service::HyperHDR,
							DiscoveryRecord::Service::WLED,
							DiscoveryRecord::Service::PhilipsHue,
							DiscoveryRecord::Service::HomeAssistant })
			{
				if (_scanService & (1 << scanner))
				{
					_scanService &= ~(1 << scanner);

					const QByteArray& name = (DiscoveryRecord::getmDnsHeader(scanner) + ".local").toLocal8Bit();
					mdns_query_t serviceQuery{ MDNS_RECORDTYPE_PTR, name.data(), strlen(name.data()) };

					for (int i = 0; i < num_sockets; i++)
						mdns_multiquery_send(sockets[i], &serviceQuery, 1, mainBuffer.data(), mainBuffer.length(), 0);
				}
			}
			_scanService = 0;
		}
	}

	// Send goodbye
	Info(_log, "Sending goodbye");

	for (int i = 0; i < num_sockets; i++)
		mdns_goodbye_multicast(sockets[i], mainBuffer.data(), mainBuffer.length(), service.record_ptr, 0, 0,
			serviceMessage.data(), serviceMessage.size());

	// free resources
	for (int i = 0; i < num_sockets; i++)
		mdns_socket_close(sockets[i]);

	Info(_log, "The thread exits now");

	return 0;
}

int BonjourServiceHelper::serviceCallback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
	uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
	size_t size, size_t name_offset, size_t /*name_length*/, size_t record_offset,
	size_t record_length, void* user_data)
{
	(void)sizeof(ttl);

	if (entry != MDNS_ENTRYTYPE_QUESTION && rtype != MDNS_RECORDTYPE_SRV && rtype != MDNS_RECORDTYPE_A)
		return 0;


	char namebuffer[256];
	char sendbuffer[1024];
	const service_t* service = (const service_t*)user_data;
	size_t offset = name_offset;
	mdns_string_t name = mdns_string_extract(data, size, &offset, namebuffer, sizeof(namebuffer));

	if (entry != MDNS_ENTRYTYPE_QUESTION)
	{
		if (rtype == MDNS_RECORDTYPE_SRV) {
			mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length,
				sendbuffer, sizeof(sendbuffer));

			if (user_data != nullptr)
			{
				BonjourServiceRegister* receive = service->parent->_register;
				emit receive->SignalMessageFromFriend(ttl > 0, QString::fromLocal8Bit(name.str, (int) name.length), QString::fromLocal8Bit(srv.name.str, (int) srv.name.length), srv.port);
			}
		}
		else if (rtype == MDNS_RECORDTYPE_A) {
			struct sockaddr_in addr = {};
			mdns_record_parse_a(data, size, record_offset, record_length, &addr);
			QString addrstr = ip_address_to_string(&addr, sizeof(addr));

			if (user_data != nullptr)
			{
				BonjourServiceRegister* receive = service->parent->_register;
				emit receive->SignalIpResolved(QString::fromLocal8Bit(name.str, (int) name.length), addrstr);
			}
		}
		return 0;
	}


	const char dns_sd[] = "_services._dns-sd._udp.local.";

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
			mdns_record_t answer = {};
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

			std::vector<mdns_record_t> additional;

			// SRV record mapping "<hostname>.<_service-name>._tcp.local." to
			// "<hostname>.local." with port. Set weight & priority to 0.
			additional.push_back(service->record_srv);

			// A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
			if (service->address_ipv4.sin_family == AF_INET)
				additional.push_back(service->record_a);
			if (service->address_ipv6.sin6_family == AF_INET6)
				additional.push_back(service->record_aaaa);

			// Add two test TXT records for our service instance name, will be coalesced into
			// one record with both key-value pair strings by the library
			additional.push_back(service->txt_record[0]);

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0,
					additional.data(), additional.size());
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
					additional.data(), additional.size());
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

			std::vector<mdns_record_t> additional;

			// A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
			if (service->address_ipv4.sin_family == AF_INET)
				additional.push_back(service->record_a);
			if (service->address_ipv6.sin6_family == AF_INET6)
				additional.push_back(service->record_aaaa);

			// Add two test TXT records for our service instance name, will be coalesced into
			// one record with both key-value pair strings by the library
			additional.push_back(service->txt_record[0]);

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0,
					additional.data(), additional.size());
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
					additional.data(), additional.size());
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

			std::vector<mdns_record_t> additional;

			// AAAA record mapping "<hostname>.local." to IPv6 addresses
			if (service->address_ipv6.sin6_family == AF_INET6)
				additional.push_back(service->record_aaaa);

			// Add two test TXT records for our service instance name, will be coalesced into
			// one record with both key-value pair strings by the library
			additional.push_back(service->txt_record[0]);

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0,
					additional.data(), additional.size());
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
					additional.data(), additional.size());
			}
		}
		else if (((rtype == MDNS_RECORDTYPE_AAAA) || (rtype == MDNS_RECORDTYPE_ANY)) &&
			(service->address_ipv6.sin6_family == AF_INET6)) {
			// The AAAA query was for our qualified hostname (typically "<hostname>.local.") and we
			// have an IPv6 address, answer with an AAAA record mappiing the hostname to an IPv6
			// address, as well as any IPv4 address for the hostname, and two test TXT records

			// Answer AAAA records mapping "<hostname>.local." to IPv6 address
			mdns_record_t answer = service->record_aaaa;

			std::vector<mdns_record_t> additional;

			// A record mapping "<hostname>.local." to IPv4 addresses
			if (service->address_ipv4.sin_family == AF_INET)
				additional.push_back(service->record_a);

			// Add two test TXT records for our service instance name, will be coalesced into
			// one record with both key-value pair strings by the library
			additional.push_back(service->txt_record[0]);

			// Send the answer, unicast or multicast depending on flag in query
			uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);

			if (unicast) {
				mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
					query_id, (mdns_record_type)rtype, name.str, name.length, answer, 0, 0,
					additional.data(), additional.size());
			}
			else {
				mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
					additional.data(), additional.size());
			}
		}
	}
	return 0;
}
