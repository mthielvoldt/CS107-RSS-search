#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#include "curlconnection.h"
#include "bool.h"
#include "streamtokenizer.h"
#include "mstreamtokenizer.h"
#include "html-utils.h"
#include "searchdb.h"
#include "news-thread.h"


static void Welcome(const char *welcomeTextFileName);
static void LoadStopList(hashset *stop_list);
static void BuildIndices(const char *feedsFileName, search_db_t *db);

void* DownloaderThread( void *arg );
static void ParseFeed(streamtokenizer *st, domain_t *domain);
static void DownloadArticle( int article_index, domain_t *domain, curlconnection_t *connection );

static bool GetNextItemTag(streamtokenizer *st);
static bool ParseItem(streamtokenizer *st, article_t *article );
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength);
static void ProcessArticle(article_t *article, search_db_t *db);
static void QueryIndices();
static void ProcessResponse(const char *word, search_db_t *db);
static bool WordIsWellFormed(const char *word);
static void AddPair(char *word, search_db_t *db, article_t *article_addr );


/**
 * Function: main
 * --------------
 * Serves as the entry point of the full application.
 * You'll want to update main to declare several hashsets--
 * one for stop words, another for previously seen urls, etc--
 * and pass them (by address) to BuildIndices and QueryIndices.
 * In fact, you'll need to extend many of the prototypes of the
 * supplied helpers functions to take one or more hashset *s.
 *
 * Think very carefully about how you're going to keep track of
 * all of the stop words, how you're going to keep track of
 * all the previously seen articles, and how you're going to 
 * map words to the collection of news articles where that
 * word appears.
 */

static const char *const kWelcomeTextFile = "./data/welcome.txt";
static const char *const kDefaultFeedsFile = "./data/rss-feeds-large.txt";


int main(int argc, char **argv)
{
  search_db_t db;   // the database. This will be passed down the function hierarchy.
  InitDatabase(&db);
  
  Welcome(kWelcomeTextFile);

  LoadStopList(&db.stop_words);

  BuildIndices((argc == 1) ? kDefaultFeedsFile : argv[1], &db);  // runs only once. 

  QueryIndices(&db);  

  DisposeDatabase(&db); 
  
  return 0;
}

/** 
 * Function: Welcome
 * -----------------
 * Displays the contents of the specified file, which
 * holds the introductory remarks to be printed every time
 * the application launches.  This type of overhead may
 * seem silly, but by placing the text in an external file,
 * we can change the welcome text without forcing a recompilation and
 * build of the application.  It's as if welcomeTextFileName
 * is a configuration file that travels with the application.
 */
 
static const char *const kNewLineDelimiters = "\r\n";
static void Welcome(const char *welcomeTextFileName)
{
  FILE *infile;
  streamtokenizer st;
  char buffer[1024];
  
  infile = fopen(welcomeTextFileName, "r");
  assert(infile != NULL);    
  
  STNew(&st, infile, kNewLineDelimiters, true);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    printf("%s\n", buffer);
  }
  
  printf("\n");
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one.. 
  fclose(infile);
}

static void LoadStopList(hashset *stop_list) {
  //First, we initialize the list.  Because we set this up to store char arrays 
  // of fixed width, we don't need to worry about the HashSetFree function. 
  
  int i = 0;
  FILE* infile;
  streamtokenizer st;
  char buffer[kkey_size];
  infile = fopen("data/stop-words.txt", "r");
  assert(infile != NULL);
  STNew(&st, infile, kNewLineDelimiters, true);

  while (STNextToken(&st, buffer, sizeof(buffer))) {
    //printf("%s ", buffer);
    HashSetEnter(stop_list, buffer);
    i++;
  }
  
  printf("%d stop words loaded\n", i);
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one.. 
  fclose(infile);

}

