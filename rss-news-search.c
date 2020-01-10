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


static void Welcome(const char *welcomeTextFileName);
static void LoadStopList(hashset *stop_list);
static void BuildIndices(const char *feedsFileName, search_db *db);
static void ProcessFeed(const char *remoteDocumentName, curlconnection_t *connection, search_db *db);
static bool GetNextItemTag(streamtokenizer *st);
static bool ParseItem(streamtokenizer *st, article_t *article );
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength);
static void ProcessArticle(article_t *article, curlconnection_t *connection, search_db *db);
static void QueryIndices();
static void ProcessResponse(const char *word);
static bool WordIsWellFormed(const char *word);


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
static const char *const kDefaultFeedsFile = "./data/rss-feeds-tiny.txt";


int main(int argc, char **argv)
{
  search_db db;   // the database. This will be passed down the function hierarchy.
  clock_t start, finish;
  time_t t1, t2;

  InitDatabase(&db);
  
  Welcome(kWelcomeTextFile);

  LoadStopList(&db.stop_list);

  start = clock();
  t1 = time(NULL);
  BuildIndices((argc == 1) ? kDefaultFeedsFile : argv[1], &db);  // runs only once. 
  finish = clock();
  t2 = time(NULL);
  printf("that took %ld cycles and %f seconds.\n", finish-start, difftime(t2, t1));

  QueryIndices();  

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

static void BuildIndices(const char *feedsFileName, search_db *db )
{
  curl_global_init(CURL_GLOBAL_SSL);  // Run once.  BuildIndices runs once for life of program. 

  curlconnection_t connection;    // this will be the only connection.  It will be shared throughtout.
  CurlConnectionNew(&connection);

  
  


  // This is an actual file, so we use the straight streamtokenizer, 
  // not mstreamtokenizer (memory stream version)
  FILE *infile;
  streamtokenizer st;
  char remoteFileName[1024];
  infile = fopen(feedsFileName, "r");
  assert(infile != NULL);
  STNew(&st, infile, kNewLineDelimiters, true);

  while (STSkipUntil(&st, ":") != EOF) { // ignore everything up to the first selicolon of the line
    STSkipOver(&st, ": ");		 // now ignore the semicolon and any whitespace directly after it
    STNextToken(&st, remoteFileName, sizeof(remoteFileName));   
    ProcessFeed(remoteFileName, &connection, db);
  }
  
  STDispose(&st);
  fclose(infile);
  printf("\n");

  CurlConnectionDispose(&connection);
  curl_global_cleanup();  // once for life of program. 
}


/**
 * Function: ProcessFeed
 * ---------------------
 * ProcessFeed locates the specified RSS document, and if a (possibly redirected) connection to that remote
 * document can be established, then reads the feed.  
 * 
 * Steps though the data of what is assumed to be an RSS feed identifying the titles and
 * URLs of online news articles.  Check out "datafiles/sample-rss-feed.txt" for an idea of what an
 * RSS feed from the www.nytimes.com (or anything other server that syndicates is stories).
 *
 * ProcessFeed views a typical RSS feed as a sequence of "items", where each item is detailed
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
 * ProcessFeed reads and discards all characters up through the opening <item> tag (discarding the <item> tag
 * as well, because once it's read and indentified, it's been pulled,) and then hands the state of the stream to
 * ParseItem(), which handles the job of pulling and analyzing everything up through and including the </item>
 * tag. ProcessFeed processes the entire RSS feed and repeatedly advancing to the next <item> tag and then allowing
 * ParseItem() to process everything up until </item>.
 */

static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";
static void ProcessFeed(const char *remoteDocumentName, curlconnection_t *connection, search_db *db)
{
  article_t article;  // this is all local.  Homegrown. 4kb on the stack.
  
  mstreamtokenizer_t mst;
  MSTNew(&mst, kTextDelimiters, false);

  // in next line, passing in mst.stream feels a little dirty. 
  if ( CurlConnectionFetch(remoteDocumentName, mst.stream, connection ) != CURLE_OK ) {
    printf("Problem connecting to: \n%s\nError: %s\n", remoteDocumentName, connection->error_str);
    return;
  }

  while (GetNextItemTag(&mst.st)) { // if true is returned, assume <item ...> was just read and pulled from the data stream
    if( ParseItem(&mst.st, &article ) )
      ProcessArticle(&article, connection, db);
    else printf("Could not read article link from RSS.\n");
  }
  MSTDispose(&mst);
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
 * Function: ProcessSingleNewsItem
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

  //printf("%s\n", article->url);
  
  return (article->url[0] != '\0'); // if URL is empty, return false.  Otherwise true. 
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
    if (strlen(dataBuffer) > 15) break;
  }

  RemoveEscapeCharacters(dataBuffer);
  
  STSkipUntil(st, ">");
  STSkipOver(st, ">");
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

static void ProcessArticle( article_t *article, curlconnection_t *connection, search_db *db )
{

  mstreamtokenizer_t mst;
  MSTNew(&mst, kTextDelimiters, false); 
  streamtokenizer *st = &mst.st;
  
  if(CurlConnectionFetch(article->url, mst.stream, connection) != CURLE_OK) {
    printf("Could not get article url:\n");
    return;
  }

  int numWords = 0;
  char word[1024];
  char longestWord[1024] = {'\0'};

  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); // in html-utls.h
    } else {
      RemoveEscapeCharacters(word);
      if (WordIsWellFormed(word)) {
        numWords++;
        if (strlen(word) > strlen(longestWord))
	        strcpy(longestWord, word);
      }
    }
  }

  printf("\tWe counted %d well-formed words [including duplicates].\n", numWords);
  printf("\tThe longest word scanned was \"%s\".", longestWord);
  if (strlen(longestWord) >= 15 && (strchr(longestWord, '-') == NULL)) 
    printf(" [Ooooo... long word!]");
  printf("\n");

  MSTDispose(&mst);
}

/** 
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 */

static void QueryIndices()
{
  char response[1024];
  while (true) {
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(response);
  }
}

/** 
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 */

static void ProcessResponse(const char *word)
{
  if (WordIsWellFormed(word)) {
    printf("\tWell, we don't have the database mapping words to online news articles yet, but if we DID have\n");
    printf("\tour hashset of indices, we'd list all of the articles containing \"%s\".\n", word);
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
  int i;
  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  for (i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}
