#include "rltp.h"
#include "tools.h"
#include <sys/select.h>

int rltp_connect(struct rltp_client_pcb *cpcb, char *dst_addr, int dst_port){

  int sockfd,i,srv;
  struct pkbuf *pkbuffer;
  pkbuffer = (struct pkbuf*)malloc(sizeof(struct pkbuf));
  struct timeval tv;
  char rltp_packet[RLTP_MAX_PAYLOAD_SIZE+12];
  fd_set readfds;
  
  /* Create socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
	  perror("ERROR opening socket");
  printf("Socket created\n");

  cpcb->sockfd = sockfd;
  printf("Socket %d stored in cpcb\n",sockfd);

  create_pkbuf(pkbuffer, RLTP_TYPE_SYN, 0,0, NULL);

  i=0;
  while(i<3) {
	  if(send_packet(pkbuffer, sockfd, dst_addr,dst_port) <0){
		  exit(-1);
	  }
	  cpcb->state = RLTP_STATE_SYN_SENT;
	  printf("State set %d\n",  cpcb->state);


	  /* clear the set ahead of time */
	  FD_ZERO(&readfds);
	  /* add our descriptors to the set */
	  FD_SET(sockfd, &readfds);

	  tv.tv_sec = 2;
     srv = select(sockfd+1, &readfds, NULL, NULL, &tv);
	  if (srv == -1) {
		  perror("select"); // error occurred in select()
	  } else if (srv == 0) {
		  printf("Timeout occurred! No data after 2 seconds.\n");
        i++;
	  } else {
		  // one or both of the descriptors have data
		  if (FD_ISSET(sockfd, &readfds)) {
		    bzero(rltp_packet, sizeof(rltp_packet));
        if(recv(sockfd,rltp_packet,sizeof(rltp_packet),0) < 0)
		    {
		      perror("Couldnt' receive from socket");
		    }

        udp_to_pkbuf(pkbuffer, rltp_packet);
        printf("Packet of type %d received\n", pkbuffer->hdr.type );
        if ( pkbuffer->hdr.type == RLTP_TYPE_ACK ) {
          printf("Connection successful\n");
          cpcb->state = RLTP_STATE_ESTABLISHED;
          break;
        } else {
          i++;
        }
	    }
	  }
  }

  return 0;
}
