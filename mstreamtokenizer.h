
#ifndef __MSTREAMTOKENIZER_H
#define __MSTREAMTOKENIZER_H

#include <stdio.h>
#include <stdlib.h>
#include "streamtokenizer.h"
#include "bool.h"

// tmstream_t is a Tokenized Memory stream. 
// it provides a streamtokenizer that is backed by a memstream open_memstream().
typedef struct {
  FILE *stream; 
  char *buffer;
  size_t length;
  streamtokenizer st;
} mstreamtokenizer_t;

void MSTNew( mstreamtokenizer_t *mst, const char *delimiters, bool discard_delim );
void MSTDispose( mstreamtokenizer_t *mst);

inline bool MSTNextToken(mstreamtokenizer_t *mst, char buffer[], int bufferLength) {
  return STNextToken( &mst->st,  buffer,  bufferLength);
}

inline bool MSTNextTokenUsingDifferentDelimiters(
  mstreamtokenizer_t *mst, char buffer[], int bufferLength, const char *delimiters) 
{
  return STNextTokenUsingDifferentDelimiters(&mst->st, buffer, bufferLength, delimiters );
}

inline int MSTSkipOver(mstreamtokenizer_t *mst, const char *skipSet) {
  return STSkipOver(&mst->st, skipSet);
}


inline int MSTSkipUntil(mstreamtokenizer_t *mst, const char *skipUntilSet) {
  return STSkipUntil(&mst->st, skipUntilSet);
}

#endif