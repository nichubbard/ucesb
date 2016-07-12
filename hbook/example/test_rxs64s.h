#ifndef __TEST_RXS64S__
#define __TEST_RXS64S__

#include <inttypes.h>

uint64_t rxs64s(uint64_t *x) // xorshift64star, see wikipedia
{
  *x ^= *x >> 12;
  *x ^= *x << 25;
  *x ^= *x >> 27;
  return *x * 2685821657736338717ull;
}

#endif/*__TEST_RXS64S__*/
