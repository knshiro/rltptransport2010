#include <stdlib.h>
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include "rtlp.h"


int treat_socket_buf(struct rtlp_client_pcb *cpcb);
void print_state_cpcb(struct rtlp_client_pcb *cpcb);
void write_to_output(struct pkbuf* buffer, FILE *output,struct rtlp_client_pcb *cpcb);
int treat_rtlp_buf(struct rtlp_client_pcb *cpcb, FILE *output, int sendAck);
int fill_pck_buf(struct rtlp_client_pcb *cpcb,struct pkbuf * pkbuffer);

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
                    cpcb->total_msg_size = 1;
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



int rtlp_transfer(struct rtlp_client_pcb *cpcb, void *data, int len,
        char* outfile){


    printf(">>>>Transfert\n");


    int i,sendAck = 0,returnValue = 0;
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
            if(treat_socket_buf(cpcb)>0){
                sendAck = 1;
            }
        }
        /* clear the set ahead of time */
        FD_ZERO(&readfds);
        /* add our descriptors to the set */
        FD_SET(cpcb->sockfd, &readfds);
    }
    treat_rtlp_buf(cpcb,output,sendAck);

    sendAck = 0;

    printf("First check done\n");


    if(data != NULL){
        //There is data to send, try to put it in the buffer a first time
        printf(">>>Try to send data (first time)\n");
    
        create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,(cpcb->last_seq_num_sent+1),cpcb->total_msg_size,data,len); 
        
        if( (returnValue = fill_pck_buf(cpcb,&pkbuffer))<1){       
            //If there was not free slot in the packet buffer wait for some ack and try again
            printf(">>No space in the buffer, try to wait...\n");
            // clear the set ahead of time 
            FD_ZERO(&readfds);
            // add our descriptors to the set
            FD_SET(cpcb->sockfd, &readfds);

            tv.tv_sec = 5;
            if(select(cpcb->sockfd+1, &readfds, NULL, NULL, &tv)>0){
                if (FD_ISSET(cpcb->sockfd, &readfds)) {
                    sendAck = treat_socket_buf(cpcb);
                }
                /* clear the set ahead of time */
                FD_ZERO(&readfds);
                /* add our descriptors to the set */
                FD_SET(cpcb->sockfd, &readfds);    
            }
            treat_rtlp_buf(cpcb,output,sendAck);
            sendAck = 0;

            //Fill the packet buffer
            returnValue = fill_pck_buf(cpcb,&pkbuffer);
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
    printf("<<<<<<< Transfert with return value = %d\n",returnValue);
    return returnValue;
}




