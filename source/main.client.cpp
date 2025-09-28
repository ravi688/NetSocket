#include <iostream>
#undef _ASSERT
#include <spdlog/spdlog.h>

#include <netsocket/netsocket.hpp>
#include <netsocket/netinterface.hpp>
#include <netsocket/assert.hpp>

#include <cstring> // for std::memcmp

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

        std::string ipAddress = netsocket::TrySelectingPhysicalInterfaceIPAddress(ipAddresses, "192.168.1.1");

        spdlog::info("Selected IP address: {}", ipAddress);

	netsocket::Socket mySocket(netsocket::SocketType::Stream, 
								netsocket::IPAddressFamily::IPv4,
								netsocket::IPProtocol::TCP);

	spdlog::info("Connecting to {}:{}", ipAddress, gPortNumber);
	netsocket::Result result = mySocket.connect(ipAddress, gPortNumber);
	netsocket_assert((result == netsocket::Result::Success) && "Failed to connect");

	spdlog::info("Connection successful");

	spdlog::info("Receiving Data...");

	constexpr char* refData = "Hello World";
	constexpr u32 refDataLen = std::strlen(refData);
	char receiveBuffer[refDataLen];
	result = mySocket.receive(reinterpret_cast<u8*>(receiveBuffer), refDataLen);
	netsocket_assert(result == netsocket::Result::Success);
	bool isEqual = std::memcmp(receiveBuffer, refData, refDataLen) == 0;
	netsocket_assert(isEqual);
	spdlog::info("Received data is correct");

	return 0;
}
