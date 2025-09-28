#include <netsocket/netasyncsocket.hpp>

#undef _ASSERT
#include <spdlog/spdlog.h>

#include <functional>

namespace netsocket
{
	AsyncSocket::Transxn::Transxn(const u8* data, u32 dataSize, Type type) : m_type(type), m_isValid(false)
	{
		netsocket_assert(type == Type::Send);
		m_buffer = buf_create(sizeof(u8), dataSize, 0);
		buf_push_pseudo(&m_buffer, dataSize);
		void* ptr = buf_get_ptr(&m_buffer);
		memcpy(ptr, reinterpret_cast<void*>(const_cast<u8*>(data)), dataSize);
		m_isValid = true;
	}

	AsyncSocket::Transxn::Transxn(ReceiveCallbackHandler receiveHandler, void* userData, BinaryFormatter& receiveFormatter, Type type) : 
																																			  m_receiveHandler(receiveHandler),
																																			  m_userData(userData), 
																																			  m_type(type), 
																																			  m_receiveFormatter(receiveFormatter), 
																																			  m_isValid(false)
	{
		netsocket_assert(type == Type::Receive);
		m_buffer = buf_create(sizeof(u8), 0, 0);
		m_isValid = true;
	}
	
	AsyncSocket::Transxn::Transxn(Transxn&& transxn) : 
														 	m_receiveHandler(transxn.m_receiveHandler),
														 	m_userData(transxn.m_userData),
															m_type(transxn.m_type),
															m_receiveFormatter(transxn.m_receiveFormatter),
															m_isValid(transxn.m_isValid)
	{
		buf_move(&transxn.m_buffer, &m_buffer);
		transxn.m_isValid = false;
	}

	AsyncSocket::Transxn::~Transxn()
	{
		if(!m_isValid)
			return;
		netsocket_assert(m_isValid);
		buf_free(&m_buffer);
		m_isValid = false;
	}
	
	u8* AsyncSocket::Transxn::getBufferPtr() { return buf_get_ptr_typeof(&m_buffer, u8); }
	u32 AsyncSocket::Transxn::getBufferSize() { return static_cast<u32>(buf_get_element_count(&m_buffer)); }
	
	bool AsyncSocket::Transxn::doit(Socket& socket)
	{
		switch(m_type)
		{
			case Type::Send:
			{
				// debug_log_info("Sending Data...");
				return socket.send(buf_get_ptr_typeof(&m_buffer, u8), static_cast<u32>(buf_get_element_count(&m_buffer))) == Result::Success;
			}
			case Type::Receive:
			{
				// debug_log_info("Receiving Data...");
				std::pair<Socket*, decltype(this)> socketTransxnPair { &socket, this };

				bool result = m_receiveFormatter->format([](u32 size, void* userData) -> void* 
														{
															auto* pair = reinterpret_cast<std::pair<Socket*, Transxn*>*>(userData);
															Socket* socket = pair->first;
															Transxn* transxn = pair->second;
															auto index = buf_get_element_count(&transxn->m_buffer);
															buf_push_pseudo(&transxn->m_buffer, size);
															void* ptr = buf_get_ptr_at(&transxn->m_buffer, index);
															netsocket_assert(ptr != NULL);
															bool result = socket->receive(reinterpret_cast<u8*>(ptr), size) == netsocket::Result::Success;
															if(!result)
															{
																spdlog::error("Failed to receive from socket");
																return NULL;
															}
															return ptr;
														}, reinterpret_cast<void*>(&socketTransxnPair));

				netsocket_assert(m_receiveHandler != NULL);
				if(result)
					m_receiveHandler(getBufferPtr(), getBufferSize(), m_userData);
				else
					m_receiveHandler(NULL, getBufferSize(), m_userData);
				return result;
			}
			default:
			{
				spdlog::error("Unrecognized Transxn::Type: {}", com::EnumClassToInt(m_type));
				break;
			}
		}
		return false;
	}
	
	AsyncSocket::AsyncSocket(Socket&& socket) : AsyncSocket(*m_socket)
	{
		m_socket = std::move(socket);
	}

	AsyncSocket::AsyncSocket(Socket& socket) : m_socketRef(socket), m_isValid(false), m_isCanSendOrReceive(false)
	{
		m_isValid = true; 
		if(m_socketRef.isConnected())
		{
			m_isCanSendOrReceive = true;
			m_isStopThread = false;
			m_thread = std::move(std::unique_ptr<std::thread>(new std::thread(&AsyncSocket::threadHandler, this)));
		}
		else 
			m_isStopThread = true;
	}

	AsyncSocket::~AsyncSocket()
	{
		if(m_isValid)
			close();
	}
	
	Result AsyncSocket::connect(const char* ipAddress, const char* port)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		netsocket_assert(!m_socketRef.isConnected() && "Already connected AsyncSocket, first close it");
		auto result = m_socketRef.connect(ipAddress, port);
		if(result == Result::Success)
		{
			m_isCanSendOrReceive = true;
			m_isStopThread = false;
			m_thread = std::unique_ptr<std::thread>(new std::thread(&AsyncSocket::threadHandler, this));
		}
		return result;
	}
	
	Result AsyncSocket::close()
	{
		Result result = Result::Success;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if(m_socket.has_value())
			{
				result = m_socketRef.close();
				if(result != Result::Success)
					return result;
			}
		}
		if(m_thread)
		{
			m_isStopThread = true;
			m_dataAvailableCV.notify_one();
			if(m_thread->joinable())
				m_thread->join();
		}
		return result;
	}
	
	void AsyncSocket::send(const u8* bytes, u32 size)
	{
		Transxn transxn(bytes, size, Transxn::Type::Send);
		std::unique_lock<std::mutex> lock(m_mutex);
		m_transxnQueue.push_front(std::move(transxn));
		lock.unlock();
		m_dataAvailableCV.notify_one();
	}
	
	void AsyncSocket::receive(Transxn::ReceiveCallbackHandler receiveHandler, void* userData, BinaryFormatter& receiveFormatter)
	{
		Transxn transxn(receiveHandler, userData, receiveFormatter, Transxn::Type::Receive);
		std::unique_lock<std::mutex> lock(m_mutex);
		m_transxnQueue.push_front(std::move(transxn));
		lock.unlock();
		m_dataAvailableCV.notify_one();
	}
	
	void AsyncSocket::threadHandler()
	{
		while(true)
		{
			/* lock and copy/move the Transxn object into the local storage of this thread */
			std::unique_lock<std::mutex> lock(m_mutex);
			while(m_transxnQueue.empty() && (!m_isStopThread))
				m_dataAvailableCV.wait(lock);
			if(m_isStopThread)
				break;
			// debug_log_info("Request received for Data Receive");
			Transxn transxn(std::move(m_transxnQueue.back()));
			/* now we are done moving/copying, and let the client thread add more send and receive transactions into the queue */
			lock.unlock();

			/* Transxn::doit takes a long time to process due to network latency */
			bool result = transxn.doit(m_socketRef);
			if(!result)
			{
				m_isCanSendOrReceive = false;
				spdlog::error("Failed to do transaction");
					
				lock.lock();
				m_transxnQueue.pop_back();
				lock.unlock();

				/* socket might have been closed, so terminate this thread */
				break;
			}

			lock.lock();
			m_transxnQueue.pop_back();
			lock.unlock();
		}
	}
}
