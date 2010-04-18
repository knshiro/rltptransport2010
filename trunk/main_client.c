#include "rtlp.h"
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
    struct rtlp_client_pcb cpcb;
    char *dst_addr= "127.0.0.1";
    int check = rtlp_connect(&cpcb, dst_addr,4500);
    printf("rtlp_connect ends : %i\n",check);

    char entry[100];
    char cmd[5];
    char outfile[50];
    char payloadbuff[RTLP_MAX_PAYLOAD_SIZE];

    printf(">>>>1st transfert\n");


    while(1){


        //fgets(entry,100,stdin);
        scanf("%s",cmd);
        printf("Cmd : %s\n",cmd);
        if(strcmp(cmd,"r")==0){
            rtlp_transfer(&cpcb,NULL,0,NULL);	
        }
        if(strcmp(cmd,"q")==0){
            break;
        }

        if(strcmp(cmd,"SLIST")==0){
            rtlp_transfer(&cpcb,"SLIST",5,NULL);
        } 
        else if (strcmp(cmd,"CLIST")==0){
            //rtlp_transfer(&cpcb,"SLIST",5,outfile);
        }
        else {

            //GET command
            if(strcmp(cmd,"GET")==0){
                scanf("%s",outfile);			
                strcpy(entry,cmd);			
                strcat(entry," ");
                strcat(entry,outfile);
                printf("Cmd : %s Outfile : %s, Entry: %s\n", cmd, outfile,entry);
                rtlp_transfer(&cpcb,entry,strlen(entry),"output");
                while(cpcb.total_msg_size != cpcb.size_received ){
                    scanf("%s",cmd);
                    if(strcmp(cmd,"r")==0){
                        rtlp_transfer(&cpcb,NULL,0,"output");	
                        printf("Received %d of %d packets\n",cpcb.size_received,cpcb.total_msg_size);
                    }
                    if(strcmp(cmd,"q")==0){
                        printf("Transfert interrupted by user\n");
                        rtlp_client_reset(&cpcb);
                        return 0;
                    }
                }
                rtlp_close(&cpcb);
                return 0; 
            }


            else if(strcmp(cmd,"PUT")==0){
                int i,msg_size,longlen,length_last_packet,current_length;
                FILE * f;
                char data[RTLP_MAX_PAYLOAD_SIZE];

                //Write command 
                scanf("%s",outfile);			
                strcpy(entry,cmd);			
                strcat(entry," ");
                strcat(entry,outfile);
                printf("Cmd : %s Outfile : %s, Entry: %s\n", cmd, outfile,entry);

                //Send command
                rtlp_transfer(&cpcb,entry,strlen(entry),NULL);

                //Open file
                fopen(outfile,"rb");
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
                length_last_packet = longlen - (msg_size-1)*RTLP_MAX_PAYLOAD_SIZE; 
                i=0;
                while(i<msg_size){

                    if(i<msg_size-1){
                        current_length = RTLP_MAX_PAYLOAD_SIZE;
                    }
                    else {
                        current_length = length_last_packet;
                    }
                    scanf("%s",cmd);
                    
                    if(strcmp(cmd,"n")==0){
                        //if(
                        rtlp_transfer(&cpcb,NULL,0,NULL);	
                    }
                    
                    if(strcmp(cmd,"r")==0){
                        rtlp_transfer(&cpcb,NULL,0,NULL);	
                    }
                    if(strcmp(cmd,"q")==0){
                        printf("Transfert interrupted by user\n");
                        break;
                    }
                }
                break;
            }
        }

    }


    return 0;
}




int rtlp_test(struct rtlp_client_pcb *cpcb){

    int sockfd,i,srv,numRead;
    sockfd=cpcb->sockfd;
struct pkbuf *pkbuffer;
pkbuffer = (struct pkbuf*)malloc(sizeof(struct pkbuf));

char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];
fd_set readfds;

while(1) {

srv = select(sockfd+1, &readfds, NULL, NULL, 0);
if (srv == -1) {
	perror("select"); // error occurred in select()
} else if (srv == 0) {
	printf("Timeout occurred! No data after 2 seconds.\n");
        i++;
} else {
	// one or both of the descriptors have data
	if (FD_ISSET(sockfd, &readfds)) {
		bzero(rtlp_packet, sizeof(rtlp_packet));
        	if(numRead = recv(sockfd,rtlp_packet,sizeof(rtlp_packet),0) < 0)
		{
		      perror("Couldnt' receive from socket");
		}

        	udp_to_pkbuf(pkbuffer, rtlp_packet, numRead);
        	printf("PPPPacket of type %d received\n", pkbuffer->hdr.type );
        	if ( pkbuffer->hdr.type == RTLP_TYPE_DATA ) {
          		printf("Received Data\n");
         		printf("pkbuffer.payload: %s\n",pkbuffer->payload);	
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
