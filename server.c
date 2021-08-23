/* server.c - code for server. Do not rename this file */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h> /* Added for the nonblocking socket */
#include "linked_list.h"
#include <ctype.h>
#include <wctype.h>

#define QLEN 6 /* size of request queue */
#define MAX(a, b) ((a > b) ? a : b)

/* Simple function to loop through the characters in a string and check if each character is either a letter
   a number or an underscore. Returns 0 if all chars are valid, 1 if false. */
int checkUsername(char *userName)
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

/* Function to determine if a string contains at least 1 whitespace char
    To be used when a clients message contains an @ symbol
*/
int whiteSpaceCheck(char *message)
{
	int flag = 0;
	int i;
	int len = strlen(message);

	for (i = 0; i < len; i++)
	{
		if (iswspace(message[i]))
		{
			flag = 1;
		}
	}
	return flag;
}

// Public flag: Set to 1 if public message, 0 if private
// Mutates param #1, string newMessage which is the prepended message (caller saved variable)
void prepend(char *newMessage, char *message, char *username, int public)
{
	memset(newMessage, '\0', strlen(newMessage));
	if (public)
	{
		strcpy(newMessage, ">");
	}
	else
	{
		strcpy(newMessage, "\%"); //make sure this works
	}
       
	for (int i = 0; i < 11 - strlen(username); i++)
	{
		strcat(newMessage, " ");
	}
	strcat(newMessage, username);
	strcat(newMessage, ":");
	strcat(newMessage, " ");
	strcat(newMessage, message);
}

// Returns node with matching username or NULL if not found
struct node *findPtcpMatch(char *username, struct node *ptcp_head)
{
	if (!isEmpty(ptcp_head))
	{
		struct node *current = ptcp_head;
		int isNext = 1;
		while (isNext)
		{
			if (current->isSet)
			{
				if (strcmp(current->username, username) == 0)
				{
					return current;
				}
			}
			if (current->next == NULL)
			{
				isNext = 0;
			}
			else
			{
				current = current->next;
			}
		}
	}
	return NULL;
}

// takes in a socket descriptoer (sd) and returns a pointer to the node that contains the participant/observer that
//matches sd
struct node *findNode(int sd, struct node *ptcp_head, struct node *obsv_head)
{
	if (!isEmpty(ptcp_head))
	{
		struct node *current = ptcp_head;
		int isNext = 1;
		while (isNext)
		{
			if (current->ptcp_fd == sd)
			{
				return current;
			}
			if (current->next == NULL)
			{
				isNext = 0;
			}
			else
			{
				current = current->next;
			}
		}
	}

	if (!isEmpty(obsv_head))
	{
		//navigate through list
		struct node *current = obsv_head;
		int isNext = 1;
		while (isNext)
		{
			if (current->obsv_fd == sd)
			{
				return current;
			}
			if (current->next == NULL)
			{
				isNext = 0;
			}
			else
			{
				current = current->next;
			}
		}
	}
	return NULL;
}

/* Function to check if a node has timed out or not */
void timeoutCheck(struct node *current, fd_set readfds)
{
	if (!(current->isPaired))
	{
		time_t end = time(NULL);
		int time_taken = (end - (current->start));
	  
		if (time_taken >= 60)
		{
		       
			close(current->ptcp_fd);
			//current->ptcp_fd = -1;
			FD_CLR(current->ptcp_fd, &readfds);
		}
	}
}

/* Function that takes in a message and broadcasts to all active observers in a list */
void broadcast(char *message, struct node *head)
{
	printList(head);
	printf("\n");

	struct node *current = head;
	if (head == NULL)
	{
		printf("List is empty.\n");
		return;
	}

	while (current != NULL)
	{
		if (current->isPaired)
		{
			uint16_t len = htons(strlen(message));
		     
			send(current->obsv_fd, &len, sizeof(uint16_t), 0);
			send(current->obsv_fd, message, strlen(message), 0);
		}
		current = current->next;
	}
}

/* Frees up memory used to hold list of active nodes */
void freeList(struct node *head)
{
	struct node *current = head;
	if (head == NULL)
	{
		printf("List is empty.\n");
		return;
	}
	struct node *temp = NULL;
	while (current != NULL)
	{
		temp = current->next;
		free(current);
		current = temp;
	}
}

