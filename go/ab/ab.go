package ab

import (
	"errors"
	"runtime"
	"sync"
	"unsafe"
)

/*
#cgo LDFLAGS: -lab -lstdc++

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
	callbacksNum *C.int
	// User-provided callback handlers
	callbackHandler CallbackHandler
	// Channel to use for the result of append.
	appendResult chan appendResult
}

// Results of ab_append() as a struct.
type appendResult struct {
	status int
}

// CallbackHandler is an interface that is used by a Node
// when callbacks occur. These are called by libab's event
// loop thread. You should not block within these functions
// since that will block the entire event loop.
type CallbackHandler interface {
	// OnAppend is called when a new message is broadcast.
	// ConfirmAppend should be called with the same round
	// number to acknowledge the append.
	OnAppend(node *Node, round uint64, data string)
	// GainedLeadership is called when the Node has gained leadership status
	// and can broadcast new messages.
	GainedLeadership(node *Node)
	// LostLeadership is called when the Node has lost leadership status.
	LostLeadership(node *Node)
	// OnLeaderChange is called when the Node's current leader changes.
	// This may be called along with LostLeadership.
	OnLeaderChange(node *Node, leaderID uint64)
}

// NewNode creates a new libab instance with the given ID, listen address, callback handler,
// and cluster size.
// The ID should be unique across the cluster.
// The listen address can be either an IPv4 or IPv6 address in the following forms:
// 	"127.0.0.1:2020"
// 	"[::1]:2020"
// The cluster size is the size of the entire cluster including this node.
func NewNode(id uint64,
	listen string,
	callbackHandler CallbackHandler,
	clusterSize int) (*Node, error) {

	// Create a new Go handle
	n := &Node{
		callbackHandler: callbackHandler,
		callbacksNum:    (*C.int)(C.malloc(C.sizeof_int)),
	}
	runtime.SetFinalizer(n, func(node *Node) {
		C.free(unsafe.Pointer(node.callbacksNum))
	})

	// Set C callbacks
	cCallbacks := C.ab_callbacks_t{}
	C.set_callbacks(&cCallbacks)

	// Register the node
	registeredNodesLock.Lock()
	defer registeredNodesLock.Unlock()
	registrationCounter++
	registeredNodes[registrationCounter] = n
	*n.callbacksNum = C.int(registrationCounter)

	// Create the ab_node_t handle
	ptr := C.ab_node_create(C.uint64_t(id), C.int(clusterSize), cCallbacks,
		unsafe.Pointer(n.callbacksNum))

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

// SetKey sets the shared cluster encryption key.
// This should be called before Run.
func (n *Node) SetKey(key string) error {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))
	ret := C.ab_set_key(n.ptr, cKey, C.int(len(key)))
	if ret < 0 {
		return errors.New("ab: invalid key")
	}
	return nil
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

// Run initializes the event loop and runs the Node.
// This function blocks until the Node is shut down
// or an error occurs starting the event loop, so you
// should start this in a separate goroutine.
func (n *Node) Run() error {
	res := C.ab_run(n.ptr)
	if int(res) < 0 {
		return errors.New("ab: event loop exited with an error")
	}
	return nil
}

// Append broadcasts data to the cluster.
func (n *Node) Append(data string) error {
	n.appendResult = make(chan appendResult)
	cData := C.CString(data)
	defer C.free(unsafe.Pointer(cData))
	C.append_go_gateway(n.ptr, cData, C.int(len(data)), *n.callbacksNum)
	result := <-n.appendResult
	n.appendResult = nil
	if result.status == 0 {
		return nil
	}
	return errors.New("ab: append failed")
}

// ConfirmAppend confirms that the message corresponding to the given
// round has been durably stored.
func (n *Node) ConfirmAppend(round uint64) {
	C.ab_confirm_append(n.ptr, C.uint64_t(round))
}

// Destroy stops the Node and frees up all of its resources.
func (n *Node) Destroy() error {
	registeredNodesLock.Lock()
	defer registeredNodesLock.Unlock()
	if int(C.ab_destroy(n.ptr)) < 0 {
		return errors.New("ab: failed to destroy Node")
	}
	delete(registeredNodes, int(*n.callbacksNum))
	return nil
}

// cgo-related stuff 👇

var registeredNodesLock sync.RWMutex
var registrationCounter int
var registeredNodes = map[int]*Node{}

//export onAppendGoCb
func onAppendGoCb(round C.uint64_t, str *C.char, length C.int, p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	if node.callbackHandler != nil {
		go func() { node.callbackHandler.OnAppend(node, uint64(round), C.GoStringN(str, length)) }()
	}
}

//export gainedLeadershipGoCb
func gainedLeadershipGoCb(p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	if node.callbackHandler != nil {
		go func() { node.callbackHandler.GainedLeadership(node) }()
	}
}

//export lostLeadershipGoCb
func lostLeadershipGoCb(p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	if node.callbackHandler != nil {
		go func() { node.callbackHandler.LostLeadership(node) }()
	}
}

//export onLeaderChangeGoCb
func onLeaderChangeGoCb(id C.uint64_t, p unsafe.Pointer) {
	i := *(*int)(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	if node.callbackHandler != nil {
		go func() { node.callbackHandler.OnLeaderChange(node, uint64(id)) }()
	}
}

//export appendGoCb
func appendGoCb(status C.int, p unsafe.Pointer) {
	i := int(*(*C.int)(p))
	C.free(p)
	registeredNodesLock.RLock()
	defer registeredNodesLock.RUnlock()
	node := registeredNodes[i]
	if node != nil && node.appendResult != nil {
		go func() {
			node.appendResult <- appendResult{
				status: int(status),
			}
		}()
	}
}
