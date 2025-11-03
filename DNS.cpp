#include "pch.h"

void hostname_to_queryname(const char* hostname, unsigned char* queryname) {

	char* label_start = (char*)hostname;
	unsigned char* queryname_ptr = queryname;

	for (const char* ptr = label_start; true; ptr++) {

		if (*ptr == '.' || *ptr == '\0') {
			size_t label_length = ptr - label_start;

			if (label_length > MAX_LABEL_SIZE)
				label_length = MAX_LABEL_SIZE;

			*queryname_ptr = (unsigned char)label_length;
			queryname_ptr++;


			memcpy(queryname_ptr, label_start, label_length);
			queryname_ptr += label_length;

			label_start = (char*)(ptr + 1);

			if (*ptr == '\0')
				break;

		}

	}

	*queryname_ptr = NULL;

}

void ipv4_to_arpa(const char* ipv4_address, char* arpa_address) {
	unsigned int first;
	unsigned int second;
	unsigned int third;
	unsigned int fourth;
	// Cool flipping trick :D
	if (sscanf_s(ipv4_address, "%3u.%3u.%3u.%3u", &first, &second, &third, &fourth) == 4)
		snprintf(arpa_address, MAX_ARPA_IPV4_SIZE, "%u.%u.%u.%u.in-addr.arpa", fourth, third, second, first);
	else arpa_address[0] = NULL;
}

bool createAndBindUDPSocket(SOCKET& sock) {
	
	
	// Create a udp socket
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		printf("Failed to create socket\n");
		return false;
	}


	// Bind to anything
	struct sockaddr_in temp;
	memset(&temp, 0, sizeof(temp));
	temp.sin_family = AF_INET;
	temp.sin_addr.s_addr = INADDR_ANY;
	temp.sin_port = htons(0);

	if (bind(sock, (struct sockaddr*)&temp, sizeof(temp)) == SOCKET_ERROR) {
		printf("Failed to bind socket\n");
		closesocket(sock);
		return false;
	}

	return true;
}

bool createPacket(const char *hostname, unsigned char *packet_buffer, USHORT& packet_size, USHORT& generated_TXID) {
	char display_name[MAX_QUERYNAME_SIZE]; // for dbg output


	// Create DNS header
	HEADER* dns_header = (HEADER*)packet_buffer;
	dns_header->id = htons(rand() & 0xFFFF); // I do not think this actually matters what it is?
	dns_header->flags = htons(FLAG_QUERY | FLAG_STDQUERY | FLAG_RECUR_DESIR); // std query flag according to the slides
	dns_header->num_questions = htons(1);
	dns_header->num_answers = htons(0);
	dns_header->num_authorities = htons(0);
	dns_header->num_additional = htons(0);

	// save txid
	generated_TXID = ntohs(dns_header->id);

	// Determine query type
	unsigned short query_type;
	unsigned char query_name[MAX_QUERYNAME_SIZE];
	if (inet_addr(hostname) != INADDR_NONE) {
		// Is ip address
		char arpa[MAX_ARPA_IPV4_SIZE];
		ipv4_to_arpa(hostname, arpa);
		hostname_to_queryname(arpa, query_name);
		query_type = TYPE_PTR;

		strncpy_s(display_name, arpa, sizeof(display_name));
	}
	else {
		// Not ip address
		hostname_to_queryname(hostname, query_name);
		query_type = TYPE_A;

		strncpy_s(display_name, hostname, sizeof(display_name));
	}


	// Add questions
	USHORT OFFSET = sizeof(HEADER);
	USHORT queryname_length = (USHORT) strlen((const char*)query_name) + 1; // + 1 for null term
	if ((OFFSET + queryname_length + sizeof(QUESTION)) > MAX_PACKET_BUFFER_SIZE) {
		// uh oh!
		printf("Exceeded packet length? :(\n");
		return false;
	}
	memcpy(packet_buffer + OFFSET, query_name, queryname_length);
	OFFSET += queryname_length;

	QUESTION question;
	question.type = htons(query_type);
	question.class_ = htons(CLASS_INET);
	memcpy(packet_buffer + OFFSET, &question, sizeof(question));
	OFFSET += sizeof(question);


	packet_size = OFFSET;

	printf("Lookup	: %s\n", hostname);
	printf("Query	: %s, type %d, TXID 0x%04X\n", display_name, query_type, ntohs(dns_header->id));
	

	return true;
}

bool sendPacket(const SOCKET& sock, const char* dns_ip, const unsigned char* packet_buffer, USHORT& packet_size) {


	// Create desination struct
	struct sockaddr_in destination;
	memset(&destination, 0, sizeof(destination));
	destination.sin_family = AF_INET;
	destination.sin_port = htons(PORT);
	if (inet_pton(AF_INET, dns_ip, &destination.sin_addr) != 1) {
		printf("Invalid DNS ip: %s\n", dns_ip);
		return false;
	}

	// NOTE: might need to check what the packet size is!!!!!
	// Send packet
	if (
		sendto(
			sock,
			(const char*)packet_buffer,
			packet_size,
			0,
			(struct sockaddr*)&destination,
			sizeof(destination)
		) == SOCKET_ERROR)
	{
		printf("sendto failed :(\n");
		return false;
	}

	return true;
}

