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

#define  PAMU_INTERNAL_FLAG_FREE  ((PAMU_T_MARKER)1<<((8*PAMU_T_MARKER_SIZE)-1))
#define  PAMU_INTERNAL_FLAG_ERR   ((PAMU_T_MARKER)1<<((8*PAMU_T_MARKER_SIZE)-2))
#define  PAMU_INTERNAL_FLAGS      (PAMU_INTERNAL_FLAG_FREE)

#define MAX(a,b) ((a)>(b)?(a):(b))

// Overloaded ntoh & hton
int64_t ntoh_i64(int64_t v) {
  return be64toh(v);
}
int64_t hton_i64(int64_t v) {
  return htobe64(v);
}
uint64_t ntoh_u64(uint64_t v) {
  return be64toh(v);
}
uint64_t hton_u64(uint64_t v) {
  return htobe64(v);
}
int32_t ntoh_i32(int32_t v) {
  return be32toh(v);
}
int32_t hton_i32(int32_t v) {
  return htobe32(v);
}
uint32_t ntoh_u32(uint32_t v) {
  return be32toh(v);
}
uint32_t hton_u32(uint32_t v) {
  return htobe32(v);
}
#define hton(v) _Generic(v, uint32_t: hton_u32, int32_t: hton_i32, int64_t: hton_i64, uint64_t: hton_u64)(v)
#define ntoh(v) _Generic(v, uint32_t: ntoh_u32, int32_t: ntoh_i32, int64_t: ntoh_i64, uint64_t: ntoh_u64)(v)

struct pamu_medium_stat {
  int32_t flags;
  int32_t headerSize;
  PAMU_T_MARKER mediumSize;
};

// Uses outer address
// Returns inner size in bytes
PAMU_T_MARKER _pamu_find_sizeFlags(int fd, PAMU_T_POINTER addr) {
  PAMU_T_MARKER beSize;

  // Go to the requested address
  PAMU_T_POINTER _addr = lseek(fd, addr, SEEK_SET);
  if (addr != _addr) {
    return PAMU_ERR_READ_MALFORMED;
  }

  // Read the size|flags
  ssize_t rc = read(fd, &beSize, PAMU_T_MARKER_SIZE);
  if (rc != PAMU_T_MARKER_SIZE) {
    return PAMU_ERR_READ_MALFORMED;
  }

  // And return the size without the flags
  return ntoh(beSize);
}

PAMU_T_MARKER _pamu_find_size(int fd, PAMU_T_POINTER addr) {
  return _pamu_find_sizeFlags(fd, addr) & (~PAMU_INTERNAL_FLAGS);
}

PAMU_T_MARKER _pamu_find_flags(int fd, PAMU_T_POINTER addr) {
  PAMU_T_MARKER raw = _pamu_find_sizeFlags(fd, addr);
  if (raw & PAMU_INTERNAL_FLAG_ERR) return raw;
  return raw & PAMU_INTERNAL_FLAGS;
}

