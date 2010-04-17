#include <stdlib.h>
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include "rtlp.h"


int treat_arq(struct rtlp_client_pcb *cpcb, char *outfile);

int compare_int (const int *a, const int *b)
{
  return (*a > *b) - (*a < *b);
}


int rtlp_connect(struct rtlp_client_pcb *cpcb, char *dst_addr, int dst_port){

  printf(">>> Function rtlp_connect\n");

  int sockfd,i,srv,numRead;
  struct pkbuf pkbuffer;
  //pkbuffer = (struct pkbuf*)malloc(sizeof(struct pkbuf));
  struct timeval tv;
  char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];
  fd_set readfds;

  struct hostent *server;

  /* Create socket */
  sockfd=create_socket(-1);
  if(sockfd<0)  {
    printf("Impossible to create socket\n");
    exit(-1);
  }

  cpcb->sockfd = sockfd;
  printf("Socket %d stored in cpcb\n",sockfd);

  cpcb->window_size = 8;
  printf("Window size set :%d\n",cpcb->window_size);

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

  create_pkbuf(&pkbuffer, RTLP_TYPE_SYN, 0,0, NULL,0);

  i=0;
  while(i<3) {
    if(send_packet(&pkbuffer, cpcb->sockfd, cpcb->serv_addr) <0){
      exit(-1);
    }
    cpcb->state = RTLP_STATE_SYN_SENT;
    printf("State set %d\n",  cpcb->state);
    /* store the value of the sequence number of the packet sent. If there is a retransmission, the same seq number is used */
    cpcb->last_seq_num_sent = 0;

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
        if((numRead = recv(sockfd,rtlp_packet,sizeof(rtlp_packet),0)) < 0)
        {
          perror("Couldnt' receive from socket");
        }

        udp_to_pkbuf(&pkbuffer, rtlp_packet, numRead);
        printf("Packet of type %d received\n", pkbuffer.hdr.type );
        if ( pkbuffer.hdr.type == RTLP_TYPE_ACK ) {
          printf("Connection successful\n");
          cpcb->state = RTLP_STATE_ESTABLISHED;
          /* store the value of the sequence number of the packet received !!!*/
          cpcb->last_seq_num_ack = pkbuffer.hdr.seqnbr;
          printf("TEST:(=1)pkbuffer->hdr.seqnbr: %i\n",pkbuffer.hdr.seqnbr);

          //Init the buffers
          printf("<Buffers init\n");
          for (i=0;i<cpcb->window_size;i++){
            cpcb->recv_buf[i].hdr.seqnbr=-1;
            cpcb->send_buf[i].hdr.seqnbr=-1;
          }
          printf(">Buffers init checked\n");

          return 0;
        } else {
          rtlp_client_reset(cpcb);
          printf("Server misbehaviour\n");
          return -1;
        }
      }
    }
  }
  printf("!!! Connection failed 3 timeouts !!!\n");
  printf("\n<<< Function rtlp_connect\n\n");  
  return -1;
}

int rtlp_client_reset(struct rtlp_client_pcb *cpcb){

  struct pkbuf pkbuffer;
  create_pkbuf(&pkbuffer, RTLP_TYPE_RST, 0,0, NULL,0);

  printf("Packet created\n");

  if(send_packet(&pkbuffer, cpcb->sockfd, cpcb->serv_addr) <0){
    return -1;
  }
  cpcb->state = RTLP_STATE_CLOSED;

  return 0;

}

