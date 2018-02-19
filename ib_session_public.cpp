#include "ib_session.h"

void ib_session::mem_reg(void* host_mem_addr, const size_t host_mem_size) {
    mem_req_q.push(std::bind(this->request_of_mem_reg, host_mem_addr, host_mem_size));
}

void ib_session::mem_dereg(const size_t host_mem_handle) {
    mem_req_q.push(std::bind(this->request_of_mem_reg, host_mem_handle));
}

void ib_session::sending(const size_t host_mem_handle,
    const size_t host_mem_offset, const size_t msg_sz) {
    transfer_req_q.push(std::bind(this->request_of_send,
        host_mem_handle, host_mem_offset, msg_sz));
}

void ib_session::receiving(const size_t host_mem_handle,
    const size_t host_mem_offset, const size_t msg_sz) {
    transfer_req_q.push(std::bind(this->request_of_send,
        host_mem_handle, host_mem_offset, msg_sz));
}


int ib_session::sending_wait() {
    if (this->is_request_reserved == false) {
        return -1;
    }

    if (this->is_request_sending == false) {
        return -1;
    }

    // Wait interrupt from InfiniBand HCA
    this->msg_comp_checker_thread->wait();

    // Send complete signal to remote node
    uint64_t complete_signal = 0xFFFFFFFFFFFFFFFF;
    if (::send(this->my_sock_fd, &complete_signal, sizeof(complete_signal), 0) == -1) {
        this->error(error_code::socket_send);
        return -1;
    }

    this->is_request_reserved = false;

    return 0;
}

int ib_session::receiving_wait() {
    if (this->is_request_reserved == false) {
        return -1;
    }

    if (this->is_request_sending == true) {
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

    this->is_request_reserved = false;

    return 0;
}

bool ib_session::is_responder() {
    return this->responder;
}

size_t ib_session::mem_handle_count() {
    return this->mr_map.size();
}

bool ib_session::mem_is_exist(int handle) {
    if (handle < 0) {
        return false;
    }

    if (this->mr_map.count(handle) == 0) {
        return false;
    } else {
        return true;
    }
}

void* ib_session::mem_address_of(int handle) {
    if (handle < 0) {
        return nullptr;
    }

    if (this->mr_map.count(handle) == 0) {
        return nullptr;
    } else {
        return this->mr_map[handle]->addr;
    }
}

size_t ib_session::mem_size_of(int handle) {
    if (handle < 0) {
        return 0;
    }

    if (this->mr_map.count(handle) == 0) {
        return 0;
    } else {
        return this->mr_map[handle]->length;
    }
}

int ib_session::mem_handle_of(void* addr) {
    if (addr == nullptr) {
        return -1;
    }

    for (auto i = this->mr_map.begin(); i != this->mr_map.end(); ++i) {
        if ((uint64_t)i->second->addr == (uint64_t)(uintptr_t)addr) {
            return i->first;
        }
    }

    return -1;
}
