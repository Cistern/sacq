#include "node.hpp"

int
Node :: start(std::string address) {
	// Create a libuv event loop.
	m_uv_loop = std::make_unique<uv_loop_t>();
	if (uv_loop_init(m_uv_loop.get()) < 0) {
		return -1;
	}

	// Parse address string.
	struct sockaddr_storage sockaddr;
	cpl::net::SockAddr addr;
	if (addr.parse(address) < 0) {
		return -2;
	}
	addr.get_sockaddr(reinterpret_cast<struct sockaddr*>(&sockaddr));

	// Create a TCP handle.
	m_tcp = std::make_unique<uv_tcp_t>();
	if (uv_tcp_init(m_uv_loop.get(), m_tcp.get()) < 0) {
		return -3;
	}
	m_tcp->data = this;
	auto status = uv_tcp_bind(m_tcp.get(), reinterpret_cast<struct sockaddr*>(&sockaddr), 0);
	if (status < 0) {
		return -4;
	}

	if (uv_listen((uv_stream_t*)m_tcp.get(), 8, Node::on_connect) < 0) {
		return -5;
	}

	m_listen_address = address;
	return 0;
}

void
Node :: connect_to_peer(cpl::net::SockAddr& addr) {
	// Create a TCP handle.
	auto handle = std::make_unique<uv_tcp_t>();
	if (uv_tcp_init(m_uv_loop.get(), handle.get()) < 0) {
		return;
	}

	IdentityMessage ident_msg(m_id, m_listen_address);
	auto peer = std::make_shared<Peer>(m_codec, [=](const Message* m) {
		handle_message(m);
	}, addr, std::move(handle), ident_msg);
	m_peer_registry->register_peer(++m_index_counter, peer);
	peer->send(&ident_msg);
	return;
}

void
Node :: run() {
	m_lock->lock();
	m_timer = std::make_unique<uv_timer_t>();
	uv_timer_init(m_uv_loop.get(), m_timer.get());
	m_timer->data = this;
	uv_timer_start(m_timer.get(), [](uv_timer_t* timer) {
		auto self = (Node*)timer->data;
		self->periodic();
	},
	16, 16);
	if (uv_run(m_uv_loop.get(), UV_RUN_DEFAULT) < 0) {
		m_lock->unlock();
		return;
	}
	m_lock->unlock();
}

void
Node ::	on_connect(uv_stream_t* server, int status) {
	auto self = (Node*)server->data;
	if (status < 0) {
		return;
	}
	auto client = std::make_unique<uv_tcp_t>();
	uv_tcp_init(server->loop, client.get());
	uv_accept(server, (uv_stream_t*)client.get());
	IdentityMessage ident_msg(self->m_id, self->m_listen_address);
	auto peer = std::make_shared<Peer>(self->m_codec, [=](const Message* m) {
		self->handle_message(m);
	}, std::move(client), ident_msg);
	self->m_peer_registry->register_peer(++self->m_index_counter, peer);
	peer->send(&ident_msg);
}

void
Node :: periodic() {
	// Clean up the registry
	m_peer_registry->cleanup();
	uint64_t now = uv_hrtime();
	m_role->periodic(now);
}

void
Node :: handle_message(const Message* msg) {
	uint64_t now = uv_hrtime();
	IdentityMessage ident_msg(m_id, m_listen_address);
	switch (msg->type) {
	case MSG_IDENT:
		ident_msg = static_cast<const IdentityMessage&>(*msg);
		m_peer_registry->set_identity(ident_msg.source, ident_msg.id, ident_msg.address);
		break;
	case MSG_IDENT_REQUEST:
		m_peer_registry->send(msg->source, &ident_msg);
		break;
	case MSG_LEADER_ACTIVE:
		m_role->handle_leader_active(now, static_cast<const LeaderActiveMessage&>(*msg));
		break;
	case MSG_LEADER_ACTIVE_ACK:
		m_role->handle_leader_active_ack(now, static_cast<const LeaderActiveAck&>(*msg));
		break;
	}
}
