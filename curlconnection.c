
#include <stdlib.h>     // strdup() open_memstream() free()
#include <assert.h>
#include "curlconnection.h"


FILE* open_memstream(char **ptr, size_t *sizeloc);

// Initializes a curl connection handle to be used in fetching resources.
// this does not call global init function. 
void CurlConnectionNew( curlconnection *cc, const char *url ) {

    cc->url = strdup( url );
    assert(cc->url);

    cc->mstream = open_memstream( &cc->mbuffer, &cc->stream_len);

    cc->curl_handle = curl_easy_init();
    cc->error = CURLE_OK;
    cc->error_str = "Fetch not called yet";

    // This line tells the curl library to put received input into the provided stream
    curl_easy_setopt(cc->curl_handle, CURLOPT_WRITEDATA, cc->mstream);
    // Here we stop trying if we get an error. 
    curl_easy_setopt(cc->curl_handle, CURLOPT_FAILONERROR, 1L);
    // If the server replies with a header with code 303/304 curl will follow the redirects.
    curl_easy_setopt(cc->curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    // Now we put in the url. 
    curl_easy_setopt(cc->curl_handle, CURLOPT_URL, url);
}

void CurlConnectionSetURL(curlconnection *cc, const char *url) {
    free(cc->url);
    cc->url = strdup(url);
    curl_easy_setopt(cc->curl_handle, CURLOPT_URL, url);
}

// Fetches the resource presumably stored in ConnectionNew or SetURL, 
// stores it in the stream, which is flushed so it is ready to read. 
int CurlConnectionFetch( curlconnection *cc ) {
    
    cc->error = curl_easy_perform(cc->curl_handle);
    cc->error_str = curl_easy_strerror(cc->error);
    fflush(cc->mstream);
    return cc->error;
}

// Performs cleanup functions. 
void CurlConnectionDispose( curlconnection *cc) {
    free(cc->url);
    fclose(cc->mstream);
    free(cc->mbuffer);
    curl_easy_cleanup(cc->curl_handle);

}
