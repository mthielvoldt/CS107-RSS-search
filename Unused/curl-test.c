#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

 
FILE* open_memstream(char **ptr, size_t *sizeloc);
 
int main(int argc, char *argv[])
{
    // We're going to download information, so we need a place to put it. 
    // Rather than writing to an actual file, let's just write to a memory stream and pretend 
    // it is a file.  
    FILE *memstream;   // declare a "file" pointer.  FILE is typedef'd by stdio.h
    char *membuffer_p; 
    size_t memstream_size; 

    // This awesome function initializes a memory stream that automatically grows the allocated buffer
    // as needed when it is written to.  This is considered an output stream.  
    memstream = open_memstream( &membuffer_p, &memstream_size);

    // If I were to use an actual file, I would open it for writing as follows.  
    //memfile_p = fopen(argv[2], "wb");  // "wb" means write-mode and binary-mode.  This mode is necessary. 


    // integer for capturing the state of the curl read.  Tells me if download is successful or not. 
    CURLcode result;
    // this is automatically called with assumed params if I don't call it. Better style to do this here. 
    curl_global_init(CURL_GLOBAL_SSL);
    
    // Define a pointer to a curl handle. 
    CURL *curl;
    // initialize a new curl handle.  The handle is responsible for networking ops. 
    curl = curl_easy_init();


    // In next 3 lines, we configure the handle by setting the handle options. 
    // this option specifies the URL. 
    curl_easy_setopt(curl, CURLOPT_URL, argv[1]);
    // add the file pointer to the curl handle so it knows where to put the incoming data.
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, memstream);
    // curl library doesn't consider sttp errors as real errors
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    // Curl handle is now fully config'd. 
    // Initiate the download.  This download happens synchronously, so program will pause here while download happens. 
    result = curl_easy_perform(curl);

    // Now we dispose the curl handle. 
    curl_easy_cleanup(curl);
    // And dispose the globals as well. 
    curl_global_cleanup();

    // let's output what's in the stream.
    fflush(memstream);      // resets us ot the beginning of the stream for reading. 
    printf("%s", membuffer_p);
    fclose(memstream);      // this updates membuffer_p to now point to the first element in the buffer. 
    
    // after closing, we have to free the buffer.  fclose() does not do this because it normally handles files. 
    free(membuffer_p);

    if(result == CURLE_OK) {
        printf("download successful\n");
        return EXIT_SUCCESS;                //EXIT_SUCCESS defined in stdlib.h
    }
    else {
        printf("ERROR: %s\n", curl_easy_strerror(result));
        return EXIT_FAILURE;
    }
}