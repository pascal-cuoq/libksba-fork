/* ber-help.c - BER herlper functions
 *      Copyright (C) 2001 g10 Code GmbH
 *
 * This file is part of KSBA.
 *
 * KSBA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * KSBA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util.h"

#include "asn1-func.h" /* need some constants */
#include "ber-help.h"


static int
read_byte (KsbaReader reader)
{
  unsigned char buf;
  size_t nread;
  int rc;

  do
    rc = ksba_reader_read (reader, &buf, 1, &nread);
  while (!rc && !nread);
  return rc? -1: buf;
}

static int
eof_or_error (KsbaReader reader, struct tag_info *ti, int premature)
{
  if (ksba_reader_error (reader))
    {
      ti->err_string = "read error";
      return KSBA_Read_Error;
    }
  if (premature)
    {
      ti->err_string = "premature EOF";
      return KSBA_BER_Error;
    }
  return -1;
}



/*
   Read the tag and the length part from the TLV triplet. 
 */
KsbaError
_ksba_ber_read_tl (KsbaReader reader, struct tag_info *ti)
{
  int c;
  unsigned long tag;

  ti->length = 0;
  ti->ndef = 0;
  ti->nhdr = 0;
  ti->err_string = NULL;
  ti->non_der = 0;

  /* Get the tag */
  c = read_byte (reader);
  if (c==-1)
    return eof_or_error (reader, ti, 0);

  ti->buf[ti->nhdr++] = c;
  ti->class = (c & 0xc0) >> 6;
  ti->is_constructed = !!(c & 0x20);
  tag = c & 0x1f;

  if (tag == 0x1f)
    {
      tag = 0;
      do
        {
          /* fixme: check for overflow of out datatype */
          tag <<= 7;
          c = read_byte (reader);
          if (c == -1)
            return eof_or_error (reader, ti, 1);
          if (ti->nhdr >= DIM (ti->buf))
            {
              ti->err_string = "tag+length header too large";
              return KSBA_BER_Error;
            }
          ti->buf[ti->nhdr++] = c;
          tag |= c & 0x7f;
        }
      while (c & 0x80);
    }
  ti->tag = tag;

  /* Get the length */
  c = read_byte (reader);
  if (c == -1)
    return eof_or_error (reader, ti, 1);
  if (ti->nhdr >= DIM (ti->buf))
    {
      ti->err_string = "tag+length header too large";
      return KSBA_BER_Error;
    }
  ti->buf[ti->nhdr++] = c;

  if ( !(c & 0x80) )
    ti->length = c;
  else if (c == 0x80)
    {
      ti->ndef = 1;
      ti->non_der = 1;
    }
  else if (c == 0xff)
    {
      ti->err_string = "forbidden length value";
      return KSBA_BER_Error;
    }
  else
    {
      unsigned long len = 0;
      int count = c & 0x7f;

      /* fixme: check for overflow of our length type */
      for (; count; count--)
        {
          len <<= 8;
          c = read_byte (reader);
          if (c == -1)
            return eof_or_error (reader, ti, 1);
          if (ti->nhdr >= DIM (ti->buf))
            {
              ti->err_string = "tag+length header too large";
              return KSBA_BER_Error;
            }
          ti->buf[ti->nhdr++] = c;
          len |= c & 0xff;
        }
      ti->length = len;
    }
  
  /* Without this kludge some example certs can't be parsed */
  if (ti->class == CLASS_UNIVERSAL && !ti->tag)
    ti->length = 0;
  
  return 0;
}

