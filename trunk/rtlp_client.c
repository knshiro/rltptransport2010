#include <stdlib.h>
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include "rtlp.h"


int treat_arq(struct rtlp_client_pcb *cpcb, FILE *outfile);
void print_state_cpcb(struct rtlp_client_pcb *cpcb);
void write_to_output(struct pkbuf* buffer, FILE *output,struct rtlp_client_pcb *cpcb);
/*
int compare_int (const int *a, const int *b)
{
  return (*a > *b) - (*a < *b);
}
*/

int swap(struct pkbuf* pkarray,int i, int j){

  int size = sizeof(pkarray);
  struct pkbuf temp;

  if (i>=size || j>=size){
    return -1;   //TODO stderr
  } 
    temp = pkarray[i];
    pkarray[i] = pkarray[j];
    pkarray[j] = temp;

 return 0;
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
          cpcb->size_received = 0;
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


  int i,msg_size,done,sendAck;
  struct pkbuf pkbuffer;
  FILE *output = NULL;
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

  print_state_cpcb(cpcb);

  if(outfile != NULL){
    printf("Out = %s\n",outfile);
    output = fopen( outfile, "a");
  }



  //Check first time if there are ack in the buffer

  /* clear the set ahead of time */
  FD_ZERO(&readfds);
  /* add our descriptors to the set */
  FD_SET(cpcb->sockfd, &readfds);

  tv.tv_sec = 0;
  while(select(cpcb->sockfd+1, &readfds, NULL, NULL, &tv)>0){
    if (FD_ISSET(cpcb->sockfd, &readfds)) {
      if(treat_arq(cpcb,output)>0){
        sendAck = 1;
      }
    }
    /* clear the set ahead of time */
    FD_ZERO(&readfds);
    /* add our descriptors to the set */
    FD_SET(cpcb->sockfd, &readfds);
  }
  if(sendAck == 1){
    // ACK last ack number
    printf("Send ack number %d\n",cpcb->last_seq_num_sent);
    create_pkbuf(&pkbuffer,RTLP_TYPE_ACK,cpcb->last_seq_num_sent,0,NULL,0);
    send_packet(&pkbuffer,cpcb->sockfd,cpcb->serv_addr);
  }
  sendAck = 0;

  printf("First check done\n");


  if(data != NULL){
    done = 0;
    i= 0;
    //Fill the packet buffer
    while(!done && i < cpcb->window_size){
      if(cpcb->send_buf[i].hdr.seqnbr == -1){ 
        cpcb->last_seq_num_sent++;
        create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,cpcb->last_seq_num_sent,msg_size,data,len); 
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
          sendAck = treat_arq(cpcb,output);
        }
        /* clear the set ahead of time */
        FD_ZERO(&readfds);
        /* add our descriptors to the set */
        FD_SET(cpcb->sockfd, &readfds);
        if (sendAck == 1){
          // ACK last ack number
          printf("Send ack number %d\n",cpcb->last_seq_num_sent);
          create_pkbuf(&pkbuffer,RTLP_TYPE_ACK,cpcb->last_seq_num_sent,0,NULL,0);
          send_packet(&pkbuffer,cpcb->sockfd,cpcb->serv_addr);
        }
      }

      //Fill the packet buffer
      while(!done && i < cpcb->window_size){
        if(cpcb->send_buf[i].hdr.seqnbr == -1){ 
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
        printf("Timeout packet number : %d\n",cpcb->send_buf[i].hdr.seqnbr);
        send_packet(&cpcb->send_buf[i],cpcb->sockfd,cpcb->serv_addr);
        cpcb->time_send[i] = current_time;
      }
    }
  }
  
  if(output != NULL){
    fclose(output);
  }
  printf("<<<<<Transfert\n");
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

  if((cpcb->state == 1) && ((cpcb->total_msg_size == cpcb->size_received) || (cpcb->last_seq_num_ack == cpcb->last_seq_num_sent +1))) {
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

int treat_arq(struct rtlp_client_pcb *cpcb, FILE *output) {

  
  printf(">>>Treat arq\n");

  int i,numRead,returnValue=0;
  struct pkbuf pkbuffer;
  char udp_buffer[RTLP_MAX_PAYLOAD_SIZE+12];

  print_state_cpcb(cpcb);
 
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
          printf("Ack packet %d\n",cpcb->send_buf[i].hdr.seqnbr);
          cpcb->send_buf[i].hdr.seqnbr = -1;
        }
      }
    }
  }


  else if ( pkbuffer.hdr.type == RTLP_TYPE_DATA ) {  // If the received message is data 
    printf("Packet type DATA\n");
    i = pkbuffer.hdr.seqnbr - (cpcb->last_seq_num_ack+1); 
    printf("Place in the buffer : %d\n",i);
    printf("Seqnbr of packet i : %d\n",cpcb->recv_buf[i].hdr.seqnbr);
    if( (i>-1) && ( i < cpcb->window_size )  && (cpcb->recv_buf[i].hdr.seqnbr == -1)){
      printf("Packet put in the buffer slot %d\n",i);
      memcpy(&cpcb->recv_buf[i],&pkbuffer,sizeof(struct pkbuf));
    }
    else{      // Erreur pas de place dans le buffer
      if ( i>=0 && (( i > cpcb->window_size ) || (cpcb->recv_buf[i].hdr.seqnbr == -1)) ){
        printf("!!!!!!!!!!!!!!!!!!!!!ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        return -1;
      }
    }

    i=0;
    while(cpcb->recv_buf[i].hdr.seqnbr != -1 && i<cpcb->window_size+1){
      write_to_output(&cpcb->recv_buf[i],output,cpcb); 
      cpcb->recv_buf[i].hdr.seqnbr = -1;
      i++;
    }
    if(i>0){
      printf("Max i written : %d\n",i);
      cpcb->last_seq_num_ack =  cpcb->last_seq_num_ack + i ;
      cpcb->last_seq_num_sent = cpcb->last_seq_num_ack +1;
      returnValue = 1;
    }
  }


  else {                     // Packet type incorrect
    rtlp_client_reset(cpcb);
    printf("Server misbehaviour\n");  //TODO stderr
    return -1;
  }

    for(i=0;i<cpcb->window_size;i++){
    if( cpcb->recv_buf[i].hdr.seqnbr != -1 ) {
      swap(cpcb->recv_buf,i,cpcb->recv_buf[i].hdr.seqnbr - (cpcb->last_seq_num_ack+1));
    }
  }

  return returnValue;
}

