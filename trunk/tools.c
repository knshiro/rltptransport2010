#include "tools.h"

struct pkbuf* create_pkbuf(struct pkbuf* buff, int type,int seqnbr,int msg_size, char * payload){

	struct rltp_hdr hdr;
   int payload_size;

	hdr.type = type;
	hdr.seqnbr = seqnbr;
	hdr.total_msg_size = msg_size;


   if(payload != NULL){
		payload_size = sizeof(payload);
		if (payload_size > RLTP_MAX_PAYLOAD_SIZE){
		  fprintf(stderr,"payload exceed max size (%d)\n", payload_size);
		  return NULL;
		}
		if(payload_size>0) {
		  printf("Copying payload %d\n",payload_size);
		  memcpy(buff->payload,payload,payload_size);
		  buff->len = payload_size;
		} else {
		  buff->len = 0;
		}
   } else {
     buff->len = 0;
   }
	memcpy(&buff->hdr,&hdr,sizeof(hdr));

	return buff;

}

int send_packet(struct pkbuf* packet, int sockfd, char *dst_addr, int dst_port){

  struct sockaddr_in serv_addr;
  char rltp_packet[RLTP_MAX_PAYLOAD_SIZE+12];
  struct hostent *server;

  /* Get Server address from supplied hostname */
  server = gethostbyname(dst_addr);
  if (server == NULL) {
	  fprintf(stderr,"ERROR, no such host\n");
	  exit(0);
  }
  printf("Name resolved\n");

  /* Zero out server address - IMPORTANT! */
  bzero((char *) &serv_addr, sizeof(serv_addr));
  /* Set address type to IPv4 */
  serv_addr.sin_family = AF_INET;
  /* Copy discovered address from hostname to struct */
  bcopy((char *)server->h_addr, 
    (char *)&serv_addr.sin_addr.s_addr,
    server->h_length);
  /* Assign port number, using network byte order */
  serv_addr.sin_port = htons(dst_port);
  printf("Address set\n");

  bzero((char*)rltp_packet, 100);
  memcpy(rltp_packet,&packet->hdr.type,4);
  memcpy(rltp_packet+4,&packet->hdr.seqnbr,4);
  memcpy(rltp_packet+8,&packet->hdr.total_msg_size,4);
  memcpy(rltp_packet+12,packet->payload,packet->len);
  printf("Data set\n");
  
  /* write message to socket */
  if(sendto(sockfd, rltp_packet, strlen(rltp_packet), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("Could not send packet!");
    return -1;
  }
  printf("Packet sent %d\n",  strlen(rltp_packet));
  return 0;

}

struct pkbuf* udp_to_pkbuf(struct pkbuf* pkbuffer, char * udppacket){

  int type,seqnbr,total_msg_size;
  char * payload;
  
  memcpy(&type,udppacket,4);
  memcpy(&seqnbr,udppacket+4,4);
  memcpy(&total_msg_size,udppacket+8,4);
  payload= udppacket+12;
  
  return create_pkbuf(pkbuffer,type,seqnbr,total_msg_size,payload);

}




