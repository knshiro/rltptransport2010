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
	char *filename=(char *)malloc(sizeof(char));
	char data[RTLP_MAX_PAYLOAD_SIZE];

	struct sockaddr_in from;
	unsigned int fromlen;
	fromlen = sizeof(from);

	int len_packet;
	int send=0; 
	FILE * f;

	char * list_to_send = (char*)malloc(sizeof(char));

	char ** packets_received = (char**)malloc(sizeof(char*));
	int* packets_received_check = (int*)malloc(sizeof(int));
	int first_seq_number;
	int nb_acks=0;
	int check_reception=1;
	int length_last_packet;
	int check_ack;

	while(1) {
		printf("Entry of the loop\n");
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
				printf("1st packet ACK created\n");
				if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
					return -1;
				}
				spcb->last_seq_num_sent = spcb->last_seq_num_sent +1;
				udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
				
				if (pkbuffer.hdr.type == RTLP_TYPE_DATA) {              

					//Read the data to know what the client asked for
					//The client wants the list of the files
					if(strcmp(pkbuffer.payload,"SLIST")==0) {       
						printf("Enter SLIST IF\n");     
						//Read the directory
						char **files;
						const char *path = ".";
						int* nb_files=(int*)malloc(sizeof(int));
						*nb_files=0;
						files = get_all_files(files,path,nb_files);
						printf("nb_files: %i\n",*nb_files);
						if (!files) {
							printf("Cannot read the directory\n");
							return -1;
						}
						//Put the char** files into a char*
						u=0;
						int nb_files2= *nb_files;
						while(u<nb_files2) {  
							strcat(list_to_send,files[u]); 
							strcat(list_to_send,"\n");
							u++;                    
						}               
						printf("list to send: %s\n",list_to_send);
						//Number of packets: here it is 1 since we dont have much to send (only the list).
						msg_size = 1;
						send=0;   // to enter the next loop (in order to send the list)
						memcpy(data,list_to_send,strlen(list_to_send));

						length_last_packet = strlen(list_to_send);
						first_seq_number = pkbuffer.hdr.seqnbr+2;   //first sequence number of the file sent
						printf("End SLIST IF\n");
					}
					//The client requests a file (GET)
					else if (pkbuffer.payload[0] == 'G' && pkbuffer.payload[1] == 'E' && pkbuffer.payload[2] == 'T') {      

						//Get the name of the file
						len = strlen(pkbuffer.payload);
						for(k=0;k<len-4;k++)
							filename[k]=pkbuffer.payload[k+4];

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
						printf("seqnbr from GET: %d\n",pkbuffer.hdr.seqnbr);
					}
					//The client wants to put a file on the server.
					else if(pkbuffer.payload[0] == 'P' && pkbuffer.payload[1] == 'U' && pkbuffer.payload[2] == 'T') {
						send=2;
						//Indicate the first seq nbr of the file to receive
						first_seq_number = pkbuffer.hdr.seqnbr;
					}
				}
				//The server receives a FIN packet
				else if(pkbuffer.hdr.type == RTLP_TYPE_FIN) {
					create_pkbuf(&pkbuffer, RTLP_TYPE_ACK,spcb->last_seq_num_received+1,0, NULL,0);
					printf("Packet FIN created\n");
					if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
						return -1;
					}
					spcb->last_seq_num_sent = spcb->last_seq_num_sent +1;
					spcb->state = RTLP_STATE_CLOSED;
					printf("Termination successful\n");
					break;
				}
				else {
					rtlp_server_reset(spcb);
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
					bzero(data,RTLP_MAX_PAYLOAD_SIZE);
					if(i<msg_size-1 && send == 1)                
						fread(data,RTLP_MAX_PAYLOAD_SIZE,1,f);
					else if(send == 1) {
						fread(data,length_last_packet,1,f);
					}
					//printf("long: %d\n",length_last_packet);
					//printf("data in pkbuffer: %s\n",data);
					bzero(payloadbuff,sizeof(payloadbuff));
					//memcpy(payloadbuff,data,RTLP_MAX_PAYLOAD_SIZE); //read MAX_PAYLOAD_SIZE and put it in the buffer
					if(i==msg_size-1){
						create_pkbuf(&pkbuffer, RTLP_TYPE_DATA,spcb->last_seq_num_sent + 1,msg_size,data,length_last_packet); 
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

			tv.tv_sec = 3;
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
						for(k=0;k<spcb->window_size;k++){    // We delete the acquitted message from the send_packet_buffer
							if(spcb->send_buf[k].hdr.seqnbr <  pkbuffer.hdr.seqnbr){  
								j--;
								nb_timeout = 0;
								spcb->send_buf[k].hdr.seqnbr = -1;
								if(pkbuffer.hdr.seqnbr>spcb->max_ack_received)
									spcb->max_ack_received=pkbuffer.hdr.seqnbr;
								printf("pkbuffer.hdr.seqnbr: %i\n",pkbuffer.hdr.seqnbr);
								repeat=-1;
							}
						}
					} 
					else if ( pkbuffer.hdr.type == RTLP_TYPE_DATA ) {
						//ERROR?!
					}
					else {
						rtlp_server_reset(spcb);
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

			tv.tv_sec = 3;
			//If there were not already ACKs, we wait for some.
			if(jump_select!=1) {
				if(select(spcb->sockfd+1, &readfds, NULL, NULL, &tv)>0) {
					if (FD_ISSET(spcb->sockfd, &readfds)) {

						bzero(udp_buffer, sizeof(udp_buffer));
						if((len_packet=recv(spcb->sockfd,udp_buffer,sizeof(udp_buffer),0)) < 0) {
							perror("Couldn't receive from socket");
						}
						udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
						printf("dPacket of type %d received\n", pkbuffer.hdr.type );

						if ( pkbuffer.hdr.type == RTLP_TYPE_ACK ) {  // If the received message is an ACK
							for(k=0;k<spcb->window_size;k++){           // We delete the acquitted message from the send_packet_buffer
								if(spcb->send_buf[k].hdr.seqnbr <  pkbuffer.hdr.seqnbr){   
									j--;
									nb_timeout = 0;
									spcb->send_buf[k].hdr.seqnbr = -1;
									if(pkbuffer.hdr.seqnbr>spcb->max_ack_received)
										spcb->max_ack_received=pkbuffer.hdr.seqnbr;
								}
							}
							jump_select=0;
						} 
						else if ( pkbuffer.hdr.type == RTLP_TYPE_DATA ) {
							//ERROR?!
						}
						else {
							rtlp_server_reset(spcb);
							printf("Server misbehaviour\n");
							return -1;
						}
					}
				}
			}
			check_ack=spcb->max_ack_received-first_seq_number;
			repeat = repeat + 1;
			printf("check_ack: %d,first_seq_number: %d ,max_ack_received:%d   ,  msg_size: %d\n",check_ack,first_seq_number,spcb->max_ack_received,msg_size);
		}  




		//The client sends a file to the server
		while(send==2){
			/* 
			   Regarder dans la socket, mettre les paquets dans le pkbuffer(udp_to_pkbuff),acquitter ceux qui peuvent. Les mettre tous dans un buffer 
			   intermédiaire classé(pkbuffer2).Quand il y a une suite de données dans le pkbuffer2, on les écrit dans le fichier.
			   Recommencer.
			   */

			//clear the set ahead of time
			FD_ZERO(&readfds);
			// add our descriptors to the set 
			FD_SET(spcb->sockfd, &readfds);

			//intitialization:we receive the first packet.
			srv = select(spcb->sockfd+1, &readfds, NULL, NULL, 0);
			if (srv == -1) {
				perror("select"); // error occurred in select()
				rtlp_server_reset(spcb);
				exit(-1);
			} 
			else {
				// one or both of the descriptors have data
				if (FD_ISSET(spcb->sockfd, &readfds)) {

					bzero(udp_buffer, sizeof(udp_buffer));
					if((len_packet=recvfrom(spcb->sockfd,udp_buffer,sizeof(udp_buffer), 0, (struct sockaddr*)&from, &fromlen)) < 0) {
						perror("Couldn't receive from socket");
					}
					udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
					printf("ePacket of type %d received\n", pkbuffer.hdr.type);
					if (pkbuffer.hdr.type == RTLP_TYPE_DATA) {
						msg_size  = pkbuffer.hdr.total_msg_size;
						u = pkbuffer.hdr.seqnbr - spcb->last_seq_num_received;
						packets_received[u-1] = (char*)malloc(sizeof(char));
						packets_received[u-1] = pkbuffer.payload;
						if(u==1) {              
							//Update the value of the last packet acknowledged.
							spcb->last_seq_num_received = pkbuffer.hdr.seqnbr;
							packets_received_check[u-1] = 1;
							//acknowledge
							create_pkbuf(&pkbuffer, RTLP_TYPE_ACK, spcb->last_seq_num_received+1,0, NULL,0);
							if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
								exit(-1);
								nb_acks= nb_acks+1;
							}
						}
						else {  
							//We don't acknowledge.
							packets_received_check[u-1] = 1;
						}
					}

				}
			}
			i=0;
			while(i<msg_size-1) {
				// Receive the rest of the data and handle ACKS
				srv = select(spcb->sockfd+1, &readfds, NULL, NULL, &tv);
				if (srv == -1) {
					perror("select"); // error occurred in select()
					return -1;
				}  
				else if (srv == 0) {
					printf("Timeout occurred! No data after 2 seconds.\n");

				}
				else {
					// one or both of the descriptors have data
					if (FD_ISSET(spcb->sockfd, &readfds)) {
						bzero(udp_buffer, sizeof(udp_buffer));
						if((len_packet=recvfrom(spcb->sockfd,udp_buffer,sizeof(udp_buffer), 0, (struct sockaddr*)&from, &fromlen)) < 0) {
							perror("Couldn't receive from socket");
						}
						udp_to_pkbuf(&pkbuffer, udp_buffer,len_packet);
						printf("Packet of type %d received\n", pkbuffer.hdr.type);
						if (pkbuffer.hdr.type == RTLP_TYPE_DATA) {
							u = pkbuffer.hdr.seqnbr - first_seq_number - nb_acks;
							packets_received[u-1] = (char*)malloc(sizeof(char));
							packets_received[u-1] = pkbuffer.payload;
							//Check if we can send an ACK.  PROBLEME: on check seulement jusqu'a u-1 mais on peut avoir deja recu des paquets situé apres !!!
							for(t=spcb->last_seq_num_received;t<u-1;t++) {
								if(packets_received_check[t]==0) {
									check_reception=0;
									break;
								}                                       
							}
							if(check_reception==1) {
								//We can ack
								spcb->last_seq_num_received = pkbuffer.hdr.seqnbr; //pas tout le temps!
								create_pkbuf(&pkbuffer, RTLP_TYPE_ACK, spcb->last_seq_num_received+1,0, NULL,0);
								if(send_packet(&pkbuffer, spcb->sockfd, from) <0){
									exit(-1);
									nb_acks= nb_acks+1;
								}       
							}
						}
					}
				}
				i++;
			}
			send=0;
		}
	}
	return 0;       
}




