#include "rltp.h"
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct pkbuf* create_pkbuf(struct pkbuf* buff, int type,int seqnbr,int msg_size,int len, char * payload);

int send_packet(struct pkbuf* packet, int sockfd, char *dst_addr, int dst_port);
