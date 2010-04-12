LIBS =
CC = gcc -Wall -g

all:   client server 

client:    rtlp_client.o RTLP_util.o
	${CC} -o $@ $^ ${LIBS}

server:    server.o RTLP_util.o
	${CC} -o $@ $^ ${LIBS}

clean:
	rm *.o client server

%.o: %.c
	${CC} -o $@ -c $< ${LIBS}
