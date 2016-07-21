#pragma once

#include <mutex>
#include <thread>
#include <memory>
#include <cerrno>
#include <future>
#include <iostream>

#include <uv.h>
#include <cpl/net/sockaddr.hpp>

#include "ab.h"
#include "role.hpp"
#include "peer/peer.hpp"
#include "peer_registry.hpp"
#include "message/codec.hpp"
#include "message/message.hpp"

const uint64_t leader_timeout_ns = 1e9;

class Node
{
public:
	Node(uint64_t id, int cluster_size, ab_callbacks_t callbacks, void* callbacks_data)
	: m_id(id)
	, m_cluster_size(cluster_size)
	, m_peer_registry(std::make_unique<PeerRegistry>())
	, m_codec(std::make_shared<Codec>())
	, m_index_counter(0)
	, m_trusted_peer(0)
	, m_last_leader_active(uv_hrtime())
	, m_role(std::make_unique<Role>(*m_peer_registry, id, cluster_size))
	, m_mutex(std::make_unique<std::mutex>())
	{
		m_role->set_callbacks(callbacks, callbacks_data);
	}

	// start starts a node listening at address.
	// A negative value is returned for errors.
	int
	start(std::string address);

	// run starts the main processing routine.
	int
	run();

	// connect_to_peer connects to a peer with the given
	// address and creates a new Peer instance for the node.
	// If the node is unable to establish a connection, the
	// peer is registered as a failed node.
	void
	connect_to_peer(cpl::net::SockAddr&);

	void
	append(std::string content, ab_append_cb cb, void* data)
	{
		auto async = new uv_async_t;
		auto task = new std::packaged_task<void()>([=]() {
			m_role->send_append(content, cb, data);
			uv_close((uv_handle_t*)async, [](uv_handle_t* handle) {
				// delete packaged_task
				auto func = reinterpret_cast<std::packaged_task<void()>*>(handle->data);
				delete func;
				// delete async
				delete handle;
			});
		});
		async->data = task;
		uv_async_init(m_uv_loop.get(), async, [](uv_async_t* handle) {
			auto func = reinterpret_cast<std::packaged_task<void()>*>(handle->data);
			(*func)();
		});
		uv_async_send(async);
	}

	void
	confirm_append(uint64_t round, uint64_t commit)
	{
		auto async = new uv_async_t;
		auto task = new std::packaged_task<void()>([=]() {
			m_role->client_confirm_append(round, commit);
			uv_close((uv_handle_t*)async, [](uv_handle_t* handle) {
				// delete packaged_task
				auto func = reinterpret_cast<std::packaged_task<void()>*>(handle->data);
				delete func;
				// delete async
				delete handle;
			});
		});
		async->data = task;
		uv_async_init(m_uv_loop.get(), async, [](uv_async_t* handle) {
			auto func = reinterpret_cast<std::packaged_task<void()>*>(handle->data);
			(*func)();
		});
		uv_async_send(async);
	}

	void
	set_committed(uint64_t round, uint64_t commit)
	{
		m_role->set_committed(round, commit);
	}

	void
	set_key(const std::string& key)
	{
		m_codec->set_key(key);
	}

	// shutdown shuts down the Node's event loop and cleans up resources.
	void
	shutdown()
	{
		uv_async_init(m_uv_loop.get(), &m_async, [](uv_async_t* handle) {
			auto self = (Node*)(handle->data);
			auto timer = self->m_timer.get();
			uv_timer_stop(timer);

			uv_close((uv_handle_t*)timer, [](uv_handle_t* handle) {
				auto self = (Node*)(handle->data);
				auto tcp = self->m_tcp.get();
				auto loop = self->m_uv_loop.get();

				self->m_peer_registry = nullptr;

				uv_close((uv_handle_t*)tcp, [](uv_handle_t* handle) {
					auto self = (Node*)(handle->data);
					auto loop = self->m_uv_loop.get();

					uv_walk(loop, [](uv_handle_t* handle, void* arg) {
						if (uv_is_closing(handle) == 0) {
							uv_close(handle, [](uv_handle_t* h){});
						}
					}, nullptr);
				});
			});
		});
		m_async.data = this;
		uv_async_send(&m_async);
	}

	~Node()
	{
		std::lock_guard<std::mutex> lock(*m_mutex);
		uv_loop_close(m_uv_loop.get());
	}

private:
	uint64_t                      m_id;
	std::string                   m_listen_address;
	std::unique_ptr<uv_loop_t>    m_uv_loop;
	std::unique_ptr<uv_tcp_t>     m_tcp;
	std::unique_ptr<uv_timer_t>   m_timer;
	std::unique_ptr<PeerRegistry> m_peer_registry;
	std::shared_ptr<Codec>        m_codec;
	int                           m_index_counter;
	int                           m_cluster_size;
	ab_callbacks_t*               m_client_callbacks;
	void*                         m_client_callbacks_data;

	uint64_t                      m_trusted_peer;
	uint64_t                      m_last_leader_active;
	std::unique_ptr<Role>         m_role; // TODO

	std::unique_ptr<std::mutex>   m_mutex;
	uv_async_t                    m_async;

	void
	periodic();

	static void
	on_connect(uv_stream_t* server, int status);

	void
	handle_message(const Message*);
}; // Node
