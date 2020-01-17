
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


const int kthread_sharing = 0;
void InitDomain(domain_t *d) {
    
    //d->n_threads = 0;
    //assert( sem_init(&d->n_threads_lock, kthread_sharing, 1) == 0);

    d->n_unclaimed_feeds = 0;
    assert( sem_init(&d->n_unclaimed_feeds_lock, kthread_sharing, 1) == 0);

    d->articles_tail = 0;
    d->n_articles = 0;

    // titles_hashset elements are only char[TITLE_N_BYTES].  No pointers to heap memory.
    // in the hashset element, so there is no need for a free function. 
    HashSetNew(&d->titles_hashset, TITLE_N_BYTES, 1007, StringHash, StringCompare, NULL);
    assert( sem_init(&d->titles_input_lock, kthread_sharing, 1) == 0);

    // articles_vector elements are article_t's.  These contain pointers to heap memstreams. 
    // however, all the elements will get copied to the db->articles hashset before destroying. 
    VectorNew(&d->articles_vector, sizeof(article_t), NULL, 250 );
    assert( sem_init(&d->articles_retreival_lock, kthread_sharing, 1) == 0);
}

void DomainDispose(domain_t *d) {
    // torch the vector of articles in each domain.  
    // This does not free the mstreams with the full article text. 
    // we keep that because db.articles still points to those streams. 
    // and we want to be able to print out the full articles
    VectorDispose(&d->articles_vector);
    HashSetDispose(&d->titles_hashset);
    assert( sem_destroy(&d->articles_retreival_lock) == 0 );
    assert( sem_destroy(&d->titles_input_lock) == 0 );
    assert( sem_destroy(&d->n_unclaimed_feeds_lock) == 0 );
    assert( sem_destroy(&d->n_threads_lock) == 0 );
}
