#include <netsocket/websocket.hpp>
#include <netsocket/assert.hpp>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>

#include <cstring> // for std::memcpy

#include <iostream>

static CONSTRUCTOR_FUNCTION void InitializeWebSocket()
{
	ix::initNetSystem();
}

static DESTRUCTOR_FUNCTION void DeinitializeWebSocket()
{
	ix::uninitNetSystem();
}

namespace netsocket
{
	template<typename T>
	static void NoDeleter(T*) { }

	WebSocket::WebSocket() : m_serverSocket(nullptr, NoDeleter<ix::WebSocketServer>),
							m_clientSocket(nullptr, NoDeleter<ix::WebSocket>),
							m_isConnected(false),
							m_isError(false),
							m_hasReceiveData(false),
							m_isServerOwnedClient(false)
							
	{
	}
	
	WebSocket::~WebSocket()
	{
		close();
	}

	Result WebSocket::listen()
	{
		netsocket_assert(m_serverSocket.get() != nullptr);
		auto res = m_serverSocket->listen();
		if (!res.first)
		{
			std::cout << (res.second);
		    return Result::Failed;
		}
		m_serverSocket->start();
		return Result::Success;
	}
	
	std::unique_ptr<WebSocket> WebSocket::accept()
	{
		netsocket_assert(m_serverSocket && "WebSocket is not usable as server");
		return m_acceptedSockets->pop();
	}

