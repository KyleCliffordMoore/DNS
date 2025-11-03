#include "pch.h"

// Note to future self: the AAAA are wrong and should not be printed according to the doc

int main(int argc, char **argv) {
    
    char *hostname = NULL;
    char *dns_ip = NULL;

    if (!handleUserInput(argc, argv, hostname, dns_ip))
        return 1;

    if (!startWSA())
        return 1;

    // Create UDP Socket
    SOCKET sock;
    if (!createAndBindUDPSocket(sock))
        return 1;

    // Create Packet
    unsigned char packet[MAX_PACKET_BUFFER_SIZE];
    USHORT packet_size;
    USHORT TXID;
    if (!createPacket(hostname, packet, packet_size, TXID))
        return 1;
    printf("Server  : %s\n********************************\n", dns_ip);


    // Send packet
    if (!sendPacket(sock, dns_ip, packet, packet_size))
        return 1;

    // Recieve packet
    unsigned char response_packet[MAX_PACKET_BUFFER_SIZE];
    USHORT response_packet_size;
    if (!recvPacket(sock, response_packet, packet_size, response_packet_size))
        return 1;

    //printPacketAsHex(response_packet, response_packet_size);
        

    

    struct HEADER header;
    int position = readHeader(response_packet, &header, TXID);

    position = readQuestions(response_packet, response_packet_size, position, header.num_questions);
    position = printResourceRecords(response_packet, response_packet_size, position, "answers",    header.num_answers);
    position = printResourceRecords(response_packet, response_packet_size, position, "authority",  header.num_authorities);
    position = printResourceRecords(response_packet, response_packet_size, position, "additional", header.num_additional);

    //printf("\n\n");
    
    closesocket(sock);
    WSACleanup();
    return 0;
}

