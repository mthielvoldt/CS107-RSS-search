
#ifndef __curl_connection_
#define __curl_connection_


#include <stdio.h>
#include <curl/curl.h>


typedef struct {
    char *url;
    FILE *mstream;       // memory stream (not a file)
    char *mbuffer;
    size_t log_length;
    CURL *curl_handle;  
    CURLcode result;    // enum 
} CurlConnection;

// Initializes a curl connection handle to be used in fetching resources.
// this does not call global init function. 
void CurlConnectionNew( CurlConnection* connection);

// Sets the URL and fetches the resource, storing it in the stream.  The stream is flushed so it is ready to read. 
int CurlConnectionFetch( CurlConnection* cc, char url[] );

// Performs cleanup functions. 
void CurlConnectionDispose( CurlConnection* cc);

#endif