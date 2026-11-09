/* Colorplane: independent lossless JPEG state for memory-backed DNG tiles. */
#ifndef LIBRAW_DNG_TILE_DECODER_H
#define LIBRAW_DNG_TILE_DECODER_H

#ifndef LIBRAW_LIBRARY_BUILD
#define LIBRAW_LIBRARY_BUILD
#endif
#include "../libraw/libraw.h"
#include <algorithm>

// Keep the input cursor, bit reservoir, Huffman tables and row predictors local
// to each worker. This class never owns the source bytes or destination pixels.
class LibRaw_dng_tile_decoder : public LibRaw
{
public:
  explicit LibRaw_dng_tile_decoder(unsigned version)
      : LibRaw(LIBRAW_OPTIONS_NO_DATAERR_CALLBACK)
  {
    imgdata.idata.dng_version = version;
  }

  bool supports(const unsigned char *bytes, size_t length,
                unsigned tileWidth, unsigned tileHeight, unsigned sensorWidth)
  {
    LibRaw_buffer_datastream stream(bytes, length);
    libraw_internal_data.internal_data.input = &stream;
    struct jhead header = {};
    const bool valid = ljpeg_start(&header, 1) && header.algo == 0xc3 &&
        !header.sraw && header.restart == INT_MAX &&
        header.psv >= 1 && header.psv <= 7 &&
        size_t(header.wide) * header.clrs * header.high == size_t(tileWidth) * tileHeight &&
        // Retain the existing decoder for its special multi-component layout.
        !(header.clrs > 1 && unsigned(header.wide * header.clrs * header.clrs) == sensorWidth);
    libraw_internal_data.internal_data.input = nullptr;
    return valid;
  }

  void decode(const unsigned char *bytes, size_t length,
              unsigned tileWidth, unsigned tileHeight, unsigned tileX, unsigned tileY,
              unsigned sensorWidth, unsigned sensorHeight,
              const unsigned short *lookup, unsigned short *destination)
  {
    LibRaw_buffer_datastream stream(bytes, length);
    libraw_internal_data.internal_data.input = &stream;
    libraw_internal_data.unpacker_data.data_error = 0;
    struct jhead header = {};
    try
    {
      if (!ljpeg_start(&header, 0) || header.algo != 0xc3 || !header.row ||
          size_t(header.wide) * header.clrs * header.high != size_t(tileWidth) * tileHeight)
        throw LIBRAW_EXCEPTION_IO_CORRUPT;
      const unsigned rowSamples = header.wide * header.clrs;
      for (unsigned jpegRow = 0; jpegRow < unsigned(header.high); ++jpegRow)
      {
        const unsigned short *pixels = ljpeg_row(jpegRow, &header);
        if (libraw_internal_data.unpacker_data.data_error)
          throw LIBRAW_EXCEPTION_IO_CORRUPT;
        unsigned consumed = 0;
        while (consumed < rowSamples)
        {
          const size_t position = size_t(jpegRow) * rowSamples + consumed;
          const unsigned localX = position % tileWidth;
          const unsigned y = tileY + position / tileWidth;
          const unsigned x = tileX + localX;
          const unsigned count = std::min(rowSamples - consumed, tileWidth - localX);
          if (y < sensorHeight && x < sensorWidth)
          {
            unsigned short *out = destination + size_t(y) * sensorWidth + x;
            const unsigned visible = std::min(count, sensorWidth - x);
            for (unsigned i = 0; i < visible; ++i)
              out[i] = lookup[pixels[consumed + i]];
          }
          consumed += count;
        }
      }
      ljpeg_end(&header);
      libraw_internal_data.internal_data.input = nullptr;
    }
    catch (...)
    {
      ljpeg_end(&header);
      libraw_internal_data.internal_data.input = nullptr;
      throw;
    }
  }
};
#endif
