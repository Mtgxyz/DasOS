#include <inttypes.h>
#include <stddef.h>
#include "pmm.hpp"

/**
 * Number stored of pages in the bitmap
 */
static const uint32_t BitmapSize = 4096; /* 16 MB */

/**
 * Number of 32-bit tuples in the bitmap.
 */
static const uint32_t BitmapLength = BitmapSize / 32; 

/**
 * Bitmap that stores the page status.
 * A set bit marks a used page
 * An unset bit marks an unused page
 */
static uint32_t bitmap[BitmapLength];

/**
 * Casts a pointer to an integer.
 */
static uint32_t ptrcast(void *ptr)
{
  return reinterpret_cast<uint32_t>(ptr);
}

/**
 * Casts an integer to a pointer
 */
static void *ptrcast(uint32_t ptr)
{
  return reinterpret_cast<void*>(ptr);
}

/**
 * Checks if an interger is 4096-aligned
 */
static bool isAligned(uint32_t ptr)
{
  return (ptr % 4096) == 0;
}

bool PMM::markOccupied(void *page)
{
  uint32_t ptr = ptrcast(page);
  if(!isAligned(ptr))
    ; // Do something about it!
  uint32_t pageId = ptr / 4096;
  
  uint32_t idx = pageId / 32;
  uint32_t bit = pageId % 32;
  
  bool wasSet = (bitmap[idx] & (1<<bit)) != 0;
  bitmap[idx] |= (1<<bit);
  return wasSet;
}

void *PMM::alloc(bool &success)
{
  for(uint32_t idx = 0; idx < BitmapLength; idx++) {
    // fast skip when all bits are set
    if(bitmap[idx] == 0xFFFFFFFF) {
      continue;
    }
    for(uint32_t bit = 0; bit < 32; bit++) {
      uint32_t mask = (1<<bit);
      if((bitmap[idx] & mask) != 0) {
        continue;
      }
      
      // Mark the selected bit as used.
      bitmap[idx] |= mask;
      
      uint32_t pageId = 32 * idx + bit;
      uint32_t ptr = 4096 * pageId;
      
      success = true;
      
      return ptrcast(ptr);
    }
  }
  // Do something here!
  success = false;
  return nullptr;
}

void PMM::free(void *page)
{
  uint32_t ptr = ptrcast(page);
  if(!isAligned(ptr))
    ; // Do something about it!
  uint32_t pageId = ptr / 4096;
  
  uint32_t idx = pageId / 32;
  uint32_t bit = pageId % 32;
  
  // Mark the selected bit as free.
  bitmap[idx] &= ~(1<<bit);
}