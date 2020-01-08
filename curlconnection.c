
#include <assert.h>
#include "curlconnection.h"


FILE* open_memstream(char **ptr, size_t *sizeloc);

// Initializes a curl connection handle to be used in fetching resources.
// this does not call global init function. 
void CurlConnectionNew( CurlConnection* cc) {
    cc->mstream = open_memstream( &cc->mbuffer, &cc->log_length);
    cc->curl_handle = curl_easy_init();

    curl_easy_setopt(cc->curl_handle, CURLOPT_WRITEDATA, cc->mstream);
    curl_easy_setopt(cc->curl_handle, CURLOPT_FAILONERROR, 1l);

}

// Sets the URL and fetches the resource, storing it in the stream.  The stream is flushed so it is ready to read. 
int CurlConnectionFetch( CurlConnection* cc, char url[] ) {
    cc->url = strdup( url );
    assert(cc->url);

    curl_easy_setopt(cc->curl_handle, CURLOPT_URL, url);
    cc->result = curl_easy_perform(cc->curl_handle);
    fflush(cc->mstream);


}

// Performs cleanup functions. 
void CurlConnectionDispose( CurlConnection* cc) {
    free(cc->url);
    fclose(cc->mstream);
    free(cc->mbuffer);
    curl_easy_cleanup(cc->curl_handle);

}
