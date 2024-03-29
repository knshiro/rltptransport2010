#include "rtlp.h"
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

int clist();

int debug;
float lossprob;

int main(int argc, char **argv)
{
    debug = 0;
    lossprob = 0;
    struct rtlp_client_pcb cpcb;
    char *dst_addr = NULL;
    int port;

    char entry[100];
    char cmd[5];
    char outfile[50];

    fd_set readfds;
    struct timeval tv;
    int i,msg_size,longlen,length_last_packet,current_length,dataSent,srv,nb_timeout;
    FILE * f;
    char data[RTLP_MAX_PAYLOAD_SIZE];
    int window_size = 8;
    double lossprob = 0;
    char * lvalue = NULL;
    char * wvalue = NULL;
    char * payload = NULL;
    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "dw:l:")) != -1) {
        switch (c)
        {
            case 'd':
                debug = 1;
                break;
            case 'w':
                wvalue = optarg; 
                break;
            case 'l':
                lvalue = optarg;
                break; 
            case '?':
                if (optopt == 'w' || optopt == 'l')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                return 1;
            default:
                abort ();
        }
    }
    if(argc - optind< 2) {
        fprintf(stderr,"Usage %s [-d] [-l lossprob] [-w send_window] server port\n", argv[0]); 
        return 1;
    } else {
        dst_addr = argv[optind];
        port = atoi(argv[optind+1]);
    }
    if(lvalue != NULL){
        lossprob = atof(lvalue);
        printf("lva %s, los %f\n",lvalue,lossprob);
        if(lossprob > 1 || lossprob < 0){
            fprintf (stderr, "lossprob parameter must be between 0 and 1\n");
            return 1;
        }
    }
    if(wvalue != NULL){
        window_size = atoi(wvalue);
        if(window_size > RTLP_MAX_SEND_BUF_SIZE){
            window_size = RTLP_MAX_SEND_BUF_SIZE;
            printf("Warning : window size exceed max size, set to %d\n",window_size);
        } 
        else if (window_size < 1){
            window_size = 8;
            fprintf (stderr, "Window size must be > 1\n");
            return 1;
        }
    }
    //Connection
    printf("Try to connect to %s port %d\n",dst_addr,port);
    if(rtlp_connect(&cpcb, dst_addr,port)<0){
        fprintf(stderr,"Connexion impossible\n");
        return 1;
    }

    printf(">>>>1st transfert\n");


    while(1){

        /* clear the set ahead of time */
        FD_ZERO(&readfds);
        /* add our descriptors to the set */
        FD_SET(0, &readfds);
        FD_SET(cpcb.sockfd,&readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        if( select(cpcb.sockfd+1, &readfds, NULL, NULL, &tv)<0){
            perror("Select :");
            return 1;
        }  
        if(FD_ISSET(cpcb.sockfd, &readfds)) {
            rtlp_transfer(&cpcb,NULL,0,NULL);	
        }
        if(FD_ISSET(0, &readfds)) {
            rtlp_transfer(&cpcb,NULL,0,NULL);	
            //fgets(entry,100,stdin);
            scanf("%s",cmd);
            if(strcmp(cmd,"q")==0){
                break;
            }

            if(strcmp(cmd,"SLIST")==0){
                rtlp_transfer(&cpcb,"SLIST",5,NULL);
            } 
            else if (strcmp(cmd,"CLIST")==0){
                int check = clist();
            }
            else if (strcmp(cmd,"HELP")==0){
                printf("GET <filename> - Gets the file from the server\n");
                printf("PUT <filename> - Transfers the file to the server\n");
                printf("SLIST - Lists the files which the server can access\n");
                printf("CLIST - Lists the files which the client can access\n");
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

                    fflush(stdout);
                    nb_timeout=0;
                    while((cpcb.total_msg_size != cpcb.size_received) && nb_timeout<10 ){
                        tv.tv_sec = 1;
                        tv.tv_usec = 0;
                        fflush(stdout);
                        /* clear the set ahead of time */
                        FD_ZERO(&readfds);
                        /* add our descriptors to the set */
                        FD_SET(0, &readfds);
                        FD_SET(cpcb.sockfd,&readfds);
                        srv = select(cpcb.sockfd+1, &readfds, NULL, NULL, &tv);  
                        if(srv>0){
                            nb_timeout = 0;
                        } 
                        else if(srv == 0) {
                            nb_timeout++;
                        }
                        else if(srv == -1){
                            perror("Select");
                            rtlp_client_reset(&cpcb);
                            return 1;
                        }
                        printf("Timeout : %d\n",nb_timeout);
                        /* User typed into the keyboard */
                        if(FD_ISSET(0, &readfds)) {
                            fgets(entry, sizeof(entry), stdin);
                            if(entry[0] == 'q') {
                                printf("Transfert interrupted by user\n");
                                rtlp_client_reset(&cpcb);
                                return 0;
                            } 
                        }

                        if(rtlp_transfer(&cpcb,NULL,0,"output") < 0){
                            fprintf(stderr,"Transfert interrupted by server\n");
                            return 1;
                        };	
                        printf("Received %d of %d packets\n",cpcb.size_received,cpcb.total_msg_size);
                    }
                    if(nb_timeout == 10){
                        printf("Lost connection, too many timeouts\n");
                        rtlp_client_reset(&cpcb);
                        return 1;
                    }
                    else {
                        rtlp_close(&cpcb);
                        return 0; 
                    } 
                }

                //PUT command

                else if(strcmp(cmd,"PUT")==0){
                    //Write command 
                    scanf("%s",outfile);			
                    strcpy(entry,cmd);			
                    strcat(entry," ");
                    strcat(entry,outfile);
                    printf("Cmd : %s Outfile : %s, Entry: %s\n", cmd, outfile,entry);

                    //Send command
                    rtlp_transfer(&cpcb,entry,strlen(entry),NULL);

                    //Open file
                    f = fopen(outfile,"rb");
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
                    dataSent = 1;
                    cpcb.total_msg_size = msg_size;


                    nb_timeout = 0;

                    while((i<msg_size || (cpcb.last_seq_num_sent+1 != cpcb.last_seq_num_ack)) && nb_timeout < 10){

                        tv.tv_sec = 1;
                        tv.tv_usec = 0;
                        fflush(stdout);
                        /* clear the set ahead of time */
                        FD_ZERO(&readfds);
                        /* add our descriptors to the set */
                        FD_SET(cpcb.sockfd, &readfds);
                        FD_SET(0, &readfds);
                        srv = select(cpcb.sockfd+1, &readfds, NULL, NULL, &tv);  
                        if(srv>0){
                            nb_timeout = 0;
                        }
                        else if(srv == 0) {
                            nb_timeout++;
                        } else if(srv == -1){
                            perror("Select");
                            rtlp_client_reset(&cpcb);
                            return 1;
                        }
                        /* User typed into the keyboard */
                        if(FD_ISSET(0, &readfds)) {
                            fgets(entry, sizeof(entry), stdin);
                            if(entry[0] == 'q') {
                                printf("Transfert interrupted by user\n");
                                rtlp_client_reset(&cpcb);
                                return 0;
                            } 
                        }
                        printf(">>>>>>>>>>>>>>Try to send packet %d<<<<<<<<<<\n\n",i);
                        if(i<msg_size-1){
                            current_length = RTLP_MAX_PAYLOAD_SIZE;
                        } else {
                            current_length = length_last_packet;
                        }
                        if(i<msg_size){
                            if(dataSent){
                                bzero(data,RTLP_MAX_PAYLOAD_SIZE);           
                                fread(data,current_length,1,f);
                                payload = data;
                            } 
                        } else {
                            payload = NULL;
                        }

                        printf("Packet number %d of size %d\n",i,current_length);

                        if(rtlp_transfer(&cpcb,payload,current_length,NULL) && i<msg_size)
                        {
                            dataSent = 1;
                            i++;
                        }
                        fflush(stdout);
                    }
                    if(nb_timeout == 10){
                        printf("Lost connection, too many timeouts\n");
                        rtlp_client_reset(&cpcb);
                        return 1;
                    }
                    else {
                        rtlp_close(&cpcb);
                        return 0; 
                    }
                }
            }

        }
    }

    return 0;
}


int clist(){

    char **files;
    const char *path = ".";
    int nb_files;
    nb_files=0;
    int u;
    char list_client[RTLP_MAX_PAYLOAD_SIZE];
    files = get_all_files(files,path,&nb_files);
    printf("nb_files: %i\n",nb_files);
    if (!files) {
        printf("Cannot read the directory\n");
        return -1;
    }
    //Put the char** files into a char*
    u=0;

    while(u<nb_files-2) {  
        if(strcmp(files[u],".")!=0 || strcmp(files[u],"..")!=0){
            strcat(list_client,files[u]); 
            strcat(list_client,"\n");
            u++;                 
        }   
    }            
    printf("List Client:\n%s\n",list_client);
    while(u--) {
        free(files[u]);
    }
    return 0;
}



/*
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
*/
