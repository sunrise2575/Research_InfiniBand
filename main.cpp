// Time measurement
#include <chrono> // std::chrono::duration

// Thread
#include <mutex>
#include <future>
#include <thread>

// Containers
#include <vector>
#include <algorithm>

// InfiniBand
#include "ib_session.h"

constexpr size_t ONE_GIGABYTE = 1073741824;

void hex_test(const uint8_t* buffer, const size_t length, const uint8_t testing) {
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] != testing) {
            std::cout << "Bad Point: " << i << std::endl;
            break;
        }
    }
}

void hex_dump(const uint8_t* buffer, const size_t length) {
    size_t i;
    for (i = 0; i < length; i++) {
        if (i % 16 == 0) {
            printf("%#016lX ", (uint64_t)&buffer[i]);
        }
        printf("%02X ", buffer[i]);
        if (i % 16 - 15 == 0) {
            size_t j;
            printf(" ");
            for (j = i - 15; j <= i; j++) {
                if (isprint(buffer[j])) {
                    printf("%c", buffer[j]);
                } else {
                    printf(".");
                }
            }
            printf("\n");
        }
    }

    if (i % 16 != 0) {
        size_t j;
        size_t spaces = (length - i + 16 - i % 16) * 3 + 2;
        for (j = 0; j < spaces; j++) {
            printf(" ");
        }
        for (j = i - i % 16; j < length; j++) {
            if (isprint(buffer[j])) {
                printf("%c", buffer[j]);
            } else {
                printf(".");
            }
        }
    }
    printf("\n");
}

void test_thread(ib_session& session, const size_t iteration) {
    int handle = session.mem_handle_count() - 1;
    size_t msg_size = session.mem_size_of(handle);

    for (size_t i = 0; i < iteration; i++) {
        if (session.is_responder()) {
            session.sending(handle, 0, msg_size);
            session.sending_wait();
        } else {
            session.receiving(handle, 0, msg_size);
            session.receiving_wait();
        }
    }
}

void print_msg_size(const size_t message_byte) {
    if (message_byte < 1024)
        std::cout << (double)message_byte << "byte" << std::endl;
    else if (1024 <= message_byte && message_byte < 1048576)
        std::cout << (double)message_byte/(double)1024 << "KB" << std::endl;
    else if (1048576 <= message_byte && message_byte < 1073741824)
        std::cout << (double)message_byte/(double)1048576 << "MB" << std::endl;
    else
        std::cout << (double)message_byte/(double)1073741824 << "GB" << std::endl;
}

void print_throughput(const size_t message_byte, const double elapsed_time) {
    double throughput = (double)message_byte / (double)elapsed_time;

    if (throughput < 1024)
        std::cout << (double)throughput << "byte/s" << std::endl;
    else if (1024 <= throughput && throughput < 1048576)
        std::cout << (double)throughput/(double)1024 << "KB/s" << std::endl;
    else if (1048576 <= throughput && throughput < 1073741824)
        std::cout << (double)throughput/(double)1048576 << "MB/s" << std::endl;
    else
        std::cout << (double)throughput/(double)1073741824 << "GB/s" << std::endl;
}

void print_throughput(const double throughput) {
    if (throughput < 1024)
        std::cout << (double)throughput << "byte/s" << std::endl;
    else if (1024 <= throughput && throughput < 1048576)
        std::cout << (double)throughput/(double)1024 << "KB/s" << std::endl;
    else if (1048576 <= throughput && throughput < 1073741824)
        std::cout << (double)throughput/(double)1048576 << "MB/s" << std::endl;
    else
        std::cout << (double)throughput/(double)1073741824 << "GB/s" << std::endl;
}

void draw_histogram(const std::vector<double> result_vector, const size_t start, const size_t end, const size_t gap) {
    if ((end - start) % gap != 0) {
        return;
    }

    for (size_t i = start; i < end; i += gap) {
        if (i < 1024) {
            printf("%4.1lf", (double)i); printf("     ");
        } else if (1024 <= i && i < 1048576) {
            printf("%4.1lf", (double)i/(double)1024); printf("KB/s ");
        } else if (1048576 <= i && i < 1073741824) {
            printf("%4.1lf", (double)i/(double)1048576); printf("MB/s ");
        } else {
            printf("%4.1lf", (double)i/(double)1073741824); printf("GB/s ");
        }

        size_t count = 0;

        for (size_t j = 0; j < result_vector.size(); j++) {
            if ((double)i <= result_vector[j] && result_vector[j] < (double)(i + gap)) {
                printf("|");
                count++;
            }
        }

        if (count != 0) {
            printf(" %ld", count);
        }

        printf("\n");
    }

}

int main(int argc, char** argv) {
    bool is_responder = false;
    char* ip_addr     = nullptr;
    uint16_t ip_port  = 7777;
    size_t msg_size   = ONE_GIGABYTE * (size_t)1;
    size_t iteration  = 1;
    size_t thread_count = 20;

    int c = 0;
    while ((c = getopt(argc, argv, "ra:p:m:t:i:")) != -1) {
        switch (c) {
        case 'r': is_responder = true; break;
        case 'a': ip_addr = strdup(optarg); break;
        case 'p': ip_port = strtol(optarg, nullptr, 0); break;
        case 'm': msg_size = strtol(optarg, nullptr, 0); break;
        case 't': thread_count = strtol(optarg, nullptr, 0); break;
        case 'i': iteration = strtol(optarg, nullptr, 0); break;
        default:
            printf("Parameter error\n");
            exit(EXIT_FAILURE);
        }
    }

    const uint8_t content = 0xFF;

    // Initialize
    std::vector<uint8_t*> buffer_vector;
    std::vector<ib_session*> session_vector;
    std::vector<int> handle_vector;

    for (size_t i = 0; i < thread_count; i++) {
        session_vector.push_back(new ib_session(0, 1, ip_addr, ip_port, is_responder));
        buffer_vector.push_back((uint8_t*)malloc(msg_size));
        if (is_responder) {
            memset(buffer_vector[i], content, msg_size);
        } else {
            memset(buffer_vector[i], 0x00, msg_size);
        }
        handle_vector.push_back(session_vector[i]->mem_reg(buffer_vector[i], msg_size));
    }

    // Time measure start
    auto start = std::chrono::system_clock::now();

    // Launch threads
    std::vector<std::thread> thread_vector;
    for (size_t i = 0; i < thread_count; i++) {
        thread_vector.push_back(std::thread(&test_thread, std::ref(*session_vector[i]), iteration));
    }

    // Wait threads
    for (size_t i = 0; i < thread_count; i++) {
        thread_vector[i].join();
    }

    // Time measure done
    std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - start;

    // Display results
    size_t total_size = iteration * thread_count * msg_size;
    double avg_throughput = 0.0;
    avg_throughput = (double)total_size / elapsed.count();

    printf("Test Result\n");
    printf("    Time          : "); printf("%lf (sec)\n", elapsed.count());
    printf("    Message       : "); print_msg_size(msg_size);
    printf("    Threads       : "); printf("%ld\n", thread_count);
    printf("    Iteration     : "); printf("%ld\n", iteration);
    printf("    Total size    : "); print_msg_size(total_size);
    printf("    Avg Throughput: "); print_throughput(avg_throughput);

    // Check if transferred message is broken
    for (size_t i = 0; i < thread_count; i++) {
        if (!is_responder) {
            hex_test(buffer_vector[i], msg_size, content);
        }
        if (session_vector[i] != nullptr) {
            delete session_vector[i];
        }
        free(buffer_vector[i]);
    }

    return 0;
}
