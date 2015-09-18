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
#define REQUEST_PERIOD 2000000 //usec
#define RESPONSE_TIMEOUT 100000 //usec

#define MAX_MSG 100

#define RESPONSE 0
#define REQUEST 1
#define SUCC_FIRST_DIED_REQUEST 11
#define SUCC_SECOND_DIED_REQUEST 12
#define SUCC_FIRST_DIED_RESPONSE 21
#define SUCC_SECOND_DIED_RESPONSE 22

#define NORMAL 0
#define WATING_FOR_REQUEST 1

#define LOST_PACKET_THRESHOLD 4

#define TRUE 1
#define FALSE 0

struct peermsg {
	char msgtype;
	char id;
	char successor_first;
	char successor_second;
	int  seqnum;
};

typedef struct peermsg *peermsg;

char myid;		// the id of the active peer
char successor_first;	// the active peers current first successor
char successor_second;	// the active peers current second successor
char status;	// the active peers current message status
				// 0->normal, 1->waiting for response
				// 2->waiting for first successor update
				// 3->waiting for second successor update
int seqcount1;
int seqresp1;
int seqcount2;
int seqresp2;

struct timeval tv;

void *send_dead_first_sucessor_request () {

	printf("pinging successor two\n");

	int sd;
	struct sockaddr_in sendaddr;

	peermsg reqmsg = malloc(sizeof(struct peermsg));

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	bzero(&sendaddr,sizeof(sendaddr));
	sendaddr.sin_family = AF_INET;
	sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	sendaddr.sin_port=htons(SERVER_PORT+successor_second);

	reqmsg->msgtype = SUCC_FIRST_DIED_REQUEST;

	sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

	close(sd);

	free(reqmsg);

	return NULL;
}

void *send_dead_second_sucessor_request () {

	printf("pinging successor one\n");

	int sd;
	struct sockaddr_in sendaddr;

	peermsg reqmsg = malloc(sizeof(struct peermsg));

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	bzero(&sendaddr,sizeof(sendaddr));
	sendaddr.sin_family = AF_INET;
	sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	sendaddr.sin_port=htons(SERVER_PORT+successor_first);

	reqmsg->msgtype = SUCC_SECOND_DIED_REQUEST;

	sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

	close(sd);

	free(reqmsg);

	return NULL;
}

void *successor_first_ping (void *argv)
{

	peermsg reqmsg = malloc(sizeof(struct peermsg));


	int sd;
	struct sockaddr_in sendaddr;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	while (TRUE) {

		reqmsg->msgtype = REQUEST;
		reqmsg->id = myid;
		reqmsg->successor_first = successor_first;
		reqmsg->successor_second = successor_second;
		reqmsg->seqnum = ++seqcount1;

		bzero(&sendaddr,sizeof(sendaddr));
		sendaddr.sin_family = AF_INET;
		sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		sendaddr.sin_port=htons(SERVER_PORT+successor_first);

		//printf("Sending request %d %d to port %d\n",reqmsg->msgtype, reqmsg->id, SERVER_PORT+successor_first);
		printf("Sending request to port %d\n", SERVER_PORT+successor_first);

		sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));
		status = WATING_FOR_REQUEST;

		// if we have sent a certain amount of packets more than we have received
		if (seqcount1 - LOST_PACKET_THRESHOLD > seqresp1) {

			pthread_t pthD;	
			pthread_create(&pthD, NULL, send_dead_first_sucessor_request, NULL);
		}
		
		usleep(REQUEST_PERIOD);
	}

	return NULL;
}

