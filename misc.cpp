#pragma once
#include "pch.h"

bool handleUserInput(int argc, char** argv, char*& hostname, char*& dns_ip) {

	if (argc != 3) {
		printf("Usage: <hostname> <dns_ip>\n");
		return false;
	}

	hostname = argv[1];
	dns_ip = argv[2];

	return true;
}

bool startWSA() {
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		printf("WSA failed? :(\n");
		return false;
	}

	return true;
}


clock_t clockDelta(clock_t startTime, clock_t endTime) {
	return double(endTime - startTime); // / CLOCKS_PER_SEC
}