bool recvPacket(const SOCKET& sock, unsigned char* response_packet, const USHORT packet_size_FOR_DEBUGGING_THIS_IS_NOT_A_PTR_DO_NOT_USE_THIS, USHORT& recv_size) {
	struct sockaddr_in from_sockaddr_in;
	socklen_t from_sockaddr_in_size = sizeof(from_sockaddr_in);
	int attempts = 0;

attempt:
	printf("Attempt %d with %u bytes... ", attempts, packet_size_FOR_DEBUGGING_THIS_IS_NOT_A_PTR_DO_NOT_USE_THIS);
	attempts++;

	// Setup timeout things
	clock_t startTime = clock();
	fd_set fd;
	FD_ZERO(&fd);
	FD_SET(sock, &fd);
	struct timeval time;
	time.tv_sec = TIMEOUT_SEC;
	time.tv_usec = TIMEOUT_U_SEC;
	int available = select(0, &fd, NULL, NULL, &time);
	clock_t endTime = clock();
	clock_t deltaTime = clockDelta(startTime, endTime);

	// Timed out
	if (available == 0) {
		printf("timeout in %d ms\n", deltaTime);
		if (attempts < MAX_ATTEMPTS)
			goto attempt;
		return false;
	}
	
	// Select failed?
	if (available < 0) {
		printf("socket error %d\n", WSAGetLastError());
		return false;
	}

	// recv bytes :D
	int num_bytes = recvfrom(
		sock,
		(char*)response_packet,
		MAX_PACKET_BUFFER_SIZE,
		0,
		(struct sockaddr*)&from_sockaddr_in,
		&from_sockaddr_in_size
	);


	if (num_bytes == SOCKET_ERROR) {
		printf("socket error %d\n", WSAGetLastError());
		return false;
	}

	if (num_bytes < sizeof(HEADER)) {
		printf("++ invalid reply: packet smaller than fixed DNS header\n");
		return false;
	}

	printf("response in %d ms with %d bytes\n", deltaTime, num_bytes);
	recv_size = num_bytes;
	return true;
}

void printPacketAsHex(const unsigned char* packet, const USHORT packet_size) {
	for (size_t i = 0; i < packet_size; i++)
		printf("%02X ", packet[i]);
	printf("\n");
}

bool getTypeName(USHORT type, char* name, size_t name_size) {
	const char* type_name = NULL;
	switch (type) {
	case TYPE_A:
		type_name = "A";
		break;
	case TYPE_NS:
		type_name = "NS";
		break;
	case TYPE_CNAME:
		type_name = "CNAME";
		break;
	case TYPE_PTR:
		type_name = "PTR";
		break;
	case TYPE_HINFO:
		type_name = "HINFO";
		break;
	case TYPE_MX:
		type_name = "MX";
		break;
	case TYPE_AAAA:
		type_name = "AAAA";
		break;
	case TYPE_AXFR:
		type_name = "AXFR";
		break;
	case TYPE_ANY:
		type_name = "ANY";
		break;
	default:
		type_name = "UKNOWN";
		snprintf(name, name_size, "%s", type_name);
		return false;
	}

	snprintf(name, name_size, "%s", type_name);
	return true;
}

int readHeader(const unsigned char *response_packet, struct HEADER *header, const USHORT TXID_CHECK) {

	memcpy(header, response_packet, sizeof(HEADER));

	//htons stuff
	header->id = ntohs(header->id);
	header->flags = ntohs(header->flags);
	header->num_questions = ntohs(header->num_questions);
	header->num_answers = ntohs(header->num_answers);
	header->num_authorities = ntohs(header->num_authorities);
	header->num_additional = ntohs(header->num_additional);

	printf
	(
		"  TXID 0x%04X flags 0x%04X questions %d answers %d authority %d additional %d\n",
		header->id,
		header->flags,
		header->num_questions,
		header->num_answers,
		header->num_authorities,
		header->num_additional
	);

	if (header->id != TXID_CHECK) {
		printf("++ invalid reply: TXID mismatch, sent 0x%04X, received 0x%04X", TXID_CHECK, header->id);
		exit(1);
	}

	const USHORT RESULT = header->flags & 0x000F; // last 4 bytes of the flags
	if (RESULT == RESULT_OK)
		printf("  succeeded with Rcode = %u\n", RESULT);
	else {
		printf("  failed with Rcode = %u\n", RESULT);
		exit(1);
	}
		

	// kind of not needed...
	return sizeof(HEADER);
}

