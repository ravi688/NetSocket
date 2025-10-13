#include <iostream>
#include <netsocket/websocket.hpp>
#include <netsocket/assert.hpp>
#undef _ASSERT
#include <spdlog/spdlog.h>

static constexpr std::string_view gPortNumber = "8000";

int main()
{
    // Required on Windows
    ix::initNetSystem();


    netsocket::WebSocket mySocket;

    const std::string ipAddress = "127.0.0.1";

    spdlog::info("Connecting to {}:{}", ipAddress, gPortNumber);
    netsocket::Result result = mySocket.connect(ipAddress, gPortNumber);
    netsocket_assert((result == netsocket::Result::Success) && "Failed to connect");

    spdlog::info("Connection successful");

    spdlog::info("Receiving Data...");

    constexpr char* refData = "Hello World";
    constexpr u32 refDataLen = std::strlen(refData);
    char receiveBuffer[refDataLen];
    std::this_thread::sleep_for(std::chrono::seconds(2));
    result = mySocket.receive(reinterpret_cast<u8*>(receiveBuffer), refDataLen);
    netsocket_assert(result == netsocket::Result::Success);
    bool isEqual = std::memcmp(receiveBuffer, refData, refDataLen) == 0;
    netsocket_assert(isEqual);
    spdlog::info("Received data is correct");

    result = mySocket.close();
    netsocket_assert(result == netsocket::Result::Success);
    spdlog::info("Connection closed successfully");
  
    ix::uninitNetSystem();

    return 0;
}