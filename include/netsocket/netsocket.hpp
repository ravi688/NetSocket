#pragma once

#include <netsocket/defines.hpp>
#include <common/platform.h>
#include <common/defines.hpp>
#include <optional>

#ifdef PLATFORM_WINDOWS
#	include <winsock2.h>
#endif

namespace netsocket
{
	enum class IPAddressFamily : int
	{
		IPv4, /*AF_INET*/
		IPv6  /*AF_INET6*/
	};

	enum class SocketType : int
	{
		Stream,
		Raw
	};

	enum class IPProtocol : int
	{
		TCP,
		UDP,
		RM
	};

	enum class Result
	{
		Success = 0,
		Failed,
		SocketError
	};

	#ifdef PLATFORM_WINDOWS
		using SocketHandle = SOCKET;
	#elif PLATFORM_LINUX
		using SocketHandle = int;
	#endif // PLATFORM_LINUX

	class NETSOCKET_API Socket
	{
	private:
		SocketHandle m_socket;
		int m_ipaFamily;
		int m_socketType;
		int m_ipProtocol;
		bool m_isConnected;
		bool m_isValid;

		void (*m_onDisconnect)(Socket& socket, void* userData);
		void* m_userData;

		Socket() : m_socket(INVALID_SOCKET), m_onDisconnect(NULL), m_userData(NULL) { }

		static Socket CreateAcceptedSocket(SOCKET socket, int socketType, int ipAddressFamily, int ipProtocol)
		{
			Socket s;
			s.m_socket = socket;
			s.m_ipaFamily = ipAddressFamily;
			s.m_socketType = socketType;
			s.m_ipProtocol = ipProtocol;
			s.m_isConnected = true;
			s.m_isValid = true;
			return s;
		}

		void callOnDisconnect();

	public:

		static Socket CreateInvalid()
		{
			Socket s;
			s.m_socket = INVALID_SOCKET;
			s.m_ipaFamily = 0;
			s.m_socketType = 0;
			s.m_ipProtocol = 0;
			s.m_isConnected = false;
			s.m_isValid = false;
			return s;
		}

		Socket(SocketType socketType, IPAddressFamily ipAddressFamily, IPProtocol ipProtocol);
		Socket(Socket&& socket);
		Socket& operator=(Socket&& socket);
		Socket(Socket& socket) = delete;
		Socket& operator=(Socket& socket) = delete;
		~Socket();

		bool isConnected() const noexcept { return m_isConnected && isValid(); }
		bool isValid() const noexcept { return m_isValid; }
		Result listen();
		std::optional<Socket> accept();
		Result bind(const std::string_view ipAddress, const std::string_view portNumber);
		Result connect(const std::string_view ipAddress, const std::string_view port);
		Result close();

		Result send(const u8* bytes, u32 size);
		Result receive(u8* bytes, u32 size);

		void setOnDisconnect(void (*onDisconnect)(Socket& socket, void* userData), void* userData);
	};
}