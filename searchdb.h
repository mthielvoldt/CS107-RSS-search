
#ifndef __SEARCHDB_
#define __SEARCHDB_

#include <stdlib.h> // defines: NULL
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "hashset.h"
#include "vector.h"
#include "mstreamtokenizer.h"


//#include <ctype.h>

#define WORD_N_BYTES 32
#define TITLE_N_BYTES 512

typedef struct {
  char title[TITLE_N_BYTES];
  char desc[1024];
  char url[2048];
  mstreamtokenizer_t mst;
} article_t;

typedef struct {
  char word[WORD_N_BYTES];
  vector occurrances; 
} occurrance_list_t;

typedef struct {
  int count; 
  article_t *article_p; 
} occurrance_t; 

typedef struct {
  hashset stop_words; 
  hashset articles;
  hashset words;
} search_db; 

char *strcpy(char *dest, const char *src);  // in string.h

static const int kstopword_buckets = 4001;
static const int karticle_buckets = 997;
static const int kword_buckets = 10007;  //100_003
static const int kkey_size = 32;

int StringHash(const void *string, int numBuckets);

int StringCompare(const void *a, const void*b);

/**
 * Initializes all three hashsets in the db, hooking them up with the 
 * correct HashFunctions, CompareFunctions and FreeFunctions.  
*/
void InitDatabase(search_db *db);

// Torches all of it. 
void DisposeDatabase(search_db *db);

/**
 * Checks if the word is present in the words hashset.  
 * If word is already present, it does nothing and returns the address of the 
 * occurrance_list_t that it found. 
 * If word is not already present, AddWordIfAbsent builds a occurrance_list_t 
 * element, Inserts it into the words hashset, and returns the address of the new
 * occurrance_list_t. 
 */
occurrance_list_t* AddWordIfAbsent(char *word);

/**
 * RecordOccurrance
 * Records the occurrance of a meaningful word in a word-index database.
 * Aassumes an article is present in articles hashset that matches the 
 * article parameter.
 * 
 * Checks if the word matchs a stopword.  If it does, RecordOccurrance
 * will return false without making any changes to the database.
 * 
 * Adds the word if it isn't already in words hashset. 
 * 
 * Looks at the last element of the occurrances vector for this word. 
 * (Each unique word has an occurrances vector associated with it)
 * 
 * If the last occurrance in occurrances vector points to an article_t that 
 * matches the article parameter, then RecordOccurrance increments 
 * the occurrance count. 
 * 
 * If the last occurrance ponts to a different article, RecordOccurrances
 * will build a new occurrance_t which will have its occurrance count = 1
 * and append the new occurrance to the occurrances vector.
 * 
 * Returns false if word is a stopword. Otherwise, returns true. 
 * 
 * Asserts: 
 * - the article is already present in the articles database.
 * - the word is at least 1 character long. 
 */
bool RecordOccurrance(char *word, article_t *article_in, search_db *db);

void SortOccurrances(search_db *db);

void PrintArticles( const char *word, search_db *db);

#endif  // __SEARCHDB_