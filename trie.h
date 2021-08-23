struct Trie
{
	int isLeaf;
	struct Trie* character[26];
};



// TODO: Implement the following functions in trie.c
struct Trie  *trieCreate(void);  // Returns a pointer to an empty trie
int trieInsert(struct Trie *head, char *word);  // Returns 1 on success, 0 on failure
int trieSearch(struct Trie* head, char *word);  // Returns 1 if word exists in trie, otherwise returns 0