static void BuildDomains(const char *feedsFileName, domain_t domains[], int *n_domains) {

  int path_index;
  domain_t *active_domain; 
  char full_url[1024], domain_name[512], previous_domain_name[512], rss_label[400];
  FILE *infile;
  streamtokenizer st;
  
  infile = fopen(feedsFileName, "r");
  assert(infile != NULL);
  STNew(&st, infile, kNewLineDelimiters, true);
    // build the domains. 
  *n_domains = 0;
  while (STNextTokenUsingDifferentDelimiters(&st, rss_label, 400, ":")) { // ignore everything up to the first selicolon of the line
    STSkipOver(&st, ": ");		 // now ignore the semicolon and any whitespace directly after it
    STNextToken(&st, full_url, sizeof(full_url));   
    STSkipOver(&st, "\r\n");

    // find the first slash after position 10  https://subdomain.domain.com/
    // to separate out just the domain from the full URL. 
    path_index = (strchr(full_url + 10, '/') - full_url);
    strncpy(domain_name, full_url, path_index );
    domain_name[path_index] = '\0';


    // if this domain is not the same as the previous domain, advance to the next domain.
    if( strcmp(domain_name, previous_domain_name) ) { 
      active_domain = &domains[*n_domains];
      (*n_domains)++;
      //printf("domains %d", *n_domains);

      InitDomain(active_domain);
      // advance previous domain name
      strcpy( previous_domain_name, domain_name);
    }
    // copy the feed url to the active domain. 
    strcpy( active_domain->rss_url[active_domain->n_unclaimed_feeds], full_url);
    active_domain->n_unclaimed_feeds++;
  }
  STDispose(&st);
  fclose(infile);
}

static void DownloadWithThreads(domain_t domains[], int n_domains) {
  pthread_t threads[50];
  int i, j, n_threads = 0;

  for(i = 0; i < n_domains; i++ )
  {
    //printf("\nDomain %d:\n", i );
    for(j = 0; j < domains[i].n_unclaimed_feeds; j++) 
    {
      //printf("%s\n", domains[i].rss_url[j]);
      pthread_create(&threads[n_threads], NULL, DownloaderThread, &domains[i] );
      n_threads++;
    }
  }

  // don't return until all threads have exited.
  for( i = 0; i < n_threads; i++ ) {
    pthread_join(threads[i], NULL);
  }
}

static void MergeDomainData(domain_t domains[], int n_domains, search_db_t *db) {
  int n_articles;
  article_t *article_j; 

   for( int i = 0; i < n_domains; i++ )
  {
    n_articles = domains[i].n_articles;
    for( int j = 0; j < n_articles; j++) 
    {
      article_j = VectorNth( &domains[i].articles_vector, j);
      ProcessArticle(article_j, db);
    }
    // now that all the articles are copied, to the db, we can toss the domain. 
    DomainDispose( &domains[i] );    
  }
}
/**
 * Function: BuildIndices
 * ----------------------
 * As far as the user is concerned, BuildIndices needs to read each and every
 * one of the feeds listed in the specied feedsFileName, and for each feed parse
 * content of all referenced articles and store the content in the hashset of indices.
 * Each line of the specified feeds file looks like this:
 *
 *   <feed name>: <URL of remore xml document>
 *
 * Each iteration of the supplied while loop parses and discards the feed name (it's
 * in the file for humans to read, but our aggregator doesn't care what the name is)
 * and then extracts the URL.  It then relies on ProcessFeed to pull the remote
 * document and index its content.
 */

static void BuildIndices(const char *feedsFileName, search_db_t *db )
{
  curl_global_init(CURL_GLOBAL_SSL);  // Run once.  BuildIndices runs once for life of program. 
  domain_t domains[10]; 
  int n_domains;
  time_t t1;

  // Reads the feeds file and groups feeds by the domain, 
  // initializing a domain structure for each unique domain. 
  BuildDomains(feedsFileName, domains, &n_domains); // this builds the domains. 

  // this is blocking. It spaws threads, but rejoins with all before returning. 
  t1 = time(NULL);
  DownloadWithThreads(domains, n_domains);
  printf("Downloads took %f seconds.\n\n", difftime(time(NULL), t1));

  // Iterates through all the domain data and populates db. 
  t1 = time(NULL);
  MergeDomainData(domains, n_domains, db);
  printf("Processing took %f seconds.\n", difftime(time(NULL), t1));
  printf("Processed %d unique articles. \n\n", HashSetCount(&db->articles));

  // now we sort all vectors that store the occurrance of a word in an article.  
  // When we search for a term, the articles we get back will be in sorted order from those
  // articles that mention the term the most, to those the the fewest mentions. 
  SortOccurrances(db);

  curl_global_cleanup();  // once for life of program. 
}

