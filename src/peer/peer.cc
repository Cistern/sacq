#include "peer.hpp"

void
Peer :: run() {
	m_last_reconnect = uv_hrtime();
	uv_read_start((uv_stream_t*)m_tcp.get(),
		// Buffer allocation callback
		[](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
			buf->base = ((Peer*)handle->data)->m_alloc_buf;
			buf->len = READ_BUFFER_SIZE;
		},

		// On read callback
		[](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
			auto peer = (Peer*)stream->data;
			if (nread < 0) {
				uv_close((uv_handle_t*)stream, [](uv_handle_t* handle) {
					auto self = (Peer*)handle->data;
					self->m_active = false;
					self->m_tcp = nullptr;
					// Make sure our reconnect time is at least a few seconds
					// after now.
					self->m_last_reconnect = uv_hrtime();
				});
				return;
			}
			for (int i = 0; i < nread; i++) {
				peer->m_read_buf.push_back(buf->base[i]);
				peer->m_pending_msg_size++;
			}
			while (true) {
				auto msg_length = peer->m_codec->decode_message_length(peer->m_read_buf.data(),
					peer->m_read_buf.size());
				if (msg_length > 0 && peer->m_pending_msg_size >= msg_length) {
					peer->process_message_data(peer->m_read_buf.data(), msg_length);
					// Trim the buffer.
					std::vector<uint8_t> replacement;
					for (auto& i : peer->m_read_buf) {
						if (msg_length > 0) {
							msg_length--;
							continue;
						}
						replacement.push_back(i);
					}
					//peer->m_read_buf.erase(peer->m_read_buf.begin(),
					//	peer->m_read_buf.begin()+msg_length);
					peer->m_read_buf = std::move(replacement);
					//peer->m_pending_msg_size -= msg_length;
					peer->m_pending_msg_size = peer->m_read_buf.size();
				} else {
					break;
				}
			}
		}
	);
}

void
Peer :: process_message_data(uint8_t* data, int size)
{
	std::unique_ptr<Message> m;
	if (m_codec->decode_message(m, data, size) >= 0) {
		m->source = m_index;
		if (m_send_to_node != nullptr) {
			m_send_to_node(m.get());
		}
	}
}

void
Peer :: init_loop_handles()
{
	m_tcp->data = this;
	m_loop = m_tcp->loop;
	uv_timer_init(m_loop, m_timer);
	m_timer->data = this;
	uv_timer_start(m_timer, [](uv_timer_t* timer) {
		auto self = (Peer*)timer->data;
		self->periodic();
	}, 16, 16);
}

void
Peer :: periodic()
{
	if (!m_active) {
		// We don't have an active connection. The only thing we can do is attempt
		// a reconnection.
		if (!m_valid) {
			// Can't reconnect since we don't have a valid address.
			return;
		}
		auto now = uv_hrtime();
		if (now - m_last_reconnect > 3e9) {
			reconnect();
			m_last_reconnect = now;
		}
		return;
	}
}

void
Peer :: reconnect()
{
	if (m_tcp == nullptr) {
		m_tcp = std::make_unique<uv_tcp_t>();
		if (uv_tcp_init(m_loop, m_tcp.get()) < 0) {
			// LOG
		}
		m_tcp->data = this;
	}
	auto req = new uv_connect_t;
	req->data = this;
	cpl::net::SockAddr addr;
	if (addr.parse(m_address) < 0) {
		// LOG
		return;
	}
	struct sockaddr_storage sockaddr;
	addr.get_sockaddr(reinterpret_cast<struct sockaddr*>(&sockaddr));
	uv_tcp_connect(req, m_tcp.get(), reinterpret_cast<struct sockaddr*>(&sockaddr),
		[](uv_connect_t* req, int status) {
			if (status < 0) {
				auto self = (Peer*)req->data;
				uv_close((uv_handle_t*)self->m_tcp.get(), [](uv_handle_t* handle) {
					auto self = (Peer*)handle->data;
					self->m_tcp = nullptr;
				});
				delete req;
				return;
			}
			auto self = (Peer*)req->data;
			self->run();
			self->m_active = true;
			self->send(&(self->m_node_ident_msg));
			delete req;
		});
}
