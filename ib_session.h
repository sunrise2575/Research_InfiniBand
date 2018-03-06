#ifndef IB_SESSION_HEADER
#define IB_SESSION_HEADER

// C headers
#include <cstring> // malloc(), strdup()
#include <cmath> // ceil()
#include <assert.h> // assert()
#include <unistd.h> // ::close()

#include <iostream>
#include <errno.h>

// socket header
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// infiniband header
#include <infiniband/verbs.h>

// custom header
#include "play_wait_thread.h"

struct ib_address{
	uint32_t lid, qpn, psn;
};

class ib_session {
private:
	// infiniband physical structure
	ibv_device**     device_list;
	ibv_device_attr* device_attr;
	ibv_port_attr*   port_attr;
	int phys_port_num;

	// infiniband logical structure
	ibv_context* context;
	ibv_pd* pd;
	ibv_cq* cq;
	ibv_qp* qp;
	ibv_comp_channel* comp_channel;

	// infiniband address (LID, QPN, PSN)
	ib_address *local, *remote;

	// memory
	ibv_mr *local_mr, remote_mr;
	uint32_t stream_count;

	play_wait_thread* msg_comp_checker_thread;
	void msg_comp_checker();

public:
	ib_session(const size_t ib_device_num = 0, const int ib_physical_port_num = 1, void* buf_addr = nullptr, const size_t buf_size = 0);
	~ib_session();

	// connection
	void listen(const uint16_t port);
	void connect(const char* ip, const uint16_t port);

	// data transfer
	void rdma_write(const size_t local_offset, const size_t remote_offset, const size_t message_size);
};

#endif