// Uses outer addresses
// Returns limit = no block found
PAMU_T_POINTER _pamu_find_free_block(int fd, PAMU_T_POINTER start, PAMU_T_POINTER limit, PAMU_T_MARKER size) {
  PAMU_T_POINTER current = start;
  PAMU_T_MARKER csize   = 0;
  PAMU_T_MARKER cflags  = 0;

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
      current += csize + (2 * PAMU_T_MARKER_SIZE);
      continue;
    }

    // Here = free

    // If large enough: return this one
    if (csize >= size) return current;

    // Skip to the next free block
    if (lseek(fd, current + (2*PAMU_T_MARKER_SIZE), SEEK_SET) != current + (2*PAMU_T_MARKER_SIZE)) {
      return PAMU_ERR_READ_MALFORMED;
    }
    if (read(fd, &current, PAMU_T_POINTER_SIZE) != PAMU_T_POINTER_SIZE) {
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
PAMU_T_POINTER _pamu_find_next(int fd, PAMU_T_POINTER current) {
  PAMU_T_MARKER size = _pamu_find_size(fd, current);
  return current + size + (2 * PAMU_T_MARKER_SIZE);
}

// Uses outer addresses
// Reads the previous' size and returns it's start
PAMU_T_POINTER _pamu_find_previous(int fd, PAMU_T_POINTER current, uint32_t header_size) {
  PAMU_T_POINTER addr = current - PAMU_T_MARKER_SIZE;
  if (addr < header_size) return PAMU_ERR_OUT_OF_BOUNDS;
  PAMU_T_MARKER size = _pamu_find_size(fd, addr);
  return current - size - (2 * PAMU_T_MARKER_SIZE);
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
    free(keyBuf);
    return (void*)PAMU_ERR_READ_MALFORMED;
  }
  uint32_t iFlaggedHeaderSize = ntoh(beFlaggedSize);
  response->flags      = iFlaggedHeaderSize &  PAMU_FLAGS;
  response->headerSize = iFlaggedHeaderSize & ~PAMU_FLAGS;

  // Find medium size
  response->mediumSize = lseek(fd, 0, SEEK_END);

  free(keyBuf);
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
    PAMU_T_MARKER_SIZE + // Start size indicator
    PAMU_T_POINTER_SIZE + // Previous free pointer in empty records
    PAMU_T_POINTER_SIZE + // Next free pointer in empty records
    PAMU_T_MARKER_SIZE + // End size indicator
    0;

  // Fetch medium size
  PAMU_T_MARKER iMediumSize = lseek(fd, 0, SEEK_END);

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
  uint32_t beHeaderSize = hton(flags | iHeaderSize);
  write(fd, &beHeaderSize, sizeof(uint32_t));

  // Initialize medium as free blob
  PAMU_T_MARKER mediumSize = lseek(fd, 0, SEEK_END);
  PAMU_T_MARKER blobSize   = mediumSize - iHeaderSize - (2 * PAMU_T_MARKER_SIZE);
  PAMU_T_MARKER blobMarker = hton(blobSize | PAMU_INTERNAL_FLAG_FREE);
  int64_t zero      = 0;
  if (!(flags & PAMU_DYNAMIC)) {
    lseek(fd, iHeaderSize, SEEK_SET);
    write(fd, &blobMarker, PAMU_T_MARKER_SIZE); // Start marker
    write(fd, &zero      , PAMU_T_POINTER_SIZE); // Previous pointer
    write(fd, &zero      , PAMU_T_POINTER_SIZE); // Next Pointer
    lseek(fd, iHeaderSize + blobSize + PAMU_T_MARKER_SIZE, SEEK_SET);
    write(fd, &blobMarker, PAMU_T_MARKER_SIZE); // End marker
  }

  return 0;
}

