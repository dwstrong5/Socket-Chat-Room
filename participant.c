/* participant.c - code for participant. Do not rename this file */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* Simple function to loop through the characters in a string and check if each character is either a letter
   a number or an underscore. Returns 0 if all chars are valid, 1 if false. */
int checkUserName(char *userName)
{

	int userNameLen;
	int invalidName = 0; /* Boolean to determine if userName contains only valid chars (0) or contains an invalid char (1) */
	int i;

	userNameLen = strlen(userName);

	for (i = 0; i < userNameLen; i++)
	{
		if (!isalpha(userName[i]) && !isdigit(userName[i]) && userName[i] != '_')
		{
			invalidName = 1;
		}
	}

	return invalidName;
}

int main(int argc, char **argv)
{

	struct hostent *ptrh;	/* pointer to a host table entry */
	struct protoent *ptrp;	/* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd;					/* socket descriptor */
	int port;				/* protocol port number */
	int i;					/* Iterator */
	char *host;				/* pointer to host name */
	char sendbuf[1000];		/* buffer for messages sent to the server */
	int MAX = 10000;
	int max_sd, retval; /* Result for calls to recv() to check for errors */
	char recvbuf[1000]; /* buffer for data from the server */
	char userName[1000];
	char fullServer[1]; /* Used to determine if the server has room for another participant */
	uint8_t msgLen, userNameLen;
	int flag; /* Used to check for non-whitespace char */
	fd_set readfds, wrk_readfds;

	struct timeval tv;

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
	if (!strncmp("N", fullServer, 1))
	{
		printf("Server is full...\n");
		close(sd);
		exit(EXIT_SUCCESS);
	}
	else
	{
		printf("Joining chat room...\n");
	}

	/* Begin negotiating a Username */
	printf("Choose a username: ");
	scanf("%s", userName);

	/* First check for username validity before initial send to server */
	userNameLen = strlen(userName);
	while (userNameLen > 10 || checkUserName(userName))
	{
		printf("Choose a username (upto 10 characters long; allowed characters are alphabets, digits, and underscores): ");
		scanf("%s", userName);
		userNameLen = strlen(userName);
	}

	/* Initial send of userNameLen to server, followed by userName */
      
	send(sd, &userNameLen, sizeof(uint8_t), 0);
	send(sd, &userName, strlen(userName), 0);
      
	/* Receive response from server whether username was taken or not. Recycle fullServer and store response there */
	memset(fullServer, '\0', 1);
	if (recv(sd, &fullServer, sizeof(char), 0) < 0)
	{
		perror("Error receiving response regarding username availabilty from server\n");
		exit(1);
	}
      
	/* While loop to handle the negotiation with the server. Loop breaks once Server accepts username */
	while (fullServer[0] != 'Y')
	{

		/* If server replies that username is taken ('T'), resets timer to 60 seconds */
		if (fullServer[0] == 'T')
		{
			printf("Username is already taken. Choose a differenct username: ");
			scanf("%s", userName);

			/* Check validity as we did before */
			userNameLen = strlen(userName);
			while (userNameLen > 10 || checkUserName(userName))
			{
				printf("Choose a username (upto 10 characters long; allowed characters are alphabets, digits, and underscores): ");
				scanf("%s", userName);
				userNameLen = strlen(userName);
			}

			/* Send len and username to server for validation */
			send(sd, &userNameLen, sizeof(uint8_t), 0);
			send(sd, &userName, strlen(userName), 0);
		}

		/* If server replies that the username is invalie ('I'), DOES NOT reset timer */
		else if (fullServer[0] == 'I')
		{
			printf("Username is invalid. Choose a valid username: ");
			scanf("%s", userName);

			/* Check validity as we did before */
			userNameLen = strlen(userName);
			while (userNameLen > 10 || checkUserName(userName))
			{
				printf("Choose a username upto 10 characters long; allowed characters are alphabets, digits, and underscores): ");
				scanf("%s", userName);
				userNameLen = strlen(userName);
			}

			/* Send len and username to server for validation */
			send(sd, &userNameLen, sizeof(uint8_t), 0);
			send(sd, &userName, strlen(userName), 0);
		}

		/* Edge case where we receive something unintended from server */
		else
		{
			perror("Received an invalid response from server regarding username...\n");
			exit(1);
		}

		/* Receive response from server whether username was taken/valid or not */
		memset(fullServer, '\0', 1);
		if (recv(sd, &fullServer, sizeof(char), 0) < 0)
		{
			perror("Error receiving response regarding username availability from server\n");
			exit(1);
		}
	}

	/* The Server has accepted our username... */
	printf("Username accepted.\n");

	max_sd = sd;

	/* Infinite loop for chat room functionality (SECTION 4.1) */
	while (1)
	{

		char buf[MAX];

		FD_ZERO(&readfds);
		FD_SET(sd, &readfds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/* Check if socket sd is still open */
		retval = select(max_sd + 1, &wrk_readfds, NULL, NULL, &tv);

		/* If socket is closed on server end */
		if (retval)
		{

			/* Check to make sure recv returns 0 */
			if (!recv(sd, &recvbuf, sizeof(recvbuf), 0))
			{
				close(sd);
				exit(EXIT_SUCCESS);
			}
			/* Recv returns > 0 */
			else
			{
				perror("Server closed socket but we continued to read data...\n");
				exit(1);
			}
		}

		/* sd isn't ready to be read which means socket is still open */
		else
		{

			flag = 0;

			while (!flag)
			{
				/* Prompt user for message */
				printf("Enter message: ");
				fgets(buf, MAX, stdin);

				/* Send msg length */
				msgLen = strlen(buf);

				for (i = 0; i < msgLen; i++)
				{
					if (!isspace(buf[i]))
					{
						flag = 1;
					}
				}
			}

			send(sd, &msgLen, sizeof(uint8_t), 0);

			memset(sendbuf, '\0', 1000);

			/* Copy message to sendbuf to be sent to server */
			//TODO: ADD SUPPORT FOR FRAGMENTING MESSAGES >1000 CHARACTERS
			strncpy(sendbuf, buf, (size_t)msgLen);

			send(sd, &sendbuf, strlen(sendbuf), 0);
		}
	}

	/* Close our socket after the game */
	close(sd);
	exit(EXIT_SUCCESS);
}