	Result WebSocket::bind(const std::string_view ipAddress, const std::string_view portNumber)
	{
		auto iPortNumber = std::stoul(std::string { portNumber });
		m_serverSocket = MakeUnique<ix::WebSocketServer>(iPortNumber, std::string { ipAddress });

		m_serverSocket->setOnClientMessageCallback([this](std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket & webSocket, const ix::WebSocketMessagePtr & msg) {
    		// The ConnectionState object contains information about the connection,
    		// at this point only the client ip address and the port.
    		std::cout << "Remote ip: " << connectionState->getRemoteIp() << std::endl;

    		if (msg->type == ix::WebSocketMessageType::Open)
    		{
    		    std::cout << "New connection" << std::endl;

    		    auto acceptedSocket = CreateAcceptedSocket(webSocket);
    		    m_acceptedSockets->push(std::move(acceptedSocket));

        		// A connection state object is available, and has a default id
        		// You can subclass ConnectionState and pass an alternate factory
        		// to override it. It is useful if you want to store custom
        		// attributes per connection (authenticated bool flag, attributes, etc...)
        		std::cout << "id: " << connectionState->getId() << std::endl;

        		// The uri the client did connect to.
        		std::cout << "Uri: " << msg->openInfo.uri << std::endl;

        		std::cout << "Headers:" << std::endl;
        		for (auto it : msg->openInfo.headers)
        		{
        		    std::cout << "\t" << it.first << ": " << it.second << std::endl;
        		}
    		}
		});

		m_acceptedSockets = std::make_unique<com::ProducerConsumerBuffer<std::unique_ptr<WebSocket>>>();

		return Result::Success;
	}
	Result WebSocket::connect(const std::string_view ipAddress, const std::string_view port)
	{
		m_clientSocket = MakeUnique<ix::WebSocket>();
    	std::string url(std::format("ws://{}:{}", ipAddress, port));
    	m_clientSocket->setUrl(url);

    	m_clientSocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg)
    	{
    		this->processMessage(msg);
    	});

    	m_clientSocket->start();
    	std::unique_lock<std::mutex> lock(m_receiveMutex);
    	m_receiveCV.wait(lock, [this] { return this->m_isConnected || this->m_isError; });
    	if(this->m_isError)
    	{
    		close();
    		return Result::Failed;
    	}
    	else
    		return Result::Success;
	}

	Result WebSocket::close()
	{
		if(m_clientSocket)
		{
			std::unique_lock<std::mutex> lock(m_receiveMutex);
			// It is possible that the remote client has closed the connection
			// And the server thread destroyes the websocket instance associated the remote client connection
			// So, check whether the 
			if(!m_isConnected)
			{
				if(m_clientSocket)
					m_clientSocket.reset();
				return Result::Success;
			}
			m_clientSocket->close();			
			// Wait for the socket to be disconnected completely
			m_receiveCV.wait(lock, [this] { return !this->m_isConnected; });

			m_isServerOwnedClient = false;
			m_clientSocket.reset();
		}
		else if(m_serverSocket)
		{
			// NOTE: call to stop() over ix::WebSocketServer is not idempotent, it will fire redundant socket closure error if stop() is followed by destructor
			// m_serverSocket->stop();
			// Calling wait() blocks the calling thread forever, I checked the implementation of wait(), it just waits for nothing.
			// m_serverSocket->wait();
			m_serverSocket.reset();
		}

		return Result::Success;
	}

	Result WebSocket::send(const u8* bytes, u32 size)
	{
		if(!isConnected())
			return Result::SocketError;
		netsocket_debug_assert(m_clientSocket.get() != nullptr);
		ix::IXWebSocketSendData data(reinterpret_cast<const char*>(bytes), static_cast<size_t>(size));
		ix::WebSocketSendInfo result = m_clientSocket->sendBinary(data);
		if(result.success)
			return Result::Success;
		else
			return Result::Failed;
	}

	Result WebSocket::receive(u8* bytes, u32 size)
	{
		if(!isConnected())
			return Result::SocketError;
		netsocket_debug_assert(m_clientSocket.get() != nullptr);
		std::unique_lock<std::mutex> lock(m_receiveMutex);
		m_receiveCV.wait(lock, [this] { return this->m_hasReceiveData; });
		if(!isConnected())
			return Result::SocketError;
		if(m_receiveBuffer.size() != size)
		{
			m_receiveCV.notify_one();			
			return Result::Failed;
		}
		std::memcpy(bytes, m_receiveBuffer.data(), size);
		m_hasReceiveData = false;
		m_receiveCV.notify_one();
		return Result::Success;
	}

	Result WebSocket::finish()
	{
		// TODO
		return netsocket::Result::Success;
	}

	void WebSocket::callOnDisconnect()
	{
		if(m_onDisconnectCallback)
			m_onDisconnectCallback(*this);
	}

	void WebSocket::setOnDisconnect(const OnDisconnectCallback& callback)
	{
		m_onDisconnectCallback = callback;
	}

	std::unique_ptr<WebSocket> WebSocket::CreateAcceptedSocket(ix::WebSocket& ixWebSocket)
	{
		std::unique_ptr<WebSocket> webSocket = std::make_unique<WebSocket>();

		webSocket->m_isConnected = true;
		webSocket->m_isServerOwnedClient = true;
		auto webSocketPtr = UniquePtr<ix::WebSocket>(&ixWebSocket, NoDeleter<ix::WebSocket>);
    	webSocketPtr->setOnMessageCallback([rawPtr = webSocket.get()](const ix::WebSocketMessagePtr& msg)
    	{
    		rawPtr->processMessage(msg);
    	});
    	webSocket->m_clientSocket = std::move(webSocketPtr);
		return webSocket;
	}

	void WebSocket::postMessageInReceiveBuffer(const u8* const data, const u32 size)
	{
		std::unique_lock<std::mutex> lock(m_receiveMutex);
		m_receiveCV.wait(lock, [this] { return !m_hasReceiveData; });
		m_receiveBuffer.resize(size);
		std::memcpy(m_receiveBuffer.data(), data, size);
		m_hasReceiveData = true;
		m_receiveCV.notify_one();
	}

	void WebSocket::processMessage(const ix::WebSocketMessagePtr& msg)
	{
		if (msg->type == ix::WebSocketMessageType::Message)
		{
			const auto& str = msg->str;
			postMessageInReceiveBuffer(reinterpret_cast<const u8*>(str.data()), str.size());
		}
		else if (msg->type == ix::WebSocketMessageType::Open)
		{
		    m_isConnected = true;
		    m_receiveCV.notify_one();			
		}
		else if (msg->type == ix::WebSocketMessageType::Error)
		{
			m_isError = true;
			m_receiveCV.notify_one();
		}
		else if (msg->type == ix::WebSocketMessageType::Close)
		{
			// NOTE: Race condition:
			// Given that close() has been called (from destructor) and waiting over m_isConnected to become false,
			// It is possible that m_receiveCV would be destroyed as soon as m_isConnected is set to false and m_receiveCV.notify_one() would become garbage.
			// So, it is important here to keep the mutex locked until notify_one is called and then let the close() proceed further.
			std::lock_guard<std::mutex> lock(m_receiveMutex);
		    m_isConnected = false;
		    callOnDisconnect();
		    m_receiveCV.notify_one();
		}
	}
}
