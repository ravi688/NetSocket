#pragma once

#include <netsocket/defines.hpp>
#include <netsocket/result.hpp> // for netsocket::Result enum
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

	#ifdef PLATFORM_WINDOWS
		using SocketHandle = SOCKET;
		#define NETSOCKET_INVALID_SOCKET_HANDLE INVALID_SOCKET // invalid socket handle
	#elif defined(PLATFORM_LINUX)
		using SocketHandle = int;
		#define NETSOCKET_INVALID_SOCKET_HANDLE -1 // invalid file descriptor
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

		Socket() : m_socket(NETSOCKET_INVALID_SOCKET_HANDLE), m_onDisconnect(NULL), m_userData(NULL) { }

		static Socket CreateAcceptedSocket(SocketHandle socket, int socketType, int ipAddressFamily, int ipProtocol)
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
			s.m_socket = NETSOCKET_INVALID_SOCKET_HANDLE;
			s.m_ipaFamily = 0;
			s.m_socketType = 0;
			s.m_ipProtocol = 0;
			s.m_isConnected = false;
			s.m_isValid = false;
			return s;
		}

		Socket(SocketType socketType, IPAddressFamily ipAddressFamily, IPProtocol ipProtocol);
		
		// Movable
		Socket(Socket&& socket);
		Socket& operator=(Socket&& socket);

		// Not copyable
		Socket(Socket& socket) = delete;
		
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


		template<typename T>
		bool sendNonEnum(const T& value)
		{
			return send(reinterpret_cast<const u8*>(&value), sizeof(value)) == Result::Success;
		}

		template<typename EnumClassType>
		bool sendEnum(const EnumClassType& value)
		{
			auto intValue = com::EnumClassToInt<EnumClassType>(value);
			return send<decltype(intValue)>(intValue);
		}

		template<typename T>
		bool send(const T& value)
		{
			if constexpr (std::is_enum<T>::value)
				return sendEnum<T>(value);
			else
				return sendNonEnum<T>(value);
		}

		template<typename T>
		std::optional<T> receiveNonEnum()
		{
			T value;
			auto result = receive(reinterpret_cast<u8*>(&value), sizeof(value));
			if(result == Result::Success)
				return { value };
			else
				return { };
		}

		template<typename EnumClassType>
		std::optional<EnumClassType> receiveEnum()
		{
			using IntType = typename std::underlying_type<EnumClassType>::type;
			auto intValue = receive<IntType>();
			if(intValue)
				return { com::IntToEnumClass<EnumClassType>(*intValue) };
			else
				return { };
		}

		template<typename T>
		std::optional<T> receive()
		{
			if constexpr (std::is_enum<T>::value)
				return receiveEnum<T>();
			else
				return receiveNonEnum<T>();
		}
	};
}
