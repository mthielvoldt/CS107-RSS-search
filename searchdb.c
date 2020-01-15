
#include "searchdb.h"

/** 
 * StringHash                     
 * ----------  
 * This function adapted from Eric Roberts' "The Art and Science of C"
 * It takes a string and uses it to derive a hash code, which   
 * is an integer in the range [0, numBuckets).  The hash code is computed  
 * using a method called "linear congruence."  A similar function using this     
 * method is described on page 144 of Kernighan and Ritchie.  The choice of                                                     
 * the value for the kHashMultiplier can have a significant effect on the                            
 * performance of the algorithm, but not on its correctness.                                                    
 * This hash function has the additional feature of being case-insensitive,  
 * hashing "Peter Pawlowski" and "PETER PAWLOWSKI" to the same code.  
 */  
static const signed long kHashMultiplier = -1664117991L;
static int StringHash(const void *string, int numBuckets)  
{           
  char *s = (char*)string; 
  int i;
  unsigned long hashcode = 0;
  
  for (i = 0; i < strlen(s); i++)  
    hashcode = hashcode * kHashMultiplier + tolower(s[i]);  
  return hashcode % numBuckets;                                
}

static int ArticleHash(const void *article_p, int numBuckets) {
    char *key = ((article_t*)article_p)->title;   // keys on the article title (c string)
    return StringHash(key, numBuckets);
}

static int WordHash( const void *article_list_p, int numBuckets) {
  char *key = ((article_list_t*)article_list_p)->word;    // keys on the word (c string)
  return StringHash(key, numBuckets);
}

// Compare functions /////////////////

static int StringCompare(const void *a, const void*b) {
  return strcasecmp( (const char*)a, (const char*)b );
}

static int ArticleCompare(const void *article_a, const void *article_b ) {
  // WE know the parameters are pointers to article_t structures.  re-cast them as such, and get the title field
    char* a_key = ((article_t*)article_a)->title;
    char* b_key = ((article_t*)article_b)->title;
    return strcasecmp(a_key, b_key);
}

static int WordCompare(const void *article_list_a, const void *article_list_b) {
  // WE know the parameters are pointers to article_list_t structures.  re-cast them as such, and get the word field.
  char *a_key = ((article_list_t*)article_list_a)->word;
  char *b_key = ((article_list_t*)article_list_b)->word;
  return strcasecmp(a_key, b_key);
}

// A pointer to this function will be stored in hashset internal variables.  It must be able to be called 
// via it's function pointer, so it can not be inline. 
static void ArticleListFreeFn( void * elem ) {
  //First we re-hydrate the type
  article_list_t *article_list_p = (article_list_t*)elem;

  // Note: the vector itself *could* have its own freefn defined, if we had passed it one in each call to VectorNEw. 
  //  In this program, we did not because the occurrances aren't responsible for freeing the articles. 
  VectorDispose(&article_list_p->occurrances);
}


void InitDatabase(search_db *db) {
  HashSetNew(&db->stop_words, sizeof(char)*kkey_size, kstopword_buckets, StringHash, StringCompare, NULL);
  HashSetNew(&db->articles, sizeof(article_t), karticle_buckets, ArticleHash, ArticleCompare, NULL );
  HashSetNew( &db->words, sizeof(article_list_t), kword_buckets, WordHash, WordCompare, ArticleListFreeFn);
}

void DisposeDatabase(search_db *db) {
  HashSetDispose(&db->stop_words);
  HashSetDispose(&db->articles);
  HashSetDispose(&db->words);
}

void AddNewOccurrance( article_t *article_p, article_list_t *word_p) {
  occurrance_t new_occurrance;
  new_occurrance.article_p = article_p; // set the address of the article
  new_occurrance.count = 1;
  VectorAppend(&word_p->occurrances, &new_occurrance );
}



// Returns either the index of an occurrance in the article_list whose pointer to 
// an article address matches the pointer passed in. 
// or returns NULL if there is no matching occurrance
occurrance_t* MatchingOccurrance( article_t *article_p, article_list_t *word_p)
{
  int n_occurrances = VectorLength(&word_p->occurrances);
  if (n_occurrances == 0) return NULL;

  occurrance_t *last_occurrance = VectorNth(&word_p->occurrances, n_occurrances-1);

  if (last_occurrance->article_p == article_p) return last_occurrance;
  else return NULL; 
}