void *successor_second_ping (void *argv)
{

	peermsg reqmsg = malloc(sizeof(struct peermsg));


	int sd;
	struct sockaddr_in sendaddr;

	sd=socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0) {
		perror("cannot open socket ");
		exit(1);
	}

	while (TRUE) {

		reqmsg->msgtype = REQUEST;
		reqmsg->id = myid;
		reqmsg->successor_first = successor_first;
		reqmsg->successor_second = successor_second;

		bzero(&sendaddr,sizeof(sendaddr));
		sendaddr.sin_family = AF_INET;
		sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		sendaddr.sin_port=htons(SERVER_PORT+successor_second);

		//printf("Sending request %d %d to port %d\n",reqmsg->msgtype, reqmsg->id, SERVER_PORT+successor_first);
		printf("Sending request to port %d\n", SERVER_PORT+successor_second);

		sendto(sd,reqmsg,sizeof(struct peermsg),0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));
		status = WATING_FOR_REQUEST;

		if (seqcount1 - LOST_PACKET_THRESHOLD > seqresp1) {

			pthread_t pthD;	
			pthread_create(&pthD, NULL, send_dead_second_sucessor_request, NULL);
		}

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

	respmsg->msgtype = 0;
	respmsg->id = myid;


	while (TRUE) {

		bzero(&myaddr,sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		myaddr.sin_port=htons(SERVER_PORT+myid);
		bind(sd,(struct sockaddr *)&myaddr,sizeof(myaddr));

		socklen_t len = sizeof(recvaddr);

		recvfrom(sd,recvline,MAX_MSG,0,(struct sockaddr *)&recvaddr,&len);
		//printf("A message was received: %d %d\n", ((peermsg)recvline)->msgtype, ((peermsg)recvline)->id);
		if (((peermsg)recvline)->msgtype == RESPONSE) { 
			printf("A ping response message was received from Peer %d\n",((peermsg)recvline)->id);
			status = 0;
			seqresp1 = ((peermsg)recvline)->seqnum;
		} else if (((peermsg)recvline)->msgtype == REQUEST) {
			printf("A ping request message was received from Peer %d\n",((peermsg)recvline)->id);

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->id);

			respmsg->successor_first = successor_first;
			respmsg->successor_second = successor_second;

			printf("Sending response %d to port %d\n",respmsg->msgtype,SERVER_PORT+((peermsg)recvline)->id);

			sendto(sd,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		} else if (((peermsg)recvline)->msgtype == SUCC_FIRST_DIED_REQUEST) {

			printf("successor_first died Request received from %d\n",((peermsg)recvline)->id);

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->id);

			respmsg->msgtype = 21;
			respmsg->successor_first = successor_first;
			respmsg->successor_second = successor_second;

			printf("Sending response %d to port %d\n",respmsg->msgtype,SERVER_PORT+((peermsg)recvline)->id);

			sendto(sd,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		} else if (((peermsg)recvline)->msgtype == SUCC_SECOND_DIED_REQUEST) {

			printf("successor_second died Request received from %d\n",((peermsg)recvline)->id);

			bzero(&sendaddr,sizeof(sendaddr));
			sendaddr.sin_family = AF_INET;
			sendaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
			sendaddr.sin_port=htons(SERVER_PORT+((peermsg)recvline)->id);

			respmsg->msgtype = 22;
			respmsg->successor_first = successor_first;
			respmsg->successor_second = successor_second;

			printf("Sending response %d to port %d\n",respmsg->msgtype,SERVER_PORT+((peermsg)recvline)->id);

			sendto(sd,respmsg,4,0,(struct sockaddr *)&sendaddr,sizeof(sendaddr));

		} else if (((peermsg)recvline)->msgtype == SUCC_FIRST_DIED_RESPONSE) {

			printf("successor_first died Response received from %d\n",((peermsg)recvline)->id);
			successor_first = successor_second;
			successor_second = ((peermsg)recvline)->successor_first;

		} else if (((peermsg)recvline)->msgtype == SUCC_SECOND_DIED_RESPONSE) {

			printf("successor_second died Response received from %d\n",((peermsg)recvline)->id);
			successor_second = ((peermsg)recvline)->successor_first;

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
	successor_first = atoi(argv[2]);
	successor_second = atoi(argv[3]);
	seqcount1 = 256;
	seqresp1 = 256;
	seqcount2 = 65536;
	seqresp2 = 65536;

	status = 0;

	// set up socket timeout delay;
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT_PERIOD;
	
	pthread_t pthSF;
	pthread_t pthSS;
	pthread_t pthR;	
	pthread_create(&pthSF, NULL, successor_first_ping, NULL);
	usleep(1000);
	pthread_create(&pthSS, NULL, successor_second_ping, NULL);
	usleep(1000);
	pthread_create(&pthR, NULL, receiver, NULL);

	while(TRUE);
}