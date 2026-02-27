#pragma once

#include <common/defines.h>
#include <common/ProducerConsumerBuffer.hpp> // thread safe producer consumer buffer

#include <netsocket/defines.hpp>
#include <netsocket/result.hpp>

#include <ixwebsocket/IXWebSocketServer.h>

#include <mutex>
#include <vector>
#include <condition_variable>
#include <memory>

namespace netsocket
{
	class NETSOCKET_API WebSocket
	{
		template<typename T>
		using Deleter = void (*)(T*);
		template<typename T>
		using UniquePtr = std::unique_ptr<T, Deleter<T>>;
	private:
		UniquePtr<ix::WebSocketServer> m_serverSocket;
		UniquePtr<ix::WebSocket> m_clientSocket;
		std::unique_ptr<com::ProducerConsumerBuffer<std::unique_ptr<WebSocket>>> m_acceptedSockets;
		
		std::atomic<bool> m_isConnected;
		std::atomic<bool> m_isError;

		std::mutex m_receiveMutex;
		std::condition_variable m_receiveCV;
		bool m_hasReceiveData;
		std::vector<u8> m_receiveBuffer;

		// True if this socket is a client socket and on the server machine
		bool m_isServerOwnedClient;

		static std::unique_ptr<WebSocket> CreateAcceptedSocket(ix::WebSocket& webSocket);

		template<typename T>
		static void DefaultDeleter(T* v) { delete v; }

		template<typename T, typename... Args>
		static UniquePtr<T> MakeUnique(Args&&... args)
		{
			UniquePtr<T> ptr(new T(std::forward<Args>(args)...), DefaultDeleter<T>);
			return ptr;
		}

		void postMessageInReceiveBuffer(const u8* const data, const u32 size);
		void processMessage(const ix::WebSocketMessagePtr& msg);

	public:
		WebSocket();
		~WebSocket();
		bool isConnected() const noexcept { return m_isConnected && isValid(); }
		bool isValid() const noexcept { return m_serverSocket || m_clientSocket; }
		Result listen();
		std::unique_ptr<WebSocket> accept();
		Result bind(const std::string_view ipAddress, const std::string_view portNumber);
		Result connect(const std::string_view ipAddress, const std::string_view port);
		Result close();

		Result send(const u8* bytes, u32 size);
		Result receive(u8* bytes, u32 size);

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