#include <iostream>
#undef _ASSERT
#include <spdlog/spdlog.h>

#include <netsocket/netsocket.hpp>
#include <netsocket/netinterface.hpp>
#include <netsocket/assert.hpp>

static constexpr std::string_view gPortNumber = "8000";

int main()
{
	spdlog::info("NetSocket client");

        std::vector<std::pair<std::string, netsocket::IPv4Address>> ipAddresses = netsocket::GetInterfaceIPv4Addresses();
        spdlog::info("Following IPv4 Addresses have been assigned to interfaces on this machine:");
        for(std::uint32_t index = 0; const auto& pair : ipAddresses)
        {
                const auto ipAddress = pair.second;
                spdlog::info("\t[{}] {} -> {}.{}.{}.{}", index, pair.first, ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
                ++index;
        }

        std::string ipAddress = netsocket::TrySelectingPhysicalInterfaceIPAddress(ipAddresses);

        spdlog::info("Selected IP address: {}", ipAddress);

	netsocket::Socket mySocket(netsocket::SocketType::Stream, 
								netsocket::IPAddressFamily::IPv4,
								netsocket::IPProtocol::TCP);

	spdlog::info("Connecting to {}:{}", ipAddress, gPortNumber);
	netsocket::Result result = mySocket.connect(ipAddress, gPortNumber);
	netsocket_assert((result == netsocket::Result::Success) && "Failed to connect");

	spdlog::info("Connection successful");

	return 0;
}