/* Main driver code */
int main(int argc, char **argv)
{
	struct protoent *ptrp;		   /* pointer to a protocol table entry */
	struct sockaddr_in sad;		   /* structure to hold server's participant address */
	struct sockaddr_in sad2;	   /* structure to hold server's observer address */
	struct sockaddr_in p_cad;	   /* structure to hold participant's address */
	struct sockaddr_in o_cad;	   /* structure to hold observer's address */
	int sd, sd2, ptcp_sd, obsv_sd; /* socket descriptors */
	uint16_t ptcp_port, obsv_port; /* protocol port number */
	socklen_t alen = 0;			   /* length of address */
	int optval = 1;				   /* boolean value when we set socket option */
	int capacity = 255; // maximum observer and participant capacity
	int ptcp_count = 0; // counter for number of active participants
	int obsv_count = 0; // counter for number of active observers
	fd_set readfds, writefds;
	fd_set wrk_readfds;
	char sendbuf[1024]; // buffer used to send data to observers
	int pListSize = 0; // size of list of active participants
	int oListSize = 0; // size of list of active observers
	struct node *ptcp_head = NULL;
	struct node *obsv_head = NULL;
	char y = 'Y';
	char n = 'N';
	int smax;
	/* Check to ensure proper usage */
	if (argc != 3)
	{
		fprintf(stderr, "Error: Wrong number of arguments\n");
		fprintf(stderr, "usage:\n");
		fprintf(stderr, "./server participant_port observer_port\n");
		exit(EXIT_FAILURE);
	}
	memset((char *)&sad, 0, sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET;			  /* set family to Internet */
	sad.sin_addr.s_addr = INADDR_ANY;	  /* set the local IP address */

	memset((char *)&sad2, 0, sizeof(sad2)); /* clear sockaddr structure */
	sad2.sin_family = AF_INET;				/* set family to Internet */
	sad2.sin_addr.s_addr = INADDR_ANY;		/* set the local IP address */

	/* Check for proper participant port number in argv[1] */
	ptcp_port = atoi(argv[1]); /* convert argument to binary */
	if (ptcp_port > 0)
	{ /* test for illegal value */
		sad.sin_port = htons((u_short)ptcp_port);
	}
	else
	{ /* print error message and exit */
		fprintf(stderr, "Error: Bad port number %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	/* Check for proper observer port number in argv[2] */
	obsv_port = atoi(argv[2]); /* convert argument to binary */
	if (obsv_port > 0)
	{ /* test for illegal value */
		sad2.sin_port = htons((u_short)obsv_port);
	}
	else
	{ /* print error message and exit */
		fprintf(stderr, "Error: Bad port number %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if (((long int)(ptrp = getprotobyname("tcp"))) == 0)
	{
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create ptcp listening socket */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0)
	{
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Create obsv listening socket */
	sd2 = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd2 < 0)
	{
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow server to specify timeout for the socket */
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
	{
		fprintf(stderr, "setsockopt failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow server to specify timeout for the socket */
	if (setsockopt(sd2, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
	{
		fprintf(stderr, "setsockopt failed\n");
		exit(EXIT_FAILURE);
	}

	fcntl(sd, F_SETFL, O_NONBLOCK);
	fcntl(sd2, F_SETFL, O_NONBLOCK);

	/* Bind a local address to the socket */
	if (bind(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0)
	{
		fprintf(stderr, "Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(sd2, (struct sockaddr *)&sad2, sizeof(sad2)) < 0)
	{
		fprintf(stderr, "Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	if (listen(sd, QLEN) < 0)
	{
		fprintf(stderr, "Error: Listen on sd failed\n");
		exit(EXIT_FAILURE);
	}

	if (listen(sd2, QLEN) < 0)
	{
		fprintf(stderr, "Error: Listen on sd failed\n");
		exit(EXIT_FAILURE);
	}

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_SET(sd, &readfds);  //put participant socket in readfds set
	FD_SET(sd2, &readfds); //put observer socket in readfds set
	int retval;

	// Main while loop for chat room functionality 
	while (1)
	{

		FD_ZERO(&wrk_readfds);
		smax = 0;
		for (int i = 0; i < FD_SETSIZE; ++i)
		{
			if (FD_ISSET(i, &readfds))
			{
				smax = i;
				FD_SET(i, &wrk_readfds);
			}
		}

		retval = select(smax+1, &wrk_readfds, NULL, NULL, NULL);

		/* If select returns a positive sd */
		if (retval > 0)
		{

		  /* If a participant exits early, close the matching observer */
			struct node *p = findNode(sd, ptcp_head, obsv_head);
			u_int16_t disconnectCheck = 1;
			int recvRet = recv(sd, &disconnectCheck, sizeof(u_int16_t), 0);
			if (!recvRet && p->isSet)
			{
				if (p->obsv_fd)
				{
					deleteByObsv(obsv_head, p->obsv_fd);
					close(p->obsv_fd);
					FD_CLR(p->obsv_fd, &readfds);
					p->obsv_fd = -1;
				}
				close(sd);
				FD_CLR(sd, &readfds);
				sd = -1;
				continue;
			}
			
			else if (!recvRet)
			{
				close(sd);
				FD_CLR(sd, &readfds);
				sd = -1;
				continue;
			}

			// New participant requesting connection
			if (FD_ISSET(sd, &wrk_readfds))
			{
				printf("Participant requesting connection\n");
				if ((ptcp_sd = accept(sd, (struct sockaddr *)&p_cad, &alen)) < 0)
				{
					fprintf(stderr, "Error: Accept sd failed\n");
					exit(EXIT_FAILURE);
				}

				ptcp_head = insertFirst(ptcp_head, ptcp_sd, -1, NULL, 0);

				pListSize++;
				fcntl(ptcp_sd, F_SETFL, O_NONBLOCK); //Set ptcp_sd to non-blocking

				/* If we're already at capacity, close socket */
				if (ptcp_count >= capacity)
				{
					send(ptcp_sd, &n, sizeof(char), 0);
					close(ptcp_sd);
					FD_CLR(ptcp_sd, &readfds);
					ptcp_sd = -1;
					continue;
				}
				/* We have room for new connection */
				else
				{
					send(ptcp_sd, &y, sizeof(char), 0);
					FD_SET(ptcp_sd, &readfds);
				}

			     
			}
			/* If we have a new observer trying to connect */
			else if (FD_ISSET(sd2, &wrk_readfds))
			{
				printf("Observer requesting connection\n");
				if ((obsv_sd = accept(sd2, (struct sockaddr *)&o_cad, &alen)) < 0)
				{
					fprintf(stderr, "Error: Accept sd failed\n");
					exit(EXIT_FAILURE);
				}

				/* Add new observer to our list */
				obsv_head = insertFirst(obsv_head, -1, obsv_sd, NULL, 0);
				oListSize++; // increment count
				fcntl(obsv_sd, F_SETFL, O_NONBLOCK); //Set obsv_sd to non-blocking

				/* If we're at capacity... */
				if (obsv_count >= capacity)
				{
					send(obsv_sd, &n, sizeof(char), 0);
					close(obsv_sd);
					FD_CLR(obsv_sd, &readfds);
					obsv_sd = -1;
					continue;
				}
				/* We have room for another observer */
				else
				{
					send(obsv_sd, &y, sizeof(char), 0);
				    
					FD_SET(obsv_sd, &readfds);
				}

			       
			}

			/* A paired participant is sending a message to the server */
			else
			{
				printf("Being contacted by preexisting client\n");
				//Find which active participant (socket descriptor) is attempting to contact server
				for (int i = 0; i < FD_SETSIZE; ++i)
				{
					if (FD_ISSET(i, &wrk_readfds))
					{
						//find the corresponding node for the socket descriptor from our linked list of participant / observers to decide what action to take
						struct node *targetNode = findNode(i, ptcp_head, obsv_head);
						if (targetNode == NULL)
						{
							exit(EXIT_FAILURE);
						}
						if (!(targetNode->isPaired))
						{
							if (targetNode->obsv_fd == -1)
							{ // Unpaired participant contact, meaning username attempt

							  /* Obtain username length */
							  uint8_t username_length;
								int r = recv(targetNode->ptcp_fd, &username_length, sizeof(uint8_t), 0);
								if (r < 0)
								{
									perror("recv error ptcp 1\n");
									exit(EXIT_FAILURE);
								}
								/* Obtain username */
								char username[username_length];
								memset(username, '\0', strlen(username));
								r = recv(targetNode->ptcp_fd, &username, username_length, 0);
								username[username_length] = '\0';
								if (r < 0)
								{
									perror("recv error ptcp 2\n");
									exit(EXIT_FAILURE);
								}

								/* if username is valid and not currently in use */
								if (!checkUsername(username) && !findPtcpMatch(username, ptcp_head))
								{
									targetNode->username = strdup(username);
									targetNode->isSet = 1;
									send(targetNode->ptcp_fd, &y, sizeof(char), 0);
									ptcp_count++;

									/* Send new user has joined message to observers */
									memset(sendbuf, '\0', strlen(sendbuf));
									strcpy(sendbuf, "User ");
									strcat(sendbuf, username);
									strcat(sendbuf, " has joined");
									if (obsv_head)
									{
										broadcast(sendbuf, obsv_head);
									}
								}

								/* If username is already taken */
								else if (findPtcpMatch(username, ptcp_head))
								{
									printf("Username taken\n");
									char t = 'T';
									send(targetNode->ptcp_fd, &t, sizeof(char), 0);
									targetNode->start = time(NULL);
								}

								/* username is invalid */
								else
								{
									printf("Username invalid\n");
									char invalid = 'I';
									send(targetNode->ptcp_fd, &invalid, sizeof(char), 0);
								}
							}

							/* An unpaired observer contacts the server */
							else if (targetNode->ptcp_fd == -1)
							{ // Unpaired observer contact
							  uint8_t username_length;
								int r = recv(targetNode->obsv_fd, &username_length, sizeof(uint8_t), 0);
								if (r < 0)
								{
									perror("recv error obsv 1\n");
									exit(EXIT_FAILURE);
								}
								// obtain username
								char username[username_length];
								memset(username, '\0', strlen(username));
								r = recv(targetNode->obsv_fd, &username, username_length, 0);
								username[username_length] = '\0';
								if (r < 0)
								{
									perror("recv error obsv 2\n");
									exit(EXIT_FAILURE);
								}
								struct node *match = findPtcpMatch(username, ptcp_head);
								if (!checkUsername(username) && match && !match->isPaired)
								{ // Pre-existing non-paired participant has matching username
									printf("Username pair success\n");
									match->isPaired = 1;
									targetNode->isPaired = 1;
									match->obsv_fd = targetNode->obsv_fd;
									targetNode->ptcp_fd = match->ptcp_fd;
									targetNode->username = strdup(username);

									//send Y to observer for valid pairing
									send(targetNode->obsv_fd, &y, sizeof(char), 0);
									obsv_count++;
									memset(sendbuf, '\0', strlen(sendbuf));
									strcpy(sendbuf, "A new observer has joined");
									if (obsv_head)
									{
										broadcast(sendbuf, obsv_head);
									}
								}

								/* If the username is already paired to an observer */
								else if (!checkUsername(username) && match && match->isPaired)
								{
									printf("Username already paired\n");
									char t = 'T';
									send(targetNode->obsv_fd, &t, sizeof(char), 0);
									targetNode->start = time(NULL);
								}

								/* The username is invalid */
								else
								{
									printf("Username invalid\n");
									send(targetNode->obsv_fd, &n, sizeof(char), 0);
									close(targetNode->obsv_fd);
									FD_CLR(targetNode->obsv_fd, &readfds);
									targetNode->obsv_fd = -1;
									deleteByObsv(targetNode, targetNode->obsv_fd);
									continue;
								}
							}
						}

						/* A paired participant is contacting the server */
						else if (targetNode->isPaired)
						{
						  /* Get message length */
							uint8_t message_length;
							int r = recv(targetNode->ptcp_fd, &message_length, sizeof(uint8_t), 0);
							if (r < 0)
							{
								perror("recv error obsv 1\n");
								exit(EXIT_FAILURE);
							}

							/* If the message is too long, close the server */
							if (message_length > 1000)
							{
								close(targetNode->ptcp_fd);
								close(targetNode->obsv_fd);
								FD_CLR(targetNode->ptcp_fd, &readfds);
								FD_CLR(targetNode->obsv_fd, &readfds);
								targetNode->ptcp_fd = -1;
								targetNode->obsv_fd = -1;
							}

							/*obtain message */
							char message[message_length];
							memset(message, '\0', strlen(message));
							r = recv(targetNode->ptcp_fd, &message, message_length, 0);
							message[message_length] = '\0';
							if (r < 0)
							{
								perror("recv error obsv 2\n");
								exit(EXIT_FAILURE);
							}
							/* Check that message has at least 1 space and < 1000 chars */
							if (whiteSpaceCheck(message) && message_length <= 1000)
							{
							  //if we have a private message 
								char username[10];
								memset(username, '\0', sizeof(username));
								if (strcmp(message, ""))
								{
									if (message[0] == '@')
									{ //error handle for space after '@'
										int i = 1;
										while (message[i] != ' ' && message[i] != '\0')
										{
											username[i - 1] = message[i];
											i++;
										}
									}
									else
									{
										printf("3\n");
									}
								}
								char *sender = targetNode->username;
								char newMessage[strlen(sender) + 14];
							 
								if (strcmp(username, ""))
								{ //private message
									printf("Private message detected\n");

									// format buffer to hold username and private message
									memset(newMessage, '\0', strlen(newMessage));
									strcpy(newMessage, "\%"); //make sure this works

									//concatenate message */
									for (int i = 0; i < 11 - strlen(sender); i++)
									{
										strcat(newMessage, " ");
									}
									strcat(newMessage, sender);
									strcat(newMessage, ":");
									strcat(newMessage, " ");
									strcat(newMessage, message);

									struct node *match = findPtcpMatch(username, ptcp_head);
									if (match)
									{ //valid recipient username
								       
										uint16_t mSize = htons(strlen(newMessage));
										send(targetNode->obsv_fd, &mSize, sizeof(uint16_t), 0);		   // send message size to sender observer
										send(targetNode->obsv_fd, &newMessage, strlen(newMessage), 0); // send message to sender observer
										send(match->obsv_fd, &mSize, sizeof(uint16_t), 0);			   // send message size to recipient observer
										send(match->obsv_fd, &newMessage, strlen(newMessage), 0);	   // send message to recipient observer
									}
									else
									{ //invalid recipient username
									   
										memset(sendbuf, '\0', strlen(sendbuf));
										strcpy(sendbuf, "Warning: user ");
										strcat(sendbuf, username);
										strcat(sendbuf, " doesn't exist...");
										uint16_t mSize = htons(strlen(sendbuf));
										send(targetNode->obsv_fd, &mSize, sizeof(uint16_t), 0);	 // send alert size to sender observer
										send(targetNode->obsv_fd, &sendbuf, strlen(sendbuf), 0); // send alert to sender
									}
								}
								else
								{ //public message

									memset(newMessage, '\0', strlen(newMessage));
									strcpy(newMessage, ">");

									// concatenate messages
									for (int i = 0; i < 11 - strlen(sender); i++)
									{
										strcat(newMessage, " ");
									}
									strcat(newMessage, sender);
									strcat(newMessage, ":");
									strcat(newMessage, " ");
									strcat(newMessage, message);
									broadcast(newMessage, obsv_head);
								}
							}
							else
							{
								close(targetNode->ptcp_fd);
								FD_CLR(targetNode->ptcp_fd, &readfds);
								targetNode->ptcp_fd = -1;
								close(targetNode->obsv_fd);
								FD_CLR(targetNode->obsv_fd, &readfds);
								targetNode->obsv_fd = -1;
							}
						}
					}
				}
			}

		 

			if (!isEmpty(ptcp_head))
			{
				struct node *current = ptcp_head;
				int isNext = 1;
				while (isNext)
				{
					//if it is last node
					if (current->next == NULL)
					{
						timeoutCheck(current, readfds);
						isNext = 0;
						// If current node is not yet active, check if it has timed out. If so, close the socket.
					}
					else
					{
						// If current node is not yet active, check if it has timed out. If so, close the socket.
						timeoutCheck(current, readfds);
						current = current->next;
					}
				}
			}

			if (!isEmpty(obsv_head))
			{
				struct node *current = obsv_head;
				int isNext = 1;
				while (isNext)
				{
					//if it is last node
					if (current->next == NULL)
					{
						timeoutCheck(current, readfds);
						isNext = 0;
						// If current node is not yet active, check if it has timed out. If so, close the socket.
					}
					else
					{
						// If current node is not yet active, check if it has timed out. If so, close the socket.
						timeoutCheck(current, readfds);
						current = current->next;
					}
				}
			}
		}
		else if (retval == -1)
		{
			fprintf(stderr, "Error: select() failed. %s\n", strerror(errno));
			// TODO: handle this case when observer disconnects
			exit(EXIT_FAILURE);
		}
     
	}
}
