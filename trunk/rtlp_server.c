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
#include <dirent.h>


int rtlp_listen(struct rtlp_server_pcb *spcb, int port){


	int sockfd;
	//create socket
	sockfd = create_socket(port);

	if(sockfd<0)  {
		printf("Impossible to create socket\n");
		exit(-1);
	}

	spcb->sockfd = sockfd;
	printf("Socket %d stored in spcb\n",sockfd);
	spcb->state=5;
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
	int out_loop;
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
				udp_to_pkbuf(&pkbuffer, rtlp_packet,numread);
				printf("Received message from port %d and address %s.\n", ntohs(from.sin_port), inet_ntoa(from.sin_addr));
				if ( pkbuffer.hdr.type == RTLP_TYPE_SYN) {
					printf("Received Packet type: %i\n",pkbuffer.hdr.type);


					// Get client address from supplied hostname 

					struct sockaddr_in client_addr;
					spcb->client_addr=client_addr;
					struct hostent *client;
					client = gethostbyname(inet_ntoa(from.sin_addr));
					if (client == NULL) {
						fprintf(stderr,"ERROR, no such host\n");
						exit(0);
					}
					printf("Name resolved\n");


					//create pkbuf and send packet
					create_pkbuf(&pkbuffer, RTLP_TYPE_ACK, 1,0, NULL,0);
					if((out_loop = send_packet(&pkbuffer, spcb->sockfd, from)) < 0){
						printf("Problem: cannot send packet\n");
						exit(-1);
					}
					spcb->last_seq_num_sent = 1;
					if(out_loop == 0) {
						spcb->state = RTLP_STATE_ESTABLISHED;
						printf("Connection successful\n");
						//fork
						break;
					}

				}
			}

		}
	}
	return 0;
}





