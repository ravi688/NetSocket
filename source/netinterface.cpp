#include <netsocket/netinterface.hpp>
#include <netsocket/assert.hpp>

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <ranges>
#include <string>
#include <sstream>

namespace netsocket
{
	std::string IPv4Address::str() const
	{
		std::stringstream stream;
		stream << static_cast<u32>(parts[0])
			<< "." << static_cast<u32>(parts[1])
			<< "." << static_cast<u32>(parts[2])
			<< "." << static_cast<u32>(parts[3]);
		return stream.str();
	}

	NETSOCKET_API std::vector<std::pair<std::string, IPv4Address>> GetInterfaceIPv4Addresses()
	{
		std::vector<std::pair<std::string, IPv4Address>> addresses;

		struct ifaddrs* ifaddr;
		char ip[INET_ADDRSTRLEN];

		int result = getifaddrs(&ifaddr);
		netsocket_assert(result != -1);

		for(struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
		{
			if(ifa->ifa_addr == NULL) continue;

			if(ifa->ifa_addr->sa_family == AF_INET)
			{
				struct sockaddr_in *addr = (struct sockaddr_in*)ifa->ifa_addr;
				inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
				std::string s { ip };
				IPv4Address ipAddress;
				for(u32 i = 0; auto part : std::views::split(s, '.'))
					ipAddress[i++] = std::stoul(std::string {part.begin(), part.end() });
				std::string ifName { ifa->ifa_name };
				addresses.push_back({ ifName, ipAddress });
			}
		}
		
		freeifaddrs(ifaddr);

		return addresses;
	}

	NETSOCKET_API std::string TrySelectingPhysicalInterfaceIPAddress(const std::vector<std::pair<std::string, IPv4Address>>& ipAddresses)
	{
		auto it = std::ranges::find_if(ipAddresses, [](const auto& pair)
                        {
				const std::string& interfaceName = pair.first;
#if PLATFORM_WINDOWS
                                return interfaceName.starts_with("Ethernet adapter") || interfaceName.starts_with("Wireless LAN");
#else // if PLATFORM_LINUX
                                return interfaceName.starts_with("en") ||
                                	interfaceName.starts_with("eth") ||
                                	interfaceName.starts_with("wl");
#endif
                        });
		std::string ipAddress;
		if(it != ipAddresses.end())
			ipAddress = it->second.str();
		else
			ipAddress = "127.0.0.1";
		return ipAddress;
	}
} 