/**
 * Function: DownloaderThread
 * ---------------------
 * DownloaderThread locates the specified RSS document, and if a (possibly redirected) connection to that remote
 * document can be established, then reads the feed.  
 * 
 * Steps though the data of what is assumed to be an RSS feed identifying the titles and
 * URLs of online news articles.  Check out "datafiles/sample-rss-feed.txt" for an idea of what an
 * RSS feed from the www.nytimes.com (or anything other server that syndicates is stories).
 *
 * DownloaderThread views a typical RSS feed as a sequence of "items", where each item is detailed
 * using a generalization of HTML called XML.  A typical XML fragment for a single news item will certainly
 * adhere to the format of the following example:
 *
 * <item>
 *   <title>At Installation Mass, New Pope Strikes a Tone of Openness</title>
 *   <link>http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</link>
 *   <description>The Mass, which drew 350,000 spectators, marked an important moment in the transformation of Benedict XVI.</description>
 *   <author>By IAN FISHER and LAURIE GOODSTEIN</author>
 *   <pubDate>Sun, 24 Apr 2005 00:00:00 EDT</pubDate>
 *   <guid isPermaLink="false">http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</guid>
 * </item>
 *
 * DownloaderThread reads and discards all characters up through the opening <item> tag (discarding the <item> tag
 * as well, because once it's read and indentified, it's been pulled,) and then hands the state of the stream to
 * ParseItem(), which handles the job of pulling and analyzing everything up through and including the </item>
 * tag. DownloaderThread processes the entire RSS feed and repeatedly advancing to the next <item> tag and then allowing
 * ParseItem() to process everything up until </item>.
 */

static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";

void* DownloaderThread( void *arg ) {
  domain_t *domain = (domain_t*)arg; 

  curlconnection_t connection;    // Each thread has a single connection.
  CurlConnectionNew(&connection);
  int l_rss_index, article_index;
  mstreamtokenizer_t mst;

  //printf("Thread Started \n");


  // Download all feeds.  Loop until all feeds are claimed by a thread. 
  while(true) {
    //critital regtion where we read the feed counter and claim one. 
    sem_wait(&domain->n_unclaimed_feeds_lock);
    if( domain->n_unclaimed_feeds <= 0 ) {    // Have all feeds been claimed? 
      sem_post(&domain->n_unclaimed_feeds_lock);
      break;
    }
    l_rss_index = --(domain->n_unclaimed_feeds);  // decrement and copy locally.
    sem_post(&domain->n_unclaimed_feeds_lock);     // end of critical region. 

    // Download and store in local mstream. 
    MSTNew(&mst, kTextDelimiters, false);
    if ( CurlConnectionFetch(domain->rss_url[l_rss_index], mst.stream, &connection ) != CURLE_OK ) {
      printf("Problem connecting to: \n%s\nError: %s\n", domain->rss_url[l_rss_index], connection.error_str);
    }
    else {
      // critical in section where it checks for duplicate articles and writes
      // writes articles stored in memstream to the articles vector and titles hashset
      ParseFeed(&mst.st, domain);
    }
    MSTDispose(&mst);
  }
  // All feeds have now been claimed by threads.  As each thread finishes its 
  // feed, it moves on to downloading and storing article text.  
  // Loop until all articles are done or claimed. 

  while(true) {

    // Here we claim an article by its index. 
    sem_wait(&domain->articles_retreival_lock);
    if( domain->articles_tail >= domain->n_articles ) {
      sem_post(&domain->articles_retreival_lock);
      break;
    }
    article_index = domain->articles_tail;
    domain->articles_tail++;
    sem_post(&domain->articles_retreival_lock);


    DownloadArticle(article_index, domain, &connection );  //TODO: separate download from process article. 

  }
  CurlConnectionDispose(&connection);
  printf("Thread Finished \n");
  return NULL;
}


static void ParseFeed(streamtokenizer *st, domain_t *domain)
{
  article_t article;  

  while (GetNextItemTag(st)) { // if true, <item ...> was just read and pulled from the data stream
    // parse each <item> section into an article structure. 
    if( !ParseItem(st, &article ) )
      printf("Failed to read either title or url fields from RSS.\n");
    else {

      //put the article into the domain lists. 
      //This is a critical section. 
      sem_wait(&domain->titles_input_lock);
      // only add articles with titles we haven't seen before. 
      if(HashSetLookup(&domain->titles_hashset, article.title) == NULL) {
        // we enter article into hashset of titles so we can quickly identify duplicate titles.
        HashSetEnter( &domain->titles_hashset, article.title );
        // the vector is what will be used later to download the full articles' html. 
        VectorAppend(&domain->articles_vector, &article);
        domain->n_articles++;

        //printf("    %s\n", article.title);
      }
      //else printf("duplicate ignored\n");
      sem_post(&domain->titles_input_lock);

      
    }
  }
}

