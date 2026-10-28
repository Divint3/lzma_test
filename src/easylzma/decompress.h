/*
 * Written in 2009 by Lloyd Hilaiel
 *
 * License
 * 
 * All the cruft you find here is public domain.  You don't have to credit
 * anyone to use this code, but my personal request is that you mention
 * Igor Pavlov for his hard, high quality work.
 *
 * easylzma/decompress.h - The API for LZMA decompression using easylzma
 */

#ifndef __EASYLZMADECOMPRESS_H__ 
#define __EASYLZMADECOMPRESS_H__ 

#include "easylzma/common.h"

#ifdef __cplusplus
extern "C" {
#endif    

/** an opaque handle to an lzma decompressor */
typedef struct _elzma_decompress_handle * elzma_decompress_handle;

/**
 * Allocate a handle to an LZMA decompressor object.
 */ 
elzma_decompress_handle EASYLZMA_API elzma_decompress_alloc();

/**
 * Configure streaming decompression.
 *
 * uncompressedSize is required for ELZMA_raw_lzma because raw LZMA streams
 * do not necessarily include an end marker or an uncompressed size.  It is
 * ignored for ELZMA_lzma and ELZMA_lzip.
 *
 * inputBufferSize and outputBufferSize control the maximum amount of
 * compressed and decompressed data held in RAM at once.  A value of zero
 * selects the 4096-byte default.  Decompressed data is passed to the output
 * callback immediately and is not retained by the decoder.
 */
int EASYLZMA_API elzma_decompress_config(
    elzma_decompress_handle hand,
    unsigned long long uncompressedSize,
    size_t inputBufferSize,
    size_t outputBufferSize);

/**
 * set allocation routines (optional, if not called malloc & free will
 * be used) 
 */ 
void EASYLZMA_API elzma_decompress_set_allocation_callbacks(
    elzma_decompress_handle hand,
    elzma_malloc mallocFunc, void * mallocFuncContext,
    elzma_free freeFunc, void * freeFuncContext);

/**
 * Free all data associated with an LZMA decompressor object.
 */ 
void EASYLZMA_API elzma_decompress_free(elzma_decompress_handle * hand);

/**
 * Perform decompression
 *
 * The output callback should persist each chunk before returning, for
 * example by writing it directly to flash.  Buffers passed to the callback
 * are reused after the callback returns.
 *
 * XXX: should the library automatically detect format by reading stream?
 *      currently it's based on data external to stream (such as extension
 *      or convention)
 */ 
int EASYLZMA_API elzma_decompress_run(
    elzma_decompress_handle hand,
    elzma_read_callback inputStream, void * inputContext,
    elzma_write_callback outputStream, void * outputContext,
    elzma_file_format format);


#ifdef __cplusplus
};
#endif    

#endif