/*

int rtlp_transfer2(struct rtlp_client_pcb *cpcb, void *data, int len,
    char* outfile){
  int i,j,k,msg_size,nb_timeout=0,srv;
  struct pkbuf pkbuffer;
  char udp_buffer[RTLP_MAX_PAYLOAD_SIZE+12];
  char payloadbuff[RTLP_MAX_PAYLOAD_SIZE];
  struct timeval tv;
  fd_set readfds;


  //Check if the state is connected
  if (cpcb->state != RTLP_STATE_ESTABLISHED){
    return -1;
  }

  //Find the number of necessary packets
  if(len%RTLP_MAX_PAYLOAD_SIZE >0){
    msg_size = len/RTLP_MAX_PAYLOAD_SIZE + 1;
  }
  else {
    msg_size = len/RTLP_MAX_PAYLOAD_SIZE;
  }

  //Initialize send_packet_buffer
  for(j=0;j<cpcb->window_size;j++){
    cpcb->send_buf[j].hdr.seqnbr = -1;
  }

  i=0;
  while(i<msg_size && j>0){
    j=0;  
    //Fill the packet buffer
    while(i<msg_size && j<cpcb->window_size){
      if(cpcb->send_buf[j].hdr.seqnbr == -1){ 
        bzero(payloadbuff,sizeof(payloadbuff));
        memcpy(payloadbuff,data+i*RTLP_MAX_PAYLOAD_SIZE,RTLP_MAX_PAYLOAD_SIZE);
        create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,i,msg_size,payloadbuff,RTLP_MAX_PAYLOAD_SIZE); 
        memcpy(&cpcb->send_buf[j],&pkbuffer,sizeof(struct pkbuf));
        i++;
      }
      j++;
    }
    j=0;
    //Send the packets in the buffer
    for(k=0;k<cpcb->window_size;k++){
      if(cpcb->send_buf[j].hdr.seqnbr != -1){
        send_packet(&cpcb->send_buf[j], cpcb->sockfd, cpcb->serv_addr);
        j++;
      }
    }

    while(j>0){    
      // clear the set ahead of time 
      FD_ZERO(&readfds);
      // add our descriptors to the set 
      FD_SET(cpcb->sockfd, &readfds);

      tv.tv_sec = 2;
      srv = select(cpcb->sockfd+1, &readfds, NULL, NULL, &tv);
      if (srv == -1) {
        perror("select"); // error occurred in select()
        return -1;
      } 
      else if (srv == 0) {
        printf("Timeout occurred! No data after 2 seconds.\n");
        if(nb_timeout++>5){
          return -1;
        }
        else {
          break;
        } 
      }
      else {
        // one or both of the descriptors have data
        if (FD_ISSET(cpcb->sockfd, &readfds)) {

          bzero(udp_buffer, sizeof(udp_buffer));
          if(recv(cpcb->sockfd,udp_buffer,sizeof(udp_buffer),0) < 0)
          {
            perror("Couldn't receive from socket");
          }
          udp_to_pkbuf(&pkbuffer, udp_buffer);
          printf("Packet of type %d received\n", pkbuffer.hdr.type );

          if ( pkbuffer.hdr.type == RTLP_TYPE_ACK ) {  // If the received message is an ACK
            for(k=0;k<cpcb->window_size;k++){           // We delete the acquitted message from the send_packet_buffer
              if(cpcb->send_buf[k].hdr.seqnbr ==  pkbuffer.hdr.seqnbr){
                j--;
                nb_timeout = 0;
                cpcb->send_buf[k].hdr.seqnbr = -1;
              }
            }
          } 
          else if ( pkbuffer.hdr.type == RTLP_TYPE_DATA ) {
          }
          else {
            rtlp_client_reset(cpcb);
            printf("Server misbehaviour\n");
            return -1;
          }
        }
      }   
    }
  }
  return 0;
}        
*/

