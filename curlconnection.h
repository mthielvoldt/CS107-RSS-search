
#ifndef __curl_connection_
#define __curl_connection_


#include <curl/curl.h>


typedef struct {
    CURL *curl_handle;  
    CURLcode error;    // enum 
    const char *error_str;
} curlconnection_t;

// Initializes a curl connection handle to be used in fetching resources.
// this does not call global init function. 
void CurlConnectionNew( curlconnection_t *cc );

//void CurlConnectionSetURL(curlconnection_t *cc, const char *url);
//void CurlConnectionSetStream( curlconnection_t *cc, FILE *stream);


// Sets the URL and fetches the resource, storing it in the stream.  
// The stream is flushed so it is ready to read. 
// Returns 0 for status okay.  Otherwise, returns status. 
int CurlConnectionFetch(const char *url, FILE *stream, curlconnection_t *cc );

// Performs cleanup functions. 
void CurlConnectionDispose( curlconnection_t *cc);

#endif