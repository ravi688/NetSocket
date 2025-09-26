#include <netsocket/netinterface.hpp>
#include <netsocket/assert.hpp>

#ifdef PLATFORM_WINDOWS
#	include <winsock2.h>
#	include <iphlpapi.h>
#	include <ws2tcpip.h>
#else // PLATFORM_LINUX
#	include <ifaddrs.h>
#	include <arpa/inet.h>
#	include <netinet/in.h>
#endif

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

#ifdef PLATFORM_WINDOWS
		ULONG outBufLen = 15000; // large enough buffer
		PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
		if(!pAddresses)
		    throw std::runtime_error("Memory allocation failed for IP_ADAPTER_ADDRESSES");

		DWORD dwRet = GetAdaptersAddresses(AF_INET, 0, nullptr, pAddresses, &outBufLen);
		if(dwRet != NO_ERROR)
		{
    		free(pAddresses);
    		throw std::runtime_error("GetAdaptersAddresses failed");
		}

    	for(PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != nullptr; pCurr = pCurr->Next)
    	{
        	for(PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress; pUnicast != nullptr; pUnicast = pUnicast->Next) 
        	{
        	    SOCKADDR* addr = pUnicast->Address.lpSockaddr;
        	    if(addr->sa_family == AF_INET)
        	    {
        	        char ip[INET_ADDRSTRLEN];
        	        inet_ntop(AF_INET, &((struct sockaddr_in*)addr)->sin_addr, ip, sizeof(ip));

					std::string s { ip };
					IPv4Address ipAddress;
					for(uint32_t i = 0; auto part : std::views::split(s, '.'))
						ipAddress[i++] = std::stoul(std::string { part.begin(), part.end() });

					// Convert wide FriendlyName -> narrow string
					std::wstring wname = pCurr->FriendlyName;
					std::string ifName(wname.begin(), wname.end());

					addresses.emplace_back(ifName, ipAddress);
				}
			}
		}
		free(pAddresses);
#else // PLATFORM_LINUX
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
#endif
		return addresses;
	}

	NETSOCKET_API std::string TrySelectingPhysicalInterfaceIPAddress(const std::vector<std::pair<std::string, IPv4Address>>& ipAddresses)
	{
		auto it = std::ranges::find_if(ipAddresses, [](const auto& pair)
                        {
				const std::string& interfaceName = pair.first;
#ifdef PLATFORM_WINDOWS
                                return interfaceName.starts_with("Ethernet") ||
                                	interfaceName.starts_with("Wireless LAN") ||
                                	interfaceName.starts_with("Network Bridge");
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