int rtlp_transfer(struct rtlp_client_pcb *cpcb, void *data, int len,
    char* outfile){


  printf(">>>>Transfert\n");


  int i,msg_size,done;
  struct pkbuf pkbuffer;

  char payloadbuff[RTLP_MAX_PAYLOAD_SIZE];
  struct timeval tv;
  time_t current_time;
  fd_set readfds;

  //Check if the data is not too big
  if(len-RTLP_MAX_PAYLOAD_SIZE >0){
    return -1;                        //TODO stderr
  }

  //Check if the state is connected
  if (cpcb == NULL || cpcb->state != RTLP_STATE_ESTABLISHED){
    return -1;                        //TODO stderr
  }
 
  //Check first time if there are ack in the buffer

  /* clear the set ahead of time */
  FD_ZERO(&readfds);
  /* add our descriptors to the set */
  FD_SET(cpcb->sockfd, &readfds);

  tv.tv_sec = 0;
  while(select(cpcb->sockfd+1, &readfds, NULL, NULL, &tv)>0){
    if (FD_ISSET(cpcb->sockfd, &readfds)) {
      treat_arq(cpcb,outfile);
    }
    /* clear the set ahead of time */
    FD_ZERO(&readfds);
    /* add our descriptors to the set */
    FD_SET(cpcb->sockfd, &readfds);
  }
   
  printf("First check done\n");


  if(data != NULL){
    done = 0;
    i= 0;
    //Fill the packet buffer
    while(!done && i < cpcb->window_size){
      if(cpcb->send_buf[i].hdr.seqnbr == -1){ 
        bzero(payloadbuff,sizeof(payloadbuff));
        create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,cpcb->last_seq_num_sent++,msg_size,data,len); 
        memcpy(&cpcb->send_buf[i],&pkbuffer,sizeof(struct pkbuf));
        done = 1; 
      }
      i++;
    }

    if(!done){        //If there was not free slot in the packet buffer wait for some ack and try again
      // clear the set ahead of time 
      FD_ZERO(&readfds);
      // add our descriptors to the set
      FD_SET(cpcb->sockfd, &readfds);

      tv.tv_sec = 5;
      if(select(cpcb->sockfd+1, &readfds, NULL, NULL, &tv)>0){
        if (FD_ISSET(cpcb->sockfd, &readfds)) {
          treat_arq(cpcb,outfile);
        }
        /* clear the set ahead of time */
        FD_ZERO(&readfds);
        /* add our descriptors to the set */
        FD_SET(cpcb->sockfd, &readfds);
      }

      //Fill the packet buffer
      while(!done && i < cpcb->window_size){
        if(cpcb->send_buf[i].hdr.seqnbr == -1){ 
          bzero(payloadbuff,sizeof(payloadbuff));
          create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,cpcb->last_seq_num_sent++,msg_size,data,len); 
          memcpy(&cpcb->send_buf[i],&pkbuffer,sizeof(struct pkbuf));
          done = 1; 
        }
        i++;
      }
    }
  }
  for(i=0;i<cpcb->window_size;i++){
    if(cpcb->send_buf[i].hdr.seqnbr != -1){
      current_time = time(0);
      if(current_time - cpcb->time_send[i] > 2){
        send_packet(&cpcb->send_buf[i],cpcb->sockfd,cpcb->serv_addr);
        cpcb->time_send[i] = current_time;
      }
    }
  }


  return 0;
}




