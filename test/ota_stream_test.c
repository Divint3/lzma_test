/*
 * Streaming OTA LZMA decompression example.
 *
 * The OTA header used by the firmware supplied with this project is 16 bytes.
 * Bytes 8 through 11 contain the little-endian uncompressed firmware size.
 * The remaining bytes form a raw LZMA stream: five property bytes followed by
 * compressed data without an end marker.
 */

#include "easylzma/decompress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OTA_HEADER_SIZE 16u
#define INPUT_BUFFER_SIZE 1024u
#define FLASH_WRITE_CHUNK_SIZE 256u
#define INPUT_READ_CHUNK_SIZE 333u
#define DECODER_MEMORY_BUDGET (128u * 1024u)

struct input_context {
    FILE * file;
};

struct output_context {
    FILE * file;
    unsigned long long bytesWritten;
    size_t largestChunk;
};

struct allocation_context {
    size_t requestedBytes;
    size_t largestAllocation;
};

static void *
trackedAlloc(void * context, unsigned int size)
{
    struct allocation_context * allocation =
        (struct allocation_context *) context;

    allocation->requestedBytes += size;
    if (size > allocation->largestAllocation) {
        allocation->largestAllocation = size;
    }
    return malloc(size);
}

static void
trackedFree(void * context, void * pointer)
{
    (void) context;
    free(pointer);
}

static size_t
writeOutput(void * context, const void * buffer, size_t size)
{
    struct output_context * output = (struct output_context *) context;
    size_t written;

    /*
     * An MCU application should replace fwrite() with its flash page writer.
     * The buffer is only valid until this callback returns.
     */
    written = fwrite(buffer, 1, size, output->file);
    output->bytesWritten += written;
    if (size > output->largestChunk) {
        output->largestChunk = size;
    }
    return written;
}

static int
readInput(void * context, void * buffer, size_t * size)
{
    struct input_context * input = (struct input_context *) context;
    size_t requestSize = *size;
    size_t readSize;

    /* Keep reads irregular to exercise streaming rather than one-shot input. */
    if (requestSize > INPUT_READ_CHUNK_SIZE) {
        requestSize = INPUT_READ_CHUNK_SIZE;
    }
    readSize = fread(buffer, 1, requestSize, input->file);

    if (ferror(input->file)) {
        return 1;
    }

    *size = readSize;
    return 0;
}

static unsigned int
readLe32(const unsigned char * buffer)
{
    return ((unsigned int) buffer[0]) |
           ((unsigned int) buffer[1] << 8) |
           ((unsigned int) buffer[2] << 16) |
           ((unsigned int) buffer[3] << 24);
}

static void
usage(const char * program)
{
    fprintf(stderr, "Usage: %s <ota-firmware> <output-file>\n", program);
}

int
main(int argc, char ** argv)
{
    unsigned char otaHeader[OTA_HEADER_SIZE];
    unsigned long long uncompressedSize;
    elzma_decompress_handle handle = NULL;
    struct input_context input;
    struct output_context output;
    struct allocation_context allocation;
    int result;
    int exitCode = EXIT_FAILURE;

    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    memset(&allocation, 0, sizeof(allocation));

    input.file = fopen(argv[1], "rb");
    if (input.file == NULL) {
        fprintf(stderr, "Could not open OTA firmware: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (fread(otaHeader, 1, sizeof(otaHeader), input.file) != sizeof(otaHeader)) {
        fprintf(stderr, "OTA firmware is shorter than its 16-byte header\n");
        fclose(input.file);
        return EXIT_FAILURE;
    }

    uncompressedSize = readLe32(otaHeader + 8);
    if (uncompressedSize == 0) {
        fprintf(stderr, "OTA header contains an invalid uncompressed size\n");
        fclose(input.file);
        return EXIT_FAILURE;
    }

    output.file = fopen(argv[2], "wb");
    if (output.file == NULL) {
        fprintf(stderr, "Could not open output file: %s\n", argv[2]);
        fclose(input.file);
        return EXIT_FAILURE;
    }

    handle = elzma_decompress_alloc();
    if (handle == NULL) {
        fprintf(stderr, "Could not allocate decompression handle\n");
        goto cleanup;
    }

    elzma_decompress_set_allocation_callbacks(handle,
                                              trackedAlloc, &allocation,
                                              trackedFree, &allocation);

    result = elzma_decompress_config(handle, uncompressedSize,
                                     INPUT_BUFFER_SIZE,
                                     FLASH_WRITE_CHUNK_SIZE);
    if (result != ELZMA_E_OK) {
        fprintf(stderr, "Could not configure decompressor (%d)\n", result);
        goto cleanup;
    }

    result = elzma_decompress_run(handle, readInput, &input,
                                  writeOutput, &output, ELZMA_raw_lzma);
    if (result != ELZMA_E_OK) {
        fprintf(stderr, "Streaming decompression failed (%d)\n", result);
        goto cleanup;
    }

    if (output.bytesWritten != uncompressedSize) {
        fprintf(stderr, "Output size mismatch: got %llu, expected %llu\n",
                output.bytesWritten, uncompressedSize);
        goto cleanup;
    }

    if (allocation.requestedBytes > DECODER_MEMORY_BUDGET) {
        fprintf(stderr, "Decoder allocation exceeded budget: %lu > %u bytes\n",
                (unsigned long) allocation.requestedBytes,
                DECODER_MEMORY_BUDGET);
        goto cleanup;
    }

    printf("Streaming decompression passed: %llu bytes, "
           "largest output chunk %lu bytes, decoder allocations %lu bytes, "
           "largest block %lu bytes\n",
           output.bytesWritten, (unsigned long) output.largestChunk,
           (unsigned long) allocation.requestedBytes,
           (unsigned long) allocation.largestAllocation);
    exitCode = EXIT_SUCCESS;

cleanup:
    elzma_decompress_free(&handle);
    fclose(input.file);
    if (fclose(output.file) != 0 && exitCode == EXIT_SUCCESS) {
        fprintf(stderr, "Could not flush output file\n");
        exitCode = EXIT_FAILURE;
    }
    if (exitCode != EXIT_SUCCESS) {
        remove(argv[2]);
    }

    return exitCode;
}
