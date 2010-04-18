LIBS =
CC = gcc -Wall -g -std=gnu99

all:   client server 

client:    main_client.o rtlp_client.o RTLP_util.o sendto_unrel.o
	${CC} -o $@ $^ ${LIBS}

server:    main_server.o rtlp_server.o RTLP_util.o sendto_unrel.o
	${CC} -o $@ $^ ${LIBS}

clean:
	rm *.o client server

%.o: %.c
	${CC} -o $@ -c $< ${LIBS}
