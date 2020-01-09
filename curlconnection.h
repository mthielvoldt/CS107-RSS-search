
#ifndef __curl_connection_
#define __curl_connection_


#include <stdio.h>
//#include <string.h>
#include <curl/curl.h>


typedef struct {
    char *url;
    FILE *mstream;       // memory stream (not a file)
    char *mbuffer;
    size_t stream_len;
    CURL *curl_handle;  
    CURLcode error;    // enum 
    const char *error_str;
} curlconnection;

char *strdup(const char *s);

// Initializes a curl connection handle to be used in fetching resources.
// this does not call global init function. 
void CurlConnectionNew( curlconnection *cc, const char *url );

// Re-sets the url in the curl struct.  
// Useful for getting different files from the same server because the connection
// does not have to be closed. 
void CurlConnectionSetURL(curlconnection *cc, const char *url);


// Sets the URL and fetches the resource, storing it in the stream.  
// The stream is flushed so it is ready to read. 
// Returns 0 for status okay.  Otherwise, returns status. 
int CurlConnectionFetch( curlconnection *cc );

// Performs cleanup functions. 
void CurlConnectionDispose( curlconnection *cc);

#endif