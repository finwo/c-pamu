#ifndef __FINWO_PAMU_H__
#define __FINWO_PAMU_H__

#include <stdint.h>
#include <stdio.h>

#define  PAMU_DEFAULT  (0)
#define  PAMU_DYNAMIC  (1 << 31)
#define  PAMU_FLAGS    (PAMU_DYNAMIC)

#define  PAMU_ERR_NONE                 ( 0)
#define  PAMU_ERR_MEDIUM_SIZE          (-1)
#define  PAMU_ERR_SEEK                 (-2)
#define  PAMU_ERR_NEGATIVE_SIZE        (-3)
#define  PAMU_ERR_READ_MALFORMED       (-4)
#define  PAMU_ERR_MEDIUM_UNINITIALIZED (-5)
#define  PAMU_ERR_MEDIUM_FULL          (-6)
#define  PAMU_ERR_WRITE                (-7)
#define  PAMU_ERR_OUT_OF_BOUNDS        (-8)

// In-file structure
//   header:
//     "PAMU"     Keyword           To check if an FD was already initialized
//     uint32_t   flags|headerSize  Feature flags + size of the header on medium
//   entry_free:
//     uint64_t   free|size         Free marker/flag + size of the entry
//     uint64_t   pointer           Pointer to the previous free entry
//     uint64_t   pointer           Pointer to the next free entry
//     char[]     blob              Unused space
//     uint64_t   free|size         Free marker/flag + size of the entry
//   entry_allocated:
//     uint64_t   size              Size of the entry
//     char[16+]  blob              Application data
//     uint64_t   size              Size of the entry

// Open/close functionality
int pamu_init(int fd, uint32_t flags);

// Core, alloc & free within the medium
int64_t  pamu_alloc(int fd, int64_t  size);
int      pamu_free(int fd , int64_t  addr);
int64_t  pamu_size(int fd , int64_t  addr);

// Iteration, so clients can find a reference
int64_t  pamu_next(int fd, int64_t addr);

#endif // __FINWO_PAMU_H__
