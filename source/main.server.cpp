#include <iostream>
#include <netsocket/netsocket.hpp>
#undef _ASSERT
#include <spdlog/spdlog.h>

#include <netsocket/assert.hpp>

static constexpr std::string_view gIPAddress = "192.168.1.3";
static constexpr std::string_view gPortNumber = "8000";

int main()
{
	spdlog::info("NetSocket server");

	netsocket::Socket mySocket(netsocket::SocketType::Stream, 
								netsocket::IPAddressFamily::IPv4,
								netsocket::IPProtocol::TCP);

	netsocket::Result result = mySocket.bind(gIPAddress, gPortNumber);
	netsocket_assert(result == netsocket::Result::Success);

	spdlog::info("Listening on {}:{}", gIPAddress, gPortNumber);
	result = mySocket.listen();
	netsocket_assert((result == netsocket::Result::Success) && "Failed to listen");

	std::optional<netsocket::Socket> clientSocket = mySocket.accept();
	netsocket_assert((result == netsocket::Result::Success) && "Failed to accept connection");
	netsocket_assert(clientSocket->isConnected());

	spdlog::info("Connection accepted");

	return 0;
}
