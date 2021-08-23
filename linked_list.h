// Source: https://www.tutorialspoint.com/data_structures_algorithms/linked_list_program_in_c.htm
#include <stdbool.h>
#include <time.h>

struct node {
    int isSet;
    int isPaired;
    int obsv_fd;
	int ptcp_fd;
	char *username;
    time_t start;
    struct node *next;
};

//display the list
void printList(struct node *head);

//insert link at the first location
struct node* insertFirst(struct node *head, int ptcp_fd, int obsv_fd, char username[10], int isPaired);

//delete first item
struct node* deleteFirst(struct node *head);

//is list empty
bool isEmpty(struct node *head);

//return length of list
int length();

//find a link with given username
struct node* find(struct node *head, int ptcp_fd); //can only use this if usernames are populated. Make head passed as param rather than stored in LL.c

//delete a link with given ptcp_fd
struct node* deleteByPtcp(struct node *head, int ptcp_fd);
struct node* deleteByObsv(struct node *head, int obsv_fd);

