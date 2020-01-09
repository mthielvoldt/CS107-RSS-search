#include "mstreamtokenizer.h"

FILE* open_memstream(char **ptr, size_t *sizeloc);

void MSTNew( mstreamtokenizer_t *mst, const char *delimiters, bool discard_delim) {

    // first we must open a new memstream. 
    mst->stream = open_memstream(&mst->buffer, &mst->length);

    // now that we have a pointer to an open stream, we can pass that to a new streamtokenizer
    STNew( &mst->st, mst->stream, delimiters, discard_delim );
}

void MSTDispose( mstreamtokenizer_t *mst) {

    //  frees the strdup'd delimeters string. Nothing happens to the file.  It's still open. 
    STDispose(&mst->st);    

    fclose(mst->stream); // does not free the file.  Does reset buffer to the first element. 

    free(mst->buffer);  // fclosing (above) resets the buffer to first elem, so now we can free it. 
}