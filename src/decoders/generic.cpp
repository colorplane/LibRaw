/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 *
 LibRaw uses code from dcraw.c -- Dave Coffin's raw photo decoder,
 dcraw.c is copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net.
 LibRaw do not use RESTRICTED code from dcraw.c

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/dcraw_defs.h"

void LibRaw::unpacked_load_raw()
{
  read_shorts(raw_image, raw_width * raw_height);
  fseek(ifp, -2, SEEK_CUR); // avoid EOF error
  checkCancel();
  // Colorplane retains the sensor samples without a per-pixel range-validation
  // pass. Unshifted data is already decoded by read_shorts(); only formats
  // with a recorded bit shift require another pass over the plane.
  if (load_flags)
    for (int row = 0; row < raw_height; row++)
    {
      checkCancel();
      ushort *pixels = raw_image + size_t(row) * raw_width;
      for (int col = 0; col < raw_width; col++)
        pixels[col] >>= load_flags;
    }
}

void LibRaw::packed_load_raw()
{
  int vbits = 0, bwide, rbits, bite, half, irow, row, col, val, i;
  UINT64 bitbuf = 0;

  bwide = raw_width * tiff_bps / 8;
  bwide += bwide & load_flags >> 7;
  rbits = bwide * 8 - raw_width * tiff_bps;
  if (load_flags & 1)
    bwide = bwide * 16 / 15;
  bite = 8 + (load_flags & 24);
  half = (raw_height + 1) >> 1;
  for (irow = 0; irow < raw_height; irow++)
  {
    checkCancel();
    row = irow;
    if (load_flags & 2 && (row = irow % half * 2 + irow / half) == 1 &&
        load_flags & 4)
    {
      if (vbits = 0, tiff_compress)
        fseek(ifp, data_offset - (-half * bwide & -2048), SEEK_SET);
      else
      {
        fseek(ifp, 0, SEEK_END);
        fseek(ifp, ftell(ifp) >> 3 << 2, SEEK_SET);
      }
    }
    if (feof(ifp))
      throw LIBRAW_EXCEPTION_IO_EOF;
    for (col = 0; col < raw_width; col++)
    {
      for (vbits -= tiff_bps; vbits < 0; vbits += bite)
      {
        bitbuf <<= bite;
        for (i = 0; i < bite; i += 8)
          bitbuf |= (unsigned(fgetc(ifp)) << i);
      }
      val = int((bitbuf << (64 - tiff_bps - vbits) >> (64 - tiff_bps)) & 0x7fffffff);
      RAW(row, col ^ (load_flags >> 6 & 1)) = val;
      if (load_flags & 1 && (col % 10) == 9 && fgetc(ifp) &&
          row < height + top_margin && col < width + left_margin)
        derror();
    }
    vbits -= rbits;
  }
}

void LibRaw::eight_bit_load_raw()
{
  unsigned row, col;

  std::vector<uchar> pixel(raw_width);
  for (row = 0; row < raw_height; row++)
  {
      checkCancel();
      if (fread(pixel.data(), 1, raw_width, ifp) < raw_width)
          derror();
      for (col = 0; col < raw_width; col++)
          RAW(row, col) = curve[pixel[col]];
  }
  maximum = curve[0xff];
}
