#include "rtlp.h"
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

int main(int argc, char **argv)
{

  struct rtlp_server_pcb spcb;


  spcb.window_size=8;

  double lossprob = 0;
  char * lvalue = NULL;
  char * wvalue = NULL;
  int c;
  opterr = 0;
  int port;
  int debug = 0;

  while ((c = getopt (argc, argv, "dl:w:")) != -1) {	
	switch (c)
        {
        	case 'd':
        		debug = 1;
               		break;
		case 'l':
               		lvalue = optarg;
                	break; 
            	case 'w':
                	wvalue = optarg; 
                	break;
            	case '?':
                	if (optopt == 'w' || optopt == 'l')
                    		fprintf (stderr, "Option -%c requires an argument.\n", optopt);
               		else if (isprint (optopt))
                    		fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                	else
                    		fprintf (stderr,"Unknown option character `\\x%x'.\n",optopt);
                	return 1;
            	default:
                	abort ();
        }
  }

  if(argc - optind< 1) {
	fprintf(stderr,"Usage %s [-d] [-w send_window] [-l lossprob] port\n", argv[0]); 
        return 1;
  } 
  else {
        port = atoi(argv[optind]);
  }

  if(lvalue != NULL){
        lossprob = atof(lvalue);
        if(lossprob > 1 || lossprob < 0){
            fprintf (stderr, "lossprob parameter must be between 0 and 1\n");
            return 1;
        }
  }

  if(wvalue != NULL){
        spcb.window_size = atoi(wvalue);
        if(spcb.window_size > RTLP_MAX_SEND_BUF_SIZE){
            spcb.window_size = RTLP_MAX_SEND_BUF_SIZE;
            printf("Warning : window size exceed max size, set to %d\n",spcb.window_size);
        } 
  }
  else if (spcb.window_size < 1){
            spcb.window_size = 8;
            fprintf (stderr, "Window size must be > 1\n");
            return 1;
  }
    
int check = rtlp_listen(&spcb, port);
spcb.lossprob = lossprob;
printf("The server is listening....\n");
check = rtlp_accept(&spcb);
int check3 = rtlp_transfer_loop(&spcb);
return 0;
}
