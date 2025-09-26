#include <iostream>
#include <netsocket/netsocket.hpp>
#undef _ASSERT
#include <spdlog/spdlog.h>

#include <netsocket/assert.hpp>

static constexpr std::string_view gIPAddress = "192.168.1.3";
static constexpr std::string_view gPortNumber = "8000";

int main()
{
	spdlog::info("NetSocket client");

	netsocket::Socket mySocket(netsocket::SocketType::Stream, 
								netsocket::IPAddressFamily::IPv4,
								netsocket::IPProtocol::TCP);

	spdlog::info("Connecting to {}:{}", gIPAddress, gPortNumber);
	netsocket::Result result = mySocket.connect(gIPAddress, gPortNumber);
	netsocket_assert((result == netsocket::Result::Success) && "Failed to connect");

	spdlog::info("Connection successful");

	return 0;
}
