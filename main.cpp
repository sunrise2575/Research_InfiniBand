#include <iostream>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include "ib_session.h"

void usage() {
	std::cout
		<< "Usage\n"
		<< "\t-l : is this listener?\n"
		<< "\t-a : address (listener doesn't need this)\n"
		<< "\t-p : port\n" << std::endl;
}

void hex_test(const uint8_t* buffer, const size_t length, const uint8_t testing) {
	for (size_t i = 0; i < length; i++) {
		if (buffer[i] != testing) {
			std::cout << "Bad Point: " << i << std::endl;
			break;
		}
	}
}

int main(int argc, char** argv) {
	if (argc <= 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	bool is_listener = false;
	char* addr = nullptr;
	uint16_t port = 7777;
	char message[1024];

	int c = 0;
	while ((c = getopt(argc, argv, "la:p:")) != -1) {
		switch (c) {
		case 'l': is_listener = true; break;
		case 'a': addr = strdup(optarg); break;
		case 'p': port = strtol(optarg, nullptr, 0); break;
		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}

	ib_session ibs(0, 1, message, 1024);
	memset(message, 0, 1024);
	
	if (is_listener) {
		ibs.listen(port);
		while (true) {
			scanf("%s", message);
			ibs.rdma_write(0, 0, strlen(message));
		}
	} else {
		ibs.connect(addr, port);
		while (true) {
			usleep(100);
			if (strlen(message) > 0) {
				printf("%s\n", message);
				memset(&message, 0, strlen(message));
			}
		}
	}

	return 0;
}