// Returns inner address or error
PAMU_T_POINTER pamu_alloc(int fd, PAMU_T_MARKER size) {
  if (size <= 0) return PAMU_ERR_NEGATIVE_SIZE;
  if (size < (2*PAMU_T_POINTER_SIZE)) size = 2*PAMU_T_POINTER_SIZE;

  // Fetch info (or return error code)
  struct pamu_medium_stat *stat = _pamu_medium_stat(fd);
  if (stat < 0) return (PAMU_T_POINTER)stat;

  // Find a pre-existing block with the correct size (or throw error)
  PAMU_T_POINTER block = _pamu_find_free_block(fd, stat->headerSize, stat->mediumSize, size);

  // Error during finding
  if (block < 0) {
    free(stat);
    return block;
  }

  // Throw error if non-dynamic & not enough space
  if (
    (block + (2*PAMU_T_MARKER_SIZE) + size >= stat->mediumSize) &&
    (!(stat->flags & PAMU_DYNAMIC))
  ) {
    free(stat);
    return PAMU_ERR_MEDIUM_FULL;
  }

  // Here = got the space

  // Fetch or build block size
  PAMU_T_MARKER blockSize = block == stat->mediumSize
    ? size
    : _pamu_find_size(fd, block);
  PAMU_T_MARKER blockMarker = hton(blockSize | PAMU_INTERNAL_FLAG_FREE);

  // Split free block if large enough
  int64_t zero = 0;
  PAMU_T_POINTER previousFree;
  PAMU_T_POINTER nextFree;
  PAMU_T_POINTER newFree;
  PAMU_T_MARKER  newFreeSize;
  PAMU_T_MARKER  newFreeMarker;
  PAMU_T_POINTER beBlock = hton(block);
  if ((blockSize - size) > ((2 * PAMU_T_POINTER_SIZE) + (2*PAMU_T_MARKER_SIZE))) {
    lseek(fd, block + PAMU_T_MARKER_SIZE, SEEK_SET);
    read(fd, &previousFree, PAMU_T_POINTER_SIZE);
    read(fd, &nextFree, PAMU_T_POINTER_SIZE);

    newFree       = hton((PAMU_T_POINTER)(block     + size + (2 * PAMU_T_MARKER_SIZE)));
    newFreeSize   =          blockSize - size - (2 * PAMU_T_MARKER_SIZE) ;
    newFreeMarker = hton((PAMU_T_MARKER)(newFreeSize | PAMU_INTERNAL_FLAG_FREE));

    // Update the current block
    blockSize   = size;
    blockMarker = hton(blockSize | PAMU_INTERNAL_FLAG_FREE);
    lseek(fd, block, SEEK_SET);
    write(fd, &blockMarker , PAMU_T_MARKER_SIZE);  // Start marker
    write(fd, &previousFree, PAMU_T_POINTER_SIZE); // Previous free
    write(fd, &newFree     , PAMU_T_POINTER_SIZE); // Next/new free
    lseek(fd, block + blockSize + PAMU_T_MARKER_SIZE, SEEK_SET);
    write(fd, &blockMarker , PAMU_T_MARKER_SIZE);  // End marker

    // Build new free block
    write(fd, &newFreeMarker, PAMU_T_MARKER_SIZE); // Start marker
    write(fd, &beBlock      , PAMU_T_POINTER_SIZE); // Previous/current free
    write(fd, &nextFree     , PAMU_T_POINTER_SIZE); // Next free
    lseek(fd, ntoh(newFree) + newFreeSize + PAMU_T_MARKER_SIZE, SEEK_SET);
    write(fd, &newFreeMarker, PAMU_T_MARKER_SIZE); // End marker

    // Update next block to point it's previous to the new free
    if (nextFree) {
      lseek(fd, ntoh(nextFree) + PAMU_T_MARKER_SIZE, SEEK_SET);
      write(fd, &newFree, PAMU_T_POINTER_SIZE);
    }

    // And the new free is now our next free
    nextFree = newFree;
    // No need to update previous free block in this step
  }

  // Grow medium in dynamic mode
  // Init as free block without prev/next
  if (
    (stat->flags & PAMU_DYNAMIC) &&
    (block == stat->mediumSize)
  ) {
    lseek(fd, block, SEEK_SET);
    write(fd, &blockMarker, PAMU_T_MARKER_SIZE);  // Start marker
    write(fd, &zero       , PAMU_T_POINTER_SIZE); // Previous free
    write(fd, &zero       , PAMU_T_POINTER_SIZE); // Next free
    lseek(fd, block + blockSize + PAMU_T_MARKER_SIZE, SEEK_SET);
    write(fd, &blockMarker, PAMU_T_MARKER_SIZE);  // End marker
  }

  // We don't need the stat anymore
  free(stat);

  // Mark the current block as allocated & read previous/next free pointers
  blockMarker = hton(blockSize);
  lseek(fd, block, SEEK_SET);
  write(fd, &blockMarker, PAMU_T_MARKER_SIZE);
  read(fd, &previousFree, PAMU_T_POINTER_SIZE);
  read(fd, &nextFree    , PAMU_T_POINTER_SIZE);
  lseek(fd, block + blockSize + PAMU_T_MARKER_SIZE, SEEK_SET);
  write(fd, &blockMarker, PAMU_T_MARKER_SIZE);

  // Update the previous free's next pointer
  if (previousFree) {
    lseek(fd, ntoh(previousFree) + PAMU_T_MARKER_SIZE + PAMU_T_POINTER_SIZE, SEEK_SET);
    write(fd, &nextFree, PAMU_T_POINTER_SIZE);
  }

  // Update the next free's previous pointer
  if (nextFree) {
    lseek(fd, ntoh(nextFree) + PAMU_T_MARKER_SIZE, SEEK_SET);
    write(fd, &previousFree, PAMU_T_POINTER_SIZE);
  }

  // Return pointer to innards
  return block + PAMU_T_MARKER_SIZE;
}

