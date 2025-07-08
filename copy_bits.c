/* This function copies n bits from src starting at bit offset o to dest, which is must point to enough bytes for n bits. The copied bits must be packed to the right end of dest. */

#include <stdint.h>
#include <string.h>

// Copies 'n' bits from 'src' starting at bit offset 'o', packs to LSB of 'dest'.
void copy_bits_packed_right(const void *src, void *dest, size_t o, size_t n) {
    const uint8_t *src_bytes = (const uint8_t *)src;
    uint8_t *dest_bytes = (uint8_t *)dest;

    // Zero out destination storage (assumes enough bytes allocated).
    size_t dest_bytes_len = (n + 7) / 8;
    memset(dest_bytes, 0, dest_bytes_len);

    for (size_t i = 0; i < n; ++i) {
        size_t src_bit_pos = o + i;
        size_t src_byte = src_bit_pos / 8;
        size_t src_bit = src_bit_pos % 8;

        // Extract bit from src
        uint8_t bit_val = (src_bytes[src_byte] >> src_bit) & 1;

        // Set bit in dest (packed to right)
        size_t dest_byte = i / 8;
        size_t dest_bit = i % 8;
        dest_bytes[dest_byte] |= (bit_val << dest_bit);
    }
}