/*
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

void print_state_cpcb(struct rtlp_client_pcb *cpcb){
  int i;

  printf("=================CPCB state===================\n");

  printf("Window size : %d\n",cpcb->window_size);

  printf("Last seq nbr ack : %d\n",cpcb->last_seq_num_ack);
  printf("Last seq nbr sent : %d\n",cpcb->last_seq_num_sent);
 
  printf("Total msg size : %d\n",cpcb->total_msg_size);
  printf("Total received : %d\n",cpcb->size_received);
 
  printf("Buffer send state\n");
  for(i=0;i<cpcb->window_size;i++){
    printf("%d ",cpcb->send_buf[i].hdr.seqnbr);
  }

  printf("\nBuffer recv state\n");
  for(i=0;i<cpcb->window_size;i++){
     printf("%d ",cpcb->recv_buf[i].hdr.seqnbr);
  }
  printf("\n");

  printf("=================CPCB state===================\n");
}

void write_to_output(struct pkbuf* buffer, FILE *output, struct rtlp_client_pcb *cpcb){
  
  if(output == NULL){
    printf("%s",buffer->payload);
  }
  else {
    printf("Writing in the file %d bytes\n",buffer->len);
    cpcb->size_received++;
    cpcb->total_msg_size = buffer->hdr.total_msg_size;
    printf("Payload written %s\n", buffer->payload);
    fwrite(buffer->payload,sizeof(char),buffer->len,output);
  }

}