bool RecordOccurrance(char *word, article_t *article_in, search_db *db){

  int word_length = strlen(word);
  assert(word_length > 0);

  // this function doesn't know how long word might be. 
  // but it can only store [kkey_size] words (including null terminator).
  // so we check it it's too long, and if it is, insert a null terminator 
  // so later we don't copy too much.
  if( (word_length + 1) > kkey_size )
    word[kkey_size-1] = '\0';

  int title_length = strlen(article_in->title);
  assert(title_length > 0);
  assert(title_length < TITLE_N_BYTES);

  // Is Word in stop list? 
  if (HashSetLookup(&db->stop_words, word) != NULL) return false;

  // Find the address of the article *in the hashset* that matches article.
  // We don't assume we're given a pointer to an article in the articles hashset, 
  // But we do assume we're given a pointer to a valid article_t somewhere. 
  article_t *article_p = HashSetLookup(&db->articles, article_in);
  // Make sure the article was already in articles hashset.
  assert(article_p != NULL);

    
  article_list_t *word_p = (article_list_t*)HashSetLookup(&db->words, word);
  article_list_t new_word; 
 
  // if the word isn't present already, build and insert a new article_list. 
  if (word_p == NULL) {
    // build a new article_list_t 
    // the occurrances vector doesn't need a free function because it is not responsible
    // for freeing the articles.  The articles hashset does that. 
    VectorNew(&new_word.occurrances, sizeof(occurrance_t), NULL, 100);
    strcpy(new_word.word, word); //everything is null-terminated, so we can use strcpy.
    HashSetEnter(&db->words, &new_word); // this memcpy's the new_word.
    // Now we need the location of the newly-minted word in the hashset. 
    word_p = (article_list_t*)HashSetLookup(&db->words, word);

    // we just added this word, so we know we don't have a matching article occurrance. 
    AddNewOccurrance(article_p, word_p);
    return true;
  }

  // Let's search for a matching occurrance of this article-word.  
  // MatchingOccurrance returns either the pointer of an occurrance that maches or NULL. 
  occurrance_t *match = MatchingOccurrance(article_p, word_p );

  // If the last occurrance points to a different article, or if there are no occurrances, we append. 
  if ( match == NULL )
    AddNewOccurrance(article_p, word_p);
  // If the last occurrance does match the article (we've seen this word in this article before)
  else 
    match->count++;

  return true;
}

static void PrintArticle( void *elem_addr, void *auxData) {
  occurrance_t *occurrance = (occurrance_t*)elem_addr;
  article_t *article = occurrance->article_p;
  int *article_count = (int*)auxData;
  
  // article_count indicates how many we have already printed. 
  // we can use this to stop printing after 10 articles. 
  if (*article_count >= 10 ) return; 

  printf("\t%d.) \"%s\"\n", ++*article_count, article->title);
  printf("\t    %s\n", article->url);
  printf("\t    [search term occurred %d times]\n\n", occurrance->count);
}
/**
 * Searches the database for the word.  If it's found it lists all the articles containing that word
 */ 
void PrintArticles( const char *word, search_db *db) {

  // is it a stop-word? 
  if (HashSetLookup(&db->stop_words, word) != NULL) {
    printf("That word is too common to produce a meaningful search.\n\n");
    return;
  }
  // find the word
  article_list_t *word_p; 
  word_p = HashSetLookup(&db->words, word);
  
  if(word_p == NULL) {
    printf("None of today's articles mention that word.  Sorry.\n\n");
    return;
  }

  int n_articles = VectorLength(&word_p->occurrances);
  printf("We found %d articles containing the word \"%s\".", n_articles, word);
  if (n_articles>10)
    printf("  Here are the top 10.\n\n");
  else 
    printf("\n\n");
  
  // this is a counter passed as aux_data to PrintArticle to let PrintArticle keep track of how many it has printed. 
  int articles_printed = 0;   
  VectorMap(&word_p->occurrances, PrintArticle, &articles_printed);

}

static int CompareOccurrances( const void *a, const void *b) {
  occurrance_t *occurrance_a = (occurrance_t*)a;
  occurrance_t *occurrance_b = (occurrance_t*)b;
  return occurrance_b->count - occurrance_a->count;
}

static void MappedSorter( void *elem_addr, void *aux_data) {
  article_list_t *article_list = (article_list_t*)elem_addr;
  VectorSort(&article_list->occurrances, CompareOccurrances );
}
void SortOccurrances(search_db *db) {
  HashSetMap(&db->words, MappedSorter, NULL);
}

/*I don't think we need this. 
int AddressCompare(void *elemAddr1, void *elemAddr2 ) {
  occurrance_t *occurrance_p1 = (occurrance_t*)elemAddr1;
  occurrance_t *occurrance_p2 = (occurrance_t*)elemAddr2;
  // we're subtracting to see if the pointers are the same (point to the same location)
  return occurrance_p1->article_p - occurrance_p2->article_p;
} */