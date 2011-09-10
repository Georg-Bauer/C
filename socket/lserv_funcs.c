/*******************************************************************
 * lserv_funcs.c
 * functions for the license server
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/errno.h>
#include "dgram.h"

#define SERVER_PORTNUM 2020				// our server's port number
#define MSGLEN	128					// size of out datagrams
#define TICKET_AVAIL 0					// Slot is available for use
#define MAXUSERS 3					// Only 3 users for us
#define RECLAIM_INTERVAL 60				// reclaim every 60 seconds

#define oops(x) {perror(x);exit(-1);}

/*******************************************************************
 * Important variables
 */
int ticket_array[MAXUSERS];				// Our ticket array
int sd = -1;						// Our socket
int num_tickets_out = 0;				// Number of tickets outstanding

char *do_hello();
char *do_goodbye();
void narrate(char *,char *,struct sockaddr_in *);
void free_all_tickets();
static char * do_validate(char *);

/*******************************************************************
 * setup() - initialize license server
 */
int setup() {
	sd = make_dgram_server_socket(SERVER_PORTNUM);

	if(sd == -1)
		oops("make socket");
	free_all_tickets();

	return sd;
}

/*******************************************************************
 * free_all_tickets
 */
void free_all_tickets() {
	int i;

	for(i = 0; i < MAXUSERS; ++i)
		ticket_array[i] = TICKET_AVAIL;
}

/*******************************************************************
 * shut_down() - close down license server
 */
void shut_down() {
	close(sd);
}

/*******************************************************************
 * handle_request(request,clientaddr,addrlen)
 * branch on code in request
 */
void handle_request(char *req,struct sockaddr_in *client,socklen_t addlen) {
	char *response;
	int ret;

	// act and compose a response
	if(strncmp(req,"HELLO",4) == 0)
		response = do_hello(req);
	else if(strncmp(req,"GBYE",4) == 0)
		response = do_goodbye(req);
	else if(strncmp(req,"VALD",4) == 0)
		response = do_validate(req);
	else 
		response = "FAIL invalid request";
	
	// send the response to the client
	narrate("SAID",response,client);
	ret = sendto(sd,response,strlen(response),0,
		(struct sockaddr *)client,addlen);
	if(ret == -1)
		perror("SERVER sendto failed");
} // handle_request

/*******************************************************************
 * do_hello
 * Give out a ticket if any are available
 * IN msg_p	message received from client
 * Result: ptr to response
 * Note: return is in stiatic buffer over written by each call
 */
char * do_hello(char *msg_p) {
	int x;
	static char replybuf[MSGLEN];

	if(num_tickets_out >= MAXUSERS)
		return("FAIL no tickets available");
	
	// else find a free ticket and give it to client
	for(x = 0; x < MAXUSERS && ticket_array[x] != TICKET_AVAIL; ++x);

	// A sanity check - should nenever happen
	if(x == MAXUSERS) {
		narrate("database corrupt","",NULL);
		
		return("FAIL database corrupt");
	}

	/* Found a free ticket. Record "name" of user (pid) in array.
	 * generate ticket of form: pid.slot
	 */
	ticket_array[x] = atoi(msg_p + 5);		// get pid in msg
	sprintf(replybuf,"TICK %d.%d", ticket_array[x],x);
	num_tickets_out++;

	return replybuf;
} // do_hello

/*******************************************************************
 * do_goodbye
 * Tack back ticket client is returning
 * IN msg_p	message received from client
 * Result: ptr to response
 * Note: retur is in static buffer over written by each call
 */
char * do_goodbye(char *msg_p) {
	int pid,slot;				// componse of ticket

	/* The user's giving us back a ticket. First we need to get
	 * the ticket out of the message, which look like:
	 * GBYE pid.slot
	 */
	if((sscanf((msg_p + 5),"%d.%d",&pid,&slot) != 2) 
		|| (ticket_array[slot] != pid)) {
		narrate("Bogus ticket",msg_p + 5,NULL);

		return "FAIL invalid ticket";
	}

	// The tickets si valid. Release it
	ticket_array[slot] = TICKET_AVAIL;
	--num_tickets_out;

	// return response
	return "THNX See ya!";
} // do_goodbye

/*******************************************************************
 * narrate() - chatty news for debugging and logging purpose
 */
void narrate(char *msg1,char *msg2,struct sockaddr_in *clientp) {
	fprintf(stderr,"\t\tSERVER: %s %s ",msg1,msg2);
	if(clientp)
		fprintf(stderr,"%s:%d",inet_ntoa(clientp -> sin_addr),
			ntohs(clientp -> sin_port));
	putc('\n',stderr);
}// narrate

/*******************************************************************
 * ticket_reclaim
 * go through all tickets and reclaim ones belonging to dead processes
 * Result: none
 */
void ticket_reclaim() {
	int i;

	char tick[BUFSIZ];

	for(i = 0; i < MAXUSERS; ++i) {
		if(ticket_array[i] != TICKET_AVAIL 
			&& (kill(ticket_array[i],0) == -1)
			&& errno == ESRCH) {
			// Proccess is gone - free up slot
			sprintf(tick,"%d.%d",ticket_array[i],i);
			narrate("freeing",tick,NULL);
			ticket_array[i] = TICKET_AVAIL;
			--num_tickets_out;
		}
	}
	alarm(RECLAIM_INTERVAL);			// reset alarm clock
} // ticket_reclaim

/*******************************************************************
 * do_validate
 * Validate client's ticket
 * IN msg_p message from client
 * Result: ptr to response
 * NOTE: return is in staitc buffer over written by each call
 */
static char *do_validate(char *msg) {
	int pid,slot;					// components of ticket

	// message looks like VALD pid.slot
	if(sscanf(msg + 5,"%d.%d",&pid,&slot) == 2 
		&& ticket_array[slot] == pid)
		return "GOOD Valid ticket";
	
	// bad ticket
	narrate("Bogus ticket",msg + 5,NULL);

	return "FAIL invalid ticket";
}