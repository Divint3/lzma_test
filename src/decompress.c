/*
 * Written in 2009 by Lloyd Hilaiel
 *
 * License
 * 
 * All the cruft you find here is public domain.  You don't have to credit
 * anyone to use this code, but my personal request is that you mention
 * Igor Pavlov for his hard, high quality work.
 */

#include "easylzma/decompress.h"
#include "pavlov/LzmaDec.h"
#include "pavlov/7zCrc.h"
#include "common_internal.h"
#include "lzma_header.h"
#include "lzip_header.h"

#include <string.h>

#define ELZMA_DECOMPRESS_BUFSIZE_DEFAULT (1024 * 4)

/** an opaque handle to an lzma decompressor */
struct _elzma_decompress_handle {
    size_t inputBufferSize;
    size_t outputBufferSize;
    unsigned long long uncompressedSize;
    struct elzma_alloc_struct allocStruct;
};

static int
readExact(elzma_read_callback inputStream, void * inputContext,
          unsigned char * buffer, size_t size)
{
    size_t total = 0;

    while (total < size) {
        size_t amount = size - total;
        if (inputStream(inputContext, buffer + total, &amount) != 0) {
            return ELZMA_E_INPUT_ERROR;
        }
        if (amount == 0) {
            return ELZMA_E_INSUFFICIENT_INPUT;
        }
        if (amount > size - total) {
            return ELZMA_E_INPUT_ERROR;
        }
        total += amount;
    }

    return ELZMA_E_OK;
}

elzma_decompress_handle
elzma_decompress_alloc()
{
    elzma_decompress_handle hand = malloc(sizeof(struct _elzma_decompress_handle));
    if (hand == NULL) {
        return NULL;
    }

    memset((void *) hand, 0, sizeof(struct _elzma_decompress_handle));
    hand->inputBufferSize = ELZMA_DECOMPRESS_BUFSIZE_DEFAULT;
    hand->outputBufferSize = ELZMA_DECOMPRESS_BUFSIZE_DEFAULT;
    init_alloc_struct(&(hand->allocStruct), NULL, NULL, NULL, NULL);
    return hand;
}

int
elzma_decompress_config(elzma_decompress_handle hand,
                        unsigned long long uncompressedSize,
                        size_t inputBufferSize,
                        size_t outputBufferSize)
{
    if (hand == NULL) {
        return ELZMA_E_BAD_PARAMS;
    }

    hand->uncompressedSize = uncompressedSize;
    hand->inputBufferSize = inputBufferSize != 0
        ? inputBufferSize : ELZMA_DECOMPRESS_BUFSIZE_DEFAULT;
    hand->outputBufferSize = outputBufferSize != 0
        ? outputBufferSize : ELZMA_DECOMPRESS_BUFSIZE_DEFAULT;
    return ELZMA_E_OK;
}

void
elzma_decompress_set_allocation_callbacks(
    elzma_decompress_handle hand,
    elzma_malloc mallocFunc, void * mallocFuncContext,
    elzma_free freeFunc, void * freeFuncContext)
{
    if (hand) {
        init_alloc_struct(&(hand->allocStruct),
                          mallocFunc, mallocFuncContext,
                          freeFunc, freeFuncContext);
    }
}

void
elzma_decompress_free(elzma_decompress_handle * hand)
{
    if (hand && *hand) {
        free(*hand);
    }
    if (hand) {
        *hand = NULL;
    }
}