int rtlp_close(struct rtlp_client_pcb *cpcb)
{
    printf(">>>>>>>>>>>>>>>Closing connection<<<<<<<<<<<<<<<<<<<\n"); 
    int sockfd,i,srv,numRead;
    sockfd=cpcb->sockfd;
    struct pkbuf *pkbuffer;
    pkbuffer = (struct pkbuf*)malloc(sizeof(struct pkbuf));
    struct timeval tv;
    char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];
    fd_set readfds;


    /* check that all the data has been received if the server is sending (size received is update everytime we receive a packet) or that the last packet has been acknowledged if the client is sending */

    i = 0;
    /* clear the set ahead of time */
    FD_ZERO(&readfds);
    /* add our descriptors to the set */
    FD_SET(sockfd, &readfds);

    tv.tv_sec = 2;
    while(cpcb->total_msg_size != cpcb->size_received && cpcb->last_seq_num_ack != cpcb->last_seq_num_sent +1 && i<3){
        srv = select(sockfd+1, &readfds, NULL, NULL, &tv);
        if(srv == 0){
            i++;
        }else{
            rtlp_transfer(cpcb,NULL,0,NULL);
            i=0;
        }
        /* clear the set ahead of time */
        FD_ZERO(&readfds);
        /* add our descriptors to the set */
        FD_SET(sockfd, &readfds);
    }

    if(i==3){
        printf("Not everything could have been acknowledged\n");
        return -1;       //Timeout
    } else {
        printf("Everything acknowledged\n");
        create_pkbuf(pkbuffer, RTLP_TYPE_FIN, cpcb->last_seq_num_sent+1,0, NULL,0);
        i=0;
        while(i<3) {
            if(send_packet(pkbuffer, sockfd, cpcb->serv_addr) <0){
                exit(-1);
            }

            cpcb->state = RTLP_STATE_FIN_WAIT;
            printf("RTLP_STATE_FIN_WAIT 2\n");
            printf("last_seq_num_ack : %i and last_seq_num_sent: %i\n",cpcb->last_seq_num_ack,cpcb->last_seq_num_sent);

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
                    } else if ( pkbuffer->hdr.type == RTLP_TYPE_DATA){

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
    printf("<<<<rtlp_close\n"); 
    return -1;
}


/**
 * This function looks into the socket for the first packet found and treats it
 * 
 * @return : 0 no data received
 *           1 data received
 *           -1 error
 *
 * */

int treat_socket_buf(struct rtlp_client_pcb *cpcb) {


    printf(">>>Treat socket buffer\n");

    int i,numRead,returnValue = 0;
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

    switch(pkbuffer.hdr.type){
        case RTLP_TYPE_ACK:
            // If the received message is an ACK
            printf("Packet type ACK\n");
            if(cpcb->last_seq_num_ack  <  pkbuffer.hdr.seqnbr)
            {
                cpcb->last_seq_num_ack = pkbuffer.hdr.seqnbr; 
                for(i=0;i<cpcb->window_size;i++){           // We delete the acquitted message from the send_packet_buffer
                     if(cpcb->send_buf[i].hdr.seqnbr <  pkbuffer.hdr.seqnbr){
                        printf("Ack packet in send buffer %d\n",cpcb->send_buf[i].hdr.seqnbr);
                        cpcb->send_buf[i].hdr.seqnbr = -1;
                     }

                     if(cpcb->recv_buf[i].hdr.seqnbr <  pkbuffer.hdr.seqnbr){
                        printf("Ack packet in recv buffer %d\n",cpcb->recv_buf[i].hdr.seqnbr);
                        cpcb->recv_buf[i].hdr.seqnbr = -1;
                    }

                }
            }
            break;


        case RTLP_TYPE_DATA:
            // If the received message is of type data 
            printf("Packet type DATA\n");
            returnValue =1;
            i = pkbuffer.hdr.seqnbr - (cpcb->last_seq_num_ack+1); 
            printf("Place in the buffer : %d\n",i);
            printf("Seqnbr of packet i : %d\n",cpcb->recv_buf[i].hdr.seqnbr);
            if( (i>-1) && ( i < cpcb->window_size )  && (cpcb->recv_buf[i].hdr.seqnbr == -1)){
                printf("Packet put in the buffer slot %d\n",i);
                cpcb->total_msg_size = pkbuffer.hdr.total_msg_size;
                memcpy(&cpcb->recv_buf[i],&pkbuffer,sizeof(struct pkbuf));
            }
            else{      // Erreur pas de place dans le buffer
                if ( i>=0 && (( i > cpcb->window_size ) || (cpcb->recv_buf[i].hdr.seqnbr == -1)) ){
                    printf("!!!!!!!!!!!!!!!!!!!!!ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                    return -1;
                }
            }

            break;
        case RTLP_TYPE_RST:
            printf("Server reset connection\n");  //TODO stderr
            return -1;
            break;
        default:
            printf("Server misbehaviour\n");  //TODO stderr
            rtlp_client_reset(cpcb);
            return -1;
            break;
    }

    print_state_cpcb(cpcb);


    for(i=0;i<cpcb->window_size;i++){
        if( cpcb->recv_buf[i].hdr.seqnbr != -1 ) {
            printf("Swap %d <---> %d\n",i,cpcb->recv_buf[i].hdr.seqnbr - (cpcb->last_seq_num_ack+1));
            swap(cpcb->recv_buf,i,cpcb->recv_buf[i].hdr.seqnbr - (cpcb->last_seq_num_ack+1));
        }
    }

    return returnValue;
}

/**
 * This function goes through the rtlp buffers, write to the output the packets that have to be written
 * and sends an ack
 */
int treat_rtlp_buf(struct rtlp_client_pcb *cpcb, FILE *output, int sendAck){

    printf(">>>>Treat_rtlp_buff\n");
    print_state_cpcb(cpcb);

    int i=0;
    struct pkbuf pkbuffer;
    // Write to the output
    while(cpcb->recv_buf[i].hdr.seqnbr != -1 && i<cpcb->window_size){
        write_to_output(&cpcb->recv_buf[i],output,cpcb); 
        cpcb->recv_buf[i].hdr.seqnbr = -1;
        i++;
    }
    if(i>0){
        //Update sequence numbers
        printf("Max i written : %d\n",i);
        cpcb->last_seq_num_ack =  cpcb->last_seq_num_ack + i ;
        cpcb->last_seq_num_sent = cpcb->last_seq_num_ack +1;
    }
        
    
    printf("Ack number %d because size received = %d or sendAck = %d\n",cpcb->last_seq_num_sent,cpcb->size_received,sendAck);
    if (cpcb->size_received > 0 || sendAck){    
        // ACK last ack number
        printf("Send ack number %d",cpcb->last_seq_num_sent);
        create_pkbuf(&pkbuffer,RTLP_TYPE_ACK,cpcb->last_seq_num_sent,0,NULL,0);
        send_packet(&pkbuffer,cpcb->sockfd,cpcb->serv_addr);
        for(i=0;i<cpcb->window_size;i++){
            if(cpcb->send_buf[i].hdr.seqnbr < cpcb->last_seq_num_sent){
                cpcb->send_buf[i].hdr.seqnbr = 1;
            }
        }
    }
    printf("<<<<Treat_rtlp_buff\n");
    return 0;
}

 
int fill_pck_buf(struct rtlp_client_pcb *cpcb,struct pkbuf * pkbuffer){
   int i=0,done=0;
  
    printf(">>>>Writing packet to the send buffer\n");

    while(!done && i < cpcb->window_size){
        if(cpcb->send_buf[i].hdr.seqnbr == -1){
            printf("Found the place %d in the buffer, send the packet %d\n",i,pkbuffer->hdr.seqnbr);
            cpcb->last_seq_num_sent++; 
            cpcb->time_send[i] = time(0);
            memcpy(&cpcb->send_buf[i],pkbuffer,sizeof(struct pkbuf));
            send_packet(&cpcb->send_buf[i],cpcb->sockfd,cpcb->serv_addr);
            done = 1; 
        }
        i++;
    }
    return done;
}

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
    printf("Server address %s and port %d\n",inet_ntoa(cpcb->serv_addr.sin_addr), ntohs(cpcb->serv_addr.sin_port) );

    printf("=================CPCB state===================\n");
}

void write_to_output(struct pkbuf* buffer, FILE *output, struct rtlp_client_pcb *cpcb){

    if(output == NULL){
        printf("Output from payload :\n%s",buffer->payload);
    }
    else {
        printf("Writing in the file %d bytes from packet number %d\n",buffer->len,buffer->hdr.seqnbr);
        cpcb->size_received++;
        //printf("Payload written %s\n", buffer->payload);
        fwrite(buffer->payload,sizeof(char),buffer->len,output);
    }

}
