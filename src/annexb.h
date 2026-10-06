
/*!
 *************************************************************************************
 * \file annexb.h
 *
 * \brief
 *    Annex B byte stream buffer handling.
 *
 *************************************************************************************
 */

#ifndef _ANNEXB_H_
#define _ANNEXB_H_

#include "nalu.h"

typedef struct annex_b_struct 
{
  int  BitStreamFile;                //!< the bit stream file
  byte *iobuffer;
  byte *iobufferread;
  int bytesinbuffer;
  int is_eof;
  int iIOBufferSize;

  int IsFirstByteStreamNALU;
  int nextstartcodebytes;
  byte *Buf;

  // In-memory bitstream buffer support
  int is_memory_input;
  int is_flushed;                    //!< 1 when decoder_flush has been signaled (treat buffer end as EOF)
  byte *mem_buf;
  size_t mem_buf_size;               //!< allocated capacity
  size_t mem_buf_len;                //!< total valid bytes stored
  size_t mem_read_pos;               //!< read cursor as INTEGER offset (never cached as a pointer!)
} ANNEXB_t;

extern int  get_annex_b_NALU (VideoParameters *p_Vid, NALU_t *nalu, ANNEXB_t *annex_b);

extern int  open_annex_b        (char *fn, ANNEXB_t *annex_b);
extern void open_annex_b_memory (ANNEXB_t *annex_b);
extern int  annex_b_push        (ANNEXB_t *annex_b, const byte *data, size_t size);
extern void close_annex_b       (ANNEXB_t *annex_b);
extern int  malloc_annex_b      (VideoParameters *p_Vid, ANNEXB_t **p_annex_b);
extern void free_annex_b        (ANNEXB_t **p_annex_b);
extern void init_annex_b        (ANNEXB_t *annex_b);
extern void reset_annex_b       (ANNEXB_t *annex_b);
#endif

