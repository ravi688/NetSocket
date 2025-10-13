#include <netsocket/websocket.hpp>
#include <netsocket/assert.hpp>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXUserAgent.h>

#include <cstring> // for std::memcpy

#include <iostream>

namespace netsocket
{
	template<typename T>
	static void NoDeleter(T*) { }

	WebSocket::WebSocket() : m_serverSocket(nullptr, NoDeleter<ix::WebSocketServer>),
							m_clientSocket(nullptr, NoDeleter<ix::WebSocket>),
							m_isConnected(false),
							m_hasReceiveData(false)
							
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
		std::unique_lock<std::mutex> lock(this->m_receiveMutex);
		m_receiveCV.wait(lock, [this] { return m_acceptedSocket.get() != nullptr; });
		auto acceptedSocket = std::move(m_acceptedSocket);
		netsocket_debug_assert(m_acceptedSocket.get() == nullptr);
		m_receiveCV.notify_one();
		return acceptedSocket;
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

    		    std::unique_lock<std::mutex> lock(this->m_receiveMutex);
    		    this->m_receiveCV.wait(lock, [this] { return m_acceptedSocket.get() == nullptr; });
    		    m_acceptedSocket = CreateAcceptedSocket(webSocket);
    		    lock.unlock();
    		    this->m_receiveCV.notify_one();

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
    		else if (msg->type == ix::WebSocketMessageType::Message)
    		{
    		    // For an echo server, we just send back to the client whatever was received by the server
    		    // All connected clients are available in an std::set. See the broadcast cpp example.
    		    // Second parameter tells whether we are sending the message in binary or text mode.
    		    // Here we send it in the same mode as it was received.
    		    std::cout << "Received: " << msg->str << std::endl;
		
    		    webSocket.send(msg->str, msg->binary);
    		}
		});

		return Result::Success;
	}
	Result WebSocket::connect(const std::string_view ipAddress, const std::string_view port)
	{
		m_clientSocket = MakeUnique<ix::WebSocket>();
    	std::string url(std::format("ws://{}:{}", ipAddress, port));
    	m_clientSocket->setUrl(url);

    	bool isError = false;

    	m_clientSocket->setOnMessageCallback([&isError, this](const ix::WebSocketMessagePtr& msg)
    	{
    		this->processMessage(msg, &isError);
    	});

    	m_clientSocket->start();
    	std::unique_lock<std::mutex> lock(m_receiveMutex);
    	m_receiveCV.wait(lock, [&isError, this] { return this->m_isConnected || isError; });
    	if(isError)
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
			m_clientSocket->close();
			m_clientSocket->stop();
			m_clientSocket.reset();
		}
		else if(m_serverSocket)
		{
			m_serverSocket->stop();
			m_serverSocket->wait();
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
		m_receiveCV.notify_one();
		return Result::Success;
	}

	std::unique_ptr<WebSocket> WebSocket::CreateAcceptedSocket(ix::WebSocket& ixWebSocket)
	{
		std::unique_ptr<WebSocket> webSocket = std::make_unique<WebSocket>();

		webSocket->m_isConnected = true;
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
		m_receiveCV.notify_one();
	}

	void WebSocket::processMessage(const ix::WebSocketMessagePtr& msg, bool* isError)
	{
		if (msg->type == ix::WebSocketMessageType::Message)
		{
			const auto& str = msg->str;
			std::cout << "Message Received: " << str << std::endl;
			postMessageInReceiveBuffer(reinterpret_cast<const u8*>(str.data()), str.size());
			std::cout << "Message Posted" << std::endl;
		}
		else if (msg->type == ix::WebSocketMessageType::Open
				|| msg->type == ix::WebSocketMessageType::Error
				|| msg->type == ix::WebSocketMessageType::Close)
		{
			std::lock_guard<std::mutex> lock(m_receiveMutex);
			if(msg->type == ix::WebSocketMessageType::Open)
		    	m_isConnected = true;
			if(msg->type == ix::WebSocketMessageType::Error || msg->type == ix::WebSocketMessageType::Close)
			{
				if(isError)
					*isError = true;
		    	m_isConnected = false;
			}
			m_receiveCV.notify_one();
		}
	}
}