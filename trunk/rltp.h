/*
 * File Name:        rltp.h
 *
 * Last Modified:    07/03/2010
 *
 * Author:           Marius Portmann - 2007
 * Modified By:      David Belvedere - 2009 - Added flow control
 *                   David Belvedere - 2010 - Updates from 2009. 
 *                       Addition of bidirection connection.
 *
 * Description:
 *			
 * This head file provides the basic function definitions and 
 * definitions for the implementaion of the Reliable Transport 
 * Layer Protocol (RLTP)
 */

#ifndef RLTP_H
#define RLTP_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>


/*************************** RLTP Packet Types
****************************/
#define RLTP_TYPE_DATA 1 /* Data packet */
#define RLTP_TYPE_ACK  2 /* Acknowledgement packet*/
#define RLTP_TYPE_SYN  3 /* SYN packet, initiates connection setup*/
#define RLTP_TYPE_FIN  4 /* FIN packet, initiates closing of connection */
#define RLTP_TYPE_RST  5 /* Reset packet, resets connection */


/*************************** RLTP Protocol State ****************************/
/* Max payload */
#define RLTP_MAX_PAYLOAD_SIZE		1024
#define RLTP_MAX_SEND_BUF_SIZE		4

/* Client and Server states */
#define RLTP_STATE_ESTABLISHED		1
#define RLTP_STATE_CLOSED			2

/* Client only states */
#define RLTP_STATE_SYN_SENT			3
#define RLTP_STATE_FIN_WAIT			4

/* Server only states */
#define RLTP_STATE_LISTEN			5
#define RLTP_STATE_TIME_WAIT		6

/* Structure of the header */
struct rltp_hdr {
	uint32_t type;			/* The type of packet */
	uint32_t seqnbr;		/* The sequence number of the packet */
	uint32_t total_msg_size; /* The total size of the file being sent - only
							   the server should set this */
};

/* Structure to store the packet which was sent/received */
struct pkbuf {
	struct rltp_hdr hdr;		/* Header of the packet */
	int len;					/* Length of the payload and header */
	char payload[RLTP_MAX_PAYLOAD_SIZE]; 	/* Payload of packet */
};

/* Client PCB */
struct rltp_client_pcb {
	int sockfd;			/* Corresponding UDP socket */
	int state;			/* Connection state */
	/* Current active pkbuf on the network */
	struct pkbuf send_buf[RLTP_MAX_SEND_BUF_SIZE]; 		

	/* Your extensions here */
};

/* Server PCB */
struct rltp_server_pcb {
	int sockfd;			/* Corresponding UDP packet */
	int state;			/* Connection state */

	/* Your extensions here */
};

/*************************** Function Prototypes ****************************/

/**************************** Client Prototypes *****************************/

int rltp_connect(struct rltp_client_pcb *cpcb, char *dst_addr, int dst_port);
/*
 * Functionality:
 * Called by the client to establish a connection to the server. Performs a 
 * 2-way connection establishment handshake. If successful, the client PCB 
 * structure (cpcb) is initialised.
 *
 * Parameters:
 * 		cpcb		: client PCB
 * 		dst_addr	: string with server IP address, e.g. "192.168.2.33"
 * 		dst_port	: server port number
 *
 * Returns:
 * 		0 on success, -1 on failure
 */

int rltp_close(struct rltp_client_pcb *cpcb);
/*
 * Functionality:
 * Called by the client to terminate a connection. Performs a 2-way connection
 * termination handshake. The client PCB structure (cpcb) is updated 
 * accordingly, i.e. state -> Closed. 
 *
 * This function blocks until all data packets have been acknowledged and the
 * FIN/ACK has been received or until 3 unsuccessful retransmission have 
 * occurred.
 *
 * Parameters:
 * 		cpcb	: client PCB
 *
 * Returns:
 * 		0 on success, -1 on failure
 */

