#include "rtlp.h"
#include "tools.h"
#include <sys/select.h>

int rtlp_connect(struct rtlp_client_pcb *cpcb, char *dst_addr, int dst_port){

  int sockfd,i,srv;
  struct pkbuf *pkbuffer;
  pkbuffer = (struct pkbuf*)malloc(sizeof(struct pkbuf));
  struct timeval tv;
  char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];
  fd_set readfds;

  struct hostent *server;

  /* Create socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
	  perror("ERROR opening socket");
  printf("Socket created\n");

  cpcb->sockfd = sockfd;
  printf("Socket %d stored in cpcb\n",sockfd);


  /* Get Server address from supplied hostname */
  server = gethostbyname(dst_addr);
  if (server == NULL) {
	  fprintf(stderr,"ERROR, no such host\n");
	  exit(0);
  }
  printf("Name resolved\n");

  /* Zero out server address - IMPORTANT! */
  bzero((char *) &cpcb->serv_addr, sizeof(cpcb->serv_addr));
  /* Set address type to IPv4 */
  cpcb->serv_addr.sin_family = AF_INET;
  /* Copy discovered address from hostname to struct */
  bcopy((char *)server->h_addr, 
    (char *)&cpcb->serv_addr.sin_addr.s_addr,
    server->h_length);
  /* Assign port number, using network byte order */
  cpcb->serv_addr.sin_port = htons(dst_port);
  printf("Address set\n");

  create_pkbuf(pkbuffer, RTLP_TYPE_SYN, 0,0, NULL);

  i=0;
  while(i<3) {
	  if(send_packet(pkbuffer, sockfd, cpcb->serv_addr) <0){
		  exit(-1);
	  }
	  cpcb->state = RTLP_STATE_SYN_SENT;
	  printf("State set %d\n",  cpcb->state);


	  /* clear the set ahead of time */
	  FD_ZERO(&readfds);
	  /* add our descriptors to the set */
	  FD_SET(sockfd, &readfds);

	  tv.tv_sec = 1;
     srv = select(sockfd+1, &readfds, NULL, NULL, &tv);
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
        printf("Packet of type %d received\n", pkbuffer->hdr.type );
        if ( pkbuffer->hdr.type == RTLP_TYPE_ACK ) {
          printf("Connection successful\n");
          cpcb->state = RTLP_STATE_ESTABLISHED;
          return 0;
        } else {
          rtlp_client_reset(cpcb);
          printf("Server misbehaviour\n");
          return -1;
        }
	    }
	  }
  }
  
  return -1;
}

int rtlp_client_reset(struct rtlp_client_pcb *cpcb){

  struct pkbuf pkbuffer;
  create_pkbuf(&pkbuffer, RTLP_TYPE_RST, 0,0, NULL);
  
  printf("Packet created\n");
  
  if(send_packet(&pkbuffer, cpcb->sockfd, cpcb->serv_addr) <0){
    return -1;
  }
  cpcb->state = RTLP_STATE_CLOSED;

  return 0;

}



