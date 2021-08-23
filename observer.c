/* observer.c - code for observer. Do not rename this file */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char **argv)
{

	struct hostent *ptrh;	/* pointer to a host table entry */
	struct protoent *ptrp;	/* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd;					/* socket descriptor */
	int port;				/* protocol port number */
	char *host;				/* pointer to host name */
	char recvbuf[1000];		/* buffer for data from the server */
	char userName[1000];
	char fullServer[1]; /* Used to determine if the server has room for another participant */
	uint8_t userNameLen;
	//uint8_t timeoutFlag; /* Used to check if we've timed out or not */
	uint16_t msgLen;
	struct timeval tv;

	fd_set fds;

	memset((char *)&sad, 0, sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET;			  /* set family to Internet */

	/* Ensure proper usage */
	if (argc != 3)
	{
		fprintf(stderr, "Error: Wrong number of arguments\n");
		fprintf(stderr, "usage:\n");
		fprintf(stderr, "./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	/* Obtain port number from argv */
	port = atoi(argv[2]);
	if (port > 0)
		sad.sin_port = htons((u_short)port);
	else
	{
		fprintf(stderr, "Error: bad port number %s\n", argv[2]);
	}

	host = argv[1];

	/* Convert hostname to equivalent IP and copy to sad */
	ptrh = gethostbyname(host);
	if (ptrh == NULL)
	{
		fprintf(stderr, "Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if (((long int)(ptrp = getprotobyname("tcp"))) == 0)
	{
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0)
	{
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	printf("Socket: %d\n", sd);

	/* Add sd to our list */
	FD_ZERO(&fds);
	FD_SET(sd, &fds);

	/* Connect the socket to the specified server. */
	if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0)
	{
		fprintf(stderr, "connect failed\n");
		exit(EXIT_FAILURE);
	}

	printf("connection established\n");

	/* Clear contents of fullServer and recvbuf before writing to it */
	memset(recvbuf, '\0', 1000);
	memset(fullServer, '\0', 1);
	memset(userName, '\0', 1000);

	/* Obtain whether the server has room for us (Y) or not (N) */
	if (recv(sd, &fullServer, sizeof(char), 0) < 0)
	{
		perror("Error receiving servers capacity\n");
		exit(1);
	}

	/* Determine if server chat room is full or not */
	if (fullServer[0] == 'N')
	{
		printf("Server is full\n");
		close(sd);
		exit(EXIT_SUCCESS);
	}
	else
	{
		printf("Joining chat room...\n");
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	/* Prompt observer for a participant's username. Keep prompting if length is greater than 10 */
	//TODO TIMEOUT OPERATIONS 3.1-3.4
	userNameLen = 11;
	while (userNameLen > 10)
	{
		FD_ZERO(&fds);
		FD_SET(sd, &fds);

		/* If we timeout */
		if (select(sd + 1, &fds, NULL, NULL, &tv))
		{
			char buf[100];
			if (!recv(sd, &buf, sizeof(buf), 0))
			{
				close(sd);
				printf("Time to enter a username has expired.\n");
				exit(EXIT_SUCCESS);
			}
		}
		printf("Enter a participant's username: ");
		scanf("%s", userName);
		userNameLen = strlen(userName);
	}

	/* Send both the usernamelen and username to server */
	send(sd, &userNameLen, sizeof(uint8_t), 0);
	send(sd, &userName, (int)userNameLen * sizeof(char), 0);

	/* Receive the response from the server. Recycle fullServer buffer for server response */
	memset(fullServer, '\0', 1);
	if (recv(sd, &fullServer, sizeof(char), 0) < 0)
	{
		perror("Error receiving response from server after sending username\n");
		exit(1);
	}

	/* If the username given to the server is already linked to an observer, we must give another username */
	while (fullServer[0] == 'T')
	{
		FD_ZERO(&fds);
		FD_SET(sd, &fds);

		/* If we timeout, close sd, print message, and exit */
		if (select(sd + 1, &fds, NULL, NULL, &tv))
		{
			char buf[100];
			if (!recv(sd, &buf, sizeof(buf), 0))
			{
				close(sd);
				printf("Time to enter a username has expired.\n");
				exit(EXIT_SUCCESS);
			}
		}

		userNameLen = 11;
		while (userNameLen > 10)
		{
			printf("That participant already has an observer. Enter a different participant's username: ");
			scanf("%s", userName);
			userNameLen = strlen(userName);
		}

		/* Send new username */
		send(sd, &userNameLen, sizeof(uint8_t), 0);
		send(sd, &userName, (int)userNameLen * sizeof(char), 0);

		/* get response from server */
		if (recv(sd, &fullServer, sizeof(char), 0) < 0)
		{
			perror("Error receiving response from server after sending username\n");
			exit(1);
		}
	}

	/* If the username given to the server is not an active participant, server terminates our connection */
	if (fullServer[0] == 'N')
	{
		printf("There is no active participant with that username.\n");
		exit(EXIT_SUCCESS);
	}

	/* The server sends a 'Y' indicating that the username is associated with an active user w/o an observer */
	else
	{
		while (1)
		{
			msgLen = 0;
			/* Get message len from server */
			if (recv(sd, &msgLen, sizeof(uint8_t), 0) < 0)
			{
				perror("Error getting msg length\n");
				exit(1);
			}
			/* Clear contents of recvbuf before writing to it */
			memset(recvbuf, '\0', 1000);

			if (recv(sd, &recvbuf, msgLen, 0) < 0)
			{
				perror("Error receiving message from server\n");
				exit(1);
			}

			/* If the message from server doesn't end with newline, append it */
			if (recvbuf[strlen(recvbuf)] != '\n' && recvbuf[strlen(recvbuf) - 1] != '\n' && recvbuf[strlen(recvbuf) - 2] != '\n')
			{
				recvbuf[(int)msgLen] = '\n';
			}

			printf("%s", recvbuf);
		}
	}

	/* Close our socket after the game */
	close(sd);
	exit(EXIT_SUCCESS);
}
