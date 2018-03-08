#include "ib_session.h"

// constructor
ib_session::ib_session(
		const size_t ib_device_num,
		const int ib_physical_port_num,
		void* buf_addr,
		const size_t buf_size)
{
	this->phys_port_num = ib_physical_port_num;

	// get infiniband device list
	assert(this->device_list = ibv_get_device_list(nullptr));

	// find out whether the selected device exist or not
	size_t ib_device_count = 0;
	while (this->device_list[ib_device_count]) ++ib_device_count;

	assert(ib_device_count > ib_device_num);

	// open the selected device
	assert(this->context = ibv_open_device(this->device_list[ib_device_num]));

	// create completion channel
	assert(this->comp_channel = ibv_create_comp_channel(this->context));

	// get the device attributes
	this->device_attr = (ibv_device_attr*)malloc(sizeof(ibv_device_attr));
	memset(this->device_attr, 0, sizeof(ibv_device_attr));
	assert(!ibv_query_device(this->context, this->device_attr));

	// create protection domain
	assert(this->pd = ibv_alloc_pd(this->context));

	// create completion queue
	assert(this->cq = ibv_create_cq(
			this->context,
			this->device_attr->max_cqe,
			nullptr,
			this->comp_channel,
			0));

	// create queue pair
	ibv_qp_init_attr qp_init_attr;
	memset(&qp_init_attr, 0, sizeof(ibv_qp_init_attr));

	qp_init_attr.send_cq = this->cq;
	qp_init_attr.cap.max_send_wr = 1024;//this->device_attr->max_qp_wr;
	qp_init_attr.cap.max_send_sge = 1;

	qp_init_attr.recv_cq = this->cq;
	qp_init_attr.cap.max_recv_wr = 1024;//this->device_attr->max_qp_wr;
	qp_init_attr.cap.max_recv_sge = 1;

	qp_init_attr.qp_type = IBV_QPT_RC;

	assert(this->qp = ibv_create_qp(this->pd, &qp_init_attr));

	// does the physical infiniband port exist?
	assert(1 <= this->phys_port_num && this->phys_port_num <= this->device_attr->phys_port_cnt);

	// get the port attributes
	this->port_attr = (ibv_port_attr*)malloc(sizeof(ibv_port_attr));
	memset(this->port_attr, 0, sizeof(ibv_port_attr));
	assert(!ibv_query_port(this->context, this->phys_port_num, this->port_attr));

	// fill my lid, qpn, psn
	this->local = (ib_address*)malloc(sizeof(ib_address));
	this->local->lid = this->port_attr->lid;
	this->local->qpn = this->qp->qp_num;
	this->local->psn = lrand48() & 0xFFFFFF;

	// malloc for remote lid, qpn, psn
	this->remote = (ib_address*)malloc(sizeof(ib_address));
	memset(this->remote, 0, sizeof(ib_address));

	// memory region
	assert(buf_size != 0);
	assert(buf_addr != nullptr);

	this->local_mr = ibv_reg_mr(this->pd, buf_addr, buf_size,
			IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ);
	assert(local_mr);

	this->msg_comp_checker_thread = new play_wait_thread([&]{ this->msg_comp_checker(); });
}

// destructor
ib_session::~ib_session() {
	ibv_dereg_mr(this->local_mr);
	
	if(this->device_attr) free(this->device_attr);
	if(this->port_attr) free(this->port_attr);
	if(this->local) free(this->local);
	if(this->remote) free(this->remote);

	if (this->qp) assert(!ibv_destroy_qp(this->qp));
	if (this->cq) assert(!ibv_destroy_cq(this->cq));
	if (this->pd) assert(!ibv_dealloc_pd(this->pd));

	if (this->comp_channel) assert(!ibv_destroy_comp_channel(this->comp_channel));

	if (this->msg_comp_checker_thread) delete this->msg_comp_checker_thread;

	if (this->context) assert(!ibv_close_device(this->context));
	if (this->device_list) ibv_free_device_list(this->device_list);
}

void ib_session::msg_comp_checker() {
	void* cq_context;
	for (uint32_t i = 0; i < this->stream_count; i++) {
		assert(!ibv_req_notify_cq(this->cq, 0));
		assert(!ibv_get_cq_event(this->comp_channel, &this->cq, &cq_context));
		ibv_ack_cq_events(this->cq, 1);
	}
}