int readName(const unsigned char *response_packet, int position, unsigned char* name, const USHORT packet_size) {

	int name_offset = 0;
	int offset = position;
	bool jumped = false;
	int jump_count = 0;

	while (true) {

		if (offset >= packet_size) {
			printf("++ invalid record: jump beyond packet boundary\n");
			exit(1);
		}

		// Get segment length
		unsigned char curr_byte = response_packet[offset];
		if (curr_byte == 0) {
			if (!jumped) offset++;
			break;
		}

		// check for possible jumping
		if (curr_byte >= 0xc0) {

			if (offset + 1 >= packet_size) {
				printf("++ invalid record: truncated jump offset\n");
				exit(1);
			}

			unsigned char high_order = (curr_byte & (0xff - 0xc0));
			unsigned char low_order = response_packet[offset + 1];

			int ptr = (high_order << 8) | low_order;



			if (ptr < sizeof(HEADER)) {
				printf("++ invalid record: jump into fixed DNS header\n");
				exit(1);
			}

			if (ptr >= packet_size) {
				printf("++ invalid record: jump beyond packet boundary\n");
				exit(1);
			}

			jump_count++; // this okay?
			if (jump_count > 20) {
				printf("++ invalid record: jump loop\n");
				exit(1);
			}


			if (!jumped)
				position = offset + 2;
			offset = ptr;
			jumped = true;
		}
		else {
			offset++;

			if (offset + curr_byte > packet_size) {
				printf(" ++ invalid record: truncated name\n");
				exit(1);
			}

			for (int i = 0; i < curr_byte; i++) {
				name[name_offset] = response_packet[offset];
				name_offset++;
				offset++;
			}
			name[name_offset] = '.';
			name_offset++;
		}

	}

	if (name_offset > 0)
		name[name_offset - 1] = '\0';
	else
		name[name_offset] = '\0';

	return jumped ? position : offset;
}

int readQuestions(const unsigned char *response_packet, const USHORT response_packet_size, int position, int num_questions) {

	printf("  ------------ [questions] ------------\n");
	for (int i = 0; i < num_questions; i++) {
	
		unsigned char name[MAX_QUERYNAME_SIZE];
		position = readName(response_packet, position, name, response_packet_size);

		struct QUESTION question;
		memcpy(&question, response_packet + position, sizeof(QUESTION));
		
		question.type = ntohs(question.type);
		question.class_ = ntohs(question.class_);
		
		position += sizeof(QUESTION);
		printf("	%s type %u class %u\n", name, question.type, question.class_);
	}

	return position;
}

int readResourceRecord(const unsigned char *response_packet, const USHORT response_packet_size, int position, struct RRECORD *rrecord) {

	position = readName(response_packet, position, rrecord->name, response_packet_size);

	if (position + sizeof(RHEADER) > response_packet_size) {
		printf("++ invalid record: truncated RR answer header\n");
		exit(1);
	}

	memcpy(&rrecord->rheader, response_packet + position, sizeof(RHEADER));
	position += sizeof(RHEADER);

	rrecord->rheader.type = ntohs(rrecord->rheader.type);
	rrecord->rheader.class_ = ntohs(rrecord->rheader.class_);
	rrecord->rheader.ttl = ntohl(rrecord->rheader.ttl);
	rrecord->rheader.length = ntohs(rrecord->rheader.length);

	if (position + rrecord->rheader.length > response_packet_size) {
		printf("++ invalid record: RR value length stretches the answer beyond packet\n");
		exit(1);
	}

	memcpy(rrecord->rdata, response_packet + position, rrecord->rheader.length); // no flip?
	position += rrecord->rheader.length;

	return position;
}

void printRecord(struct RRECORD *rrecord, const unsigned char* response_packet, const USHORT response_packet_size, const USHORT LAST_POSITION) {

	

	char type[12];
	getTypeName(rrecord->rheader.type, type, 12);
	

	if (rrecord->rheader.type == TYPE_A && rrecord->rheader.length == 4) {
		// ipv4
		printf("	%s ", rrecord->name);
		printf("%s ", type);
		unsigned char* ip_address = rrecord->rdata;
		printf("%u.%u.%u.%u ", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

	}
	if (
		rrecord->rheader.type == TYPE_CNAME ||
		rrecord->rheader.type == TYPE_NS	||
		rrecord->rheader.type == TYPE_PTR 
	) {
		unsigned char cname[MAX_QUERYNAME_SIZE];
		int off = (int)(LAST_POSITION - rrecord->rheader.length);
		//printf("posiition:%u", off);
		readName(response_packet, off, cname, response_packet_size);
		cname[MAX_QUERYNAME_SIZE - 1] = '\0';
		printf("	%s ", rrecord->name);
		printf("%s ", type);
		printf("%s ", cname);
	}

	printf("TTL = %u\n", rrecord->rheader.ttl);
}

int printResourceRecords(const unsigned char* response_packet, const USHORT response_packet_size, int position, const char *label, USHORT count) {
	if (count > 0)
		printf("  ------------ [%s] ------------\n", label);
	for (int i = 0; i < count; i++) {

		//printf("pos: %d\npacket_size %u\n", position, response_packet_size);
		if (position >= response_packet_size) {
			printf("++ invalid section: not enough records (e.g., declared 5 answers but only 3 found)\n");
			exit(1);
		}

		struct RRECORD rrecord;
		position = readResourceRecord(response_packet, response_packet_size, position, &rrecord);
		printRecord(&rrecord, response_packet, response_packet_size, position);
	}

	return position;
}