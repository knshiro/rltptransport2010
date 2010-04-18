#include <stdlib.h>
#include <time.h>
#include <netdb.h>

ssize_t sendto_unrel( int s, const void *msg, size_t len, int flags, const struct sockaddr *to, int tolen, float lossprob );
