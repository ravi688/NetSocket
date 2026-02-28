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

    clientSocket->setOnDisconnect([](netsocket::WebSocket& socket)
    {
        spdlog::info("OnDisconnect callback: socket has been closed");
    });

    spdlog::info("Connection accepted");

    std::this_thread::sleep_for(std::chrono::duration<float, std::ratio<1, 1>>(2));
    spdlog::info("Starting to send in 2 seconds");

    for(int i = 0; i < 1000; ++i)
    {
        spdlog::info("Sending data...");

        const char* data = "Hello World";
        result = clientSocket->send(reinterpret_cast<const u8*>(data), std::strlen(data));
        netsocket_assert(result == netsocket::Result::Success);
        bool isSuccess = clientSocket->send<u32>(static_cast<u32>(i));
        netsocket_assert(isSuccess);
    }

    spdlog::info("Closing client socket in 9 seconds");
    std::this_thread::sleep_for(std::chrono::duration<float, std::ratio<1, 1>>(9));
    result = clientSocket->close();
    netsocket_assert(result == netsocket::Result::Success);
    spdlog::info("Connection closed successfully");

    spdlog::info("Stopping the server in 5 seconds");
    std::this_thread::sleep_for(std::chrono::duration<float, std::ratio<1, 1>>(5));

    return 0;
}
