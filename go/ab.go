package ab

import (
	"errors"
	"sync"
	"unsafe"
)

/*
#cgo LDFLAGS: -lab

#include <stdlib.h>
#include <ab.h>
#include "callback.h"
*/
import "C"

// Node is a handle for a libab node instance.
// It is *not* safe to use from multiple goroutines.
type Node struct {
	// C handle
	ptr *C.ab_node_t
	// Index of this within the registeredNodes map
	callbacksNum int
	// User-provided callback handlers
	callbackHandler CallbackHandler
	// Channel to use for the result of append.
	appendResult chan appendResult
}

// Results of ab_append() as a struct.
type appendResult struct {
	status int
	round  uint64
	commit uint64
}

// CallbackHandler is an interface that is used by a Node
// when callbacks occur. These are called by libab's event
// loop thread. You should not block within these functions
// since that will block the entire event loop.
type CallbackHandler interface {
	// OnAppend is called when a new message is broadcast.
	// ConfirmAppend should be called with the same round and commit
	// numbers to acknowledge the append.
	OnAppend(round uint64, commit uint64, data string)
	// OnCommit is called when the message with the given round and commit
	// is guaranteed to be committed on a majority of nodes in the cluster.
	OnCommit(round uint64, commit uint64)
	// GainedLeadership is called when the Node has gained leadership status
	// and can broadcast new messages.
	GainedLeadership()
	// LostLeadership is called when the Node has lost leadership status.
	LostLeadership()
	// OnLeaderChange is called when the Node's current leader changes.
	// This may be called along with LostLeadership.
	OnLeaderChange(leaderID uint64)
}

// NewNode creates a new libab instance with the given ID, listen address, callback handler,
// and cluster size.
// The ID should be unique across the cluster.
// The listen address can be either an IPv4 or IPv6 address in the following forms:
// - 127.0.0.1:2020
// - [::1]:2020
// The cluster size is the size of the entire cluster including this node.
func NewNode(id uint64,
	listen string,
	callbackHandler CallbackHandler,
	clusterSize int) (*Node, error) {

	// Create a new Go handle
	n := &Node{
		callbackHandler: callbackHandler,
	}

	// Set C callbacks
	cCallbacks := C.ab_callbacks_t{}
	C.set_callbacks(&cCallbacks)

	// Register the node
	registeredNodesLock.Lock()
	defer registeredNodesLock.Unlock()
	registrationCounter++
	registeredNodes[registrationCounter] = n
	n.callbacksNum = registrationCounter

	// Create the ab_node_t handle
	ptr := C.ab_node_create(C.uint64_t(id), C.int(clusterSize), cCallbacks,
		unsafe.Pointer(&n.callbacksNum))

	// Start listening
	listenStr := C.CString(listen)
	defer C.free(unsafe.Pointer(listenStr))
	ret := C.ab_listen(ptr, listenStr)
	if ret < 0 {
		C.ab_destroy(ptr)
		return nil, errors.New("ab: error listening on address")
	}
	n.ptr = ptr

	return n, nil
}

// AddPeer adds a peer to the Node.
// This should be called before Run.
func (n *Node) AddPeer(address string) error {
	cAddr := C.CString(address)
	defer C.free(unsafe.Pointer(cAddr))
	ret := C.ab_connect_to_peer(n.ptr, cAddr)
	if ret < 0 {
		return errors.New("ab: error adding peer")
	}
	return nil
}

// SetCommitted sets the latest committed round and commit number for this Node.
// This should be called before Run.
func (n *Node) SetCommitted(round, commit uint64) {
	C.ab_set_committed(n.ptr, C.uint64_t(round), C.uint64_t(commit))
}

// Run initializes the event loop and runs the Node.
// This function blocks forever (unless there is an error),
// so you should start this in a separate goroutine.
func (n *Node) Run() error {
	C.ab_run(n.ptr)
	return errors.New("ab: event loop exited")
}

// Append broadcasts data to the cluster.
// The returned values are the new round and commit, and an error.
func (n *Node) Append(data string) (uint64, uint64, error) {
	n.appendResult = make(chan appendResult)
	cData := C.CString(data)
	defer C.free(unsafe.Pointer(cData))
	C.append_go_gateway(n.ptr, cData, C.int(len(data)), C.int(n.callbacksNum))
	result := <-n.appendResult
	n.appendResult = nil
	if result.status == 0 {
		return result.round, result.commit, nil
	}
	return 0, 0, errors.New("ab: append failed")
}

// ConfirmAppend confirms that the message corresponding to the given
// round and commit has been durably stored.
func (n *Node) ConfirmAppend(round, commit uint64) {
	C.ab_confirm_append(n.ptr, C.uint64_t(round), C.uint64_t(commit))
}

// cgo-related stuff 👇

var registeredNodesLock sync.RWMutex
var registrationCounter int
var registeredNodes = map[int]*Node{}

//export on_append_go_cb
func on_append_go_cb(round C.uint64_t, commit C.uint64_t, str *C.char, length C.int, p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	node.callbackHandler.OnAppend(uint64(round), uint64(commit), C.GoStringN(str, length))
}

//export on_commit_go_cb
func on_commit_go_cb(round C.uint64_t, commit C.uint64_t, p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	node.callbackHandler.OnCommit(uint64(round), uint64(commit))
}

//export gained_leadership_go_cb
func gained_leadership_go_cb(p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	node.callbackHandler.GainedLeadership()
}

//export lost_leadership_go_cb
func lost_leadership_go_cb(p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	node.callbackHandler.LostLeadership()
}

//export on_leader_change_go_cb
func on_leader_change_go_cb(id C.uint64_t, p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	node.callbackHandler.OnLeaderChange(uint64(id))
}

//export append_go_cb
func append_go_cb(status C.int, round C.uint64_t, commit C.uint64_t, p unsafe.Pointer) {
	i := *(*int)(p)
	C.free(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	if node != nil && node.appendResult != nil {
		node.appendResult <- appendResult{
			status: int(status),
			round:  uint64(round),
			commit: uint64(commit),
		}
	}
}