int pamu_free(int fd, PAMU_T_POINTER addr) {

  // Fetch info (or return error code)
  struct pamu_medium_stat *stat = _pamu_medium_stat(fd);
  if (stat < 0) return (int)stat;

  // Catch out-of-bounds
  if (
      (addr >= stat->mediumSize) ||
      (addr <  stat->headerSize)
  ) {
    return PAMU_ERR_OUT_OF_BOUNDS;
  }

  // Fetch block info
  int64_t zero          = 0;
  PAMU_T_POINTER block         = addr - PAMU_T_MARKER_SIZE;
  PAMU_T_POINTER beBlock       = hton(block);
  PAMU_T_MARKER blockSizeFlags = hton(_pamu_find_sizeFlags(fd, block));
  PAMU_T_MARKER blockSize      = _pamu_find_size(fd, block);
  PAMU_T_MARKER blockFlags     = _pamu_find_flags(fd, block);

  // Verify the block is supposed to be allocated
  if (blockFlags & PAMU_INTERNAL_FLAG_FREE) {
    free(stat);
    return PAMU_ERR_DOUBLE_FREE;
  }

  // Verify the end marker matches the start marker
  PAMU_T_MARKER endMarker;
  lseek(fd, block + blockSize + PAMU_T_MARKER_SIZE, SEEK_SET);
  if (read(fd, &endMarker, PAMU_T_MARKER_SIZE) != PAMU_T_MARKER_SIZE) {
    free(stat);
    return PAMU_ERR_READ_MALFORMED;
  }
  if (endMarker != blockSizeFlags) {
    free(stat);
    return PAMU_ERR_INVALID_ADDRESS;
  }

  // Actually free the block
  PAMU_T_MARKER blockMarker = hton(blockSize | PAMU_INTERNAL_FLAG_FREE);
  lseek(fd, block, SEEK_SET);
  write(fd, &blockMarker, PAMU_T_MARKER_SIZE);
  lseek(fd, block + blockSize + PAMU_T_MARKER_SIZE, SEEK_SET);
  write(fd, &blockMarker, PAMU_T_MARKER_SIZE);

  // Find next free block (or medium end)
  PAMU_T_POINTER nextFree          = _pamu_find_next(fd, block);
  PAMU_T_MARKER  nextFreeFlags     = 0;
  PAMU_T_POINTER previousFree      = 0;
  PAMU_T_MARKER  previousFreeFlags = 0;
  while(
    (nextFree) &&
    (nextFree < stat->mediumSize)
  ) {
    nextFreeFlags = _pamu_find_flags(fd, nextFree);
    if (nextFreeFlags & PAMU_INTERNAL_FLAG_FREE) { break; }
    nextFree = _pamu_find_next(fd, nextFree);
  }
  if (nextFreeFlags & PAMU_INTERNAL_FLAG_FREE) {
    lseek(fd, nextFree + PAMU_T_MARKER_SIZE, SEEK_SET);
    read(fd, &previousFree, PAMU_T_POINTER_SIZE);
    nextFree = hton(nextFree);
  } else {
    nextFree = 0;
  }

  // Find previous free block (or header) if not found yet
  if (!previousFree) {
    previousFree = _pamu_find_previous(fd, block, stat->headerSize);
    while(
      (previousFree) &&
      (previousFree >= stat->headerSize)
    ) {
      previousFreeFlags = _pamu_find_flags(fd, previousFree);
      if (previousFreeFlags & PAMU_INTERNAL_FLAG_FREE) break;
      previousFree = _pamu_find_previous(fd, previousFree, stat->headerSize);
    }
    if ((previousFreeFlags & PAMU_INTERNAL_FLAG_FREE) && (!(previousFreeFlags & PAMU_INTERNAL_FLAG_ERR))) {
      previousFree = hton(previousFree);
    } else {
      previousFree = 0;
    }
  }

  // Write next & previous pointers to current block
  lseek(fd, block + PAMU_T_MARKER_SIZE, SEEK_SET);
  write(fd, &previousFree, PAMU_T_POINTER_SIZE);
  write(fd, &nextFree    , PAMU_T_POINTER_SIZE);

  // Update next block's previous pointer
  if (nextFree) {
    lseek(fd, ntoh(nextFree) + PAMU_T_MARKER_SIZE, SEEK_SET);
    write(fd, &beBlock, PAMU_T_POINTER_SIZE);
  }

  // Update the previous block's next pointer
  if (previousFree) {
    lseek(fd, ntoh(previousFree) + PAMU_T_MARKER_SIZE + PAMU_T_POINTER_SIZE, SEEK_SET);
    write(fd, &beBlock, PAMU_T_POINTER_SIZE);
  }

  // Merge with previous block if it's our neighbour
  // Find previous block and it's flags
  // TODO: check if _pamu_find_previous == ntoh(previousFree)
  PAMU_T_POINTER previousAdjacent = _pamu_find_previous(fd, block, stat->headerSize);
  PAMU_T_MARKER  previousAdjacentFlags, previousAdjacentSize, previousAdjacentMarker;
  if (previousAdjacent >= 0) {
    previousAdjacentFlags = _pamu_find_flags(fd, previousAdjacent);
    previousAdjacentSize  = _pamu_find_size(fd, previousAdjacent);
    if (previousAdjacentFlags & PAMU_INTERNAL_FLAG_FREE) {
      // Merge the 2 blocks
      previousAdjacentSize   += blockSize + (2 * PAMU_T_MARKER_SIZE);
      previousAdjacentMarker  = hton(previousAdjacentSize | PAMU_INTERNAL_FLAG_FREE);
      lseek(fd, previousAdjacent, SEEK_SET);
      write(fd, &previousAdjacentMarker, PAMU_T_MARKER_SIZE);
      read( fd, &previousFree, PAMU_T_POINTER_SIZE);
      write(fd, &nextFree, PAMU_T_POINTER_SIZE);
      lseek(fd, previousAdjacent + PAMU_T_MARKER_SIZE + previousAdjacentSize, SEEK_SET);
      write(fd, &previousAdjacentMarker, PAMU_T_MARKER_SIZE);
      // Update our own references
      block     = previousAdjacent;
      beBlock   = hton(block);
      blockSize = previousAdjacentSize;
    } else {
      // Previous block is not free, ignore it
    }
  }

  // Merge with next block if it's our neighbour
  // Find next block and it's flags
  // TODO: check if _pamu_find_next == ntoh(nextFree)
  PAMU_T_POINTER nextAdjacent = _pamu_find_next(fd, block);
  PAMU_T_MARKER nextAdjacentFlags, nextAdjacentSize;
  if (nextAdjacent < stat->mediumSize) {
    nextAdjacentFlags = _pamu_find_flags(fd, nextAdjacent);
    nextAdjacentSize  = _pamu_find_size(fd, nextAdjacent);
    if (nextAdjacentFlags & PAMU_INTERNAL_FLAG_FREE) {
      // Merge the 2 blocks
      // Update block stats
      blockSize   += nextAdjacentSize + (2 * PAMU_T_MARKER_SIZE);
      blockMarker  = hton(blockSize | PAMU_INTERNAL_FLAG_FREE);
      // Read new next
      lseek(fd, nextAdjacent + PAMU_T_MARKER_SIZE + PAMU_T_POINTER_SIZE, SEEK_SET);
      read(fd, &nextFree, PAMU_T_POINTER_SIZE);
      // Update our current block
      lseek(fd, block, SEEK_SET);
      write(fd, &blockMarker, PAMU_T_MARKER_SIZE);
      lseek(fd, block + PAMU_T_MARKER_SIZE + PAMU_T_POINTER_SIZE, SEEK_SET);
      write(fd, &nextFree, PAMU_T_POINTER_SIZE);
      lseek(fd, block + blockSize + PAMU_T_MARKER_SIZE, SEEK_SET);
      write(fd, &blockMarker, PAMU_T_MARKER_SIZE);
      // Update nextFree's previous pointer
      if (nextFree) {
        lseek(fd, ntoh(nextFree) + PAMU_T_MARKER_SIZE, SEEK_SET);
        write(fd, &beBlock, PAMU_T_POINTER_SIZE);
      }
      // Update references?
    } else {
      // Next block is not free, ignore it
    }
  } else if (stat->flags & PAMU_DYNAMIC) {
    // Truncate the file if in dynamic mode
    // Set previousFree's next pointer to 0
    lseek(fd, ntoh(previousFree) + PAMU_T_MARKER_SIZE + PAMU_T_POINTER_SIZE, SEEK_SET);
    write(fd, &zero, PAMU_T_POINTER_SIZE);
    if (ftruncate(fd, stat->mediumSize - (2 * PAMU_T_MARKER_SIZE) - blockSize)) {
      perror("ftruncate");
      exit(1);
    }
  }

  free(stat);
  return 0;
}

PAMU_T_MARKER pamu_size(int fd, PAMU_T_POINTER addr) {
  return _pamu_find_size(fd, addr - PAMU_T_MARKER_SIZE);
}

// Iteration, so clients can find a reference
PAMU_T_POINTER pamu_next(int fd, PAMU_T_POINTER addr) {

  // Fetch info (or return error code)
  struct pamu_medium_stat *stat = _pamu_medium_stat(fd);
  if (stat < 0) return (PAMU_T_POINTER)stat;

  // Find the outer addr of current block
  PAMU_T_POINTER block = addr - PAMU_T_MARKER_SIZE;
  if (block < stat->headerSize) {
    block = stat->headerSize;
  } else {
    block = _pamu_find_next(fd, block);
  }

  PAMU_T_MARKER flags;
  while(block < stat->mediumSize) {
    flags = _pamu_find_flags(fd, block);
    if (!(flags & PAMU_INTERNAL_FLAG_FREE)) break;
    block = _pamu_find_next(fd, block);
  }

  // End of medium
  if (block == stat->mediumSize) {
    free(stat);
    return 0;
  }

  // Return the inner addr of the found allocated block
  free(stat);
  return block + PAMU_T_MARKER_SIZE;
}
