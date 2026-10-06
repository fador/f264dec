/*!
 ************************************************************************
 * \file  nalu.cpp
 *
 * \brief
 *    Decoder NALU support functions
 ************************************************************************
 */

#include "global.h"
#include "annexb.h"
#include "nalu.h"
#include "memalloc.h"

/*!
 *************************************************************************************
 * \brief
 *    Allocates memory for a NALU
 *************************************************************************************
 */
NALU_t *AllocNALU(int buffersize)
{
  NALU_t *n;

  if ((n = (NALU_t*)calloc (1, sizeof (NALU_t))) == NULL)
    no_mem_exit ("AllocNALU: n");

  n->max_size=buffersize;
  if ((n->buf = (byte*)calloc (buffersize, sizeof (byte))) == NULL)
  {
    free (n);
    no_mem_exit ("AllocNALU: n->buf");
  }

  return n;
}

/*!
 **************************************************************************************
 * \brief
 *    Frees a NALU
 **************************************************************************************
 */
void FreeNALU(NALU_t *n)
{
  if (n != NULL)
  {
    if (n->buf != NULL)
    {
      free(n->buf);
      n->buf=NULL;
    }
    free (n);
  }
}

/*!
 ************************************************************************
 * \brief
 *    Converts RBSP to string of data bits
 ************************************************************************/
int RBSPtoSODB(byte *streamBuffer, int last_byte_pos)
{
  int ctr_bit, bitoffset;

  bitoffset = 0;
  //find trailing 1
  ctr_bit = (streamBuffer[last_byte_pos-1] & (0x01<<bitoffset));   // set up control bit

  while (ctr_bit==0)
  {                 // find trailing 1 bit
    ++bitoffset;
    if(bitoffset == 8)
    {
      if(last_byte_pos == 0)
        printf(" Panic: All zero data sequence in RBSP \n");
      assert(last_byte_pos != 0);
      --last_byte_pos;
      bitoffset = 0;
    }
    ctr_bit= streamBuffer[last_byte_pos - 1] & (0x01<<(bitoffset));
  }

  return(last_byte_pos);
}

/*!
 ************************************************************************
 * \brief
 *    Converts Encapsulated Byte Sequence Packets to RBSP
 ************************************************************************/
int EBSPtoRBSP(byte *streamBuffer, int end_bytepos, int begin_bytepos)
{
  int i, j, count;
  count = 0;

  if(end_bytepos < begin_bytepos)
    return end_bytepos;

  j = begin_bytepos;

  for(i = begin_bytepos; i < end_bytepos; ++i)
  { //starting from begin_bytepos to avoid header information
    //in NAL unit, 0x000000, 0x000001 or 0x000002 shall not occur at any byte-aligned position
    if(count == ZEROBYTES_SHORTSTARTCODE && streamBuffer[i] < 0x03) 
      return -1;
    if(count == ZEROBYTES_SHORTSTARTCODE && streamBuffer[i] == 0x03)
    {
      //check the 4th byte after 0x000003, except when cabac_zero_word is used, in which case the last three bytes of this NAL unit must be 0x000003
      if((i < end_bytepos-1) && (streamBuffer[i+1] > 0x03))
        return -1;
      //if cabac_zero_word is used, the final byte of this NAL unit(0x03) is discarded, and the last two bytes of RBSP must be 0x0000
      if(i == end_bytepos-1)
        return j;

      ++i;
      count = 0;
    }
    streamBuffer[j] = streamBuffer[i];
    if(streamBuffer[i] == 0x00)
      ++count;
    else
      count = 0;
    ++j;
  }

  return j;
}

/*!
 *************************************************************************************
 * \brief
 *    Converts a NALU to an RBSP
 *************************************************************************************
 */
static int NALUtoRBSP (NALU_t *nalu)
{
  assert (nalu != NULL);

  nalu->len = EBSPtoRBSP (nalu->buf, nalu->len, 1) ;

  return nalu->len ;
}

/*!
 ************************************************************************
 * \brief
 *    Read the next NAL unit (with error handling)
 ************************************************************************
 */
int read_next_nalu(VideoParameters *p_Vid, NALU_t *nalu)
{
  InputParameters *p_Inp = p_Vid->p_Inp;
  int ret = get_annex_b_NALU(p_Vid, nalu, p_Vid->annex_b);

  if (ret < 0)
  {
    snprintf (errortext, ET_SIZE, "Error while getting the NALU in file format %s\n", p_Inp->FileFormat==PAR_OF_ANNEXB?"Annex B":"RTP");
    error (errortext, 601);
    return -1;
  }
  if (ret == 0)
  {
    return 0;
  }

  CheckZeroByteNonVCL(p_Vid, nalu);

  ret = NALUtoRBSP(nalu);

  if (ret < 0)
  {
    error ("Invalid startcode emulation prevention found.", 602);
    return -1;
  }

  if (nalu->forbidden_bit)
  {
    error ("Found NALU with forbidden_bit set, bit error?", 603);
    return -1;
  }

  return nalu->len;
}

void CheckZeroByteNonVCL(VideoParameters *p_Vid, NALU_t *nalu)
{
  int CheckZeroByte=0;

  if(nalu->nal_unit_type>=1&&nalu->nal_unit_type<=5)
    return;

  if(nalu->nal_unit_type==NALU_TYPE_SPS || nalu->nal_unit_type==NALU_TYPE_PPS)
    CheckZeroByte=1;
  if(nalu->nal_unit_type==NALU_TYPE_AUD  || nalu->nal_unit_type==NALU_TYPE_SPS ||
    nalu->nal_unit_type==NALU_TYPE_PPS || nalu->nal_unit_type==NALU_TYPE_SEI ||
    (nalu->nal_unit_type>=13 && nalu->nal_unit_type<=18))
  {
    if(p_Vid->LastAccessUnitExists)
    {
      p_Vid->LastAccessUnitExists=0;
      p_Vid->NALUCount=0;
    }
  }
  p_Vid->NALUCount++;
  if(p_Vid->NALUCount==1)
    CheckZeroByte=1;
  if(CheckZeroByte && nalu->startcodeprefix_len==3)   
  {
    printf("Warning: zero_byte shall exist\n");
  }
}

void CheckZeroByteVCL(VideoParameters *p_Vid, NALU_t *nalu)
{
  int CheckZeroByte=0;

  if(!(nalu->nal_unit_type>=NALU_TYPE_SLICE && nalu->nal_unit_type <= NALU_TYPE_IDR))
    return;

  if(p_Vid->LastAccessUnitExists)
  {
    p_Vid->NALUCount=0;
  }
  p_Vid->NALUCount++;
  if(p_Vid->NALUCount == 1)
    CheckZeroByte = 1;
  p_Vid->LastAccessUnitExists = 1;
  if(CheckZeroByte && nalu->startcodeprefix_len==3)
  {
    printf("warning: zero_byte shall exist\n");
  }
}