void ib_session::listen(const uint16_t port) {
	// create listen socket
	int listen_sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
	assert(listen_sock_fd != -1);

	// clear socket state
	const int yes = 1;
	assert(::setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != -1);

	// bind with listening port number
	sockaddr_in my_addr_in;
	memset(&my_addr_in, 0, sizeof(sockaddr_in));
	my_addr_in.sin_family = AF_INET;
	my_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	my_addr_in.sin_port = htons(port);

	assert(my_addr_in.sin_addr.s_addr != INADDR_NONE);

	assert(::bind(listen_sock_fd, (sockaddr*)(&my_addr_in), sizeof(sockaddr_in)) != -1);

	// listen
	const int listen_queue_size = 1;
	assert(::listen(listen_sock_fd, listen_queue_size) != -1);

	// accept
	sockaddr_in client_addr_in;
	memset(&client_addr_in, 0, sizeof(sockaddr_in));
	constexpr int client_addr_in_size = sizeof(sockaddr_in);
	int my_sock_fd = ::accept(
			listen_sock_fd,
			(sockaddr*)(&client_addr_in),
			(socklen_t*)(&client_addr_in_size));
	assert(my_sock_fd != -1);

	// message exchange (lid, qpn, psn)
	uint64_t message = 0;
	message = ((uint64_t)(this->local->lid) << 48)
			+ ((uint64_t)(this->local->qpn) << 24)
			+ ((uint64_t)(this->local->psn) <<  0);

	assert(::send(my_sock_fd, &message, sizeof(uint64_t), 0) != -1);

	message = 0;

	assert(::recv(my_sock_fd, &message, sizeof(uint64_t), 0) > 0);

	this->remote->lid = (uint32_t)((uint64_t)(message & 0xFFFF000000000000) >> 48);
	this->remote->qpn = (uint32_t)((uint64_t)(message & 0x0000FFFFFF000000) >> 24);
	this->remote->psn = (uint32_t)((uint64_t)(message & 0x0000000000FFFFFF) >>  0);

	// change QP state to INIT
    ibv_qp_attr qp_attr;
    memset(&qp_attr, 0x00, sizeof(ibv_qp_attr));

    qp_attr.qp_state = IBV_QPS_INIT; // INIT state
    qp_attr.pkey_index = 0;
    qp_attr.port_num = this->phys_port_num;
	qp_attr.qp_access_flags =
		IBV_ACCESS_LOCAL_WRITE |
		IBV_ACCESS_REMOTE_WRITE |
		IBV_ACCESS_REMOTE_READ;

    assert(!ibv_modify_qp(this->qp, &qp_attr,
				IBV_QP_STATE |
				IBV_QP_PKEY_INDEX |
				IBV_QP_PORT |
				IBV_QP_ACCESS_FLAGS));

    // change QP state from INIT to RTR (Ready to Receive)
    ibv_qp_attr qp_rtr_attr;
    memset(&qp_rtr_attr, 0x00, sizeof(ibv_qp_attr));

    qp_rtr_attr.qp_state = IBV_QPS_RTR;
    qp_rtr_attr.path_mtu = IBV_MTU_4096;
    qp_rtr_attr.dest_qp_num = this->remote->qpn;
    qp_rtr_attr.rq_psn = this->remote->psn;
		// PSN of local Receive Queue (== PSN of remote Send Queue)
	
    qp_rtr_attr.max_dest_rd_atomic = 0;
    qp_rtr_attr.min_rnr_timer = 0;

    qp_rtr_attr.ah_attr.is_global = 0; // Global Routing Header (GRH) true/false
    qp_rtr_attr.ah_attr.dlid = this->remote->lid; // destination LID
    qp_rtr_attr.ah_attr.sl = 0;
    qp_rtr_attr.ah_attr.src_path_bits = 0;
    qp_rtr_attr.ah_attr.port_num = this->phys_port_num;

    assert(!ibv_modify_qp(this->qp, &qp_rtr_attr,
				IBV_QP_STATE |
				IBV_QP_PATH_MTU |
                IBV_QP_DEST_QPN |
                IBV_QP_RQ_PSN |
                IBV_QP_MAX_DEST_RD_ATOMIC |
				IBV_QP_MIN_RNR_TIMER |
				IBV_QP_AV));

    // change QP state from RTR to RTS (Ready to Send)
    ibv_qp_attr qp_rts_attr;
    memset(&qp_rts_attr, 0x00, sizeof(ibv_qp_attr));

    qp_rts_attr.qp_state = IBV_QPS_RTS;
    qp_rts_attr.timeout = 0;
    qp_rts_attr.retry_cnt = 7;
    qp_rts_attr.rnr_retry = 7;
    qp_rts_attr.sq_psn = this->local->psn;
    qp_rts_attr.max_rd_atomic = 0;

    assert(!ibv_modify_qp(this->qp, &qp_rts_attr,
				IBV_QP_STATE |
                IBV_QP_TIMEOUT |
                IBV_QP_RETRY_CNT |
                IBV_QP_RNR_RETRY |
                IBV_QP_SQ_PSN |
                IBV_QP_MAX_QP_RD_ATOMIC));

	// memory region exchange
	assert(::send(my_sock_fd, (void*)this->local_mr, sizeof(ibv_mr), 0) != -1);

	assert(::recv(my_sock_fd, &this->remote_mr, sizeof(ibv_mr), 0) > 0);

	::close(my_sock_fd);
	::close(listen_sock_fd);
}

