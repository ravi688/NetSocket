#include <netsocket/netsocket.hpp>
#include <common/platform.h>
#include <common/debug.h>
#include <netsocket/assert.hpp>

#ifdef PLATFORM_WINDOWS
#	include <ws2tcpip.h>
#elif defined(PLATFORM_LINUX)
#	include <sys/socket.h>
#	include <arpa/inet.h> // for inet_addr
#	include <netdb.h> // for struct addrinfo
#	include <unistd.h> // for close
#	include <errno.h> // for errno
#	include <string.h> // for memset
#	define ZeroMemory(ptr, size) memset(ptr, 0, size) // on Linux ZeroMemory is not defined.
#else
#	error "Unsupported platform"
#endif

#ifdef PLATFORM_WINDOWS
#	define NETSOCKET_SOCKET_ERROR SOCKET_ERROR
#else
#	define NETSOCKET_SOCKET_ERROR (-1)
#	define INT int
#	define closesocket(socket_handle) ::close(socket_handle)
#endif

namespace netsocket
{
	#ifdef PLATFORM_WINDOWS
	CONSTRUCTOR_FUNCTION void InitializeWSA()
	{
		static WSADATA gWSAData;
		if(WSAStartup(MAKEWORD(2, 2), &gWSAData) != 0)
			debug_log_fetal_error("WSAStartup failed");
		debug_log_info("Initialized Windows Socket : Success");
	}

	DESTRUCTOR_FUNCTION void DeinitializeWSA()
	{
		WSACleanup();
		debug_log_info("Windows Socket uninitialized");
	}
	#endif // PLATFORM_WINDOWS

	static int GetWin32IPAddressFamily(IPAddressFamily ipaFamily)
	{
		switch(ipaFamily)
		{
			case IPAddressFamily::IPv4: return AF_INET;
			case IPAddressFamily::IPv6: return AF_INET6;
			default:
				debug_log_fetal_error("IPAddressFamily %lu is not supported as of now", ipaFamily);
		}
		return 0;
	}

	static int GetWin32SocketType(SocketType socketType)
	{
		switch(socketType)
		{
			case SocketType::Stream: return SOCK_STREAM;
			case SocketType::Raw: return SOCK_RAW;
			default:
				debug_log_fetal_error("SocketType %lu is not supported as of now", socketType);
		}
		return 0;
	}

	static int GetWin32IPProtocol(IPProtocol ipProtocol)
	{
		switch(ipProtocol)
		{
			case IPProtocol::TCP: return IPPROTO_TCP;
			case IPProtocol::UDP: return IPPROTO_UDP;
			// case IPProtocol::RM: return IPPROTO_RM;
			default:
				debug_log_fetal_error("IPProtocol %lu is not supported as of now", ipProtocol);
		}
		return 0;
	}

	Socket::Socket(SocketType socketType, IPAddressFamily ipAddressFamily, IPProtocol ipProtocol) : 
																						m_ipaFamily(GetWin32IPAddressFamily(ipAddressFamily)), 
																						m_socketType(GetWin32SocketType(socketType)), 
																						m_ipProtocol(GetWin32IPProtocol(ipProtocol)),
																						m_isConnected(false),
																						m_isValid(false),
																						m_onDisconnect(NULL),
																						m_userData(NULL)
	{
		m_socket = socket(m_ipaFamily, m_socketType, m_ipProtocol);

		if(m_socket == NETSOCKET_INVALID_SOCKET_HANDLE)
		{
#ifdef PLATFORM_WINDOWS
			debug_log_fetal_error("Unable to create socket, error code: %d", WSAGetLastError());
#else // PLATFORM_LINUX
      			debug_log_fetal_error("Unable to create socket, error: %s", strerror(errno));
#endif
		}

		m_isValid = true;
	}

	Socket::Socket(Socket&& socket) :
									m_socket(socket.m_socket),
									m_ipaFamily(socket.m_ipaFamily),
									m_socketType(socket.m_socketType),
									m_ipProtocol(socket.m_ipProtocol),
									m_isConnected(socket.m_isConnected),
									m_isValid(socket.m_isValid),
									m_onDisconnect(socket.m_onDisconnect),
									m_userData(socket.m_userData)
	{
		socket.m_socket = NETSOCKET_INVALID_SOCKET_HANDLE;
		socket.m_ipaFamily = 0;
		socket.m_socketType = 0;
		socket.m_ipProtocol = 0;
		socket.m_isConnected = false;
		socket.m_isValid = false;
		socket.m_onDisconnect = NULL;
		socket.m_userData = NULL;
	}

	Socket& Socket::operator=(Socket&& socket)
	{
		m_socket = socket.m_socket;
		m_ipaFamily = socket.m_ipaFamily;
		m_socketType = socket.m_socketType;
		m_ipProtocol = socket.m_ipProtocol;
		m_isConnected = socket.m_isConnected;
		m_isValid = socket.m_isValid;

		socket.m_socket = NETSOCKET_INVALID_SOCKET_HANDLE;
		socket.m_ipaFamily = 0;
		socket.m_socketType = 0;
		socket.m_ipProtocol = 0;
		socket.m_isConnected = false;
		socket.m_isValid = false;
		socket.m_onDisconnect = NULL;
		socket.m_userData = NULL;

		return *this;
	}

