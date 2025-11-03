#pragma once

// Types
#define TYPE_A		0x01
#define TYPE_NS		0x02
#define TYPE_CNAME	0x05
#define TYPE_PTR	0x0c
#define TYPE_HINFO	0x0d
#define TYPE_MX		0x0f
#define TYPE_AAAA	0x1c
#define TYPE_AXFR	0xfc
#define TYPE_ANY	0xff

// Classes
#define CLASS_INET	0x01

// Results
#define RESULT_OK			0x00
#define RESULT_FORMAT		0x01
#define RESULT_SERVERFAIL	0x02
#define RESULT_ERROR		0x03
#define RESULT_NOTIMPL		0x04
#define RESULT_REFUSED		0x05

// Flags
#define FLAG_QUERY			(0 << 15)
#define FLAG_RESPONSE		(1 << 15)
#define FLAG_STDQUERY		(0 << 11)
#define FLAG_AUTH_ANS		(1 << 10)
#define FLAG_TRUNC			(1 <<  9)
#define FLAG_RECUR_DESIR	(1 <<  8)
#define FLAG_RECUR_AVAIL	(1 <<  7)

// Sizes
#define MAX_PACKET_BUFFER_SIZE	512
#define MAX_LABEL_SIZE			63
#define MAX_QUERYNAME_SIZE		255
#define MAX_ARPA_IPV4_SIZE		29

// Connection stuff
#define PORT					53
#define TIMEOUT_SEC				10
#define TIMEOUT_U_SEC			0
#define MAX_ATTEMPTS			3

struct HEADER {
	USHORT id;
	USHORT flags;
	USHORT num_questions;
	USHORT num_answers;
	USHORT num_authorities;
	USHORT num_additional;
};

struct QUESTION {
	USHORT type;
	USHORT class_;
};

#pragma pack(push, 1) // According to the sheet, their should be no padding
struct RHEADER {
	USHORT type;
	USHORT class_;
	UINT ttl;
	USHORT length;
};
#pragma pack(pop)

struct RRECORD {
	unsigned char name[MAX_QUERYNAME_SIZE];
	struct RHEADER rheader;
	unsigned char rdata[MAX_PACKET_BUFFER_SIZE];
};

/**
 * @brief Converts an ipv4 address into an arpa address.
 * @param ipv4_address - an ipv4 address
 * @param arpa_address - the arpa_address to be created
 */
void ipv4_to_arpa(const char* ipv4_address, char* arpa_address);

/**
 * @brief Converts a hostname into a queryname.
 * @param ipv4_address - a hostname
 * @param arpa_address - the queryname to be created
 */
void hostname_to_queryname(const char* hostname, unsigned char* queryname);

/**
 * @brief Prints the packet as hex.
 * @param packet - the DNS packet to be printed
 */
void printPacketAsHex(const unsigned char* packet, const USHORT packet_size);

/**
 * @brief Creates and the binds the socket to any network device.
 * @param socket - the socket that will be created and bound
 * @return true on success, otherwise false
 */
bool createAndBindUDPSocket(SOCKET& sock);

/**
 * @brief Creates a DNS packet.
 * @param hostname - the name of the host
 * @param packet_buffer - the packet to be created
 * @return true on success, otherwise false
 */
bool createPacket(const char* hostname, unsigned char* packet_buffer, USHORT& packet_size, USHORT& generated_TXID);

/**
 * @brief Sends the DNS packet.
 * @param socket - the socket that will be used
 * @param dns_ip - the ip of the DNS that will be used
 * @param packet_buffer - the packet that will be sent
 * @return true on success, otherwise false
 */
bool sendPacket(const SOCKET& sock, const char* dns_ip, const unsigned char* packet_buffer, USHORT& packet_size);

/**
 * @brief Recieves a DNS packet
 * @param socket - the socket that will be used
 * @param response_packet - the buffer that will be filled with the contents of the response packet
 * @return true on success, otherwise false
 */
bool recvPacket(const SOCKET& sock, unsigned char* response_packet, const USHORT packet_size_FOR_DEBUGGING_THIS_IS_NOT_A_PTR_DO_NOT_USE_THIS, USHORT& recv_size);

bool getTypeName(USHORT type, char* name, size_t name_size);
int readHeader(const unsigned char* response_packet, struct HEADER* header, const USHORT TXID_CHECK);
int readName(const unsigned char* response_packet, int position, unsigned char* name, const USHORT packet_size);
int readQuestions(const unsigned char* response_packet, const USHORT response_packet_size, int position, int num_questions);

int readResourceRecord(const unsigned char* response_packet, const USHORT response_packet_size, int position, struct RRECORD* rrecord);

void printRecord(struct RRECORD* rrecord, const unsigned char* response_packet, const USHORT response_packet_size, const USHORT LAST_POSITION);
int printResourceRecords(const unsigned char* response_packet, const USHORT response_packet_size, int position, const char* label, USHORT count);