#include <iostream>
#undef _ASSERT
#include <spdlog/spdlog.h>

#include <netsocket/websocket.hpp>
#include <netsocket/netinterface.hpp>
#include <netsocket/assert.hpp>

static constexpr std::string_view gPortNumber = "8000";

int main()
{
    // Required on Windows
    ix::initNetSystem();


    spdlog::info("NetSocket server");

    std::string ipAddress = "127.0.0.1";

    spdlog::info("Selected IP address: {}", ipAddress);

    netsocket::WebSocket mySocket;

    netsocket::Result result = mySocket.bind(ipAddress, gPortNumber);
    netsocket_assert(result == netsocket::Result::Success);

    spdlog::info("Listening on {}:{}", ipAddress, gPortNumber);
    result = mySocket.listen();
    netsocket_assert((result == netsocket::Result::Success) && "Failed to listen");

    std::unique_ptr<netsocket::WebSocket> clientSocket = mySocket.accept();
    netsocket_assert(clientSocket && "Failed to accept connection");
    netsocket_assert(clientSocket->isConnected());

    spdlog::info("Connection accepted");

    spdlog::info("Sending data...");

    const char* data = "Hello World";
    result = clientSocket->send(reinterpret_cast<const u8*>(data), std::strlen(data));
    netsocket_assert(result == netsocket::Result::Success);

    result = clientSocket->close();
    netsocket_assert(result == netsocket::Result::Success);
    spdlog::info("Connection closed successfully");

    ix::uninitNetSystem();

    return 0;
}
