#include "multitable.h"

// used exclusively by MultiTableDispose, being passed to HashSetMap() to dispose of the vectors in the 
// key/vector pairs.  
void HSElemFree( void *hs_elem_addr, void *key_size_ptr)
{
    int key_size = *(int*)key_size_ptr;
    // When called, hs_elem_addr will have the address of the hashset elements, which are key/vector pairs. 
    // the key will go away when we free the hashset, but the vector header points to a vector body.  
    // We need to call VectorDispose() to take care of that. We were passed the key size as a void*.
    VectorDispose( (vector*)( (char*)hs_elem_addr + key_size ) );
}

void MultiTableNew(multitable *mt, int keySizeBytes, int valueSizeBytes, int numBuckets, 
            MultiTableHashFunction hashFn, MultiTableCompareFunction compareFn, VectorFreeFunction valueFreeFn) {
    
    HashSetNew( &mt->mappings, keySizeBytes + sizeof(vector), numBuckets, hashFn, compareFn, NULL );
    mt->keySize = keySizeBytes;
    mt->valueSize = valueSizeBytes;
    mt->valueFreeFn = valueFreeFn;
}

void MultiTableDispose(multitable *mt) {
    HashSetMap(&mt->mappings, HSElemFree, &mt->keySize );

    HashSetDispose(&mt->mappings);

}

/**
 * First searches for the key in the hashset.  If it's found, we find the address of the vector (behind the key)
 * and append an element to that vector. 
 * 
 * If the key doesn't exist in the hashset yet, we have to build a new key-vector_head pair, then 
 * call HashSetEnter to put it into the hashset before we can finally append a value to the newly-minted vector. 
 */

void MultiTableEnter(multitable *mt, const void *keyAddr, const void *valueAddr) {
    
    // Whether the key is in the table or not, we'll need to VectorAppend. So we need the address of the vector. 
    vector* valuesAddr;

    // Search for the key in the hashset.  
    void* pairAddr = HashSetLookup( &mt->mappings, keyAddr); // the keyAddr is also the address of just the key. 

    if (pairAddr == NULL) { 
        // The key doesn't exist in the hashset, so we need to build a new key/vector-head pair.
        // We'll start by allocating some space for building that new pair. 
        void* pairAddr = malloc( mt->keySize + sizeof(vector)); //  Let's not forget to free this when we're done copying. 
        // then we find the space in memory set aside for the new vector
        vector* valuesAddr = (vector*)( (char*)pairAddr + mt->keySize );

        // now we initialize that to be a new empty vector.  That takes care of the vector part.
        VectorNew(valuesAddr, mt->valueSize, mt->valueFreeFn, 0);
        // still need the key.  Let's copy it in from the arguments. 
        memcpy( pairAddr, keyAddr, mt->keySize);

        // finally, we put the new painstakingly constructed pair into the Hashset.  
        HashSetEnter( &mt->mappings, pairAddr );

        // Hashset makes its own memcopy of the pair, so after HashSetEnter, we need to free it. 
        free(pairAddr);
    }

    // if we did find the key, there is already a vector of values, so we should initialize valuesAddr to that. 
    else {
        valuesAddr = (vector*)( (char*)pairAddr + mt->keySize );
    }

    // At this point, we have either added a new pair or have found the key.  In either case we still need to append the value. 
    VectorAppend(valuesAddr, valuesAddr);

}

typedef struct {
    int keyBytes;
    void *mtAuxData;
    MultiTableMapFunction mtMapFn;

} hashset_aux_struct;

static void HashsetMapper( void* elemAddr, void* auxData) {
    // this elemAddr is the address of a key/values-vector pair.  Hashset gives me this, I can't make it anything else. 
    // but I can use auxData to give myself more information (like where, relative to the elemAddr, the vector address is)

    // so let's unpack the suuper-handy auxData. 
    hashset_aux_struct *aux = (hashset_aux_struct*)auxData;

    void *value;

    // first let's pull out the values address
    vector *valuesAddr = (vector*)( (char*)elemAddr + aux->keyBytes);

    for( int values_i = 0; values_i < VectorLength( valuesAddr) ; values_i++) {
        // let's get the value. 
        value = VectorNth(valuesAddr, values_i);

        // Almost there.  I need to pass this to the MultiTableMapFunction. But wait, what MultiTableMapFunction???
        aux->mtMapFn( elemAddr, value, aux->mtAuxData);
    }
}

void MultiTableMap(multitable *mt, MultiTableMapFunction mapFn, void *auxData) {

    hashset_aux_struct hashset_aux;
    
    hashset_aux.keyBytes = mt->keySize;
    hashset_aux.mtAuxData = auxData;
    hashset_aux.mtMapFn = mapFn;

    HashSetMap( &mt->mappings, HashsetMapper, &hashset_aux);
    
}

// all finished at 11:12.  Total for this section handout: 11:12 - 5:38 = 5h, 34min

//chico-mendes-the-brazilian-environmental-activist-killed-for-trying-to-save-the-amazon