int rtlp_transfer_loop(struct rtlp_server_pcb *spcb)
{

	//Check if the state is connected
	if (spcb->state != RTLP_STATE_ESTABLISHED){
		return -1;
	}


	struct timeval tv;
	fd_set readfds;
	int i,j,k,msg_size,nb_timeout=0,srv,len,u,t;
	long longlen;
	int jump_select=0;
	struct pkbuf pkbuffer;
	char payloadbuff[RTLP_MAX_PAYLOAD_SIZE];
	char udp_buffer[RTLP_MAX_PAYLOAD_SIZE+12];
	char filename[50];
	char data[RTLP_MAX_PAYLOAD_SIZE];

	struct sockaddr_in from;
	unsigned int fromlen;
	fromlen = sizeof(from);

	int len_packet;
	int send=0; 
	FILE * f;

	char list_to_send[RTLP_MAX_PAYLOAD_SIZE];

	char ** packets_received = (char**)malloc(sizeof(char*));
	int* packets_received_check = (int*)malloc(sizeof(int));
	int first_seq_number;
	int nb_acks=0;
	int check_reception=1;
	int length_last_packet;
	int check_ack;	

	while(1) {
		begin:
		printf("Entry of the main loop: Wait for the client to ask for something\n");
		//First, we check if the client asked for something and at the end if it sends a FIN packet.
		/* clear the set ahead of time */
		FD_ZERO(&readfds);
		/* add our descriptors to the set */
		FD_SET(spcb->sockfd, &readfds);

		tv.tv_sec = 2;
		srv = select(spcb->sockfd+1, &readfds, NULL, NULL, 0);
		if (srv == -1) {
			perror("select"); // error occurred in select()
			return -1;
		} 
		else {
			// one or both of the descriptors have data
			if (FD_ISSET(spcb->sockfd, &readfds)) {

				bzero(udp_buffer, sizeof(udp_buffer));
				if((len_packet=recvfrom(spcb->sockfd,udp_buffer, sizeof(udp_buffer), 0, (struct sockaddr*)&from, &fromlen)) < 0) {
					perror("Couldn't receive from socket");
				}

				printf("Packet of type %d received\n", pkbuffer.hdr.type);
				
				//Send an ACK
				create_pkbuf(&pkbuffer, RTLP_TYPE_ACK,spcb->last_seq_num_sent+1,0, NULL,0);
				if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
					return -1;
				}
				printf("ACK of the query sent\n");
				spcb->last_seq_num_sent = spcb->last_seq_num_sent +1;
				udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
				
				if (pkbuffer.hdr.type == RTLP_TYPE_DATA) {              
					//Read the data to know what the client asked for
					//The client wants the list of files
					if(strcmp(pkbuffer.payload,"SLIST")==0) {       
						printf("The client requested the LIST\n");     
						//Read the directory
						char **files;
						const char *path = ".";
						int nb_files;
						nb_files=0;
						files = get_all_files(files,path,&nb_files);
						printf("nb_files: %i\n",nb_files);
						if (!files) {
							printf("Cannot read the directory\n");
							return -1;
						}
						//Put the char** files into a char*
						u=0;
						
						while(u<nb_files) {  
							strcat(list_to_send,files[u]); 
							strcat(list_to_send,"\n");
							u++;                    
						}            
						while(u--) {
							free(files[u]);
						}
						//Number of packets: here it is 1 since we dont have much to send (only the list).
						msg_size = 1;
						send=0;   // condition to enter the next loop (in order to send the list)
						memcpy(data,list_to_send,strlen(list_to_send));
						length_last_packet = strlen(list_to_send);
						first_seq_number = pkbuffer.hdr.seqnbr+2;   //first sequence number of the file sent
						printf("List of files sent to client (first file is: %s)\n",files[1]);
					}
					//The client requests a file (GET)
					else if (pkbuffer.payload[0] == 'G' && pkbuffer.payload[1] == 'E' && pkbuffer.payload[2] == 'T') {      
						//Get the name of the file
						len = strlen(pkbuffer.payload);
						bzero(filename, 50);
						for(k=0;k<len-4;k++)
							filename[k]=pkbuffer.payload[k+4];
						printf("The client requested the file '%s'\n",filename); 
						//Get the size of the file and calculate the number of packets           
						f = fopen(filename, "rb"); 

						if (f != NULL) {
							fseek(f, 0, SEEK_END); 
							longlen= ftell(f);
							fseek(f, 0, 0);         
						}
						if(longlen%RTLP_MAX_PAYLOAD_SIZE >0){
							msg_size = longlen/RTLP_MAX_PAYLOAD_SIZE + 1;
						}
						else {
							msg_size = longlen/RTLP_MAX_PAYLOAD_SIZE;
						}

						//Put the file into void* data. We put all the file in data. It will be cut after.
						send=1;
						length_last_packet = longlen - (msg_size-1)*RTLP_MAX_PAYLOAD_SIZE;
						first_seq_number = pkbuffer.hdr.seqnbr+2;   //first sequence number of the file sent
						printf("File '%s' ready to be sent\n",filename);
					}
					//The client wants to put a file on the server.
					else if(pkbuffer.payload[0] == 'P' && pkbuffer.payload[1] == 'U' && pkbuffer.payload[2] == 'T') {
						len = strlen(pkbuffer.payload);
						for(k=0;k<len-4;k++)
							filename[k]=pkbuffer.payload[k+4];
						printf("The client wants to put the file '%s' on the server\n",filename); 
						//Indicate the first seq nbr of the file to receive
						first_seq_number = pkbuffer.hdr.seqnbr+2;
						send=2;
					}
				}
				//The server receives a FIN packet
				else if(pkbuffer.hdr.type == RTLP_TYPE_FIN) {
					create_pkbuf(&pkbuffer, RTLP_TYPE_ACK,spcb->last_seq_num_received+1,0, NULL,0);
					printf("The server wants to close the connection\n");
					if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
						return -1;
					}
					spcb->state = RTLP_STATE_FIN_WAIT;
					tv.tv_sec=10;
					if(select(spcb->sockfd+1, &readfds, NULL, NULL, &tv)>0){
						goto begin;
					}
					spcb->last_seq_num_sent = spcb->last_seq_num_sent +1;
					spcb->state = RTLP_STATE_CLOSED;
					printf("Termination successful\n");
					return 0;
				}
				else if(pkbuffer.hdr.type == RTLP_TYPE_ACK) {
				//Do nothing
				}
				else {
					rtlp_server_reset(spcb,&from);
					printf("Server misbehaviour\n");
					return -1;
				}

			}
		}                       

		//Initialize send_packet_buffer. 
		for(j=0;j<spcb->window_size;j++){
			spcb->send_buf[j].hdr.seqnbr = -1;
		}

		i=0;
		int repeat = 0;
		spcb->max_ack_received=0;
		//Process to send the WHOLE file and handle the acks.
		while( check_ack!=msg_size && send<2 ){
			j=0;  
			//Fill the packet buffer
			while(i<msg_size && j<spcb->window_size){   
				if(spcb->send_buf[j].hdr.seqnbr == -1){  //Check if the buffer has free space
					
					if(i<msg_size-1 && send == 1){  
						bzero(data,RTLP_MAX_PAYLOAD_SIZE);           
						fread(data,RTLP_MAX_PAYLOAD_SIZE,1,f);
					}
					else if(send == 1) {
						bzero(data,RTLP_MAX_PAYLOAD_SIZE); 
						fread(data,length_last_packet,1,f);
					}
					bzero(payloadbuff,sizeof(payloadbuff));
					if(i==msg_size-1){
						create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,spcb->last_seq_num_sent + 1,msg_size,data,length_last_packet); 
					printf("Payload: %s\n",pkbuffer.payload);
					}else{
						create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,spcb->last_seq_num_sent + 1,msg_size,data,RTLP_MAX_PAYLOAD_SIZE);
					}
					memcpy(&spcb->send_buf[j],&pkbuffer,sizeof(struct pkbuf));                      
					i++;
					spcb->last_seq_num_sent = spcb->last_seq_num_sent + 1; //Increase the seq. number
				}
				j++;
			}
			j=0;// j = number of packets that we sent.
			//Send the packets in the buffer
			for(j=0;j<spcb->window_size;j++){
				if(spcb->send_buf[j].hdr.seqnbr != -1){
					send_packet(&spcb->send_buf[j], spcb->sockfd, from);
				}
			}

			//handle ACKs

			//clear the set ahead of time
			FD_ZERO(&readfds);
			// add our descriptors to the set 
			FD_SET(spcb->sockfd, &readfds);
			
			tv.tv_sec = 0;
		
			//If there are already ACKs.
			while(select(spcb->sockfd+1, &readfds, NULL, NULL, &tv)>0) {
				if (FD_ISSET(spcb->sockfd, &readfds)) {

					bzero(udp_buffer, sizeof(udp_buffer));
					if((len_packet=recv(spcb->sockfd,udp_buffer,sizeof(udp_buffer),0)) < 0) {
						perror("Couldn't receive from socket");
					}
					udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
					printf("Packet of type %d received\n", pkbuffer.hdr.type );

					if ( pkbuffer.hdr.type == RTLP_TYPE_ACK ) {  // If the received message is an ACK
						printf("Packets with sequence number < %d are acknwoledged\n",pkbuffer.hdr.seqnbr);
						for(k=0;k<spcb->window_size;k++){    // We delete the acquitted message from the send_packet_buffer
							if(spcb->send_buf[k].hdr.seqnbr <  pkbuffer.hdr.seqnbr){  
								j--;
								nb_timeout = 0;
								spcb->send_buf[k].hdr.seqnbr = -1;
								if(pkbuffer.hdr.seqnbr>spcb->max_ack_received)
									spcb->max_ack_received=pkbuffer.hdr.seqnbr;
								repeat=-1;
							}
						}
					}
					else if(pkbuffer.hdr.type == RTLP_TYPE_FIN)  {
						create_pkbuf(&pkbuffer, RTLP_TYPE_ACK,spcb->last_seq_num_received+1,0, NULL,0);
						printf("The server wants to close the connection\n");
						if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
							return -1;
						}
						spcb->state = RTLP_STATE_FIN_WAIT;
						tv.tv_sec=10;
						if(select(spcb->sockfd+1, &readfds, NULL, NULL, &tv)>0){
							if (FD_ISSET(spcb->sockfd, &readfds)) {
								bzero(udp_buffer, sizeof(udp_buffer));
								if((len_packet=recv(spcb->sockfd,udp_buffer,sizeof(udp_buffer),0)) < 0) {
									perror("Couldn't receive from socket");
								}
								udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
								printf("Packet of type %d received\n", pkbuffer.hdr.type );
								if(pkbuffer.hdr.type == RTLP_TYPE_FIN)
									goto begin;
							}
						}
						spcb->last_seq_num_sent = spcb->last_seq_num_sent +1;
						spcb->state = RTLP_STATE_CLOSED;
						printf("Termination successful\n");
						return 0;
					}
					else if(pkbuffer.hdr.type == RTLP_TYPE_DATA) {
						//Acknowledge a DATA packet received out of sequence
						create_pkbuf(&pkbuffer, RTLP_TYPE_ACK,pkbuffer.hdr.seqnbr+1,0, NULL,0);
						if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
							return -1;
						}
					}
					else {
						rtlp_server_reset(spcb,&from);
						printf("Server misbehaviour\n");
						return -1;
					}
				}
				jump_select = 1;

				//clear the set ahead of time
				FD_ZERO(&readfds);
				// add our descriptors to the set 
				FD_SET(spcb->sockfd, &readfds); 
			}

			tv.tv_sec = 1;
			//If there were not already ACKs, we wait for some.
			if(jump_select!=1) {
				printf("There is no ACK in the socket, we are waiting for one second for some\n");
				if(select(spcb->sockfd+1, &readfds, NULL, NULL, &tv)>0) {
					if (FD_ISSET(spcb->sockfd, &readfds)) {

						bzero(udp_buffer, sizeof(udp_buffer));
						if((len_packet=recv(spcb->sockfd,udp_buffer,sizeof(udp_buffer),0)) < 0) {
							perror("Couldn't receive from socket");
						}
						udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
						printf("Packet of type %d received\n", pkbuffer.hdr.type );

						if ( pkbuffer.hdr.type == RTLP_TYPE_ACK ) {  // If the received message is an ACK
							printf("Packets with sequence number < %d are acknwoledged\n",pkbuffer.hdr.seqnbr);
							for(k=0;k<spcb->window_size;k++){           // We delete the acquitted message from the send_packet_buffer
								if(spcb->send_buf[k].hdr.seqnbr <  pkbuffer.hdr.seqnbr){   
									j--;
									nb_timeout = 0;
									spcb->send_buf[k].hdr.seqnbr = -1;
									if(pkbuffer.hdr.seqnbr>spcb->max_ack_received)
										spcb->max_ack_received=pkbuffer.hdr.seqnbr;
									repeat=-1;
								}
							}
							
						} 
						else if(pkbuffer.hdr.type == RTLP_TYPE_FIN)  {
							create_pkbuf(&pkbuffer, RTLP_TYPE_ACK,spcb->last_seq_num_received+1,0, NULL,0);
							printf("The server wants to close the connection\n");
							if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
								return -1;
							}
							spcb->state = RTLP_STATE_FIN_WAIT;
							tv.tv_sec=10;
							if(select(spcb->sockfd+1, &readfds, NULL, NULL, &tv)>0){
								if (FD_ISSET(spcb->sockfd, &readfds)) {
									bzero(udp_buffer, sizeof(udp_buffer));
									if((len_packet=recv(spcb->sockfd,udp_buffer,sizeof(udp_buffer),0)) < 0) {
										perror("Couldn't receive from socket");
									}
									udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
									printf("Packet of type %d received\n", pkbuffer.hdr.type );
									if(pkbuffer.hdr.type == RTLP_TYPE_FIN)
										goto begin;
								}
							}
							spcb->last_seq_num_sent = spcb->last_seq_num_sent +1;
							spcb->state = RTLP_STATE_CLOSED;
							printf("Termination successful\n");
								return 0;
						}
						else {
							rtlp_server_reset(spcb,&from);
							printf("Server misbehaviour\n");
							return -1;
						}
					}
				}
			}
			
			jump_select=0;
			check_ack = spcb->max_ack_received-first_seq_number;
			repeat=repeat+1;
			if(repeat>0)
				printf("Number of repeat: %d\n",repeat);
			//After 3 retransmissions, abort the transfer.
			if (repeat>2){
				printf("The client does not answer anymore. Abort transfer.\n");
				rtlp_server_reset(spcb,&from);
				return -1;
			}
			
			//printf("check_ack: %d,first_seq_number: %d ,max_ack_received:%d   ,  msg_size: %d\n",check_ack,first_seq_number,spcb->max_ack_received,msg_size);
		}  
		if (send==1)
			printf("The file '%s' has been sent successfully\n",filename);




		//The client sends a file to the server
		while(send==2){
		}
	}	
	return 0;       
}




int rtlp_server_reset(struct rtlp_server_pcb *spcb, struct sockaddr_in *from){

	struct pkbuf pkbuffer;
	create_pkbuf(&pkbuffer, RTLP_TYPE_RST, 0,0, NULL,0);

	printf("Reset packet created\n");

	if(send_packet(&pkbuffer, spcb->sockfd, *from) <0){
		return -1;
	}
	spcb->state = RTLP_STATE_CLOSED;

	return 0;

}



