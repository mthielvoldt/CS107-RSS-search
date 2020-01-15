#include <string.h>
#include <stdlib.h>
#include "hashset.h"

typedef struct {
    hashset mappings;
    int keySize;
    int valueSize;
    VectorFreeFunction valueFreeFn;
} multitable;

typedef int (*MultiTableHashFunction)(const void *keyAddr, int numBuckets);
typedef int (*MultiTableCompareFunction)(const void *keyAddr1, const void *keyAddr2);
typedef void (*MultiTableMapFunction)(void *keyAddr, void *valueAddr, void *auxData);

void MultiTableNew(multitable *mt, int keySizeBytes, int valueSizeBytes, int numBuckets, 
            MultiTableHashFunction hashFn, MultiTableCompareFunction compareFn, VectorFreeFunction valueFreeFn);

void MultiTableDispose(multitable *mt);

void MultiTableEnter(multitable *mt, const void *keyAddr, const void *valueAddr);

void MultiTableMap(multitable *mt, MultiTableMapFunction mapFn, void *auxData);

