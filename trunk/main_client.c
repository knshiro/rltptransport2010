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
struct rtlp_client_pcb *cpcb = (struct rtlp_client_pcb *)malloc(sizeof(struct rtlp_client_pcb));
char *dst_addr= "127.0.0.1";
int check = rtlp_connect(cpcb, dst_addr,4500);
printf("%i\n",check);
int check2 = rtlp_close(cpcb);
printf("%i\n",check2);

return 0;
}
