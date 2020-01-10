
#ifndef __SEARCHDB_
#define __SEARCHDB_

#include "hashset.h"
#include "multitable.h"
#include <strings.h>
#include <ctype.h>


typedef struct {
  char title[512];
  char desc[1024];
  char url[2048];
} article_t;

typedef struct {
  hashset stop_list; 
  hashset articles;
  multitable w_a_pairs;
} search_db; 

static const int kstopword_buckets = 4001;
static const int karticle_buckets = 997;
static const int kindex_buckets = 10007;  //100_003
static const int kkey_size = 32;

static int StringHash(const void *string, int numBuckets);

inline static int ArticleHash(const void *article_p, int numBuckets) {
    char *key = ((article_t*)article_p)->title;
    return StringHash(key, numBuckets);
}

inline static int StringCompare(const void *a, const void*b) {
  return strcasecmp( (const char*)a, (const char*)b );
}

inline static int ArticleCompare(const void *article_a, const void *article_b ) {
    char* a_key = ((article_t*)article_a)->title;
    char* b_key = ((article_t*)article_b)->title;
    return strcasecmp(a_key, b_key);
}

void InitDatabase(search_db *db);

void DisposeDatabase(search_db *db);

#endif  // __SEARCHDB_