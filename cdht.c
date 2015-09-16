/* fpont 12/99 */
/* pont.net		*/
/* TCPClient.c */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close */
#include <stdlib.h>
#include <string.h>


#include <pthread.h>
#include <sys/time.h>

#define SERVER_PORT 50000
#define TIMEOUT_PERIOD 200000 //usec
#define REQUEST_PERIOD 5000000//usec

#define MAX_MSG 100

/*
 * peermsg is the structure of all messages passed between peers
 * id for each message: 1 == ping request, 0 == ping response
 */

struct peermsg {
	int msgtype;
	int id;
};

typedef struct peermsg *peermsg;

int myid;
int child1;
int child2;

struct timeval tv;

void *sender (void *argv)
{

	peermsg reqmsg = malloc(sizeof(struct peermsg));
	reqmsg->msgtype = 1;
	reqmsg->id = myid;

	int sd;
	struct sockaddr_in sendaddr;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	while (1) {
		printf("child1: %d\n", child1);

		bzero(&sendaddr,sizeof(sendaddr));
		sendaddr.sin_family = AF_INET;
		sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		sendaddr.sin_port=htons(SERVER_PORT+child1);

		printf("Sending request %d %d to port %d\n",reqmsg->msgtype, reqmsg->id, SERVER_PORT+child1);

		sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		usleep(REQUEST_PERIOD);
	}

	return NULL;
}


void *receiver (void *argv)
{
	int sd;
	struct sockaddr_in myaddr,sendaddr,recvaddr;
	int num_timeouts = 0;
	char recvline[MAX_MSG];

	peermsg respmsg = malloc(sizeof(struct peermsg));
	respmsg->msgtype = 0;
	respmsg->id = myid;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	while (1) {
		printf("Waiting for requests\n");

		bzero(&myaddr,sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		myaddr.sin_port=htons(SERVER_PORT+myid);
		bind(sd,(struct sockaddr *)&myaddr,sizeof(myaddr));

		socklen_t len = sizeof(recvaddr);

		// Wait for TIMEOUT_PERIOD for a ping request
		/*
		bzero(&recvaddr,sizeof(recvaddr));
		if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
			perror("Error");
		}
		if(recvfrom(sd,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len) < 0){
			//timeout reached
			printf("timeout%d, no messages received on port %d.\n",num_timeouts++,SERVER_PORT+myid);
		// checks the content of the message to see if it is a ping request or ping response
		} else if (((peermsg)recvline)->msgtype == 1) {
		*/
		recvfrom(sd,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len);
		printf("A message was received: %d %d\n", ((peermsg)recvline)->msgtype, ((peermsg)recvline)->id);
		if (((peermsg)recvline)->msgtype == 0) { 
			printf("A ping response message was received from Peer %d\n",((peermsg)recvline)->id);
		} else if (((peermsg)recvline)->msgtype == 1) {
			printf("A ping request message was received from Peer %d\n",((peermsg)recvline)->id);
			//printf("received %s on port %d\n", recvline, recvaddr.sin_port);

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->id);

			respmsg->msgtype = 0;
			respmsg->id = myid;

			printf("Sending response %d to port %d\n",respmsg->msgtype,SERVER_PORT+((peermsg)recvline)->id);

			sendto(sd,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));
		}
	}
}

int main (int argc, char *argv[])
{

	if(argc != 4) {
		printf("usage: %s <id> <peer 1> <peer 2>\n",argv[0]);
		exit(1);
	}

	myid = atoi(argv[1]);
	child1 = atoi(argv[2]);
	child2 = atoi(argv[3]);

	// set up socket timeout delay;
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT_PERIOD;
	
	peermsg respmsg = malloc(sizeof(struct peermsg));

	/*
	 * Protocol has two main states;
	 * Wait for ping request; peer will wait for a message to be received on
	 * it's dedicated socket and send ping response
	 * Send ping request; if not ping reuqests are received before TIMEOUT_PERIOD
	 * the peer will initiate it's own ping request to each of it's children
	 */
	 pthread_t pthS;
	 pthread_t pthR;	
	 pthread_create(&pthS, NULL, sender, NULL);
	 pthread_create(&pthR, NULL, receiver, NULL);
	 while(1);
}

/*

	while (1) {

		printf("Waiting for requests\n");
		// Set up incoming essahe
		bzero(&myaddr,sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		myaddr.sin_port=htons(SERVER_PORT+myid);
		bind(sd1,(struct sockaddr *)&myaddr,sizeof(myaddr));

		socklen_t len = sizeof(recvaddr);

		// Wait for TIMEOUT_PERIOD for a ping request
		bzero(&recvaddr,sizeof(recvaddr));
		if (setsockopt(sd1, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
			perror("Error");
		}
		if(recvfrom(sd1,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len) < 0){
			//timeout reached
			printf("timeout%d, no messages received on port %d.\n",num_timeouts++,SERVER_PORT+myid);
		// checks the content of the message to see if it is a ping request or ping response
		} else if (((peermsg)recvline)->msgtype == 1) {
			printf("A ping request message was received from Peer %d\n",((peermsg)recvline)->id);
			//printf("received %s on port %d\n", recvline, recvaddr.sin_port);
			

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->id);

			respmsg->msgtype = 0;
			respmsg->id = myid;

			printf("Sending response %d to port %d\n",respmsg->msgtype,SERVER_PORT+reqmsg->id);

			sendto(sd2,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));
		}
		//sleep(1);
		

		bzero(&sendaddr,sizeof(sendaddr));
		sendaddr.sin_family = AF_INET;
		sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		sendaddr.sin_port=htons(SERVER_PORT+child1);

		//char msg[255] = "test";
		reqmsg->msgtype = 1;
		reqmsg->id = myid;

		printf("Sending request %d to port to %d\n",reqmsg->msgtype, SERVER_PORT+child1);

		sendto(sd2,reqmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		// Wait for response
		bzero(&recvaddr,sizeof(recvaddr));
		if (setsockopt(sd1, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
			perror("Error");
		}
		if(recvfrom(sd1,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len) < 0){
			//timeout reached
			printf("timeout%d, no response received on port %d.\n",num_timeouts++,SERVER_PORT+myid);
		// checks the content of the message to see if it is a ping request or ping response
		} else if (((peermsg)recvline)->msgtype == 0) {
			printf("A ping response message was received from Peer %d\n",((peermsg)recvline)->id);
		} else {
			printf("error: unrecognised message %d\n",((peermsg)recvline)->msgtype);
		}
		//sleep(1);
	}

	return 0;
	
}
*/

