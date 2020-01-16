
//#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "bool.h"
#include <assert.h>
#include "mstreamtokenizer.h"
#include "searchdb.h"

#define URL_LENGTH 2048
#define MAX_FEEDS_PER_DOMAIN 30


// A single domain_t structure is shared between several Downloader threads.
// Each Downloader thread only ever serves one domain.  
// No domains have access to the database.  All database entry is executed
// after all articles are downloaded. 

typedef struct {
    int n_threads; 
    sem_t n_threads_lock; 
    
    int n_unclaimed_feeds;
    sem_t n_unclaimed_feeds_lock;
    char rss_url[MAX_FEEDS_PER_DOMAIN][URL_LENGTH];

    hashset titles_hashset;   // used for skipping repeat articles upon entry
    sem_t titles_input_lock;

    vector articles_vector;
    int articles_tail, n_articles; // used for pulling articles off the list. 
    sem_t articles_retreival_lock; 

} domain_t;

typedef struct {
    int n_domains;
    domain_t domains[10];
} domain_list_t;

const int kthread_sharing = 0;
void InitDomain(domain_t *d) {
    
    d->n_threads = 0;
    assert( sem_init(&d->n_threads_lock, kthread_sharing, 1) == 0);

    d->n_unclaimed_feeds = 0;
    assert( sem_init(&d->n_unclaimed_feeds_lock, kthread_sharing, 1) == 0);

    HashSetNew(&d->titles_hashset, TITLE_N_BYTES, 1007, StringHash, StringCompare, NULL);
    assert( sem_init(&d->titles_input_lock, kthread_sharing, 1) == 0);

    VectorNew(&d->articles_vector, sizeof(article_t), NULL, 250 );
    assert( sem_init(&d->articles_retreival_lock, kthread_sharing, 1) == 0);
}
