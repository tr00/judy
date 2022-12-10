#include <stdlib.h>
#include <stdint.h>

#include <sys/mman.h>

#include <immintrin.h>

#define B64 0x00000000ffffffffull
#define B128 0x0000ffff00000000ull
#define B256 0x00ff000000000000ull
#define B512 0x0f00000000000000ull
#define B1024 0x3000000000000000ull
#define B2048 0x4000000000000000ull

// we even have 1 bit left :)

struct SLOT
{

    // 1: used
    // 0: free
    union
    {
        struct 
        {
            uint32_t mask32lo;
            uint32_t mask32hi;
        };
        uint64_t mask64;
    };

    uintptr_t base;
    struct SLOT *next;
} root;

void *alloc(size_t size)
{
    void *res = NULL;

    switch (size)
    {
    case 64:
        return alloc64();
    }

    return res;
}

uint32_t pmask64(uint32_t i);

void *alloc_64_from_slot(struct SLOT *slot)
{
    uint32_t msk = slot->mask32lo;

    if (~msk)
        return NULL;

    // 0..31
    int idx = 31 - __builtin_clz(~msk);

    slot->mask32hi |= pmask64(idx);
    slot->mask32lo |= 1u << idx;

    return (void *)(slot->base + 64 * idx);
}

// 0x0000000000000000000000000000000011111111111111111111111111111111
// 0x0000000000000000000000000000000001010101010101010101010101010101
// 0x0000000000000000111111111111111100000000000000000000000000000000
//                   ^ 47            ^ 31                           ^ 0
//

uint32_t pmask64(uint32_t i)
{
    static const uint32_t T[16] = {
        0x51010001,
        0x51010002,
        0x51020004,
        0x51020008,
        0x52040010,
        0x52040020,
        0x52080040,
        0x52080080,
        0x64100100,
        0x64100200,
        0x64200400,
        0x64200800,
        0x68401000,
        0x68402000,
        0x68804000,
        0x68808000,
    };

    return T[i >> 1];
}

uint64_t adj2(uint64_t x)
{
    const uint64_t c1 = 0x5555555555555555ull;
    const uint64_t c2 = 0xaaaaaaaaaaaaaaaaull;

    return ((x & c1) << 1) & (x & c2);
}

uint64_t adj4(uint64_t x)
{
    const uint64_t c1 = 0x3333333333333333ull;
    const uint64_t c2 = 0xccccccccccccccccull;

    return (adj2(x) << 4) & adj2(x);
}

uint32_t adj16x(uint32_t x) 
{
    __m256i vx = _mm256_set1_epi32(x);

    __m256i c0 = _mm256_set_epi32(0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8);
    __m256i c1 = _mm256_set_epi32(0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0);

    __m256i s0 = _mm256_sllv_epi32(vx, c0);
    __m256i s1 = _mm256_sllv_epi32(vx, c1);

    __m256i r0 = _mm256_and_si256(s0, s1);
    __m256i r1 = _mm256_shuffle_epi32(r0, 0b10110001);
    __m256i r2 = _mm256_and_si256(r0, r1);
    __m256i r3 = _mm256_shuffle_epi32(r0, 0b01001110);
    __m256i r4 = _mm256_and_si256(r2, r3);

    return _mm256_extract_epi32(r4, 0) & _mm256_extract_epi32(r4, 1);
}
