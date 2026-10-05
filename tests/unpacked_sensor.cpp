// Exercise the uncompressed sensor path with the same library linked into WASM.
#define LIBRAW_LIBRARY_BUILD
#include "libraw/libraw.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

class SensorDecoder : public LibRaw
{
public:
  std::vector<unsigned short> decode(const std::vector<unsigned short> &samples,
                                    unsigned shift, bool big_endian,
                                    bool truncated = false, bool cancelled = false)
  {
    std::vector<unsigned char> bytes;
    for (unsigned short sample : samples)
    {
      bytes.push_back(big_endian ? sample >> 8 : sample & 0xff);
      bytes.push_back(big_endian ? sample & 0xff : sample >> 8);
    }
    if (truncated)
      bytes.resize(bytes.size() - 2);
    LibRaw_buffer_datastream stream(bytes.data(), bytes.size());
    std::vector<unsigned short> output(samples.size());
    imgdata.sizes.raw_width = 8;
    imgdata.sizes.raw_height = unsigned(samples.size() / 8);
    imgdata.sizes.width = 4;
    imgdata.sizes.height = imgdata.sizes.raw_height - 2;
    imgdata.sizes.left_margin = 2;
    imgdata.sizes.top_margin = 1;
    imgdata.color.maximum = 16383;
    imgdata.rawdata.raw_image = output.data();
    libraw_internal_data.internal_data.input = &stream;
    libraw_internal_data.unpacker_data.order = big_endian ? 0x4d4d : 0x4949;
    libraw_internal_data.unpacker_data.load_flags = shift;
    if (cancelled)
      setCancelFlag();
    try
    {
      unpacked_load_raw();
    }
    catch (...)
    {
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
  std::vector<unsigned short> samples(8 * 6);
  for (size_t i = 0; i < samples.size(); i++)
    samples[i] = (i * 2027) & 0xffff;
  // Values above the reported sensor white, including border values, remain
  // unchanged. The custom decoder does not scan or report sample-range errors.
  for (bool big_endian : {false, true})
    for (unsigned shift : {0u, 2u, 4u})
    {
      SensorDecoder decoder;
      const auto output = decoder.decode(samples, shift, big_endian);
      assert(output.size() == samples.size());
      for (size_t i = 0; i < samples.size(); i++)
        assert(output[i] == samples[i] >> shift);
      assert(decoder.error_count() == 0);
    }
  for (bool cancelled : {false, true})
  {
    SensorDecoder decoder;
    bool rejected = false;
    try
    {
      decoder.decode(samples, 0, false, !cancelled, cancelled);
    }
    catch (LibRaw_exceptions error)
    {
      rejected = error == (cancelled ? LIBRAW_EXCEPTION_CANCELLED_BY_CALLBACK
                                    : LIBRAW_EXCEPTION_IO_EOF);
    }
    assert(rejected);
  }
  std::puts("Unpacked sensor: byte orders, shifts, retained samples, short read, cancellation passed.");
}