int rltp_client_reset(struct rltp_client_pcb *cpcb);
/*
 * Functionality:
 * Called by the client to reset a connection. Independent of the state of the
 * connection (except for the CLOSED state), this function sends a RST packet
 * to the server, and resets the connection to the CLOSED state.
 *
 * Parameters:
 * 		cpcb	: client PCB
 *
 * Returns:
 * 		0 on success, -1 on failure
 */

int strp_transfer(struct rltp_client_pcb *cpcb, void *data, int len, 
		char* outfile);
/*
 * Functionality:
 * Called by the client application to send a data packet to the server. The 
 * maximum number of bytes the function accepts is RLTP_MAX_PAYLOAD_SIZE.
 *
 * Since our protocol is implemented at the application layer, we cannot rely
 * on the kernel to do all the asyncronous ARQ processing such as receiving 
 * and processing of ACKs, managing time-outs, retransmitting data packets etc
 * . To simplify things, we can do all the ARQ processing within a
 * single flow of control, i.e. whenever rltp_send is called.
 *
 * The function therefore needs to provide functionality such as the 
 * following:
 * 		- Receiving and processing of ACKs
 *
 *		- Receiving and processing of DATA packets
 *
 * 		- If there is space in the send buffer, put data packet into send 
 * 		  buffer, send it over the network, and start a corresponding timer.
 *
 *		- ACK a corresponding DATA packet when its receiving data (this should
 *		  only occur when the user wishes to perform a SLIST or GET.
 *
 * 		- If the send buffer is already full, the function should sleep for a 
 * 		  short amount of time, and then see if there are any new ACKs that 
 * 		  would result in freed up space in the send buffer.
 *
 * 		- The function also needs to check if there are any retransmission 
 * 		  timeouts, and needs to do the necessary retransmissions, and update
 * 		  the timers.
 *
 * 		- etc.
 *
 * Parameters:
 * 		cpcb	: client PCB
 * 		data	: pointer to the buffer with data to send
 * 		len		: number of bytes to send
 *		outfile : the filename which will be create/modifed when invoking GET
 *
 * Returns:
 * 		0 on success, -1 on failure
 */

/**************************** Server Prototypes *****************************/

int rltp_listen(struct rltp_server_pcb *spcb, int port);
/* 
 * Functionality:
 * The function opens a UDP socket on the specified port and initialised the 
 * server PCB accordingly.
 *
 * Parameters:
 * 		spcb	: server PCB
 * 		port	: port number on which the server is listening
 *
 * Returns:
 * 0 on success, -1 on failure
 */

int rltp_accept(struct rltp_server_pcb *spcb);
/*
 * Functionality:
 * The function blocks and waits for SYN packers. It then performs the 2-way
 * connection establishment handshake. The function can only be called if the 
 * connection is in state LISTEN. The function updates the server PCB 
 * according to the result of the 2-way handshake.
 *
 * Parameters:
 * 		spcb	: server PCB
 *
 * Returns:
 * 0 on success, -1 on failure
 */

int rltp_server_reset(struct rltp_server_pcb *spcb);
/*
 * Functionality:
 * Independent of the state of the connection (except of the CLOSED state), 
 * this function sends a RST packet ot the client, and returns the connection
 * to the CLOSED state. The function is used to deal with protocol and other 
 * errors, e.g. unexpected packets from the client.
 *
 * Parameters:
 * 		spcb	: server PCB
 *
 * Returns:
 * 0 on success, -1 on failure
 */

int rltp_transfer_loop(struct rltp_server_pcb *spcb);

/*
 * Functionality:
 * This function handles all the ARQ processing on the server side. The 
 * function implements a loop in which data packets are received or sent
 * The function also performs the corrsponding action when receiving a packet.
 *
 * The function also needs to handle the 2-way connection termination 
 * handshake.
 *
 * The function should react to any protocol error, i.e. unexpected behaviour 
 * of the client with calling rltp_server_reset() and aborting.
 *
 * Parameters:
 * 		spcb		: server PCB
 *
 * Returns:
 * 0 on success, -1 on failure
 */

#endif

