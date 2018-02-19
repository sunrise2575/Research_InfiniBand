#ifndef IB_SESSION_HEADER
#define IB_SESSION_HEADER

#include "thread_safe_struct.h"
#include "play_n_stop_thread.h"

// C++11 headers for multithread
#include <thread> // std::thread
#include <atomic> // std::atomic
#include <mutex> // std::mutex, std::lock_guard, std::unique_lock
#include <condition_variable> // std::condition_variable

// C++11 containers & types
#include <functional> // std::function
#include <map> // std::map

// C++11 misc headers
#include <iostream> // std::cout, std::endl

// C headers
#include <cstring> // malloc(), strdup()
#include <cstdlib> // lrand48()
#include <cmath> // ceil()

// linux only headers
#include <unistd.h> // getopt()
#include <errno.h>

// socket headers
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// infiniband headers
#include <infiniband/verbs.h>

class ib_session {
private:
    enum class error_code : int {
        ibv_get_device_list,
        no_such_device,
        no_such_port,
        ibv_open_device,
        ibv_create_comp_channel,
        ibv_query_device,
        ibv_query_port,
        ibv_reg_mr,
        ibv_alloc_pd,
        ibv_create_cq,
        ibv_create_qp,
        ibv_modify_qp,
        ibv_post_send,
        ibv_post_recv,
        ibv_dereg_mr,
        ibv_destroy_qp,
        ibv_destroy_cq,
        ibv_dealloc_pd,
        ibv_destroy_comp_channel,
        ibv_close_device,
        ibv_req_notify_cq,
        ibv_get_cq_event,
        socket_socket,
        socket_setsockopt,
        socket_inaddr_none,
        socket_bind,
        socket_listen,
        socket_accept,
        socket_send,
        socket_recv,
        request_queue_wait
    };

    play_n_stop_thread* msg_comp_checker_thread;

    bool responder;
    int my_sock_fd;

    // 인피니밴드 구조체들
    ibv_device** device_list = nullptr;
    ibv_device_attr* device_attr = nullptr;
    ibv_port_attr* port_attr = nullptr;
    ibv_context* context = nullptr;

    ibv_pd* pd = nullptr;
    ibv_cq* cq = nullptr;
    ibv_qp* qp = nullptr;

    ibv_comp_channel* comp_channel = nullptr;

    // 현재 전송되는 스트림의 갯수
    std::atomic<size_t> atomic_total_stream_count;

    // 등록된 메모리 영역이 들어가는 맵
    thread_safe::map<size_t, ibv_mr*> mr_map;
    // 메모리 리퀘스트가 들어가는 큐
    thread_safe::queue<std::function<void()> > mem_req_q;
    // 전송/수신 리퀘스트가 들어가는 큐
    thread_safe::list<std::function<void()> > transmit_req_q;
    // 에러가 쌓이는 큐
    thread_safe::queue<std::function<void()> > error_q;

    // 에러 메시지 처리
    void error(const error_code e);

    // 쓰레드로 돌아가는 함수들
    // IB에서 방금 보낸 메시지가 완료했는지 체크하는 놈
    // play_n_stop_thread에서 돌아간다.
    void msg_comp_checker();
    // 메모리 리퀘스트를 처리해주는 consumer
    void mem_req_consumer();
    // 전송/수신 리퀘스트를 처리해주는 consumer
    void transmit_req_consumer();

    // 실제로 동작하는 함수
    int request_of_mem_reg(void* host_mem_addr, const size_t host_mem_size);
    void request_of_mem_dereg(const size_t host_mem_handle);
    int request_of_sending(const size_t host_mem_handle,
        const size_t host_mem_offset,
        const size_t msg_sz);
    int request_of_receiving(const size_t host_mem_handle,
        const size_t host_mem_offset,
        const size_t msg_sz);

public:
    // 현재 에러가 몇개 쌓였는지
    size_t get_error_count() {
        return error_q.size();
    }

    // 생성자
    ib_session(const size_t ib_device_num,
        const int ib_physical_port_num,
        const char* dest_ip_addr,
        const uint16_t ip_port,
        const bool is_responder);

    // 소멸자
    ~ib_session() noexcept;

    // 메모리 등록 요청을 넣는 함수
    int mem_reg(void* host_mem_addr, const size_t host_mem_size);

    // 메모리 등록해제 요청을 넣는 함수
    void mem_dereg(const size_t host_mem_handle);

    // 메시지 송신 요청을 넣는 함수
    int sending(const size_t host_mem_handle,
        const size_t host_mem_offset,
        const size_t msg_sz);

    // 메시지 수신 요청을 넣는 함수
    int receiving(const size_t host_mem_handle,
        const size_t host_mem_offset,
        const size_t msg_sz);

    // 현재 큐에 넣은 송신 요청이 다 처리될 때 까지 기다리는 함수.
    int sending_wait();

    // 현재 큐에 넣은 수신 요청이 다 처리될 때 까지 기다리는 함수.
    int receiving_wait();

    // responder/requester 상태 알 수 있는 함수
    bool is_responder();

    // 등록된 메모리를 조회할 수 있는 함수들
    size_t mem_handle_count();
    bool mem_is_exist(int handle);
    void* mem_address_of(int handle);
    size_t mem_size_of(int handle);
    int mem_handle_of(void* addr);
};

#endif
