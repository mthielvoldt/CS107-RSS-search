
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


void InitDatabase(search_db *db) {

  HashSetNew(&db->stop_list, sizeof(char)*kkey_size, kstopword_buckets, StringHash, StringCompare, NULL);

  HashSetNew(&db->articles, sizeof(article_t), karticle_buckets, ArticleHash, ArticleCompare, NULL );

  MultiTableNew(&db->w_a_pairs, sizeof(char)*kkey_size, sizeof(int) + sizeof(article_t*), 
                kindex_buckets, StringHash, StringCompare, NULL );
}

void DisposeDatabase(search_db *db) {
  HashSetDispose(&db->stop_list);
  HashSetDispose(&db->articles);
  MultiTableDispose(&db->w_a_pairs);
}
