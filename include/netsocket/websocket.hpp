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
		bool m_isError;

		std::mutex m_receiveMutex;
		std::condition_variable m_receiveCV;
		bool m_hasReceiveData;
		std::vector<u8> m_receiveBuffer;

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
	};
}