#include "pamu.h"

#include <endian.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* * * * * * * * * * * * * * * * * * * * * * * * *\
 * CAUTION                                       *
 * * * * * * * * * * * * * * * * * * * * * * * * *
 * All methods with a _ prefix use outer         *
 * addresses, which point to the size indicators *
\* * * * * * * * * * * * * * * * * * * * * * * * */

#define  PAMU_KEYWORD         "PAMU"
#define  PAMU_KEYWORD_LEN     4

#define  PAMU_INTERNAL_FLAG_FREE  ((int64_t)1<<63)
#define  PAMU_INTERNAL_FLAGS      (PAMU_INTERNAL_FLAG_FREE)

#define MAX(a,b) ((a)>(b)?(a):(b))

struct pamu_medium_stat {
  uint32_t flags;
  uint32_t headerSize;
  uint64_t mediumSize;
};

// Uses outer address
// Returns inner size in bytes
int64_t _pamu_find_sizeFlags(int fd, uint64_t addr) {
  uint64_t beSize;

  // Go to the requested address
  uint64_t _addr = lseek(fd, addr, SEEK_SET);
  if (addr != _addr) {
    return PAMU_ERR_READ_MALFORMED;
  }

  // Read the size|flags
  ssize_t rc = read(fd, &beSize, sizeof(uint64_t));
  if (rc != sizeof(uint64_t)) {
    return PAMU_ERR_READ_MALFORMED;
  }

  // And return the size without the flags
  return be64toh(beSize);
}

int64_t _pamu_find_size(int fd, uint64_t addr) {
  return _pamu_find_sizeFlags(fd, addr) & (~PAMU_INTERNAL_FLAGS);
}

int64_t _pamu_find_flags(int fd, uint64_t addr) {
  return _pamu_find_sizeFlags(fd, addr) & PAMU_INTERNAL_FLAGS;
}

// Uses outer addresses
// Returns limit = no block found
int64_t _pamu_find_free_block(int fd, int64_t start, int64_t limit, int64_t size) {
  int64_t current = start;
  int64_t csize   = 0;
  int64_t cflags  = 0;

  while(
    current &&
    (current < limit)
  ) {
    csize  = _pamu_find_size(fd, current);
    cflags = _pamu_find_flags(fd, current);

    // Return errors
    if (csize  < 0) return csize;  // Error = return error
    if (cflags & (~PAMU_INTERNAL_FLAGS)) return cflags; // Error = return error

    // If not free, go to the next entry
    if (!(cflags & PAMU_INTERNAL_FLAG_FREE)) {
      current += csize + (2 * sizeof(int64_t));
      continue;
    }

    // Here = free

    // If large enough: return this one
    if (csize >= size) return current;

    // Skip to the next free block
    if (lseek(fd, current + (2*sizeof(int64_t)), SEEK_SET) != current + (2*sizeof(int64_t))) {
      return PAMU_ERR_READ_MALFORMED;
    }
    if (read(fd, &current, sizeof(int64_t)) != sizeof(int64_t)) {
      return PAMU_ERR_READ_MALFORMED;
    }
  }

  if (
    (current == 0) &&
    (current >= limit)
  ) {
    return limit;
  }

  return current;
}

// Uses outer addresses
// Reads the current's size and returns the start of the next block
int64_t _pamu_find_next(int fd, int64_t current) {
  int64_t size = _pamu_find_size(fd, current);
  return current + size + (2 * sizeof(int64_t));
}

// Uses outer addresses
// Reads the previous' size and returns it's start
int64_t _pamu_find_previous(int fd, int64_t current, int64_t header_size) {
  int64_t addr = current - sizeof(int64_t);
  if (addr < header_size) return PAMU_ERR_OUT_OF_BOUNDS;
  int64_t size = _pamu_find_size(fd, addr);
  return current - size - (2 * sizeof(int64_t));
}

struct pamu_medium_stat * _pamu_medium_stat(int fd) {
  struct pamu_medium_stat * response = malloc(sizeof(struct pamu_medium_stat));

  // Verify keyword in header
  if (lseek(fd, 0, SEEK_SET)) {
    free(response);
    perror("lseek");
    return (void*)PAMU_ERR_SEEK;
  }
  char *keyBuf = calloc(1, PAMU_KEYWORD_LEN + 1);
  ssize_t rc = read(fd, keyBuf, PAMU_KEYWORD_LEN);
  if (rc != PAMU_KEYWORD_LEN) {
    free(response);
    free(keyBuf);
    return (void*)PAMU_ERR_READ_MALFORMED;
  }
  if (strcmp(PAMU_KEYWORD, keyBuf)) {
    free(response);
    free(keyBuf);
    return (void*)PAMU_ERR_MEDIUM_UNINITIALIZED;
  }