int
elzma_decompress_run(elzma_decompress_handle hand,
                     elzma_read_callback inputStream, void * inputContext,
                     elzma_write_callback outputStream, void * outputContext,
                     elzma_file_format format)
{
    unsigned long long totalRead = 0;
    unsigned int crc32 = CRC_INIT_VAL;
    unsigned char footerBuffer[12];
    CLzmaDec dec;
    unsigned int errorCode = ELZMA_E_OK;
    struct elzma_format_handler formatHandler;
    struct elzma_file_header h;
    struct elzma_file_footer f;
    unsigned char * headerBuffer = NULL;
    unsigned char * inputBuffer = NULL;
    unsigned char * outputBuffer = NULL;
    int decoderAllocated = 0;

    if (hand == NULL || inputStream == NULL || outputStream == NULL) {
        return ELZMA_E_BAD_PARAMS;
    }

    if (format == ELZMA_lzma) {
        initializeLZMAFormatHandler(&formatHandler);
    } else if (format == ELZMA_lzip) {
        CrcGenerateTable();
        initializeLZIPFormatHandler(&formatHandler);
    } else if (format == ELZMA_raw_lzma) {
        if (hand->uncompressedSize == 0) {
            return ELZMA_E_BAD_PARAMS;
        }
        initializeRawLZMAFormatHandler(&formatHandler);
    } else {
        return ELZMA_E_BAD_PARAMS;
    }

    f.crc32 = 0;
    f.uncompressedSize = 0;
    formatHandler.init_header(&h);
    memset((void *) footerBuffer, 0, sizeof(footerBuffer));
    memset((void *) &dec, 0, sizeof(dec));
    LzmaDec_Init(&dec);

    inputBuffer = hand->allocStruct.Alloc(&(hand->allocStruct),
                                          hand->inputBufferSize);
    outputBuffer = hand->allocStruct.Alloc(&(hand->allocStruct),
                                           hand->outputBufferSize);
    headerBuffer = hand->allocStruct.Alloc(&(hand->allocStruct),
                                           formatHandler.header_size);
    if (inputBuffer == NULL || outputBuffer == NULL || headerBuffer == NULL) {
        errorCode = ELZMA_E_DECOMPRESS_ERROR;
        goto decompressEnd;
    }

    errorCode = (unsigned int) readExact(inputStream, inputContext,
                                         headerBuffer,
                                         formatHandler.header_size);
    if (errorCode != ELZMA_E_OK) {
        goto decompressEnd;
    }

    if (formatHandler.parse_header(headerBuffer, &h) != 0) {
        errorCode = ELZMA_E_CORRUPT_HEADER;
        goto decompressEnd;
    }

    if (format == ELZMA_raw_lzma) {
        h.isStreamed = 0;
        h.uncompressedSize = hand->uncompressedSize;
    }

    {
        unsigned char propsBuffer[13];
        const unsigned char * properties = headerBuffer;

        if (format == ELZMA_lzip) {
            struct elzma_format_handler lzmaHandler;
            initializeLZMAFormatHandler(&lzmaHandler);
            lzmaHandler.serialize_header(propsBuffer, &h);
            properties = propsBuffer;
        }

        if (LzmaDec_Allocate(&dec, properties, 5,
                             (ISzAlloc *) &(hand->allocStruct)) != SZ_OK) {
            errorCode = ELZMA_E_DECOMPRESS_ERROR;
            goto decompressEnd;
        }
        decoderAllocated = 1;
    }

    {
        size_t inputSize = 0;
        size_t inputOffset = 0;

        for (;;) {
            size_t outputSize;
            size_t inputAmount;
            size_t compressedAmount;
            ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;

            if (!h.isStreamed) {
                if (totalRead > h.uncompressedSize) {
                    errorCode = ELZMA_E_SIZE_MISMATCH;
                    break;
                }
                if (totalRead == h.uncompressedSize) {
                    break;
                }
            }

            if (inputOffset >= inputSize) {
                inputSize = hand->inputBufferSize;
                if (inputStream(inputContext, inputBuffer, &inputSize) != 0) {
                    errorCode = ELZMA_E_INPUT_ERROR;
                    break;
                }
                if (inputSize == 0) {
                    errorCode = ELZMA_E_INSUFFICIENT_INPUT;
                    break;
                }
                if (inputSize > hand->inputBufferSize) {
                    errorCode = ELZMA_E_INPUT_ERROR;
                    break;
                }
                inputOffset = 0;
            }

            outputSize = hand->outputBufferSize;

            compressedAmount = inputSize - inputOffset;
            inputAmount = compressedAmount;

            if (LzmaDec_DecodeToBuf(
                    &dec, (Byte *) outputBuffer, &outputSize,
                    (Byte *) inputBuffer + inputOffset, &inputAmount,
                    LZMA_FINISH_ANY, &status) != SZ_OK)
            {
                errorCode = ELZMA_E_DECOMPRESS_ERROR;
                break;
            }

            inputOffset += inputAmount;

            if (!h.isStreamed &&
                outputSize > h.uncompressedSize - totalRead)
            {
                errorCode = ELZMA_E_SIZE_MISMATCH;
                break;
            }

            totalRead += outputSize;

            if (outputSize > 0) {
                size_t written;

                if (format == ELZMA_lzip) {
                    crc32 = CrcUpdate(crc32, outputBuffer, outputSize);
                }

                written = outputStream(outputContext, outputBuffer, outputSize);
                if (written != outputSize) {
                    errorCode = ELZMA_E_OUTPUT_ERROR;
                    break;
                }
            }

            if (!h.isStreamed && totalRead > h.uncompressedSize) {
                errorCode = ELZMA_E_SIZE_MISMATCH;
                break;
            }

            if (status == LZMA_STATUS_FINISHED_WITH_MARK) {
                if (formatHandler.footer_size > 0) {
                    size_t footerOffset = 0;

                    while (footerOffset < formatHandler.footer_size) {
                        size_t available = inputSize - inputOffset;
                        size_t amount;

                        if (available == 0) {
                            inputSize = hand->inputBufferSize;
                            if (inputStream(inputContext, inputBuffer,
                                            &inputSize) != 0)
                            {
                                errorCode = ELZMA_E_INPUT_ERROR;
                                break;
                            }
                            if (inputSize == 0) {
                                errorCode = ELZMA_E_INSUFFICIENT_INPUT;
                                break;
                            }
                            if (inputSize > hand->inputBufferSize) {
                                errorCode = ELZMA_E_INPUT_ERROR;
                                break;
                            }
                            inputOffset = 0;
                            available = inputSize;
                        }

                        amount = formatHandler.footer_size - footerOffset;
                        if (amount > available) {
                            amount = available;
                        }
                        memcpy(footerBuffer + footerOffset,
                               inputBuffer + inputOffset, amount);
                        footerOffset += amount;
                        inputOffset += amount;
                    }

                    if (errorCode != ELZMA_E_OK) {
                        break;
                    }
                    if (formatHandler.parse_footer == NULL ||
                        formatHandler.parse_footer(footerBuffer, &f) != 0)
                    {
                        errorCode = ELZMA_E_CORRUPT_HEADER;
                        break;
                    }
                }
                break;
            }

            if (inputAmount == 0 && outputSize == 0) {
                errorCode = ELZMA_E_DECOMPRESS_ERROR;
                break;
            }
        }
    }

    if (errorCode == ELZMA_E_OK) {
        if (formatHandler.footer_size > 0) {
            crc32 ^= 0xFFFFFFFF;
            if (f.crc32 != crc32) {
                errorCode = ELZMA_E_CRC32_MISMATCH;
            } else if (f.uncompressedSize != totalRead) {
                errorCode = ELZMA_E_SIZE_MISMATCH;
            }
        } else if (!h.isStreamed && h.uncompressedSize != totalRead) {
            errorCode = ELZMA_E_SIZE_MISMATCH;
        }
    }

decompressEnd:
    if (decoderAllocated) {
        LzmaDec_Free(&dec, (ISzAlloc *) &(hand->allocStruct));
    }
    if (headerBuffer != NULL) {
        hand->allocStruct.Free(&(hand->allocStruct), headerBuffer);
    }
    if (inputBuffer != NULL) {
        hand->allocStruct.Free(&(hand->allocStruct), inputBuffer);
    }
    if (outputBuffer != NULL) {
        hand->allocStruct.Free(&(hand->allocStruct), outputBuffer);
    }

    return (int) errorCode;
}
