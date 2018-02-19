#include "ib_session.h"

int ib_session::request_of_mem_reg(void* host_mem_addr,
    const size_t host_mem_size) {
    if (host_mem_size == 0) {
        return -1;
    }

    ibv_mr* memory_region = ibv_reg_mr(this->pd,
                                host_mem_addr,
                                host_mem_size,
                                IBV_ACCESS_LOCAL_WRITE |
                                IBV_ACCESS_REMOTE_WRITE |
                                IBV_ACCESS_REMOTE_READ);
    if (memory_region == nullptr) {
        this->error(error_code::ibv_reg_mr);
    }

    // 만약에 지금 넣으려는게 핸들도 존재하고 길이도 같으면
    int handle_already_exist = this->mem_handle_of(host_mem_addr);
    if (this->mem_handle_of(host_mem_addr) > 0) {
        if (this->mem_size_of(handle_already_exist) == host_mem_size) {
            return -1;
        }
    }
    // not thread safe!
    size_t generated_handle = this->mr_map.size();
    this->mr_map[generated_handle] = memory_region;

    return generated_handle;
}

void ib_session::request_of_mem_dereg(const size_t host_mem_handle) {
    if (ibv_dereg_mr(this->mr_map[host_mem_handle]) != 0) {
        this->error(error_code::ibv_dereg_mr);
    }

    this->mr_map.erase(host_mem_handle);
}

int ib_session::request_of_sending(const size_t host_mem_handle,
    const size_t host_mem_offset,
    const size_t msg_sz) {
    // map에 없는 key를 들고왔을 경우
    if (this->mr_map.find(host_mem_handle) == this->mr_map.end()) {
        return -1;
    }

    // 메모리 크기 체크
    if (this->mr_map[host_mem_handle]->length < (msg_sz + host_mem_offset)) {
        return -1;
    }

    // 인피니밴드 카드가 지원하는 크기 이상으로 wr을 넣을 수 없으므로
    // 스트림으로 쪼개서 보낸다.
    const size_t stream_size = this->port_attr->max_msg_sz;
    this->atomic_total_stream_count = ceil((double)msg_sz/(double)stream_size);

    // 만약 스트림 개수가 지원하는 WR보다 많을 경우 오류를 내뿜는다.
    if (this->atomic_total_stream_count > (size_t)this->device_attr->max_qp_wr) {
        return -1;
    }

    // 상대방의 메모리의 어디서부터 얼만큼 꽂아넣어야 하는지 알기 위해
    // 메모리 주소, 오프셋, 리모트 키에 대한 정보를 받는다.
    const size_t message_content_count = 3;
    uint64_t message[message_content_count];
    memset(message, 0x00, message_content_count * sizeof(uint64_t));

    if (::recv(this->my_sock_fd, message, message_content_count * sizeof(uint64_t), 0) <= 0) {
        this->error(error_code::socket_recv);
        return -1;
    }

    uint64_t raddr = message[0];
    uint64_t roffset = message[1];
    uint64_t rkey = message[2];

    this->msg_comp_checker_thread->play();

    // Post sending on InfiniBand HCA
    for (size_t i = 0; i < this->atomic_total_stream_count; ++i) {
        ibv_sge sge;
        memset(&sge, 0x00, sizeof(sge));

        sge.addr = (uint64_t)(this->mr_map[host_mem_handle]->addr) + (uint64_t)i * (uint64_t)stream_size;
        if (i == atomic_total_stream_count - 1) {
            sge.length = msg_sz - i * stream_size;
        } else {
            sge.length = stream_size;
        }
        sge.lkey = this->mr_map[host_mem_handle]->lkey;

        ibv_send_wr wr, *bad_wr;
        memset(&wr, 0x00, sizeof(wr));

        wr.wr_id = i;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.next = nullptr;
        wr.opcode = IBV_WR_RDMA_WRITE;
        wr.send_flags = IBV_SEND_SIGNALED;

        wr.wr.rdma.remote_addr = (uint64_t)raddr + (uint64_t)roffset + (uint64_t)i * (uint64_t)stream_size;
        wr.wr.rdma.rkey = rkey;

        if (ibv_post_send(this->qp, &wr, &bad_wr)) {
            this->error(error_code::ibv_post_send);
        }
    }

    return 0;
}

int ib_session::request_of_receiving(const size_t host_mem_handle,
    const size_t host_mem_offset,
    const size_t msg_sz) {
    // map에 없는 key를 들고왔을 경우
    if (this->mr_map.find(host_mem_handle) == this->mr_map.end()) {
        return -1;
    }

    // 메모리 크기 체크
    if (this->mr_map[host_mem_handle]->length < (msg_sz + host_mem_offset)) {
        return -1;
    }

    const size_t message_content_count = 3;
    uint64_t message[message_content_count];
    memset(message, 0x00, sizeof(*message)*message_content_count);

    message[0] = (uintptr_t)this->mr_map[host_mem_handle]->addr;
    message[1] = host_mem_offset;
    message[2] = this->mr_map[host_mem_handle]->rkey;

    if (::send(this->my_sock_fd, message, message_content_count * sizeof(uint64_t), 0) == -1) {
        this->error(error_code::socket_send);
        return -1;
    }

    // RDMA send를 한 곳의 신호를 기다린다.
    uint64_t complete_signal = 0x0000000000000000;
    if (::recv(this->my_sock_fd, &complete_signal, sizeof(complete_signal), 0) <= 0) {
        this->error(error_code::socket_recv);
        return -1;
    }

    if (complete_signal != 0xFFFFFFFFFFFFFFFF) {
        return -1;
    }

    return 0;
}
