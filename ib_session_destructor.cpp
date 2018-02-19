#include "ib_session.h"

ib_session::~ib_session() {
    // if (this->msg_comp_checker_thread != nullptr) {
    //     delete this->msg_comp_checker_thread;
    // }

    ::close(this->my_sock_fd);

    for (auto iter = mr_map.begin(); iter != mr_map.end(); ++iter) {
        mem_dereg(iter->first);
    }

    if (this->device_attr != nullptr) {
        free(this->device_attr);
    }

    if (this->port_attr != nullptr) {
        free(this->port_attr);
    }

    if (this->qp != nullptr) {
        if (ibv_destroy_qp(this->qp) != 0) {
            perror("[IBSESSION] ibv_destroy_qp()");
            this->error(error_code::ibv_destroy_qp);
        }
    }

    if (this->cq != nullptr) {
        if (ibv_destroy_cq(this->cq) != 0) {
            perror("[IBSESSION] ibv_destroy_cq()");
            this->error(error_code::ibv_destroy_cq);
        }
    }

    if (this->comp_channel != nullptr) {
        if (ibv_destroy_comp_channel(this->comp_channel) != 0) {
            this->error(error_code::ibv_destroy_comp_channel);
        }
    }

    if (this->pd != nullptr) {
        if (ibv_dealloc_pd(this->pd) != 0) {
            perror("[IBSESSION] ibv_dealloc_pd()");
            this->error(error_code::ibv_dealloc_pd);
        }
    }

    if (this->context != nullptr) {
        if (ibv_close_device(this->context) != 0) {
            perror("[IBSESSION] ibv_close_device()");
            this->error(error_code::ibv_close_device);
        }
    }

    if (this->device_list != nullptr) {
        ibv_free_device_list(this->device_list);
    }
}