/**
 * Function: GetNextItemTag
 * ------------------------
 * Works more or less like GetNextTag below, but this time
 * we're searching for an <item> tag, since that marks the
 * beginning of a block of HTML that's relevant to us.  
 * 
 * Note that each tag is compared to "<item" and not "<item>".
 * That's because the item tag, though unlikely, could include
 * attributes and perhaps look like any one of these:
 *
 *   <item>
 *   <item rdf:about="Latin America reacts to the Vatican">
 *   <item requiresPassword=true>
 *
 * We're just trying to be as general as possible without
 * going overboard.  (Note that we use strncasecmp so that
 * string comparisons are case-insensitive.  That's the case
 * throughout the entire code base.)
 * 
 * The intended side-effect is that the stream is advanced up to the 
 * point in the stream where <item ...> tag ends.  If the file end 
 * is found before another <item>, the function returns false.  
 */

static const char *const kItemTagPrefix = "<item";
static bool GetNextItemTag(streamtokenizer *st)
{
  char htmlTag[1024];
  while (GetNextTag(st, htmlTag, sizeof(htmlTag))) {
    if (strncasecmp(htmlTag, kItemTagPrefix, strlen(kItemTagPrefix)) == 0) {
      return true;
    }
  }	 
  return false;
}

/**
 * Function: ParseItem
 * -------------------------------
 * Code which parses the contents of a single <item> node within an RSS/XML feed.
 * At the moment this function is called, we're to assume that the <item> tag was just
 * read and that the streamtokenizer is currently pointing to everything else, as with:
 *   
 *      <title>Carrie Underwood takes American Idol Crown</title>
 *      <description>Oklahoma farm girl beats out Alabama rocker Bo Bice and 100,000 other contestants to win competition.</description>
 *      <link>http://www.nytimes.com/frontpagenews/2841028302.html</link>
 *   </item>
 *
 * ProcessSingleNewsItem parses everything up through and including the </item>, storing the title, link, and article
 * description in local buffers long enough so that the online new article identified by the link can itself be parsed
 * and indexed.  We don't rely on <title>, <link>, and <description> coming in any particular order.  We do asssume that
 * the link field exists (although we can certainly proceed if the title and article descrption are missing.)  There
 * are often other tags inside an item, but we ignore them.
 */

static const char *const kItemEndTag = "</item>";
static const char *const kTitleTagPrefix = "<title";
static const char *const kDescriptionTagPrefix = "<description";
static const char *const kLinkTagPrefix = "<link";
static bool ParseItem(streamtokenizer *st, article_t *article)
{
  char htmlTag[1024];
  article->title[0] = article->desc[0] = article->url[0] = '\0';
  
  // step through the tags inside an <item> section.  Pull out the Titles, Descriptions and Links. 
  while (GetNextTag(st, htmlTag, sizeof(htmlTag)) && (strcasecmp(htmlTag, kItemEndTag) != 0)) {
    
    if (strncasecmp(htmlTag, kTitleTagPrefix, strlen(kTitleTagPrefix)) == 0) 
      ExtractElement(st, htmlTag, article->title, sizeof(article->title));
    
    if (strncasecmp(htmlTag, kDescriptionTagPrefix, strlen(kDescriptionTagPrefix)) == 0) 
      ExtractElement(st, htmlTag, article->desc, sizeof(article->desc));

    if (strncasecmp(htmlTag, kLinkTagPrefix, strlen(kLinkTagPrefix)) == 0) 
      ExtractElement(st, htmlTag, article->url, sizeof(article->url));
  }
  // if URL or title are empty, return false. 
  if( article->url[0] == '\0' || article->title[0] == '\0' ) {
    printf("something is wrong with article's title: \"%s\"", article->title);
    return false; 
  }
  return true; 
}

