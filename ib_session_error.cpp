#include "ib_session.h"

void ib_session::error(const ib_session::error_code e) {
    printf("[IB SESSION ERROR] ");
    switch (e) {
        case error_code::ibv_get_device_list:
            printf("ibv_get_device_list\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::no_such_device:
            printf("no_such_device\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::no_such_port:
            printf("no_such_port\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_open_device:
            printf("ibv_open_device\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_create_comp_channel:
            printf("ibv_create_comp_channel\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_query_device:
            printf("ibv_query_device\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_query_port:
            printf("ibv_query_port\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_reg_mr:
            printf("ibv_reg_mr\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_alloc_pd:
            printf("ibv_alloc_pd\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_create_cq:
            printf("ibv_create_cq\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_create_qp:
            printf("ibv_create_qp\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_modify_qp:
            printf("ibv_modify_qp\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_post_send:
            printf("ibv_post_send\n");
            break;
        case error_code::ibv_post_recv:
            printf("ibv_post_recv\n");
            break;
        case error_code::ibv_dereg_mr:
            printf("ibv_dereg_mr\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_destroy_qp:
            printf("ibv_destroy_qp\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_destroy_cq:
            printf("ibv_destroy_cq\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_dealloc_pd:
            printf("ibv_dealloc_pd\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_destroy_comp_channel:
            printf("ibv_destroy_comp_channel\n");
            break;
        case error_code::ibv_close_device:
            printf("ibv_close_device\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::ibv_req_notify_cq:
            printf("ibv_req_notify_cq\n");
            break;
        case error_code::ibv_get_cq_event:
            printf("ibv_get_cq_event\n");
            break;
        case error_code::socket_socket:
            printf("socket\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::socket_setsockopt:
            printf("setsockopt\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::socket_inaddr_none:
            printf("inaddr_none\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::socket_bind:
            printf("bind\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::socket_listen:
            printf("listen\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::socket_accept:
            printf("accept\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::socket_send:
            printf("send\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::socket_recv:
            printf("recv\n");
            exit(EXIT_FAILURE);
            break;
        case error_code::request_queue_wait:
            printf("request_queue_wait\n");
            exit(EXIT_FAILURE);
            break;
    }
}
