#include "ib_session.h"

void ib_session::msg_comp_checker() {
    void* cq_context;
    for (size_t i = 0; i < this->atomic_total_stream_count; i++) {
        if (ibv_req_notify_cq(this->cq, 0)) {
            this->error(error_code::ibv_req_notify_cq);
        }

        if (ibv_get_cq_event(this->comp_channel, &this->cq, &cq_context)) {
            this->error(error_code::ibv_get_cq_event);
        }

        ibv_ack_cq_events(this->cq, 1);
    }
}

void ib_session::mem_req_consumer() {
    while (true) {
        try {
            if (this->mem_req_q.pop()()) {
                printf("bad_request\n");
                continue;
            }
        } catch (error_code_t e) {
            this->error_message(e);
        }
    }
}

void ib_session::transmit_req_consumer() {
    while (true) {
        try {
            if (this->mem_req_q.pop()()) {
                printf("bad_request\n");
                continue;
            }
        } catch (error_code_t e) {
            this->error_message(e);
        }
    }
}
