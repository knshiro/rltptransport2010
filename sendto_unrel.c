#include "sendto_unrel.h"

ssize_t sendto_unrel( int s, const void *msg, size_t len, int flags, const struct sockaddr *to, int tolen, float lossprob ){
   
    int randValue;
    /* initialize random seed: */
    srand ( time(NULL) );

    /* generate secret number: */
    randValue = rand() % 1001;

    if(randValue >= lossprob*1000){
        return sendto(s,msg,len,flags,to,tolen);
    } else {
        return len;
    }
    
}