  // Read header size & flags
  uint32_t beFlaggedSize;
  rc = read(fd, &beFlaggedSize, sizeof(uint32_t));
  if (rc != sizeof(uint32_t)) {
    free(response);
    return (void*)PAMU_ERR_READ_MALFORMED;
  }
  uint32_t iFlaggedHeaderSize = be32toh(beFlaggedSize);
  response->flags      = iFlaggedHeaderSize &  PAMU_FLAGS;
  response->headerSize = iFlaggedHeaderSize & ~PAMU_FLAGS;

  // Find medium size
  response->mediumSize = lseek(fd, 0, SEEK_END);

  return response;
}


// Open/close functionality
int pamu_init(int fd, uint32_t flags) {

  // "calculate" header size
  uint32_t iHeaderSize =
    PAMU_KEYWORD_LEN  + // Keyword
    sizeof(uint32_t ) + // Headersize + flags
    0;

  // "calculate" entry size
  uint32_t iEntrySize =
    sizeof(uint64_t) + // Start size indicator
    sizeof(uint64_t) + // Previous free pointer in empty records
    sizeof(uint64_t) + // Next free pointer in empty records
    sizeof(uint64_t) + // End size indicator
    0;

  // Fetch medium size
  uint64_t iMediumSize = lseek(fd, 0, SEEK_END);

  // If not dynamic: check if our header + 1 entry is going to fit
  if (
    (!(flags & PAMU_DYNAMIC)) &&
    ((iHeaderSize + iEntrySize) > iMediumSize)
  ) {
    return PAMU_ERR_MEDIUM_SIZE;
  }

  // Write "PAMU" keyword
  if (lseek(fd, 0, SEEK_SET)) {
    perror("lseek");
    return PAMU_ERR_SEEK;
  }
  write(fd, PAMU_KEYWORD, PAMU_KEYWORD_LEN);

  // Write flags | headersize uint32_t
  uint32_t beHeaderSize = htobe32(flags | iHeaderSize);
  write(fd, &beHeaderSize, sizeof(uint32_t));

  // Initialize medium as free blob
  int64_t mediumSize = lseek(fd, 0, SEEK_END);
  int64_t blobSize   = mediumSize - iHeaderSize - (2 * sizeof(int64_t));
  int64_t blobMarker = htobe64(blobSize | PAMU_INTERNAL_FLAG_FREE);
  int64_t zero       = 0;
  if (!(flags & PAMU_DYNAMIC)) {
    lseek(fd, iHeaderSize, SEEK_SET);
    write(fd, &blobMarker, sizeof(int64_t)); // Start marker
    write(fd, &zero      , sizeof(int64_t)); // Previous pointer
    write(fd, &zero      , sizeof(int64_t)); // Next Pointer
    lseek(fd, iHeaderSize + blobSize + sizeof(int64_t), SEEK_SET);
    write(fd, &blobMarker, sizeof(int64_t)); // End marker
  }

  return 0;
}

