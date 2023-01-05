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

struct pamu_medium_stat {
  uint32_t flags;
  uint32_t headerSize;
  uint64_t managedBytes;
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
    (current < limit)
  ) {
    csize  = _pamu_find_size(fd, current);
    cflags = _pamu_find_flags(fd, current);

    // Return errors
    if (csize  < 0) return csize;  // Error = return error
    if (cflags < 0) return cflags; // Error = return error

    // If not free, go to the next entry
    if (!(cflags & PAMU_INTERNAL_FLAG_FREE)) {
      current += csize + (2 * sizeof(int64_t));
      continue;
    }

    // Here = free

    // If large enough: return this one
    if (csize >= size) return current;

    // Skip to the next free block
    if (lseek(fd, current + sizeof(int64_t), SEEK_SET) != current + sizeof(int64_t)) {
      return PAMU_ERR_READ_MALFORMED;
    }
    if (read(fd, &current, sizeof(int64_t)) != sizeof(int64_t)) {
      return PAMU_ERR_READ_MALFORMED;
    }

    // 0 = last free block, return the limit
    return limit;
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

  // Write uint64_t managedSpace (same as headersize here)
  uint64_t beManagedSpace = htobe64((uint64_t)iHeaderSize);
  write(fd, &beManagedSpace, sizeof(uint64_t));

  return 0;
}

// Returns inner address or error
int64_t pamu_alloc(int fd, int64_t size) {
  if (size <= 0) return PAMU_ERR_NEGATIVE_SIZE;
  if (size < sizeof(int64_t)) size = sizeof(int64_t);

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

  // Throw error if non-dynamic & no space available
  if (
    (block == stat->managedBytes) &&
    (!(stat->flags & PAMU_DYNAMIC))
  ) {
    free(stat);
    return PAMU_ERR_MEDIUM_FULL;
  }

  // Here = dynamic

  // Fetch block size (fetch or build new)
  int64_t blockSize = block == stat->managedBytes
    ? size
    : _pamu_find_size(fd, block);
  int64_t blockEnd = block + sizeof(int64_t) + size;
  int64_t beBlockSize = htobe64(blockSize);

  // Write start marker
  if(lseek(fd, block, SEEK_SET) != block) {
    free(stat);
    return PAMU_ERR_SEEK;
  }
  if(write(fd, &beBlockSize, sizeof(int64_t)) != sizeof(int64_t)) {
    free(stat);
    return PAMU_ERR_WRITE;
  }

  // Write end marker
  if(lseek(fd, blockEnd, SEEK_SET) != blockEnd) {
    free(stat);
    return PAMU_ERR_SEEK;
  }
  if(write(fd, &beBlockSize, sizeof(int64_t)) != sizeof(int64_t)) {
    free(stat);
    return PAMU_ERR_WRITE;
  }

  // Update managed bytes in header
  int64_t managedBytesOffset = PAMU_KEYWORD_LEN + sizeof(uint32_t);
  int64_t beManagedBytesOffset = htobe64(managedBytesOffset);
  if (block == stat->managedBytes) {
    // Write new managedBytes
    if (lseek(fd, managedBytesOffset, SEEK_SET) != managedBytesOffset) {
      free(stat);
      return PAMU_ERR_SEEK;
    }
    if(write(fd, &beManagedBytesOffset, sizeof(int64_t)) != sizeof(int64_t)) {
      free(stat);
      return PAMU_ERR_WRITE;
    }
  }

  free(stat);
  // Return pointer to innards
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
