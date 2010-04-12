
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <arpa/inet.h>
#include "RTLP_util.h"
#include "rtlp.h"

int rtlp_listen(struct rtlp_server_pcb *spcb, int port){

  int sockfd;

  //create socket
  sockfd = create_socket(port);

  if(sockfd<0)  {
	printf("Impossible to create socket\n");
	exit(-1);
  }

  spcb->sockfd = sockfd;
  printf("Socket %d stored in cpcb\n",sockfd);

return 0;
}



int rtlp_accept(struct rtlp_server_pcb *spcb){

  if(spcb->state !=5) {
	printf("Cannot accept connections!\n");
	return -1;
  }

  struct pkbuf pkbuffer;
  unsigned int fromlen, numread;
char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];
  struct sockaddr_in from;
  char buf[1024];


  fd_set readfds;
  
  fromlen = sizeof(from);

  while(1) {
    FD_ZERO(&readfds);
    FD_SET(spcb->sockfd, &readfds);
    

    int srv = select(spcb->sockfd+1, &readfds, NULL, NULL, 0);
    if (srv == -1) {
		  perror("select"); // error occurred in select()
    } else {
	// one or both of the descriptors have data
        if(FD_ISSET(spcb->sockfd, &readfds)) {
      		bzero(rtlp_packet, sizeof(rtlp_packet));
      		if((numread = recvfrom(spcb->sockfd,rtlp_packet,sizeof(rtlp_packet), 0, (struct sockaddr*)&from, &fromlen)) < 0){
	  		error("Couldnt' receive from socket");
		}
      		udp_to_pkbuf(&pkbuffer, rtlp_packet);
      		printf("Received message from port %d and address %s.\n", ntohs(from.sin_port), inet_ntoa(from.sin_addr));
      		if ( pkbuffer.hdr.type == 3) {
    	 		printf("Received Packet type: %i\n",pkbuffer.hdr.type);
     	
	/*************************/
 	/* Get client address from supplied hostname */
  
  struct sockaddr_in client_addr;
  spcb->client_addr=client_addr;
  struct hostent *client;
  client = gethostbyname(inet_ntoa(from.sin_addr));
  if (client == NULL) {
	  fprintf(stderr,"ERROR, no such host\n");
	  exit(0);
  }
  printf("Name resolved\n");

  /* Zero out server address - IMPORTANT! */
  bzero((char *) &client_addr, sizeof(client_addr));
  /* Set address type to IPv4 */
  client_addr.sin_family = AF_INET;
  /* Copy discovered address from packet */
 client_addr.sin_addr.s_addr=inet_ntoa(from.sin_addr);
 printf("Client address set: %s\n",  client_addr.sin_addr.s_addr);
  /* Assign port number, using network byte order */
  client_addr.sin_port = ntohs(from.sin_port);
  printf("Client port set: %i\n", client_addr.sin_port);

	/************************/

	/************************/

		create_pkbuf(&pkbuffer, RTLP_TYPE_ACK, 1,0, NULL,0);
		if(send_packet(&pkbuffer, spcb->sockfd, client_addr) <0){
			printf("probleme\n");
		  	exit(-1);
	 	 }

	/************************/

	}
    	}

    }
  }
  return 0;
}


