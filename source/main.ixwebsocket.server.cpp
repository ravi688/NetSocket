#include <iostream>
#undef _ASSERT
#include <spdlog/spdlog.h>

#include <netsocket/websocket.hpp>
#include <netsocket/netinterface.hpp>
#include <netsocket/assert.hpp>

static constexpr std::string_view gPortNumber = "8000";

int main()
{
    spdlog::info("NetSocket-WebSocket server");

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

    netsocket::WebSocket mySocket;

    netsocket::Result result = mySocket.bind(ipAddress, gPortNumber);
    netsocket_assert(result == netsocket::Result::Success);

    spdlog::info("Listening on {}:{}", ipAddress, gPortNumber);
    result = mySocket.listen();
    netsocket_assert((result == netsocket::Result::Success) && "Failed to listen");

    spdlog::info("Waiting to accept connection");
    std::unique_ptr<netsocket::WebSocket> clientSocket = mySocket.accept();
    netsocket_assert(clientSocket && "Failed to accept connection");
    netsocket_assert(clientSocket->isConnected());

    spdlog::info("Connection accepted");

    for(int i = 0; i < 1000; ++i)
    {
        spdlog::info("Sending data...");

        const char* data = "Hello World";
        result = clientSocket->send(reinterpret_cast<const u8*>(data), std::strlen(data));
        netsocket_assert(result == netsocket::Result::Success);
    }

    result = clientSocket->close();
    netsocket_assert(result == netsocket::Result::Success);
    spdlog::info("Connection closed successfully");

    return 0;
}
