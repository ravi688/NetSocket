#include <iostream>
#undef _ASSERT
#include <spdlog/spdlog.h>

#include <netsocket/netsocket.hpp>
#include <netsocket/netinterface.hpp>
#include <netsocket/assert.hpp>

static constexpr std::string_view gPortNumber = "8000";

int main()
{
	spdlog::info("NetSocket server");

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

	netsocket::Result result = mySocket.bind(ipAddress, gPortNumber);
	netsocket_assert(result == netsocket::Result::Success);

	spdlog::info("Listening on {}:{}", ipAddress, gPortNumber);
	result = mySocket.listen();
	netsocket_assert((result == netsocket::Result::Success) && "Failed to listen");

	std::optional<netsocket::Socket> clientSocket = mySocket.accept();
	netsocket_assert((result == netsocket::Result::Success) && "Failed to accept connection");
	netsocket_assert(clientSocket->isConnected());

	spdlog::info("Connection accepted");

	spdlog::info("Sending data...");

	const char* data = "Hello World";
	result = clientSocket->send(reinterpret_cast<const u8*>(data), std::strlen(data));
	netsocket_assert(result == netsocket::Result::Success);

	result = clientSocket->close();
	netsocket_assert(result == netsocket::Result::Success);
	spdlog::info("Connection closed successfully");

	return 0;
}
