/* trie.c - code for trie data structure. Do not rename this file 
 * Source: https://www.techiedelight.com/trie-implementation-insert-search-delete */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// define character size
#define CHAR_SIZE 26
#include "trie.h"

// Function that returns a new Trie node
struct Trie* trieCreate(void)
{
	struct Trie* node = (struct Trie*)malloc(sizeof(struct Trie));
	node->isLeaf = 0;

	for (int i = 0; i < CHAR_SIZE; i++)
		node->character[i] = NULL;

	return node;
}

// Iterative function to insert a string in Trie
int trieInsert(struct Trie *head, char* str)
{
	// start from root node
	struct Trie* curr = head;
	while (*str)
	{
		// create a new node if path doesn't exists
		if (curr->character[*str - 'a'] == NULL)
			curr->character[*str - 'a'] = trieCreate();

		// go to next node
		curr = curr->character[*str - 'a'];

		// move to next character
		str++;
	}

	// mark current node as leaf
	curr->isLeaf = 1;
	return 1;
}

// Iterative function to search a string in Trie. It returns 1
// if the string is found in the Trie, else it returns 0
int trieSearch(struct Trie* head, char* str)
{
	// return 0 if Trie is empty
	if (head == NULL)
		return 0;

	struct Trie* curr = head;
	while (*str)
	{
		// go to next node
		curr = curr->character[*str - 'a'];

		// if string is invalid (reached end of path in Trie)
		if (curr == NULL)
			return 0;

		// move to next character
		str++;
	}

	// if current node is a leaf and we have reached the
	// end of the string, return 1
	return curr->isLeaf;
}

// returns 1 if given node has any children
int haveChildren(struct Trie* curr)
{
	for (int i = 0; i < CHAR_SIZE; i++)
		if (curr->character[i])
			return 1;	// child found

	return 0;
}


// Recursive function to delete a string in Trie
int deletion(struct Trie **curr, char* str)
{
	// return if Trie is empty
	if (*curr == NULL)
		return 0;

	// if we have not reached the end of the string
	if (*str)
	{
		// recur for the node corresponding to next character in
		// the string and if it returns 1, delete current node
		// (if it is non-leaf)
		if (*curr != NULL && (*curr)->character[*str - 'a'] != NULL &&
			deletion(&((*curr)->character[*str - 'a']), str + 1) &&
			(*curr)->isLeaf == 0)
		{
			if (!haveChildren(*curr))
			{
				free(*curr);
				(*curr) = NULL;
				return 1;
			}
			else {
				return 0;
			}
		}
	}

	// if we have reached the end of the string
	if (*str == '\0' && (*curr)->isLeaf)
	{
		// if current node is a leaf node and don't have any children
		if (!haveChildren(*curr))
		{
			free(*curr); // delete current node
			(*curr) = NULL;
			return 1; // delete non-leaf parent nodes
		}

		// if current node is a leaf node and have children
		else
		{
			// mark current node as non-leaf node (DON'T DELETE IT)
			(*curr)->isLeaf = 0;
			return 0;	   // don't delete its parent nodes
		}
	}

	return 0;
}

// recursive function that frees the Trie and prevents memory leaks
void free_all(struct Trie* head) {

  // if the head node is null
  if(head == NULL) {
    return;
  }
  
  // recursive case. free all characters
  for (int i = 0; i < 27; i++) {
    if (head->character[i] != NULL) {
      free_all(head->character[i]);
    }
  }

  //base case
  free(head);

}
