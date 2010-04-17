#include "rtlp.h"
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
struct rtlp_client_pcb cpcb;
char *dst_addr= "127.0.0.1";
int check = rtlp_connect(&cpcb, dst_addr,4500);
printf("rtlp_connect ends : %i\n",check);

char *data = "SLIST";
void *data2 = data;
char entry[100];
char cmd[5];
char outfile[50];
char payloadbuff[RTLP_MAX_PAYLOAD_SIZE];

/*struct pkbuf pkbuffer;
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

cpcb.recv_buf[3].hdr.seqnbr = 3;
cpcb.recv_buf[2].hdr.seqnbr = 2;

swap(cpcb.recv_buf,2,3);
printf("Packet 2 : %d, Packet 3 : %d",cpcb.recv_buf[2].hdr.seqnbr,cpcb.recv_buf[3].hdr.seqnbr);
*/

printf(">>>>1st transfert\n");
//rtlp_transfer(&cpcb,data2,5,NULL);


while(1){

	
	//fgets(entry,100,stdin);
	scanf("%s",cmd);
	printf("Cmd : %s\n",cmd);
	if(strcmp(cmd,"r")==0){
		rtlp_transfer(&cpcb,NULL,0,NULL);	
	}
	if(strcmp(cmd,"q")==0){
		break;
	}

	if(strcmp(cmd,"SLIST")==0){
		rtlp_transfer(&cpcb,"SLIST",5,NULL);
	} 
	else if (strcmp(cmd,"CLIST")==0){
		//rtlp_transfer(&cpcb,"SLIST",5,outfile);
 	}
	else {
		
//		printf("%s : Sizeof %d, Strlen %d\n", entry, sizeof(entry), strlen(entry));
		if(strcmp(cmd,"GET")==0){
			scanf("%s",outfile);			
			strcpy(entry,cmd);			
			strcat(entry," ");
			strcat(entry,outfile);
			printf("Cmd : %s Outfile : %s, Entry: %s\n", cmd, outfile,entry);
			rtlp_transfer(&cpcb,entry,strlen(entry),"output");
			while(1){
				scanf("%s",cmd);
				if(strcmp(cmd,"r")==0){
					rtlp_transfer(&cpcb,NULL,0,"output");	
				}
				if(strcmp(cmd,"q")==0){
					break;
				}
			}
			break;
		}
	}
	
}

return 0;
}




int rtlp_test(struct rtlp_client_pcb *cpcb){
 
int sockfd,i,srv,numRead;
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
        	if(numRead = recv(sockfd,rtlp_packet,sizeof(rtlp_packet),0) < 0)
		{
		      perror("Couldnt' receive from socket");
		}

        	udp_to_pkbuf(pkbuffer, rtlp_packet, numRead);
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