int rtlp_close(struct rtlp_client_pcb *cpcb)
{
  printf(">>>>rtlp_close\n"); 
  int sockfd,i,srv,numRead;
  sockfd=cpcb->sockfd;
  struct pkbuf *pkbuffer;
  pkbuffer = (struct pkbuf*)malloc(sizeof(struct pkbuf));
  struct timeval tv;
  char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];
  fd_set readfds;

  create_pkbuf(pkbuffer, RTLP_TYPE_FIN, cpcb->last_seq_num_sent+1,0, NULL,0);

  /* check that all the data has been received if the server is sending (size received is update everytime we receive a packet) or that the last packet has been acknowledged if the client is sending */

  if((cpcb->state == 1) && ((pkbuffer->hdr.total_msg_size == cpcb->size_received) || (cpcb->last_seq_num_ack == cpcb->last_seq_num_sent +1))) {
    i=0;
    while(i<3) {
      if(send_packet(pkbuffer, sockfd, cpcb->serv_addr) <0){
        exit(-1);
      }

      if(cpcb->last_seq_num_ack == cpcb->last_seq_num_sent +1) {
        cpcb->state = RTLP_STATE_FIN_WAIT;
        printf("RTLP_STATE_FIN_WAIT 2\n");
      }
      printf("last_seq_num_ack : %i and last_seq_num_sent: %i\n",cpcb->last_seq_num_ack,cpcb->last_seq_num_sent);

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
      }else {
        // one or both of the descriptors have data
        if (FD_ISSET(sockfd, &readfds)) {
          bzero(rtlp_packet, sizeof(rtlp_packet));
          if((numRead = recv(sockfd,rtlp_packet,sizeof(rtlp_packet),0)) < 0) {
            perror("Couldn't receive from socket");
          }

          udp_to_pkbuf(pkbuffer, rtlp_packet, numRead);
          printf("Packet of type %d received\n", pkbuffer->hdr.type );
          if ( pkbuffer->hdr.type == RTLP_TYPE_ACK ) {
            printf("Termination successful\n");
            if(cpcb->state != RTLP_STATE_FIN_WAIT) {
              cpcb->state = RTLP_STATE_FIN_WAIT;
              printf("RTLP_STATE_FIN_WAIT 1\n");
            }
            cpcb->state = RTLP_STATE_CLOSED;
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
  printf("<<<<rtlp_close\n"); 
  return -1;
}

int treat_arq(struct rtlp_client_pcb *cpcb, char *outfile) {

  
  printf(">>>Treat arq\n");

  int i,numRead;
  struct pkbuf pkbuffer;
  char udp_buffer[RTLP_MAX_PAYLOAD_SIZE+12];
  FILE *output;
  
  printf("Buffer send state\n");
  for(i=0;i<cpcb->window_size;i++){
    printf("%d ",cpcb->send_buf[i].hdr.seqnbr);
  }

  printf("\nBuffer send state\n");
  for(i=0;i<cpcb->window_size;i++){
     printf("%d ",cpcb->send_buf[i].hdr.seqnbr);
  }
  printf("\n");

  if(outfile == NULL){
    printf("Out = stdout\n");
    output = stdout;    
  } else {
    printf("Out = %s\n",outfile);
    output = fopen( outfile, "a");
  }

  bzero(udp_buffer, sizeof(udp_buffer));
  if((numRead = recv(cpcb->sockfd,udp_buffer,sizeof(udp_buffer),0)) < 0)
  {
    perror("Couldn't receive from socket"); //TODO stderr 
  }

  udp_to_pkbuf(&pkbuffer, udp_buffer, numRead);
  printf("Packet seqnbr %d received\n", pkbuffer.hdr.seqnbr );

  if ( pkbuffer.hdr.type == RTLP_TYPE_ACK ) {  // If the received message is an ACK
    printf("Packet type ACK\n");
    if(cpcb->last_seq_num_ack  <  pkbuffer.hdr.seqnbr)
    {
      cpcb->last_seq_num_ack = pkbuffer.hdr.seqnbr; 
      for(i=0;i<cpcb->window_size;i++){           // We delete the acquitted message from the send_packet_buffer
        if(cpcb->send_buf[i].hdr.seqnbr <  pkbuffer.hdr.seqnbr){
          cpcb->send_buf[i].hdr.seqnbr = -1;
          printf("Ack packet %d\n",cpcb->send_buf[i].hdr.seqnbr);
        }
      }
    }
  }


  else if ( pkbuffer.hdr.type == RTLP_TYPE_DATA ) {  // If the received message is data 
    printf("Packet type DATA\n");
    i = pkbuffer.hdr.seqnbr - cpcb->last_seq_num_sent; 
    if( ( i < cpcb->window_size )  && (cpcb->recv_buf[i].hdr.seqnbr != -1)){
      memcpy(&cpcb->recv_buf[i],&pkbuffer,sizeof(struct pkbuf));
    }
    else{         // Erreur pas de place dans le buffer
      return -1;
    }

    i=0;
    while(cpcb->recv_buf[i].hdr.seqnbr != -1 && i<cpcb->window_size+1){
      fwrite(cpcb->recv_buf[i].payload,sizeof(char),cpcb->recv_buf[i].len,output);
      i++;
    }
    if(i>0 && cpcb->recv_buf[i-1].hdr.seqnbr>= cpcb->last_seq_num_sent){
      cpcb->last_seq_num_sent = cpcb->recv_buf[i-1].hdr.seqnbr+1;
      create_pkbuf(&pkbuffer,RTLP_TYPE_ACK,cpcb->last_seq_num_sent,0,NULL,0);
      send_packet(&pkbuffer,cpcb->sockfd,cpcb->serv_addr);
    }
  }


  else {                     // Packet type incorrect
    rtlp_client_reset(cpcb);
    printf("Server misbehaviour\n");  //TODO stderr
    return -1;
  }


  return 0;
}

/*
int swap(struct pkbuf* pkarray,int i, int j){

  int size = sizeof(pkarray);
  struct pkbuf temp;

  if (i>=size || j>=size){
    return -1;   //TODO stderr
  } 
    temp = pkarray[i];
    pkarray[i] = pkarray[j];
    pkarray[j] = pkarray[i];
}

int sort_buff(struct pkbuf* pkarray, int len){

  int i,min=0;

  if ( len > sizeof(pkarray) ){
      return -1;    //stderr
  }
  for(i=0;i<len;i++){
    if(pkarray[i].hdr.seqnbr < min && pkarray[i].hdr
  
  }

}

*/
