#include "RTLP_util.h"
#include "sendto_unrel.h"

struct pkbuf* create_pkbuf(struct pkbuf* buff, int type,int seqnbr,int msg_size, char * payload, int len){

  printf("\n>create_pkbuf\n");

		  printf("Payload Length: %d\n",len);
	struct rtlp_hdr hdr;

	hdr.type = type;
	hdr.seqnbr = seqnbr;
	hdr.total_msg_size = msg_size;

  bzero(buff->payload, RTLP_MAX_PAYLOAD_SIZE);

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

  printf("<create_pkbuf\n");

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

  printf("\n>>send_packet\n");
  printf("Packet Buff type: %d\n",packet->hdr.type);
  printf("Packet Buff seqnbr: %d\n",packet->hdr.seqnbr);
  printf("Packet Buff total_msg_size: %d\n",packet->hdr.total_msg_size);
  printf("packet buff len : %d\n",packet->len);
  //printf("packet buff payload : %s\n",packet->payload);
  if(packet->len > RTLP_MAX_PAYLOAD_SIZE){
    return -1;            //TODO stderr
  }
  char rtlp_packet[RTLP_MAX_PAYLOAD_SIZE+12];

  bzero((char*)rtlp_packet, RTLP_MAX_PAYLOAD_SIZE+12);
  memcpy(rtlp_packet,&packet->hdr.type,4);
  memcpy(rtlp_packet+4,&packet->hdr.seqnbr,4);
  memcpy(rtlp_packet+8,&packet->hdr.total_msg_size,4);
  memcpy(rtlp_packet+12,packet->payload,packet->len);
  printf("Data set\n");
  printf("Udp packet type : %d\n",*rtlp_packet);
  printf("Udp packet seqnbr : %d\n",*(rtlp_packet+4));
  printf("Udp packet total_msg_size : %d\n",*(rtlp_packet+8));
  
  /* write message to socket */
  if(sendto_unrel(sockfd, rtlp_packet, packet->len+12, 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr), 0.5) < 0 )
  {
    perror("Could not send packet!");
    return -1;
  }
  printf("Packet sent %d\n",  strlen(rtlp_packet));
  
  printf("<<send_packet\n\n");

  return 0;

}

struct pkbuf* udp_to_pkbuf(struct pkbuf* pkbuffer, char * udppacket, int len){

  int type,seqnbr,total_msg_size;
  char * payload;
 
  printf("Packet udp size: %d\n",  sizeof(udppacket));

  memcpy(&type,udppacket,4);
  memcpy(&seqnbr,udppacket+4,4);
  memcpy(&total_msg_size,udppacket+8,4);
  payload= udppacket+12;
  
  return create_pkbuf(pkbuffer,type,seqnbr,total_msg_size,payload,len-12);

}


/* create a socket and bind it to the local address and a port  */
int create_socket(int port){
  int sockfd;
  struct sockaddr_in client_addr;
  int length = sizeof(struct sockaddr_in);

  /*create socket*/
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
	  perror("ERROR opening socket");
  printf("Socket created\n");

  /* prepare attachment addrese = local addresse */
  client_addr.sin_family = AF_INET;

  /*  Give the local IP address and assign a port*/
  client_addr.sin_addr.s_addr=htonl(INADDR_ANY);
  

  if(port>-1)
  {
	client_addr.sin_port = htons(port);
  	if(bind(sockfd,(struct sockaddr *)&client_addr,length) == -1) {
 		perror("Impossible to bind socket to local address\n");
  		close(sockfd);
  		return -1;
 	}
  }
  return sockfd;
}

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

//A mettre dans RTLP_util.c
//Read the content of a directory

char *dup_str(const char *s) {
	size_t n = strlen(s) + 1;
	char *t = malloc(n);
	if (t) {
		memcpy(t, s, n);
	}
	return t;
}

char **get_all_files(char** files, const char *path, int *i) {
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

/*
void debug(int debugMode,const char * msg, ...){
    char * fch,lch;
    int nb_args = 0,i;
    va_list ap;
    char types[10],buffer[50];

    pch=strchr(msg,'%');
    while (fch!=NULL)
    {
        types[nb_args] = *(fch+1);
        nb_args++;
        fch=strchr(fch+1,'%');
    }

    va_start(ap, nb_args); 
    for(i=0;i<nb_args;i++){
        pch=strchr(msg,'%');
        strncpy(buffer,msg, pch-msg+1);
        switch(*pch){
            case i:
            case d:
                printf(buffer,va_arg(ap, int));
                break;
            case s:
                 printf(buffer,va_arg(s,char *));
            default:
               ; 
        }
    }
    strncpy(buffer,msg, pch-msg+1);

}
*/
