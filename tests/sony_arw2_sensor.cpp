#define LIBRAW_LIBRARY_BUILD
#include "libraw/libraw.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

class SonyDecoder : public LibRaw
{
public:
  std::vector<unsigned short> decode(std::vector<unsigned char> &bytes, unsigned rows)
  {
    LibRaw_buffer_datastream stream(bytes.data(), bytes.size());
    std::vector<unsigned short> output(64 * rows);
    imgdata.sizes.raw_width = imgdata.sizes.width = 64;
    imgdata.sizes.raw_height = imgdata.sizes.height = rows;
    imgdata.rawdata.raw_image = output.data();
    for (unsigned i = 0; i < 65536; i++)
      imgdata.color.curve[i] = (i * 7) & 0xffff;
    libraw_internal_data.internal_data.input = &stream;
    libraw_internal_data.unpacker_data.order = 0x4949;
    try { sony_arw2_load_raw(); }
    catch (...) {
      imgdata.rawdata.raw_image = nullptr;
      libraw_internal_data.internal_data.input = nullptr;
      throw;
    }
    imgdata.rawdata.raw_image = nullptr;
    libraw_internal_data.internal_data.input = nullptr;
    return output;
  }
};

int main()
{
  std::vector<unsigned char> bytes;
  std::vector<unsigned short> expected;
  uint32_t random = 7319;
  for (unsigned maximum : {127u, 255u, 511u, 1023u, 2047u})
    for (unsigned max_index = 0; max_index < 16; max_index++)
      for (unsigned min_index = 0; min_index < 16; min_index++)
      {
        if (min_index == max_index) continue;
        const unsigned minimum = maximum / 5;
        unsigned shift = 0;
        while (shift < 4 && (128u << shift) <= maximum - minimum) shift++;
        unsigned short row[64] = {};
        for (unsigned block = 0; block < 4; block++)
        {
          unsigned char packed[17] = {};
          const uint32_t header = maximum | (minimum << 11) | (max_index << 22) | (min_index << 26);
          for (unsigned i = 0; i < 4; i++) packed[i] = header >> (8 * i);
          unsigned offset = 30;
          const unsigned column = (block / 2) * 32 + (block & 1);
          for (unsigned i = 0; i < 16; i++)
          {
            unsigned value;
            if (i == max_index) value = maximum;
            else if (i == min_index) value = minimum;
            else
            {
              random = random * 1664525u + 1013904223u;
              const unsigned delta = (random >> 8) & 127;
              for (unsigned bit = 0; bit < 7; bit++)
                packed[(offset + bit) / 8] |= ((delta >> bit) & 1) << ((offset + bit) % 8);
              offset += 7;
              value = (delta << shift) + minimum;
              if (value > 2047) value = 2047;
            }
            row[column + 2 * i] = (value * 14) & 0xffff;
          }
          bytes.insert(bytes.end(), packed, packed + 16);
        }
        expected.insert(expected.end(), row, row + 64);
      }
  SonyDecoder decoder;
  const auto actual = decoder.decode(bytes, unsigned(expected.size() / 64));
  assert(actual == expected);
  std::puts("Sony ARW2: all extrema positions, delta shifts, clipping, curve lookup, row interleaving passed.");
}
