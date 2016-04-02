// This file is a part of libab, which is released under the following license:
//
// Copyright (c) 2016 Preetam Jinka
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// In general, a negative return value indicates an error.

// ab_node_t is an opaque node handle.
typedef struct ab_node_t ab_node_t;

// ab_callbacks_t is a set of callbacks that are executed on certain node events, such as leadership
// changes or broadcasts. Since these functions are called from the libab event loop, it's important
// to avoid blocking actions.
typedef struct {
	// on_append is called when a new message is broadcasted from a leader.
	// The round number should monotonically increase, but the commit number may be repeated.
	// The caller maintains ownership of the data pointer.
	// ab_confirm_append should be called after the message is durably stored.
	void (*on_append)(uint64_t round, uint64_t commit, const char* data, int data_len, void* cb_data);
	// on_commit is called when the message with the given round and commit number is verified to be
	// received by a majority of the cluster and stored durably.
	void (*on_commit)(uint64_t round, uint64_t commit, void* cb_data);
	// gained_leadership is called when the node gains the leadership role.
	void (*gained_leadership)(void* cb_data);
	// lost_leadership is called when the node loses the leadership role.
	void (*lost_leadership)(void* cb_data);
	// on_leader_change is called when there is a new leader ID.
	void (*on_leader_change)(uint64_t leader_id, void* cb_data);
} ab_callbacks_t;

// ab_node_create returns a pointer to a new node handle.
// Callers should verify that the pointer is not NULL.
// Argument details:
// - id: the ID for this node, which should be nonzero and unique among the cluster.
// - cluster_size: the total size of the cluster including this node.
// - callbacks: node event callbacks (see below)
// - data: user-provided pointer passed as the last argument in the ab_callbacks_t callbacks.
// The return value should eventually be passed to ab_destroy.
ab_node_t*
ab_node_create(uint64_t id, int cluster_size, ab_callbacks_t callbacks, void* data);

// ab_set_committed initializes the node's round and commit numbers. This should be called
// with the appropriate numbers on recovery.
void
ab_set_committed(ab_node_t* node, uint64_t round, uint64_t commit);

// ab_listen sets the listen address for the node.
// address can either be an IPv4 or an IPv6 address in the following forms:
// - 127.0.0.1:2020
// - [::1]:2020
int
ab_listen(ab_node_t* node, const char* address);

// ab_connect_to_peer initializes a connection with an other cluster node at the given address.
int
ab_connect_to_peer(ab_node_t* node, const char* address);

// ab_run initializes the event loop and runs the node.
// This function blocks forever.
int
ab_run(ab_node_t* node);

// ab_append_cb is the callback passed to ab_append. status is negative on failure.
// On success, round and commit are the corresponding round and commit numbers for the
// broadcasted message.
typedef void (*ab_append_cb)(int status, uint64_t round, uint64_t commit, void* data);

// ab_append broadcasts a message with the given content to the rest of the cluster.
// ab_append_cb is called on success or failure with the provided data pointer.
int
ab_append(ab_node_t* node, const char* content, int content_len, ab_append_cb cb, void* data);

// ab_confirm_append should be called when a message is durably stored after on_append is called.
void
ab_confirm_append(ab_node_t* node, uint64_t round, uint64_t commit);

// ab_destroy frees the memory allocated for the node.
int
ab_destroy(ab_node_t* node);

#ifdef __cplusplus
}
#endif // __cplusplus
