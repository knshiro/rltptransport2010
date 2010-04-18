#include "rtlp.h"
#include "RTLP_util.h"
#include <sys/select.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{

struct rtlp_server_pcb* spcb = (struct rtlp_server_pcb *)malloc(sizeof(struct rtlp_server_pcb));
spcb->window_size=8;
int check = rtlp_listen(spcb, 4500);
printf("rtlp_listen ends: %i\n",check);
check = rtlp_accept(spcb);
printf("rtlp_accept ends: %i\n",check);
int check3 = rtlp_transfer_loop(spcb);
return 0;
}