void ib_session::connect(const char* ip, const uint16_t port) {
	int my_sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
	assert(my_sock_fd != -1);

	const int yes = 1;
	assert(::setsockopt(my_sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) != -1));

	sockaddr_in server_addr_in;
	memset(&server_addr_in, 0, sizeof(sockaddr_in));
	
	server_addr_in.sin_family = AF_INET;
	server_addr_in.sin_addr.s_addr = ::inet_addr(ip);
	server_addr_in.sin_port = htons(port);
	assert(server_addr_in.sin_addr.s_addr != INADDR_NONE);

	while(::connect(my_sock_fd, (sockaddr*)(&server_addr_in), sizeof(sockaddr_in)) == -1);

	uint64_t message = 0;

	assert(::recv(my_sock_fd, &message, sizeof(uint64_t), 0) > 0);

	this->remote->lid = (uint32_t)((uint64_t)(message & 0xFFFF000000000000) >> 48);
	this->remote->qpn = (uint32_t)((uint64_t)(message & 0x0000FFFFFF000000) >> 24);
	this->remote->psn = (uint32_t)((uint64_t)(message & 0x0000000000FFFFFF) >>  0);
	
	message = ((uint64_t)(this->local->lid) << 48)
			+ ((uint64_t)(this->local->qpn) << 24)
			+ ((uint64_t)(this->local->psn) <<  0);

	assert(::send(my_sock_fd, &message, sizeof(uint64_t), 0) != -1);

    // change QP state to INIT
    ibv_qp_attr qp_attr;
    memset(&qp_attr, 0x00, sizeof(ibv_qp_attr));

    qp_attr.qp_state = IBV_QPS_INIT; // INIT state
    qp_attr.pkey_index = 0;
    qp_attr.port_num = this->phys_port_num;
	qp_attr.qp_access_flags = 
		IBV_ACCESS_LOCAL_WRITE |
		IBV_ACCESS_REMOTE_WRITE |
		IBV_ACCESS_REMOTE_READ;

    assert(!ibv_modify_qp(this->qp, &qp_attr,
				IBV_QP_STATE |
				IBV_QP_PKEY_INDEX |
				IBV_QP_PORT |
				IBV_QP_ACCESS_FLAGS));

    // change QP state from INIT to RTR (Ready to Receive)
    ibv_qp_attr qp_rtr_attr;
    memset(&qp_rtr_attr, 0x00, sizeof(ibv_qp_attr));

    qp_rtr_attr.qp_state = IBV_QPS_RTR;
    qp_rtr_attr.path_mtu = IBV_MTU_4096;
    qp_rtr_attr.dest_qp_num = this->remote->qpn;
    qp_rtr_attr.rq_psn = this->remote->psn;
		// PSN of local Receive Queue (== PSN of remote Send Queue)
    qp_rtr_attr.max_dest_rd_atomic = 0;
    qp_rtr_attr.min_rnr_timer = 0;

    qp_rtr_attr.ah_attr.is_global = 0; // Global Routing Header (GRH) true/false
    qp_rtr_attr.ah_attr.dlid = this->remote->lid; // destination LID
    qp_rtr_attr.ah_attr.sl = 0;
    qp_rtr_attr.ah_attr.src_path_bits = 0;
    qp_rtr_attr.ah_attr.port_num = this->phys_port_num;

    assert(!ibv_modify_qp(this->qp, &qp_rtr_attr,
				IBV_QP_STATE |
				IBV_QP_PATH_MTU |
                IBV_QP_DEST_QPN |
                IBV_QP_RQ_PSN |
                IBV_QP_MAX_DEST_RD_ATOMIC |
				IBV_QP_MIN_RNR_TIMER |
				IBV_QP_AV));

    // change QP state from RTR to RTS (Ready to Send)
    ibv_qp_attr qp_rts_attr;
    memset(&qp_rts_attr, 0x00, sizeof(ibv_qp_attr));

    qp_rts_attr.qp_state = IBV_QPS_RTS;
    qp_rts_attr.timeout = 0;
    qp_rts_attr.retry_cnt = 7;
    qp_rts_attr.rnr_retry = 7;
    qp_rts_attr.sq_psn = this->local->psn;
    qp_rts_attr.max_rd_atomic = 0;

    assert(!ibv_modify_qp(this->qp, &qp_rts_attr,
				IBV_QP_STATE |
                IBV_QP_TIMEOUT |
                IBV_QP_RETRY_CNT |
                IBV_QP_RNR_RETRY |
                IBV_QP_SQ_PSN |
                IBV_QP_MAX_QP_RD_ATOMIC));

	// memory region exchange
	assert(::recv(my_sock_fd, &this->remote_mr, sizeof(ibv_mr), 0) > 0);

	assert(::send(my_sock_fd, (void*)this->local_mr, sizeof(ibv_mr), 0) != -1);

	::close(my_sock_fd);
}

