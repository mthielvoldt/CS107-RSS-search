
#include <stdlib.h>     // strdup() open_memstream() free()
#include <stdio.h>      
#include <assert.h>
#include "curlconnection.h"


// Initializes a curl connection handle to be used in fetching resources.
// this does not call global init function. 
void CurlConnectionNew( curlconnection_t *cc ) {

    cc->curl_handle = curl_easy_init();
    cc->error = CURLE_OK;
    cc->error_str = "Fetch not called yet";

    // Here we stop trying if we get an error. 
    curl_easy_setopt(cc->curl_handle, CURLOPT_FAILONERROR, 1L);
    // If the server replies with a header with code 303/304 curl will follow the redirects.
    curl_easy_setopt(cc->curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
}


// Fetches the specified URL.
// stores it in the stream, which is flushed so it is ready to read. 
int CurlConnectionFetch(const char *url, FILE *stream,  curlconnection_t *cc ) {
    assert(stream != NULL);
    
    // This line tells curl what stream to output the received data to. 
    curl_easy_setopt(cc->curl_handle, CURLOPT_WRITEDATA, stream);

    // Sets the url that Curl is going to fetch. 
    curl_easy_setopt(cc->curl_handle, CURLOPT_URL, url);
    
    cc->error = curl_easy_perform(cc->curl_handle);
    cc->error_str = curl_easy_strerror(cc->error);
    fflush(stream);
    return cc->error;
}

// Performs cleanup functions. 
void CurlConnectionDispose( curlconnection_t *cc) {
    curl_easy_cleanup(cc->curl_handle);
}
