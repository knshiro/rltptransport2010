#include "rtlp.h"
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct pkbuf* create_pkbuf(struct pkbuf* buff, int type,int seqnbr,int msg_size,char * payload);

int create_udp_payload(struct pkbuf* packet, char * rtlp_packet);

int send_packet(struct pkbuf* packet, int sockfd, struct sockaddr_in serv_addr);

struct pkbuf* udp_to_pkbuf(struct pkbuf* pkbuffer, char * udppacket);