	Socket::~Socket()
	{
		if(m_isValid)
			close();
	}

	Result Socket::listen()
	{
		if(::listen(m_socket, SOMAXCONN) == NETSOCKET_SOCKET_ERROR)
			return Result::SocketError;
		return Result::Success;
	}

	std::optional<Socket> Socket::accept()
	{
		SocketHandle acceptedSocket = NETSOCKET_INVALID_SOCKET_HANDLE;
		acceptedSocket = ::accept(m_socket, NULL, NULL);
		if(acceptedSocket == NETSOCKET_INVALID_SOCKET_HANDLE)
			return { };
		return { Socket::CreateAcceptedSocket(acceptedSocket, m_socketType, m_ipaFamily, m_ipProtocol) };
	}

	Result Socket::bind(const std::string_view ipAddress, const std::string_view portNumber)
	{
		struct addrinfo hints;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = m_ipaFamily;
		hints.ai_socktype = m_socketType;
		hints.ai_protocol = m_ipProtocol;

		struct addrinfo* addressInfo = NULL;
		INT result = getaddrinfo(ipAddress.data(), portNumber.data(), &hints, &addressInfo);
		if(result != 0)
		{
			m_isValid = false;
			return Result::Failed;
		}

		netsocket_assert((addressInfo->ai_family == m_ipaFamily) && (addressInfo->ai_socktype == m_socketType) && (addressInfo->ai_protocol == m_ipProtocol));

		int opt = 1;
		setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		result = ::bind(m_socket, addressInfo->ai_addr, (int)addressInfo->ai_addrlen);

		freeaddrinfo(addressInfo);
		if(result == NETSOCKET_SOCKET_ERROR)
		{
			m_isValid = false;
			return Result::SocketError;
		}

		return Result::Success;			
	}

	Result Socket::connect(const std::string_view ipAddress, const std::string_view portNumber)
	{
		struct addrinfo hints;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = m_ipaFamily;
		hints.ai_socktype = m_socketType;
		hints.ai_protocol = m_ipProtocol;

		struct addrinfo* addressInfo = NULL;
		INT result = getaddrinfo(ipAddress.data(), portNumber.data(), &hints, &addressInfo);
		if(result != 0)
		{
			m_isValid = false;
			return Result::Failed;
		}

		netsocket_assert((addressInfo->ai_family == m_ipaFamily) && (addressInfo->ai_socktype == m_socketType) && (addressInfo->ai_protocol == m_ipProtocol));

		result = ::connect(m_socket, addressInfo->ai_addr, (int)addressInfo->ai_addrlen);

		freeaddrinfo(addressInfo);
		if(result == NETSOCKET_SOCKET_ERROR)
		{
			m_isValid = false;
			return Result::SocketError;
		}

		m_isConnected = true;
		return Result::Success;
	}

	Result Socket::close()
	{
		if(!m_isConnected)
			return Result::Success;

		if(closesocket(m_socket) == NETSOCKET_SOCKET_ERROR)
		{
			m_isValid = false;
			m_isConnected = false;
			callOnDisconnect();
			return Result::SocketError;
		}
		m_isValid = false;
		m_isConnected = false;
		callOnDisconnect();
		return Result::Success;
	}

	Result Socket::send(const u8* bytes, u32 size)
	{
		u32 numSentBytes = 0;
		while(numSentBytes < size)
		{
			int result = ::send(m_socket, reinterpret_cast<const char*>(bytes + numSentBytes), size - numSentBytes, 0);
			if(result == NETSOCKET_SOCKET_ERROR)
			{
				m_isValid = false;
				m_isConnected = false;
				callOnDisconnect();
				return Result::SocketError;
			}
			numSentBytes += static_cast<u32>(result);
		}
		// debug_log_info("Sent: %lu bytes", numSentBytes);
		return Result::Success;
	}

	Result Socket::receive(u8* bytes, u32 size)
	{
		if(!m_isConnected)
			return Result::SocketError;

		u32 numReceivedBytes = 0;
		while(numReceivedBytes < size)
		{
			int result = ::recv(m_socket, reinterpret_cast<char*>(bytes + numReceivedBytes), size - numReceivedBytes, 0);
			if(result == NETSOCKET_SOCKET_ERROR)
			{
				m_isValid = false;
				m_isConnected = false;
				callOnDisconnect();
				return Result::SocketError;
			}
			else if(result == 0)
			{
				m_isConnected = false;
				callOnDisconnect();
				return Result::SocketError;
			}
			numReceivedBytes += static_cast<u32>(result);
		}
		return Result::Success;
	}

	void Socket::callOnDisconnect()
	{
		if(m_onDisconnect == NULL)
			return;
		m_onDisconnect(*this, m_userData);
	}

	void Socket::setOnDisconnect(void (*onDisconnect)(Socket& socket, void* userData), void* userData)
	{
		m_onDisconnect = onDisconnect;
		m_userData = userData;
	}
}
