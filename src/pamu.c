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

#define  PAMU_FREE            ((uint64_t)1<<63)
#define  PAMU_FLAGS_INTERNAL  (PAMU_FREE)

struct pamu_medium_stat {
  uint32_t flags;
  uint32_t headerSize;
  uint64_t managedBytes;
};

// Uses outer address
// Returns inner size
int64_t _pamu_find_size(int fd, uint64_t addr) {
  uint64_t beSize;
  ssize_t rc = read(fd, &beSize, sizeof(uint64_t));
  if (rc != sizeof(uint64_t)) {
    return PAMU_ERR_READ_MALFORMED;
  }
  return be64toh(beSize) & (~PAMU_FLAGS_INTERNAL);
}

// Uses outer addresses
// Returns limit = no block found
int64_t _pamu_find_free_block(int fd, int64_t start, int64_t limit, int64_t size) {
  int64_t current = start;
  int64_t csize    = 0;

  while(
    (current < limit)
  ) {
    csize = _pamu_find_size(fd, current);
    if (csize < 0) return csize; // Error = return error
    if (csize >= size) break;    // Large enough = done searching

    // Skip to the next entry
    current += csize + (2 * sizeof(int64_t));
  }

  return current;
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

  // Read managedBytes
  uint64_t beManagedBytes;
  rc = read(fd, &beManagedBytes, sizeof(uint64_t));
  if (rc != sizeof(uint64_t)) {
    free(response);
    return (void*)PAMU_ERR_READ_MALFORMED;
  }
  response->managedBytes = be64toh(beManagedBytes);

  return response;
}


// Open/close functionality
int pamu_init(int fd, uint32_t flags) {

  // "calculate" header size
  uint32_t iHeaderSize =
    PAMU_KEYWORD_LEN  + // Keyword
    sizeof(uint32_t ) + // Headersize + flags
    sizeof(uint64_t ) + // managedSpace
    0;

  // "calculate" entry size
  uint32_t iEntrySize =
    sizeof(uint64_t) + // Start size indicator
    sizeof(uint64_t) + // Next free pointer in empty records
    sizeof(uint64_t) + // End size indicator
    0;

  // Fetch medium size
  uint64_t iMediumSize = lseek(fd, 0, SEEK_END);

  // If not dynamic: check if our header + 1 entry is going to fit
  if (
    (!(flags & PAMU_DYNAMIC)) &&
    ((iHeaderSize + iEntrySize) < iMediumSize)
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

  // Write uint64_t managedSpace (same as headersize here)
  uint64_t beManagedSpace = htobe64((uint64_t)iHeaderSize);
  write(fd, &beManagedSpace, sizeof(uint64_t));

  return 0;
}

// Returns inner address or error
int64_t pamu_alloc(int fd, int64_t size) {
  if (size <= 0) {
    return PAMU_ERR_NEGATIVE_SIZE;
  }

  // Fetch info (or return error code)
  struct pamu_medium_stat *stat = _pamu_medium_stat(fd);
  if (stat < 0) return (int64_t)stat;

  // Find a pre-existing block with the correct size (or throw error)
  int64_t block = _pamu_find_free_block(fd, stat->headerSize, stat->managedBytes, size);

  // Error during finding
  if (block < 0) {
    free(stat);
    return block;
  }

  // Need to allocate more space
  if (block == stat->managedBytes) {
    if (stat->flags & PAMU_DYNAMIC) {
      // TODO: allocate space
    } else {
      // TODO: throw error
    }
    // DEBUG
    free(stat);
    return 0;
  }

  // Mark block as used
  int64_t blockSize = _pamu_find_size(fd, block);
  // TODO: lseek
  // TODO: write

  free(stat);
  return block + sizeof(int64_t);
}

int pamu_free(int fd, int64_t addr) {
  return 0;
}

int64_t pamu_size(int fd, int64_t addr) {
  return 0;
}

// Iteration, so clients can find a reference
int64_t pamu_next(int fd, int64_t addr) {
  return 0;
}
