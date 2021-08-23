// Source: https://www.tutorialspoint.com/data_structures_algorithms/linked_list_program_in_c.htm

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "linked_list.h"

//display the list
void printList(struct node *head)
{
   struct node *ptr = head;
   printf("\n[ ");

   //start from the beginning
   while (ptr != NULL)
   {
      printf("(%d) ", ptr->obsv_fd);
      ptr = ptr->next;
   }

   printf(" ]");
}

//insert link at the first location
struct node *insertFirst(struct node *head, int ptcp_fd, int obsv_fd, char username[10], int isPaired)
{
   //create a link
   struct node *link = (struct node *)malloc(sizeof(struct node));

   link->ptcp_fd = ptcp_fd;
   link->obsv_fd = obsv_fd;
   link->username = username;
   link->start = time(NULL);
   link->isPaired = 0;
   link->isSet = 0;

   //point it to old first node
   link->next = head;
   return link;
   //point first to new first node
   //head = link;

   //return head;
}

//delete first item
struct node *deleteFirst(struct node *head)
{

   //save reference to first link
   struct node *tempLink = head;

   //mark next to first link as first
   head = head->next;

   //return the deleted link
   return tempLink;
}

//is list empty
bool isEmpty(struct node *head)
{
   return head == NULL;
}

int length(struct node *head)
{
   int length = 0;
   struct node *current;

   for (current = head; current != NULL; current = current->next)
   {
      length++;
   }

   return length;
}

//find a link with given username
struct node *find(struct node *head, int ptcp_fd)
{

   //start from the first link
   struct node *current = head;

   //if list is empty
   if (head == NULL)
   {
      return NULL;
   }

   //navigate through list
   while (current->ptcp_fd != ptcp_fd)
   {

      //if it is last node
      if (current->next == NULL)
      {
         return NULL;
      }
      else
      {
         //go to next link
         current = current->next;
      }
   }

   //if data found, return the current Link
   return current;
}

//delete a link with given ptcp_fd
struct node *deleteByPtcp(struct node *head, int ptcp_fd)
{

   //start from the first link
   struct node *current = head;
   struct node *previous = NULL;

   //if list is empty
   if (head == NULL)
   {
      return NULL;
   }

   //navigate through list
   while (current->ptcp_fd != ptcp_fd)
   {

      //if it is last node
      if (current->next == NULL)
      {
         return NULL;
      }
      else
      {
         //store reference to current link
         previous = current;
         //move to next link
         current = current->next;
      }
   }

   //found a match, update the link
   if (current == head)
   {
      //change first to point to next link
      head = head->next;
   }
   else
   {
      //bypass the current link
      previous->next = current->next;
   }

   return current;
}

//delete a link with given obsv_fd
struct node *deleteByObsv(struct node *head, int obsv_fd)
{
   //start from the first link
   struct node *current = head;
   struct node *previous = NULL;
   //if list is empty
   if (head == NULL)
   {
      return NULL;
   }
   //navigate through list
   while (current->ptcp_fd != obsv_fd)
   {
      //if it is last node
      if (current->next == NULL)
      {
         return NULL;
      }
      else
      {
         //store reference to current link
         previous = current;
         //move to next link
         current = current->next;
      }
   }
   //found a match, update the link
   if (current == head)
   {
      //change first to point to next link
      head = head->next;
   }
   else
   {
      //bypass the current link
      previous->next = current->next;
   }
   return current;
}