int rtlp_server_reset(struct rtlp_server_pcb *spcb){

	struct pkbuf pkbuffer;
	create_pkbuf(&pkbuffer, RTLP_TYPE_RST, 0,0, NULL,0);

	printf("Packet created\n");

	if(send_packet(&pkbuffer, spcb->sockfd, spcb->client_addr) <0){
		return -1;
	}
	spcb->state = RTLP_STATE_CLOSED;

	return 0;

}




//A mettre dans RTLP_util.c
//Read the content of a directory

static char *dup_str(const char *s) {
	size_t n = strlen(s) + 1;
	char *t = malloc(n);
	if (t) {
		memcpy(t, s, n);
	}
	return t;
}

static char **get_all_files(char** files, const char *path, int *i) {
	DIR *dir;
	struct dirent *dp;
	//char **files;
	size_t alloc, used;

	if (!(dir = opendir(path))) {
		goto error;
	}

	used = 0;
	alloc = 10;
	if (!(files = malloc(alloc * sizeof *files))) {
		goto error_close;
	}

	while ((dp = readdir(dir))) {
		if (used + 1 >= alloc) {
			size_t new = alloc / 2 * 3;
			char **tmp = realloc(files, new * sizeof *files);
			if (!tmp) {
				goto error_free;
			}
			files = tmp;
			alloc = new;
		}
		if (!(files[used] = dup_str(dp->d_name))) {
			goto error_free;
		}
		++used;
		*i= *i +1;
	}

	files[used] = NULL;

	closedir(dir);
	return files;

error_free:
	while (used--) {
		free(files[used]);
	}
	free(files);


error_close:
	closedir(dir);

error:
	return NULL;
}