void ib_session::rdma_write(
		const size_t local_offset,
		const size_t remote_offset,
		const size_t message_size)
{
	// calculate total stream count (most time, the stream count == 1)
	const uint32_t stream_size = this->port_attr->max_msg_sz;
	this->stream_count = (uint32_t)ceil((double)message_size/(double)stream_size);

	// check the total stream count is supported
	// ex) 2000 stream <= IB card supports 300 stream -> failed!
	assert((int)this->stream_count <= this->device_attr->max_qp_wr);

	this->msg_comp_checker_thread->play();

	for (uint32_t i = 0; i < this->stream_count; ++i) {
		// create scatter_gather element
		ibv_sge sge;
		memset(&sge, 0, sizeof(ibv_sge));

		sge.addr = (uint64_t)this->local_mr->addr
			+ (uint64_t)local_offset
			+ (uint64_t)i * (uint64_t)stream_size;
		if (i == this->stream_count - 1) {
			sge.length = (uint32_t)(message_size - (size_t)i * (size_t)stream_size);
		} else {
			sge.length = stream_size;
		}

		sge.lkey = this->local_mr->lkey;

		// create work request
		ibv_send_wr wr, *bad_wr;
		memset(&wr, 0, sizeof(ibv_send_wr));

		wr.wr_id = i;
		wr.sg_list = &sge;
		wr.num_sge = 1;
		wr.next = nullptr;
		wr.opcode = IBV_WR_RDMA_WRITE;
		wr.send_flags = IBV_SEND_SIGNALED;

		wr.wr.rdma.remote_addr = (uint64_t)this->remote_mr.addr
			+ (uint64_t)remote_offset
			+ (uint64_t)i * (uint64_t)stream_size;
		wr.wr.rdma.rkey = this->remote_mr.rkey;

		// post the work request
		assert(!ibv_post_send(this->qp, &wr, &bad_wr));
	}

	this->msg_comp_checker_thread->wait();
}

void ib_session::rdma_read(
		const size_t local_offset,
		const size_t remote_offset,
		const size_t message_size)
{
	const uint32_t stream_size = this->port_attr->max_msg_sz;
	this->stream_count = (uint32_t)ceil((double)message_size/(double)stream_size);

	assert((int)this->stream_count <= this->device_attr->max_qp_wr);

	this->msg_comp_checker_thread->play();

	for (uint32_t i = 0; i < this->stream_count; ++i) {
		ibv_sge sge;
		memset(&sge, 0, sizeof(ibv_sge));

		sge.addr = (uint64_t)this->local_mr->addr
			+ (uint64_t)local_offset
			+ (uint64_t)i * (uint64_t)stream_size;
		if (i == this->stream_count - 1) {
			sge.length = (uint32_t)(message_size - (size_t)i * (size_t)stream_size);
		} else {
			sge.length = stream_size;
		}

		sge.lkey = this->local_mr->lkey;

		ibv_send_wr wr, *bad_wr;
		memset(&wr, 0, sizeof(ibv_send_wr));

		wr.wr_id = i;
		wr.sg_list = &sge;
		wr.num_sge = 1;
		wr.next = nullptr;
		wr.opcode = IBV_WR_RDMA_READ;
		wr.send_flags = IBV_SEND_SIGNALED;

		wr.wr.rdma.remote_addr = (uint64_t)this->remote_mr.addr
			+ (uint64_t)remote_offset
			+ (uint64_t)i * (uint64_t)stream_size;
		wr.wr.rdma.rkey = this->remote_mr.rkey;

		assert(!ibv_post_send(this->qp, &wr, &bad_wr));
	}

	this->msg_comp_checker_thread->wait();
}
