#pragma once

#include <uv.h>
#include <memory>
#include <cstdint>
#include <functional>
#include <cpl/net/sockaddr.hpp>

#include "message/codec.hpp"

const int READ_BUFFER_SIZE = 16*1024;

class Peer
{
public:
	Peer(std::shared_ptr<Codec> codec,
		 std::function<void(const Message*)> send_to_node,
		 std::unique_ptr<uv_tcp_t> conn,
		 IdentityMessage node_ident_msg)
	: m_codec(codec)
	, m_send_to_node(send_to_node)
	, m_active(true)
	, m_tcp(std::move(conn))
	, m_valid(false)
	, m_node_ident_msg(node_ident_msg)
	{
		init_loop_handles();
		run();
	}

	Peer(std::shared_ptr<Codec> codec,
		 std::function<void(const Message*)> send_to_node,
		 cpl::net::SockAddr& addr, std::unique_ptr<uv_tcp_t> conn,
		 IdentityMessage node_ident_msg)
	: m_codec(codec)
	, m_send_to_node(send_to_node)
	, m_active(false)
	, m_tcp(std::move(conn))
	, m_valid(true)
	, m_address(addr.str())
	, m_node_ident_msg(node_ident_msg)
	{
		init_loop_handles();

		auto req = new uv_connect_t;
		req->data = this;
		struct sockaddr_storage sockaddr;
		addr.get_sockaddr(reinterpret_cast<struct sockaddr*>(&sockaddr));
		uv_tcp_connect(req, m_tcp.get(), reinterpret_cast<struct sockaddr*>(&sockaddr),
			[](uv_connect_t* req, int status) {
				auto self = (Peer*)req->data;
				if (status < 0) {
					auto tcp_handle = self->m_tcp.release();
					self->m_tcp = nullptr;
					uv_close((uv_handle_t*)tcp_handle, [](uv_handle_t* handle) {
						delete handle;
					});
					return;
				}
				self->m_active = true;
				self->run();
				self->send(&self->m_node_ident_msg);
				delete req;
			});
	}

	uint64_t
	id()
	{
		return m_id;
	}

	std::string&
	address()
	{
		return m_address;
	}

	// Disable copying.
	Peer(const Peer& rhs) = delete;
	Peer& operator =(Peer& rhs) = delete;

	Peer& operator =(Peer&& rhs)
	{
		// Close the old TCP stream if it's active.
		if (m_tcp != nullptr) {
			auto old_stream = m_tcp.release();
			uv_close((uv_handle_t*)old_stream, [](uv_handle_t* handle) {
				delete handle;
			});
		}
		m_tcp = std::move(rhs.m_tcp);
		m_tcp->data = this;
		m_address = rhs.m_address;
		m_valid = true;
		m_active = true;
		m_read_buf = std::move(rhs.m_read_buf);
		m_pending_msg_size = rhs.m_pending_msg_size;
		rhs.m_valid = false;
		rhs.m_active = false;
		return *this;
	}

	void
	set_identity(const uint64_t id, const std::string& address)
	{
		m_id = id;
		m_address = address;
		m_valid = true;
	}

	void
	send(const Message* msg)
	{
		if (!m_active || m_tcp == nullptr) {
			return;
		}
		int size = msg->packed_size();
		auto buf = new uint8_t[size];
		uv_buf_t a[] = {
			{.base = (char*)buf, .len = (size_t)size}
		};
		auto ret = m_codec->pack_message(msg, buf, size);
		if (ret < 0) {
			// Packing failed.
			delete[] buf;
			return;
		}
		auto req = new uv_write_t;
		req->data = buf;
		uv_write(req, (uv_stream_t*)m_tcp.get(), a, 1, [](uv_write_t* req, int) {
			auto buf = (uint8_t*)(req->data);
			delete buf;
			delete req;
		});
	}

	void
	set_index(int index)
	{
		m_index = index;
	}

	bool
	done()
	{
		return !m_active && !m_valid;
	}

	~Peer()
	{
		uv_timer_stop(m_timer.get());
		auto handle = m_timer.release();
		uv_close((uv_handle_t*)handle, [](uv_handle_t* handle) {
			delete handle;
		});
	}

private:
	void
	run();

	void
	init_loop_handles();

	void
	periodic();

	void
	reconnect();

	void
	process_message_data(uint8_t* data, int size);

private:
	std::shared_ptr<Codec>              m_codec;
	std::function<void(const Message*)> m_send_to_node;
	bool                                m_active;
	bool                                m_valid;
	std::unique_ptr<uv_tcp_t>           m_tcp;
	std::unique_ptr<uv_timer_t>         m_timer;
	uv_loop_t*                          m_loop;
	int                                 m_index;
	uint64_t                            m_id;
	std::string                         m_address;
	uint64_t                            m_last_reconnect;
	IdentityMessage                     m_node_ident_msg;

	char                                m_alloc_buf[READ_BUFFER_SIZE];
	std::vector<uint8_t>                m_read_buf;
	int                                 m_pending_msg_size;
}; // Peer