/**
 * Function: ExtractElement
 * ------------------------
 * Potentially pulls text from the stream up through and including the matching end tag.  It assumes that
 * the most recently extracted HTML tag resides in the buffer addressed by htmlTag.  The implementation
 * populates the specified data buffer with all of the text up to but not including the opening '<' of the
 * closing tag, and then skips over all of the closing tag as irrelevant.  Assuming for illustration purposes
 * that htmlTag addresses a buffer containing "<description" followed by other text, these three scanarios are
 * handled:
 *
 *    Normal Situation:     <description>http://some.server.com/someRelativePath.html</description>
 *    Uncommon Situation:   <description></description>
 *    Uncommon Situation:   <description/>
 *
 * In each of the second and third scenarios, the document has omitted the data.  This is not uncommon
 * for the description data to be missing, so we need to cover all three scenarious (I've actually seen all three.)
 * It would be quite unusual for the title and/or link fields to be empty, but this handles those possibilities too.
 */
 
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength)
{
  assert(htmlTag[strlen(htmlTag) - 1] == '>');
  if (htmlTag[strlen(htmlTag) - 2] == '/') return;    // e.g. <description/> would state that a description is not being supplied

  // run through printed text (spaces and punctuation allowed, <[{}]> not) until we see "/title" "/description" or "/link"  
  // Or until we see a token that's > 10 characters long, which we assume to be a valid description. 
  while( STNextTokenUsingDifferentDelimiters(st, dataBuffer, bufferLength, "<>[]{}") ) {
    // if we encounter the close tag, we can stop looking, and zero out the buffer. 
    if (strcmp(dataBuffer, "/title") == 0 || strcmp(dataBuffer, "/description") == 0 || strcmp(dataBuffer, "/link") == 0) {
      strcpy(dataBuffer, "");
      break;
    }
    // if we find some significant amount of text, we can stop looking and call it good. 
    if (strlen(dataBuffer) > 9) break;
  }

  RemoveEscapeCharacters(dataBuffer);
  
  STSkipUntil(st, ">");
  STSkipOver(st, ">");
}

static void DownloadArticle( int article_index, domain_t *domain, curlconnection_t *connection ) {
  // we have already validated that this article is not a repeat. 
  article_t *article = (article_t*)VectorNth(&domain->articles_vector, article_index);
  mstreamtokenizer_t *mst = &article->mst;

  // we still need to initialize the memstream tokenizer. The article struct contains an mst object. 
  MSTNew(mst, kTextDelimiters, false); 
  //streamtokenizer *st = &mst->st;

  // pull the article from the interwebs. 
  if(CurlConnectionFetch(article->url, mst->stream, connection) != CURLE_OK) {
    printf("Could not get article url:%s\n", article->url);
    return;
  }
  printf("  downloaded: %s\n", article->title);

}

/**
 * Function: ProcessArticle
 * ---------------------
 * Parses the specified article, skipping over all HTML tags, and counts the numbers
 * of well-formed words that could potentially serve as keys in the set of indices.
 * Once the full article has been scanned, the number of well-formed words is
 * printed, and the longest well-formed word we encountered along the way
 * is printed as well.
 *
 * This is really a placeholder implementation for what will ultimately be
 * code that indexes the specified content.
 */

static void ProcessArticle( article_t *article, search_db_t *db )
{
  streamtokenizer *st = &article->mst.st;

  HashSetEnter(&db->articles, article); // makes its own copy. 

  int numWords = 0;
  char word[1024];
  char longestWord[1024] = {'\0'};

  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); // in html-utls.h
    } 
    else {
      RemoveEscapeCharacters(word);
      if ( WordIsWellFormed(word)) {

        RecordOccurrance(word, article, db);  // This is where we put it into the database. 
        numWords++;
        if (strlen(word) > strlen(longestWord))
	        strcpy(longestWord, word);
      }
    }
  }

  char title_substring[81];
  strncpy(title_substring, article->title, 80);
  title_substring[80] = '\0';

  //printf("    %s  Words: %d \n", title_substring, numWords);
}

/** 
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 */

static void QueryIndices(search_db_t *db)
{
  char response[1024];
  while (true) {
    printf("Please enter a single query term [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(response, db);
  }
}

/** 
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 */

static void ProcessResponse(const char *word, search_db_t *db)
{
  if (WordIsWellFormed(word)) {

    //Does word exist in words? 
    PrintArticles( word, db);

  } else {
    printf("\tWe won't be allowing words like \"%s\" into our set of indices.\n", word);
  }
}

/**
 * Predicate Function: WordIsWellFormed
 * ------------------------------------
 * Before we allow a word to be inserted into our map
 * of indices, we'd like to confirm that it's a good search term.
 * One could generalize this function to allow different criteria, but
 * this version hard codes the requirement that a word begin with 
 * a letter of the alphabet and that all letters are either letters, numbers,
 * or the '-' character.  
 */

static bool WordIsWellFormed(const char *word)
{
  int i, k;
  
  if (strlen(word) == 0) return false;  // this was true.  Not sure why an empty string in considered well-formed. 

  if (!isalpha((int) word[0])) return false;  // First character must be a letter.

  
  k = 0;
  for (i = 1; i < strlen(word); i++)
  {
    // there must not be more than one hyphen. 
    if (( word[i] == '-') && ++k > 1 ) return false;
    // letters after the first must all be either letters, numbers, or hyphens. 
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 
  }


  return true;
}
