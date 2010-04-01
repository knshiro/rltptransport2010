#include "RTLP_util.h"

struct pkbuf* create_pkbuf(struct pkbuf* buff, int type,int seqnbr,int msg_size, char * payload, int len){

  printf(">create_pkbuf\n");

	struct rtlp_hdr hdr;

	hdr.type = type;
	hdr.seqnbr = seqnbr;
	hdr.total_msg_size = msg_size;


  if(payload != NULL){
		if (len > RTLP_MAX_PAYLOAD_SIZE){
		  fprintf(stderr,"payload exceed max size (%d)\n",len );
		  return NULL;
		}
		if(len>0) {
		  printf("Copying payload %d\n",len);
		  memcpy(buff->payload,payload,len);
		  buff->len = len;
		} else {
		  buff->len = 0;
		}
   } else {
     buff->len = 0;
   }
  memcpy(&buff->hdr,&hdr,sizeof(struct rtlp_hdr));

  printf("<create_pkbuf\n\n");

  return buff;

}

int create_udp_payload(struct pkbuf* packet, char * rtlp_packet){

  bzero(rtlp_packet, sizeof(rtlp_packet));
  memcpy(rtlp_packet,&packet->hdr.type,4);
  memcpy(rtlp_packet+4,&packet->hdr.seqnbr,4);
  memcpy(rtlp_packet+8,&packet->hdr.total_msg_size,4);
  memcpy(rtlp_packet+12,packet->payload,packet->len);
  printf("Data set\n");

}


int send_packet(struct pkbuf* packet, int sockfd, struct sockaddr_in serv_addr){

  printf(">send_packet\n");
  if(packet->len > RTLP_MAX_PAYLOAD_SIZE){
    return -1;            //TODO stderr
  }
  char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];

  bzero((char*)rtlp_packet, 100);
  memcpy(rtlp_packet,&packet->hdr.type,4);
  memcpy(rtlp_packet+4,&packet->hdr.seqnbr,4);
  memcpy(rtlp_packet+8,&packet->hdr.total_msg_size,4);
  memcpy(rtlp_packet+12,packet->payload,packet->len);
  printf("Data set\n");
  printf("seqnbr : %d\n",*rtlp_packet+4);

  /* write message to socket */
  if(sendto(sockfd, rtlp_packet, packet->len+12, 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("Could not send packet!");
    return -1;
  }
  printf("Packet sent %d\n",  strlen(rtlp_packet));
  
  printf("<send_packet\n\n");

  return 0;

}

struct pkbuf* udp_to_pkbuf(struct pkbuf* pkbuffer, char * udppacket){

  int type,seqnbr,total_msg_size;
  char * payload;
  
  memcpy(&type,udppacket,4);
  memcpy(&seqnbr,udppacket+4,4);
  memcpy(&total_msg_size,udppacket+8,4);
  payload= udppacket+12;
  
  return create_pkbuf(pkbuffer,type,seqnbr,total_msg_size,payload,sizeof(udppacket-12));

}


/* create a socket and bind it to the local address and a port  */
int create_socket(int local_port){
  int sockfd;
  int length = sizeof(struct sockaddr_in);
  struct sockaddr_in client_addr;

  /*create socket*/
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
	  perror("ERROR opening socket");
  printf("Socket created\n");

  /* prepare attachment addrese = local addresse */
  client_addr.sin_family = AF_INET;

  /*  Give the local IP address and assign a port*/
  client_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  client_addr.sin_port = htons(local_port);

  if(bind(sockfd,(struct sockaddr *)&client_addr,length) == -1) {
  perror("Impossible to bind socket to local address\n");
  close(sockfd);
  return -1;
  }
return sockfd;
}



