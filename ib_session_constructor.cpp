#include "ib_session.h"

ib_session::ib_session(const size_t ib_device_num,
                       const int ib_physical_port_num,
                       const char* dest_ip_addr,
                       const uint16_t ip_port,
                       const bool is_responder) {

    this->responder = is_responder;

    // 디바이스 리스트 얻는다.
    this->device_list = ibv_get_device_list(nullptr);
    if (this->device_list == nullptr) {
        this->error(error_code::ibv_get_device_list);
    }

    size_t ib_device_count = 0;
    while (this->device_list[ib_device_count]) {
        ++ib_device_count;
    }

    // 만약에 없는 번호이면
    if (ib_device_count <= ib_device_num) {
        this->error(error_code::no_such_device);
    }

    // 장비를 연다.
    this->context = ibv_open_device(this->device_list[ib_device_num]);
    if (this->context == nullptr) {
        this->error(error_code::ibv_open_device);
    }

    // 컴플리션 채널 생성
    this->comp_channel = ibv_create_comp_channel(this->context);
    if (this->comp_channel == nullptr) {
        this->error(error_code::ibv_create_comp_channel);
    }

    // 디바이스 어트리뷰트
    this->device_attr = (ibv_device_attr*)malloc(sizeof(*this->device_attr));
    memset(this->device_attr, 0x00, sizeof(*(this->device_attr)));
    if (ibv_query_device(this->context, this->device_attr) != 0) {
        this->error(error_code::ibv_query_device);
    }

    // 프로텍션 도메인
    this->pd = ibv_alloc_pd(this->context);
    if (this->pd == nullptr) {
        this->error(error_code::ibv_alloc_pd);
    }

    // 컴플리션 큐
    this->cq = ibv_create_cq(this->context,
                             this->device_attr->max_cqe,
                             nullptr,
                             this->comp_channel,
                             0);
    if (this->cq == nullptr) {
        this->error(error_code::ibv_create_cq);
    }

    // 큐 페어
    ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0x00, sizeof(qp_init_attr));

    qp_init_attr.send_cq = this->cq;
    qp_init_attr.cap.max_send_wr = this->device_attr->max_qp_wr;
    qp_init_attr.cap.max_send_sge = 1;

    qp_init_attr.recv_cq = this->cq;
    qp_init_attr.cap.max_recv_wr = this->device_attr->max_qp_wr;
    qp_init_attr.cap.max_recv_sge = 1;

    qp_init_attr.qp_type = IBV_QPT_RC; // Reliable Connection

    this->qp = ibv_create_qp(this->pd, &qp_init_attr);
    if (this->qp == nullptr) {
        this->error(error_code::ibv_create_qp);
    }

    // 실제로 있는 포트인지 검사한다.
    if (this->device_attr->phys_port_cnt < ib_physical_port_num || ib_physical_port_num < 1) {
        this->error(error_code::no_such_port);
    }

    // 포트 어트리뷰트
    this->port_attr = (ibv_port_attr*)malloc(sizeof(*this->port_attr));
    memset(this->port_attr, 0x00, sizeof(*this->port_attr));
    if (ibv_query_port(this->context, ib_physical_port_num, this->port_attr) != 0) {
        this->error(error_code::ibv_query_port);
    }

    // 상대방에게 보내기 위한 나의 연결정보.
    const int my_lid = this->port_attr->lid;
    const int my_qpn = this->qp->qp_num;
    const int my_psn = lrand48() & 0xFFFFFF;

    // 상대방에게서 받을 연결정보.
    int remote_lid = 0;
    int remote_qpn = 0;
    int remote_psn = 0;

    if (this->responder) {
        // 서버일 경우 listen socket을 생성한다.
        int listen_sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock_fd == -1) {
            this->error(error_code::socket_socket);
        }

        int yes = 1;
        if (::setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            this->error(error_code::socket_setsockopt);
        }

        sockaddr_in my_addr_in;
        memset(&my_addr_in, 0x00, sizeof(my_addr_in));

        my_addr_in.sin_family = AF_INET;
        my_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
        my_addr_in.sin_port = htons(ip_port);

        if (my_addr_in.sin_addr.s_addr == INADDR_NONE) {
            this->error(error_code::socket_inaddr_none);
        }

        // IP주소와 소켓을 연결한다.
        if (::bind(listen_sock_fd, (sockaddr*)(&my_addr_in), sizeof(my_addr_in)) == -1) {
            this->error(error_code::socket_bind);
        }

        const int listen_queue_size = 1;
        // listen socket에서 client의 연결 요청을 대기한다.
        if (::listen(listen_sock_fd, listen_queue_size) == -1) {
            this->error(error_code::socket_listen);
        }

        sockaddr_in client_addr_in;
        memset(&client_addr_in, 0x00, sizeof(client_addr_in));

        int client_addr_in_size = sizeof(client_addr_in);

        // 요청이 들어오면 해당 client에 대응하는 socket을 만든다.
        this->my_sock_fd = ::accept(listen_sock_fd,
                                  (sockaddr*)(&client_addr_in),
                                  (socklen_t*)(&client_addr_in_size));

        if (this->my_sock_fd == -1) {
            this->error(error_code::socket_accept);
        }

        uint64_t message = 0x0000000000000000;

        // 서버측에서 먼저 자신의 LID, QPN, PSN을 전달한다.
        message = ((uint64_t)(my_lid) << 48)
                + ((uint64_t)(my_qpn) << 24)
                + ((uint64_t)(my_psn) <<  0);

        if (::send(this->my_sock_fd, &message, sizeof(message), 0) == -1) {
            this->error(error_code::socket_send);
        }

        // 클라이언트에게서 LID, QPN, PSN을 전달받는다.
        message = 0x0000000000000000;

        if (::recv(this->my_sock_fd, &message, sizeof(message), 0) <= 0) {
            this->error(error_code::socket_recv);
        }

        remote_lid = (int)((uint64_t)(message & 0xFFFF000000000000) >> 48);
        remote_qpn = (int)((uint64_t)(message & 0x0000FFFFFF000000) >> 24);
        remote_psn = (int)((uint64_t)(message & 0x0000000000FFFFFF) >>  0);

        ::close(listen_sock_fd);
    } else {
        // 클라이언트일 경우 server에 연결할 socket을 만든다.
        this->my_sock_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (this->my_sock_fd == -1) {
            this->error(error_code::socket_socket);
        }

        int yes = 1;
        if (::setsockopt(this->my_sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            this->error(error_code::socket_setsockopt);
        }

        sockaddr_in server_addr_in;
        memset(&server_addr_in, 0x00, sizeof(server_addr_in));

        server_addr_in.sin_family = AF_INET;
        server_addr_in.sin_addr.s_addr = ::inet_addr(dest_ip_addr);
        server_addr_in.sin_port = htons(ip_port);
        if (server_addr_in.sin_addr.s_addr == INADDR_NONE) {
            this->error(error_code::socket_inaddr_none);
        }

        // 서버에게 연결한다.
        while (::connect(this->my_sock_fd, (sockaddr*)(&server_addr_in), sizeof(server_addr_in)) == -1);

        uint64_t message = 0x0000000000000000;

        // 서버에게서 LID, QPN, PSN을 전달받는다.
        if (::recv(this->my_sock_fd, &message, sizeof(message), 0) <= 0) {
            this->error(error_code::socket_recv);
        }

        remote_lid = (int)((uint64_t)(message & 0xFFFF000000000000) >> 48);
        remote_qpn = (int)((uint64_t)(message & 0x0000FFFFFF000000) >> 24);
        remote_psn = (int)((uint64_t)(message & 0x0000000000FFFFFF) >>  0);

        // 자신의 LID, QPN, PSN을 서버로 전달한다.
        message = 0x0000000000000000;

        message = ((uint64_t)(my_lid) << 48)
                + ((uint64_t)(my_qpn) << 24)
                + ((uint64_t)(my_psn) <<  0);

        if (::send(this->my_sock_fd, &message, sizeof(message), 0) == -1) {
            this->error(error_code::socket_send);
        }
    }

    // QP의 상태를 INIT으로 변경한다.
    ibv_qp_attr qp_attr;
    memset(&qp_attr, 0x00, sizeof(qp_attr));

    qp_attr.qp_state = IBV_QPS_INIT; // INIT state
    qp_attr.pkey_index = 0;
    qp_attr.port_num = ib_physical_port_num;
    qp_attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_WRITE |
                              IBV_ACCESS_REMOTE_READ;
    if (ibv_modify_qp(this->qp,
                      &qp_attr,
                      IBV_QP_STATE |
                      IBV_QP_PKEY_INDEX |
                      IBV_QP_PORT |
                      IBV_QP_ACCESS_FLAGS)
    != 0) {
        this->error(error_code::ibv_modify_qp);
    }

    // INIT->RTR로 상태를 바꾼다.
    ibv_qp_attr qp_rtr_attr;
    memset(&qp_rtr_attr, 0x00, sizeof(qp_rtr_attr));

    qp_rtr_attr.qp_state = IBV_QPS_RTR;
    qp_rtr_attr.path_mtu = IBV_MTU_4096;
    qp_rtr_attr.dest_qp_num = remote_qpn; // 통신 상대의 QPN 번호
    qp_rtr_attr.rq_psn = remote_psn; // 자신의 Receive Queue의 PSN,
                                     //즉, 상대방의 Send Queue의 PSN이다.
    qp_rtr_attr.max_dest_rd_atomic = 0;
    qp_rtr_attr.min_rnr_timer = 0;

    qp_rtr_attr.ah_attr.is_global = 0; // Global Routing Header (GRH)를 사용할 것인가?
    qp_rtr_attr.ah_attr.dlid = remote_lid; // 통신 상대의 LID
    qp_rtr_attr.ah_attr.sl = 0;
    qp_rtr_attr.ah_attr.src_path_bits = 0;
    qp_rtr_attr.ah_attr.port_num = ib_physical_port_num; // 물리적인 포트 번호.

    if (ibv_modify_qp(this->qp,
                      &qp_rtr_attr,
                      IBV_QP_STATE |
                      IBV_QP_PATH_MTU |
                      IBV_QP_DEST_QPN |
                      IBV_QP_RQ_PSN |
                      IBV_QP_MAX_DEST_RD_ATOMIC |
                      IBV_QP_MIN_RNR_TIMER |
                      IBV_QP_AV)
    != 0) {
        this->error(error_code::ibv_modify_qp);
    }

    // RTR->RTS로 상태를 바꾼다.
    ibv_qp_attr qp_rts_attr;
    memset(&qp_rts_attr, 0x00, sizeof(qp_rts_attr));

    qp_rts_attr.qp_state = IBV_QPS_RTS;
    qp_rts_attr.timeout = 0;
    qp_rts_attr.retry_cnt = 7;
    qp_rts_attr.rnr_retry = 7;
    qp_rts_attr.sq_psn = my_psn; // Send Queue의 PSN을 설정한다.
    qp_rts_attr.max_rd_atomic = 0;

    if (ibv_modify_qp(this->qp,
                      &qp_rts_attr,
                      IBV_QP_STATE |
                      IBV_QP_TIMEOUT |
                      IBV_QP_RETRY_CNT |
                      IBV_QP_RNR_RETRY |
                      IBV_QP_SQ_PSN |
                      IBV_QP_MAX_QP_RD_ATOMIC)
    != 0) {
        this->error(error_code::ibv_modify_qp);
    }

    this->msg_comp_checker_thread = new play_n_stop_thread([&]{ this->msg_comp_checker(); });
}