// Returns inner address or error
int64_t pamu_alloc(int fd, int64_t size) {
  if (size <= 0) return PAMU_ERR_NEGATIVE_SIZE;
  if (size < (2*sizeof(int64_t))) size = sizeof(int64_t);

  // Fetch info (or return error code)
  struct pamu_medium_stat *stat = _pamu_medium_stat(fd);
  if (stat < 0) return (int64_t)stat;

  // Find a pre-existing block with the correct size (or throw error)
  int64_t block = _pamu_find_free_block(fd, stat->headerSize, stat->mediumSize, size);

  // Error during finding
  if (block < 0) {
    free(stat);
    return block;
  }

  // Throw error if non-dynamic & not enough space
  if (
    (block + (2*sizeof(int64_t)) + size >= stat->mediumSize) &&
    (!(stat->flags & PAMU_DYNAMIC))
  ) {
    free(stat);
    return PAMU_ERR_MEDIUM_FULL;
  }

  // Here = got the space

  // Fetch or build block size
  int64_t blockSize = block == stat->mediumSize
    ? size
    : _pamu_find_size(fd, block);
  int64_t blockMarker = htobe64(blockSize | PAMU_INTERNAL_FLAG_FREE);

  // Split free block if large enough
  int64_t zero = 0;
  int64_t previousFree;
  int64_t nextFree;
  int64_t newFree;
  int64_t newFreeSize;
  int64_t newFreeSizeFlags;
  int64_t beBlock = htobe64(block);
  if ((blockSize - size) > (4 * sizeof(int64_t))) {
    lseek(fd, block + sizeof(int64_t), SEEK_SET);
    read(fd, &previousFree, sizeof(int64_t));
    read(fd, &nextFree, sizeof(int64_t));

    newFree          = htobe64( block     + size + (2 * sizeof(int64_t)));
    newFreeSize      =          blockSize - size - (2 * sizeof(int64_t)) ;
    newFreeSizeFlags = htobe64(newFreeSize | PAMU_INTERNAL_FLAG_FREE);

    // Update the current block
    blockSize   = size;
    blockMarker = htobe64(blockSize | PAMU_INTERNAL_FLAG_FREE);
    lseek(fd, block, SEEK_SET);
    write(fd, &blockMarker , sizeof(int64_t)); // Start marker
    write(fd, &previousFree, sizeof(int64_t)); // Previous free
    write(fd, &newFree     , sizeof(int64_t)); // Next/new free
    lseek(fd, block + size + sizeof(int64_t), SEEK_SET);
    write(fd, &blockMarker , sizeof(int64_t)); // End marker

    // Build new free block
    write(fd, &newFreeSizeFlags, sizeof(int64_t)); // Start marker
    write(fd, &beBlock         , sizeof(int64_t)); // Previous/current free
    write(fd, &nextFree        , sizeof(int64_t)); // Next free
    lseek(fd, be64toh(newFree) + newFreeSize + sizeof(int64_t), SEEK_SET);
    write(fd, &newFreeSizeFlags, sizeof(int64_t)); // End marker

    // Update next block to point it's previous to the new free
    if (nextFree) {
      lseek(fd, be64toh(nextFree) + sizeof(int64_t), SEEK_SET);
      write(fd, &newFree                           , sizeof(int64_t));
    }

    // No need to update previous free block in this step
  }

  // Grow medium in dynamic mode
  // Init as free block without prev/next
  if (
    (stat->flags & PAMU_DYNAMIC) &&
    (block == stat->mediumSize)
  ) {
    lseek(fd, block, SEEK_SET);
    write(fd, &blockMarker, sizeof(int64_t)); // Start marker
    write(fd, &zero       , sizeof(int64_t)); // Previous free
    write(fd, &zero       , sizeof(int64_t)); // Next free
    lseek(fd, block + blockSize + sizeof(int64_t), SEEK_SET);
    write(fd, &blockMarker, sizeof(int64_t)); // End marker
  }

  // We don't need the stat anymore
  free(stat);

  // Mark the current block as allocated & read previous/next free pointers
  blockMarker = htobe64(blockSize);
  lseek(fd, block, SEEK_SET);
  write(fd, &blockMarker, sizeof(int64_t));
  read(fd, &previousFree, sizeof(int64_t));
  read(fd, &nextFree    , sizeof(int64_t));
  lseek(fd, block + blockSize + sizeof(int64_t), SEEK_SET);
  write(fd, &blockMarker, sizeof(int64_t));

  // Update the previous free's next pointer
  if (previousFree) {
    lseek(fd, be64toh(previousFree) + (2*sizeof(int64_t)), SEEK_SET);
    write(fd, &nextFree, sizeof(int64_t));
  }

  // Update the next free's previous pointer
  if (nextFree) {
    lseek(fd, be64toh(nextFree) + (1*sizeof(int64_t)), SEEK_SET);
    write(fd, &previousFree, sizeof(int64_t));
  }

  // Return pointer to innards
  return block + sizeof(int64_t);
}

int pamu_free(int fd, int64_t addr) {

  // Fetch info (or return error code)
  struct pamu_medium_stat *stat = _pamu_medium_stat(fd);
  if (stat < 0) return (int64_t)stat;

  // Catch out-of-bounds
  if (
      (addr >= stat->mediumSize) ||
      (addr <  stat->headerSize)
  ) {
    return PAMU_ERR_OUT_OF_BOUNDS;
  }



  return 0;
}

int64_t pamu_size(int fd, int64_t addr) {
  return 0;
}

// Iteration, so clients can find a reference
int64_t pamu_next(int fd, int64_t addr) {
  return 0;
}
