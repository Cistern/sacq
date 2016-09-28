#pragma once

#include <ab.h>

void onAppendGoCb(uint64_t round, char*, int, void*);
void gainedLeadershipGoCb(void* cb_data);
void lostLeadershipGoCb(void* cb_data);
void onLeaderChangeGoCb(uint64_t, void*);

void on_append_go_cb_gateway(uint64_t round, const char* data, int data_len, void* cb_data);
void gained_leadership_go_cb_gateway(void* cb_data);
void lost_leadership_go_cb_gateway(void* cb_data);
void on_leader_change_go_cb_gateway(uint64_t leader_id, void* cb_data);

void set_callbacks(ab_callbacks_t* callbacks);

void append_go_gateway(ab_node_t* n, char* data, int data_len, int callbackNum);
void appendGoCb(int status, void* cb_data);
