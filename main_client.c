#include "rtlp.h"
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
struct rtlp_client_pcb *cpcb = (struct rtlp_client_pcb *)malloc(sizeof(struct rtlp_client_pcb));
char *dst_addr= "127.0.0.1";
int check = rtlp_connect(cpcb, dst_addr,4500);
printf("rtlp_connect ends : %i\n",check);

char *data = "SLIST";
void *data2 = data;
char payloadbuff[RTLP_MAX_PAYLOAD_SIZE];

struct pkbuf pkbuffer;
bzero(payloadbuff,sizeof(payloadbuff));
memcpy(payloadbuff,data2,RTLP_MAX_PAYLOAD_SIZE);
create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,3,1,payloadbuff,RTLP_MAX_PAYLOAD_SIZE);
printf("data envoyé: %s\n",pkbuffer.payload);
if(send_packet(&pkbuffer, cpcb->sockfd, cpcb->serv_addr) <0){
	exit(-1);
}
check = rtlp_test(cpcb);
/*
int check2 = rtlp_close(cpcb);
printf("rtlp_close ends: %i\n",check2);
*/
return 0;
}




int rtlp_test(struct rtlp_client_pcb *cpcb){
 
int sockfd,i,srv;
sockfd=cpcb->sockfd;
struct pkbuf *pkbuffer;
pkbuffer = (struct pkbuf*)malloc(sizeof(struct pkbuf));

char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];
fd_set readfds;

while(1) {

srv = select(sockfd+1, &readfds, NULL, NULL, 0);
if (srv == -1) {
	perror("select"); // error occurred in select()
} else if (srv == 0) {
	printf("Timeout occurred! No data after 2 seconds.\n");
        i++;
} else {
	// one or both of the descriptors have data
	if (FD_ISSET(sockfd, &readfds)) {
		bzero(rtlp_packet, sizeof(rtlp_packet));
        	if(recv(sockfd,rtlp_packet,sizeof(rtlp_packet),0) < 0)
		{
		      perror("Couldnt' receive from socket");
		}

        	udp_to_pkbuf(pkbuffer, rtlp_packet);
        	printf("PPPPacket of type %d received\n", pkbuffer->hdr.type );
        	if ( pkbuffer->hdr.type == RTLP_TYPE_DATA ) {
          		printf("Received Data\n");
         		printf("pkbuffer.payload: %s\n",pkbuffer->payload);	
          		return 0;
        	} else {
          		rtlp_client_reset(cpcb);
          		printf("Server misbehaviour\n");
         		return -1;
        	}
	}
}
}

}
