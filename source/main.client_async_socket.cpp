#include <iostream>
#undef _ASSERT
#include <spdlog/spdlog.h>

#include <netsocket/netasyncsocket.hpp>
#include <netsocket/netinterface.hpp>
#include <netsocket/assert.hpp>

#include <cstring> // for std::memcmp
#include <mutex> // for std::mutex
#include <condition_variable> // for std::condition_variable

static constexpr std::string_view gPortNumber = "8000";

static std::mutex gMutex;
static std::condition_variable gCV;
static bool gIsReceiveDone = false;

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

	netsocket::AsyncSocket mySocket(netsocket::SocketType::Stream, 
								netsocket::IPAddressFamily::IPv4,
								netsocket::IPProtocol::TCP);

	spdlog::info("Connecting to {}:{}", ipAddress, gPortNumber);
	netsocket::Result result = mySocket.connect(ipAddress, gPortNumber);
	netsocket_assert((result == netsocket::Result::Success) && "Failed to connect");

	spdlog::info("Connection successful");

	spdlog::info("Receiving Data...");

	netsocket::AsyncSocket::BinaryFormatter formatter;
	formatter.add(netsocket::AsyncSocket::BinaryFormatter::Type::LengthU32);
	formatter.add(netsocket::AsyncSocket::BinaryFormatter::Type::Data);
	std::pair<std::mutex*, std::condition_variable*> _userData { &gMutex, &gCV };
	mySocket.receive([](const u8* bytes, u32 size, void* userData)
		{
			constexpr char* refData = "Hello World";
			constexpr u32 refDataLen = std::strlen(refData);
			netsocket_assert(size == (refDataLen + sizeof(u32)));
			u32 recvRefDataLen = *reinterpret_cast<const u32*>(bytes);
			netsocket_assert(recvRefDataLen == refDataLen);
			bool isEqual = std::memcmp(bytes + sizeof(u32), refData, refDataLen) == 0;
			netsocket_assert(isEqual);
			spdlog::info("Received data is correct");
			auto pair = *reinterpret_cast<std::pair<std::mutex*, std::condition_variable*>*>(userData);
			{
				std::lock_guard<std::mutex> lock(*reinterpret_cast<std::mutex*>(pair.first));
				gIsReceiveDone = true;
			}
			reinterpret_cast<std::condition_variable*>(pair.second)->notify_one();
		}, &_userData, formatter);

	std::unique_lock<std::mutex> lock(gMutex);
	gCV.wait(lock, [] { return gIsReceiveDone; });

	result = mySocket.close();
	netsocket_assert(result == netsocket::Result::Success);
	spdlog::info("Connection closed successfully");

	return 0;
}
