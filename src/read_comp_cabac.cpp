/*!
 ***********************************************************************
 * \file read_comp_cabac.c
 *
 * \brief
 *     Read Coefficient Components
 *
 * \author
 *    Main contributors (see contributors.h for copyright, address and affiliation details)
 *    - Alexis Michael Tourapis         <alexismt@ieee.org>
 ***********************************************************************
*/

#include "global.h"
#include "elements.h"
#include "macroblock.h"
#include "cabac.h"
#include "vlc.h"
#include "transform.h"

#define TRACE_STRING(s)
#define TRACE_DECBITS(i)
#define TRACE_PRINTF(s) 
#define TRACE_STRING_P(s)

static inline void read_delta_quant_cabac(Macroblock *currMB, SyntaxElement *currSE, DecodingEnvironmentPtr dep_dp)
{
  Slice *currSlice = currMB->p_Slice;
  VideoParameters *p_Vid = currMB->p_Vid;

  TRACE_STRING_P("mb_qp_delta");
  read_dQuant_CABAC(currMB, currSE, dep_dp);
  currMB->delta_quant = (short) currSE->value1;
  if ((currMB->delta_quant < -(26 + p_Vid->bitdepth_luma_qp_scale/2)) || (currMB->delta_quant > (25 + p_Vid->bitdepth_luma_qp_scale/2)))
  {
    printf("mb_qp_delta is out of range (%d)\n", currMB->delta_quant);
    currMB->delta_quant = iClip3(-(26 + p_Vid->bitdepth_luma_qp_scale/2), (25 + p_Vid->bitdepth_luma_qp_scale/2), currMB->delta_quant);
  }

  currSlice->qp = ((currSlice->qp + currMB->delta_quant + 52 + 2*p_Vid->bitdepth_luma_qp_scale)%(52+p_Vid->bitdepth_luma_qp_scale)) - p_Vid->bitdepth_luma_qp_scale;
  update_qp(currMB, currSlice->qp);
}

/*!
************************************************************************
* \brief
*    Get coefficients (run/level) of 4x4 blocks in a SMB
*    from the NAL (CABAC Mode)
************************************************************************
*/
static void read_comp_coeff_4x4_smb_CABAC (Macroblock *currMB, SyntaxElement *currSE, DecodingEnvironmentPtr dep_dp, ColorPlane pl, int block_y, int block_x, int start_scan, int64 *cbp_blk)
{
  int i,j,k;
  int i0, j0;
  int level = 1;
  Slice *currSlice = currMB->p_Slice;

  const byte (*pos_scan4x4)[2] = ((currSlice->structure == FRAME) && (!currMB->mb_field)) ? SNGL_SCAN : FIELD_SCAN;
  const byte *pos_scan_4x4 = pos_scan4x4[0];
  int **cof = currSlice->cof[pl];

  for (j = block_y; j < block_y + BLOCK_SIZE_8x8; j += 4)
  {
    currMB->subblock_y = j; // position for coeff_count ctx

    for (i = block_x; i < block_x + BLOCK_SIZE_8x8; i += 4)
    {
      currMB->subblock_x = i; // position for coeff_count ctx
      pos_scan_4x4 = pos_scan4x4[start_scan];
      level = 1;

      if (start_scan == 0)
      {
        readRunLevel_CABAC(currMB, currSE, dep_dp);
        level = currSE->value1;

        if (level != 0)    /* leave if level == 0 */
        {
          pos_scan_4x4 += 2 * currSE->value2;

          i0 = *pos_scan_4x4++;
          j0 = *pos_scan_4x4++;

          *cbp_blk |= i64_power2(j + (i >> 2));
          cof[j + j0][i + i0] = level;
        }
      }

      if (level != 0)
      {
        for(k = 1; (k < 17) && (level != 0); ++k)
        {
          readRunLevel_CABAC(currMB, currSE, dep_dp);
          level = currSE->value1;

          if (level != 0)    /* leave if level == 0 */
          {
            pos_scan_4x4 += 2 * currSE->value2;

            i0 = *pos_scan_4x4++;
            j0 = *pos_scan_4x4++;

            cof[j + j0][i + i0] = level;
          }
        }
      }
    }
  }
}

/*!
************************************************************************
* \brief
*    Get coefficients (run/level) of all 4x4 blocks in a MB
*    from the NAL (CABAC Mode)
************************************************************************
*/
static void read_comp_coeff_4x4_CABAC (Macroblock *currMB, SyntaxElement *currSE, DecodingEnvironmentPtr dep_dp, ColorPlane pl, int (*InvLevelScale4x4)[4], int qp_per, int cbp)
{
  Slice *currSlice = currMB->p_Slice;
  VideoParameters *p_Vid = currMB->p_Vid;
  int start_scan = IS_I16MB (currMB)? 1 : 0; 
  int block_y, block_x;
  int i, j;
  int64 *cbp_blk = &currMB->s_cbp[pl].blk;

  if( pl == PLANE_Y || (p_Vid->separate_colour_plane_flag != 0) )
    currSE->context = (IS_I16MB(currMB) ? LUMA_16AC: LUMA_4x4);
  else if (pl == PLANE_U)
    currSE->context = (IS_I16MB(currMB) ? CB_16AC: CB_4x4);
  else
    currSE->context = (IS_I16MB(currMB) ? CR_16AC: CR_4x4);  

  for (block_y = 0; block_y < MB_BLOCK_SIZE; block_y += BLOCK_SIZE_8x8) /* all modes */
  {
    int **cof = &currSlice->cof[pl][block_y];
    for (block_x = 0; block_x < MB_BLOCK_SIZE; block_x += BLOCK_SIZE_8x8)
    {
      if (cbp & (1 << ((block_y >> 2) + (block_x >> 3))))  // are there any coeff in current block at all
      {
        read_comp_coeff_4x4_smb_CABAC (currMB, currSE, dep_dp, pl, block_y, block_x, start_scan, cbp_blk);

        if (currMB->is_lossless == FALSE)
        {
          if (start_scan == 0)
          {
            for (j = 0; j < BLOCK_SIZE_8x8; ++j)
            {
              int *coef = &cof[j][block_x];
              int jj = j & 0x03;
              for (i = 0; i < BLOCK_SIZE_8x8; i+=4)
              {
                if (*coef)
                  *coef = rshift_rnd_sf((*coef * InvLevelScale4x4[jj][0]) << qp_per, 4);
                coef++;
                if (*coef)
                  *coef = rshift_rnd_sf((*coef * InvLevelScale4x4[jj][1]) << qp_per, 4);
                coef++;
                if (*coef)
                  *coef = rshift_rnd_sf((*coef * InvLevelScale4x4[jj][2]) << qp_per, 4);
                coef++;
                if (*coef)
                  *coef = rshift_rnd_sf((*coef * InvLevelScale4x4[jj][3]) << qp_per, 4);
                coef++;
              }
            }
          }
          else
          {                        
            for (j = 0; j < BLOCK_SIZE_8x8; ++j)
            {
              int *coef = &cof[j][block_x];
              int jj = j & 0x03;
              for (i = 0; i < BLOCK_SIZE_8x8; i += 4)
              {
                if ((jj != 0) && *coef)
                  *coef= rshift_rnd_sf((*coef * InvLevelScale4x4[jj][0]) << qp_per, 4);
                coef++;
                if (*coef)
                  *coef= rshift_rnd_sf((*coef * InvLevelScale4x4[jj][1]) << qp_per, 4);
                coef++;
                if (*coef)
                  *coef= rshift_rnd_sf((*coef * InvLevelScale4x4[jj][2]) << qp_per, 4);
                coef++;
                if (*coef)
                  *coef= rshift_rnd_sf((*coef * InvLevelScale4x4[jj][3]) << qp_per, 4);
                coef++;
              }
            }
          }
        }
      }
    }
  }
}


/*!
************************************************************************
* \brief
*    Get coefficients (run/level) of one 8x8 block
*    from the NAL (CABAC Mode)
************************************************************************
*/
static void readCompCoeff8x8_CABAC (Macroblock *currMB, SyntaxElement *currSE, DecodingEnvironmentPtr dep_dp, ColorPlane pl, int b8)
{
  if (currMB->cbp & (1<<b8))  // are there any coefficients in the current block
  {
    VideoParameters *p_Vid = currMB->p_Vid;
    int transform_pl = (p_Vid->separate_colour_plane_flag != 0) ? currMB->p_Slice->colour_plane_id : pl;

    int **tcoeffs;
    int i,j,k;
    int level = 1;

    Slice *currSlice = currMB->p_Slice;
    int boff_x, boff_y;

    int64 cbp_mask = (int64) 51 << (4 * b8 - 2 * (b8 & 0x01)); // corresponds to 110011, as if all four 4x4 blocks contain coeff, shifted to block position            
    int64 *cur_cbp = &currMB->s_cbp[pl].blk;

    // select scan type
    const byte *pos_scan8x8 = ((currSlice->structure == FRAME) && (!currMB->mb_field)) ? SNGL_SCAN8x8[0] : FIELD_SCAN8x8[0];

    int qp_per = p_Vid->qp_per_matrix[ currMB->qp_scaled[pl] ];
    int qp_rem = p_Vid->qp_rem_matrix[ currMB->qp_scaled[pl] ];
    
    int (*InvLevelScale8x8)[8] = (currMB->is_intra_block == TRUE) ? currSlice->InvLevelScale8x8_Intra[transform_pl][qp_rem] : currSlice->InvLevelScale8x8_Inter[transform_pl][qp_rem];

    // === set offset in current macroblock ===
    boff_x = (b8&0x01) << 3;
    boff_y = (b8 >> 1) << 3;
    tcoeffs = &currSlice->mb_rres[pl][boff_y];

    currMB->subblock_x = boff_x; // position for coeff_count ctx
    currMB->subblock_y = boff_y; // position for coeff_count ctx

    if (pl==PLANE_Y || (p_Vid->separate_colour_plane_flag != 0))  
      currSE->context = LUMA_8x8;
    else if (pl==PLANE_U)
      currSE->context = CB_8x8;
    else
      currSE->context = CR_8x8;  

    // Read DC
    readRunLevel_CABAC(currMB, currSE, dep_dp);
    level = currSE->value1;

    //============ decode =============
    if (level != 0)    /* leave if level == 0 */
    {
      *cur_cbp |= cbp_mask; 

      pos_scan8x8 += 2 * (currSE->value2);

      i = *pos_scan8x8++;
      j = *pos_scan8x8++;

      tcoeffs[j][boff_x + i] = currMB->is_lossless ? level : rshift_rnd_sf((level * InvLevelScale8x8[j][i]) << qp_per, 6);

      // AC coefficients
      for(k = 1;(k < 65) && (level != 0);++k)
      {
        readRunLevel_CABAC(currMB, currSE, dep_dp);
        level = currSE->value1;

        //============ decode =============
        if (level != 0)    /* leave if level == 0 */
        {
          pos_scan8x8 += 2 * (currSE->value2);

          i = *pos_scan8x8++;
          j = *pos_scan8x8++;

          tcoeffs[j][boff_x + i] = currMB->is_lossless ? level : rshift_rnd_sf((level * InvLevelScale8x8[j][i]) << qp_per, 6);
        }
      }
    }        
  }
}

static void read_comp_coeff_8x8_MB_CABAC (Macroblock *currMB, SyntaxElement *currSE, DecodingEnvironmentPtr dep_dp, ColorPlane pl)
{
  readCompCoeff8x8_CABAC (currMB, currSE, dep_dp, pl, 0); 
  readCompCoeff8x8_CABAC (currMB, currSE, dep_dp, pl, 1); 
  readCompCoeff8x8_CABAC (currMB, currSE, dep_dp, pl, 2); 
  readCompCoeff8x8_CABAC (currMB, currSE, dep_dp, pl, 3); 
}

/*!
************************************************************************
* \brief
*    Common luma coefficient and pattern decoding for CABAC mode
*    (shared across all chroma formats: 4:0:0, 4:2:0, 4:2:2, 4:4:4)
************************************************************************
*/
static int read_luma_coeff_CABAC(Macroblock *currMB, SyntaxElement *currSE, DecodingEnvironmentPtr dep_dp)
{
  Slice *currSlice = currMB->p_Slice;
  VideoParameters *p_Vid = currMB->p_Vid;
  int cbp;
  int intra = (currMB->is_intra_block == TRUE);

  const byte (*pos_scan4x4)[2] = ((currSlice->structure == FRAME) && (!currMB->mb_field)) ? SNGL_SCAN : FIELD_SCAN;

  if (!IS_I16MB(currMB))
  {
    TRACE_STRING("coded_block_pattern");
    read_CBP_CABAC(currMB, currSE, dep_dp);
    currMB->cbp = cbp = currSE->value1;

    int need_transform_size_flag = (((currMB->mb_type >= 1 && currMB->mb_type <= 3) ||
      (IS_DIRECT(currMB) && p_Vid->active_sps->direct_8x8_inference_flag) ||
      (currMB->NoMbPartLessThan8x8Flag))
      && currMB->mb_type != I8MB && currMB->mb_type != I4MB
      && (currMB->cbp & 15)
      && currSlice->Transform8x8Mode);

    if (need_transform_size_flag)
    {
      TRACE_STRING("transform_size_8x8_flag");
      readMB_transform_size_flag_CABAC(currMB, currSE, dep_dp);
      currMB->luma_transform_size_8x8_flag = (Boolean)currSE->value1;
    }

    if (cbp != 0)
    {
      read_delta_quant_cabac(currMB, currSE, dep_dp);
    }
  }
  else
  {
    cbp = currMB->cbp;
    read_delta_quant_cabac(currMB, currSE, dep_dp);

    const byte *pos_scan_4x4 = pos_scan4x4[0];
    int **cof = currSlice->cof[0];
    currSE->context = LUMA_16DC;

    int level = 1;
    for (int k = 0; (k < 17) && (level != 0); ++k)
    {
      readRunLevel_CABAC(currMB, currSE, dep_dp);
      level = currSE->value1;

      if (level != 0)
      {
        pos_scan_4x4 += (2 * currSE->value2);
        int i0 = ((*pos_scan_4x4++) << 2);
        int j0 = ((*pos_scan_4x4++) << 2);
        cof[j0][i0] = level;
      }
    }

    if (currMB->is_lossless == FALSE)
      itrans_2(currMB, (ColorPlane)currSlice->colour_plane_id);
  }

  update_qp(currMB, currSlice->qp);

  int qp_per = p_Vid->qp_per_matrix[currMB->qp_scaled[currSlice->colour_plane_id]];
  int qp_rem = p_Vid->qp_rem_matrix[currMB->qp_scaled[currSlice->colour_plane_id]];

  if (cbp)
  {
    if (currMB->luma_transform_size_8x8_flag)
    {
      read_comp_coeff_8x8_MB_CABAC(currMB, currSE, dep_dp, PLANE_Y);
    }
    else
    {
      int (*InvLevelScale4x4)[4] = intra ? currSlice->InvLevelScale4x4_Intra[currSlice->colour_plane_id][qp_rem] : currSlice->InvLevelScale4x4_Inter[currSlice->colour_plane_id][qp_rem];
      read_comp_coeff_4x4_CABAC(currMB, currSE, dep_dp, PLANE_Y, InvLevelScale4x4, qp_per, cbp);
    }
  }

  return cbp;
}

/*!
 ************************************************************************
 * \brief
 *    Get coded block pattern and coefficients (run/level)
 *    from the NAL (YUV 4:2:0)
 ************************************************************************
 */
static void read_CBP_and_coeffs_from_NAL_CABAC_420(Macroblock *currMB)
{
  int i;
  int level;
  int qp_per_uv[2];
  int qp_rem_uv[2];
  SyntaxElement currSE;
  Slice *currSlice = currMB->p_Slice;
  VideoParameters *p_Vid = currMB->p_Vid;
  DataPartition *dP = &(currSlice->partArr[0]);
  DecodingEnvironmentPtr dep_dp = &(dP->de_cabac);

  int intra = (currMB->is_intra_block == TRUE);  
  int smb = ((p_Vid->type==SP_SLICE) && (currMB->is_intra_block == FALSE)) || (p_Vid->type == SI_SLICE && currMB->mb_type == SI4MB);
  StorablePicture *dec_picture = currSlice->dec_picture;
  int yuv = dec_picture->chroma_format_idc - 1;

  int (*InvLevelScale4x4)[4] = NULL;
  const byte (*pos_scan4x4)[2] = ((currSlice->structure == FRAME) && (!currMB->mb_field)) ? SNGL_SCAN : FIELD_SCAN;
  const byte *pos_scan_4x4;

  int cbp = read_luma_coeff_CABAC(currMB, &currSE, dep_dp);

  //init quant parameters for chroma 
  for(i=0; i < 2; ++i)
  {
    qp_per_uv[i] = p_Vid->qp_per_matrix[ currMB->qp_scaled[i + 1] ];
    qp_rem_uv[i] = p_Vid->qp_rem_matrix[ currMB->qp_scaled[i + 1] ];
  }

  //========================== CHROMA DC ============================
  //-----------------------------------------------------------------
  // chroma DC coeff
  if(cbp>15)
  {
    CBPStructure  *s_cbp = &currMB->s_cbp[0];
    int uv, ll, k, coef_ctr;

    for (ll = 0; ll < 3; ll += 2)
    {
      uv = ll >> 1;

      InvLevelScale4x4 = intra ? currSlice->InvLevelScale4x4_Intra[uv + 1][qp_rem_uv[uv]] : currSlice->InvLevelScale4x4_Inter[uv + 1][qp_rem_uv[uv]];
      //===================== CHROMA DC YUV420 ======================
      memset(currSlice->cofu, 0, 4 *sizeof(int));
      coef_ctr=-1;

      level = 1;
      currMB->is_v_block  = ll;
      currSE.context      = CHROMA_DC;

      for(k = 0; (k < (p_Vid->num_cdc_coeff + 1))&&(level!=0);++k)
      {
        readRunLevel_CABAC(currMB, &currSE, dep_dp);
        level = currSE.value1;

        if (level != 0)
        {
          s_cbp->blk |= 0xf0000 << (ll<<1) ;
          coef_ctr += currSE.value2 + 1;

          assert (coef_ctr < p_Vid->num_cdc_coeff);
          currSlice->cofu[coef_ctr] = level;
        }
      }

      if (smb || (currMB->is_lossless == TRUE)) // check to see if MB type is SPred or SIntra4x4
      {        
        currSlice->cof[uv + 1][0][0] = currSlice->cofu[0];
        currSlice->cof[uv + 1][0][4] = currSlice->cofu[1];
        currSlice->cof[uv + 1][4][0] = currSlice->cofu[2];
        currSlice->cof[uv + 1][4][4] = currSlice->cofu[3];
      }
      else
      {
        int temp[4];
        int scale_dc = InvLevelScale4x4[0][0];
        int **cof = currSlice->cof[uv + 1];

        ihadamard2x2(currSlice->cofu, temp);

        cof[0][0] = (((temp[0] * scale_dc) << qp_per_uv[uv]) >> 5);
        cof[0][4] = (((temp[1] * scale_dc) << qp_per_uv[uv]) >> 5);
        cof[4][0] = (((temp[2] * scale_dc) << qp_per_uv[uv]) >> 5);
        cof[4][4] = (((temp[3] * scale_dc) << qp_per_uv[uv]) >> 5);
      }          
    }      
  }

  //========================== CHROMA AC ============================
  //-----------------------------------------------------------------
  // chroma AC coeff, all zero fram start_scan
  if (cbp >31)
  {
    currSE.context      = CHROMA_AC;

    if(currMB->is_lossless == FALSE)
    {
      int b4, b8, uv, k;
      int **cof;
      CBPStructure  *s_cbp = &currMB->s_cbp[0];
      for (b8=0; b8 < p_Vid->num_blk8x8_uv; ++b8)
      {
        currMB->is_v_block = uv = (b8 > ((p_Vid->num_uv_blocks) - 1 ));
        InvLevelScale4x4 = intra ? currSlice->InvLevelScale4x4_Intra[uv + 1][qp_rem_uv[uv]] : currSlice->InvLevelScale4x4_Inter[uv + 1][qp_rem_uv[uv]];
        cof = currSlice->cof[uv + 1];

        for (b4 = 0; b4 < 4; ++b4)
        {
          int i0, j0;
          int blk_x = cofuv_blk_x[yuv][b8][b4];
          int blk_y = cofuv_blk_y[yuv][b8][b4];

          currMB->subblock_y = subblk_offset_y[yuv][b8][b4];
          currMB->subblock_x = subblk_offset_x[yuv][b8][b4];

          pos_scan_4x4 = pos_scan4x4[1];
          level = 1;

          for(k = 0; (k < 16) && (level != 0);++k)
          {
            readRunLevel_CABAC(currMB, &currSE, dep_dp);
            level = currSE.value1;

            if (level != 0)
            {
              s_cbp->blk |= i64_power2(cbp_blk_chroma[b8][b4]);
              pos_scan_4x4 += (currSE.value2 << 1);

              i0 = *pos_scan_4x4++;
              j0 = *pos_scan_4x4++;

              cof[(blk_y<<2) + j0][(blk_x<<2) + i0] = rshift_rnd_sf((level * InvLevelScale4x4[j0][i0])<<qp_per_uv[uv], 4);
            }
          }
        }
      }
    }
    else
    {
      CBPStructure  *s_cbp = &currMB->s_cbp[0];
      int b4, b8, k;
      int uv;
      for (b8=0; b8 < p_Vid->num_blk8x8_uv; ++b8)
      {
        currMB->is_v_block = uv = (b8 > ((p_Vid->num_uv_blocks) - 1 ));

        for (b4=0; b4 < 4; ++b4)
        {
          int i0, j0;
          int blk_x = cofuv_blk_x[yuv][b8][b4];
          int blk_y = cofuv_blk_y[yuv][b8][b4];

          pos_scan_4x4 = pos_scan4x4[1];
          level=1;

          currMB->subblock_y = subblk_offset_y[yuv][b8][b4];
          currMB->subblock_x = subblk_offset_x[yuv][b8][b4];

          for(k=0;(k<16)&&(level!=0);++k)
          {
            readRunLevel_CABAC(currMB, &currSE, dep_dp);
            level = currSE.value1;

            if (level != 0)
            {
              s_cbp->blk |= i64_power2(cbp_blk_chroma[b8][b4]);
              pos_scan_4x4 += (currSE.value2 << 1);

              i0 = *pos_scan_4x4++;
              j0 = *pos_scan_4x4++;

              currSlice->cof[uv + 1][(blk_y<<2) + j0][(blk_x<<2) + i0] = level;
            }
          } 
        }
      } 
    }     
  }  
}

/*!
 ************************************************************************
 * \brief
 *    Get coded block pattern and coefficients (run/level)
 *    from the NAL (YUV 4:0:0 monochrome)
 ************************************************************************
 */
static void read_CBP_and_coeffs_from_NAL_CABAC_400(Macroblock *currMB)
{
  SyntaxElement currSE;
  Slice *currSlice = currMB->p_Slice;
  DataPartition *dP = &(currSlice->partArr[0]);
  DecodingEnvironmentPtr dep_dp = &(dP->de_cabac);

  read_luma_coeff_CABAC(currMB, &currSE, dep_dp);
}

/*!
 ************************************************************************
 * \brief
 *    Get coded block pattern and coefficients (run/level)
 *    from the NAL (YUV 4:4:4)
 ************************************************************************
 */
static void read_CBP_and_coeffs_from_NAL_CABAC_444(Macroblock *currMB)
{
  int k;
  int level;
  int qp_per, qp_rem;
  int qp_per_uv[2];
  int qp_rem_uv[2];
  int uv;
  int coef_ctr;
  int i0, j0;

  SyntaxElement currSE;
  Slice *currSlice = currMB->p_Slice;
  DataPartition *dP = &(currSlice->partArr[0]);
  DecodingEnvironmentPtr dep_dp = &(dP->de_cabac);
  VideoParameters *p_Vid = currMB->p_Vid;

  int intra = (currMB->is_intra_block == TRUE);

  int (*InvLevelScale4x4)[4] = NULL;
  const byte (*pos_scan4x4)[2] = ((currSlice->structure == FRAME) && (!currMB->mb_field)) ? SNGL_SCAN : FIELD_SCAN;

  int cbp = read_luma_coeff_CABAC(currMB, &currSE, dep_dp);

  for (uv = 0; uv < 2; ++uv )
  {
    /*----------------------16x16DC Luma_Add----------------------*/
    if (IS_I16MB (currMB)) // read DC coeffs for new intra modes       
    {
      if( (p_Vid->separate_colour_plane_flag != 0) )
        currSE.context = LUMA_16DC; 
      else
        currSE.context = (uv==0) ? CB_16DC : CR_16DC;

      coef_ctr = -1;
      level = 1;                            // just to get inside the loop

      for(k=0;(k<17) && (level!=0);++k)
      {
        readRunLevel_CABAC(currMB, &currSE, dep_dp);
        level = currSE.value1;

        if (level != 0)                     // leave if level == 0
        {
          coef_ctr += currSE.value2 + 1;

          i0 = pos_scan4x4[coef_ctr][0];
          j0 = pos_scan4x4[coef_ctr][1];
          currSlice->cof[uv + 1][j0<<2][i0<<2] = level;
        }                        
      } //k loop

      if(currMB->is_lossless == FALSE)
      {
        itrans_2(currMB, (ColorPlane) (uv + 1)); // transform new intra DC
      }
    } //IS_I16MB

    update_qp(currMB, currSlice->qp);

    qp_per = p_Vid->qp_per_matrix[ (currSlice->qp + p_Vid->bitdepth_luma_qp_scale) ];
    qp_rem = p_Vid->qp_rem_matrix[ (currSlice->qp + p_Vid->bitdepth_luma_qp_scale) ];

    //init constants for every chroma qp offset
    qp_per_uv[uv] = p_Vid->qp_per_matrix[ (currMB->qpc[uv] + p_Vid->bitdepth_chroma_qp_scale) ];
    qp_rem_uv[uv] = p_Vid->qp_rem_matrix[ (currMB->qpc[uv] + p_Vid->bitdepth_chroma_qp_scale) ];

    InvLevelScale4x4 = intra? currSlice->InvLevelScale4x4_Intra[uv + 1][qp_rem_uv[uv]] : currSlice->InvLevelScale4x4_Inter[uv + 1][qp_rem_uv[uv]];

    if (cbp)
    {
      if(currMB->luma_transform_size_8x8_flag) 
      {
        //======= 8x8 transform size & CABAC ========
        read_comp_coeff_8x8_MB_CABAC (currMB, &currSE, dep_dp, (ColorPlane) (PLANE_U + uv)); 
      }
      else //4x4
      {        
        read_comp_coeff_4x4_CABAC (currMB, &currSE, dep_dp, (ColorPlane) (PLANE_U + uv), InvLevelScale4x4,  qp_per_uv[uv], cbp);
      }
    }
  } 
}

/*!
 ************************************************************************
 * \brief
 *    Get coded block pattern and coefficients (run/level)
 *    from the NAL (YUV 4:2:2)
 ************************************************************************
 */
static void read_CBP_and_coeffs_from_NAL_CABAC_422(Macroblock *currMB)
{
  int i,j,k;
  int level;
  int qp_per_uv[2];
  int qp_rem_uv[2];
  int uv;
  int coef_ctr;
  int i0, j0;
  int ll;
  int m6[4];

  SyntaxElement currSE;
  Slice *currSlice = currMB->p_Slice;
  DataPartition *dP = &(currSlice->partArr[0]);
  DecodingEnvironmentPtr dep_dp = &(dP->de_cabac);
  VideoParameters *p_Vid = currMB->p_Vid;

  int intra = (currMB->is_intra_block == TRUE);
  StorablePicture *dec_picture = currSlice->dec_picture;
  int yuv = dec_picture->chroma_format_idc - 1;

  int (*InvLevelScale4x4)[4] = NULL;
  const byte (*pos_scan4x4)[2] = ((currSlice->structure == FRAME) && (!currMB->mb_field)) ? SNGL_SCAN : FIELD_SCAN;
  const byte *pos_scan_4x4;

  int cbp = read_luma_coeff_CABAC(currMB, &currSE, dep_dp);

  //init quant parameters for chroma 
  for(i=0; i < 2; ++i)
  {
    qp_per_uv[i] = p_Vid->qp_per_matrix[ currMB->qp_scaled[i + 1] ];
    qp_rem_uv[i] = p_Vid->qp_rem_matrix[ currMB->qp_scaled[i + 1] ];
  }

  //========================== CHROMA DC ============================
  //-----------------------------------------------------------------
  // chroma DC coeff
  if(cbp>15)
  {      
    for (ll=0;ll<3;ll+=2)
    {
      uv = ll>>1;
      {
        int **imgcof = currSlice->cof[uv + 1];
        int m3[2][4] = {{0,0,0,0},{0,0,0,0}};
        int m4[2][4] = {{0,0,0,0},{0,0,0,0}};
        int qp_per_uv_dc = p_Vid->qp_per_matrix[ (currMB->qpc[uv] + 3 + p_Vid->bitdepth_chroma_qp_scale) ];       //for YUV422 only
        int qp_rem_uv_dc = p_Vid->qp_rem_matrix[ (currMB->qpc[uv] + 3 + p_Vid->bitdepth_chroma_qp_scale) ];       //for YUV422 only
        if (intra)
          InvLevelScale4x4 = currSlice->InvLevelScale4x4_Intra[uv + 1][qp_rem_uv_dc];
        else 
          InvLevelScale4x4 = currSlice->InvLevelScale4x4_Inter[uv + 1][qp_rem_uv_dc];

        //===================== CHROMA DC YUV422 ======================
        {
          CBPStructure  *s_cbp = &currMB->s_cbp[0];
          coef_ctr=-1;
          level=1;
          for(k=0;(k<9)&&(level!=0);++k)
          {
            currSE.context      = CHROMA_DC_2x4;
            currMB->is_v_block     = ll;

            readRunLevel_CABAC(currMB, &currSE, dep_dp);
            level = currSE.value1;

            if (level != 0)
            {
              s_cbp->blk |= ((int64)0xff0000) << (ll<<2) ;
              coef_ctr += currSE.value2 + 1;
              assert (coef_ctr < p_Vid->num_cdc_coeff);
              i0=SCAN_YUV422[coef_ctr][0];
              j0=SCAN_YUV422[coef_ctr][1];

              m3[i0][j0]=level;
            }
          }
        }
        // inverse CHROMA DC YUV422 transform
        // horizontal
        if(currMB->is_lossless == FALSE)
        {
          m4[0][0] = m3[0][0] + m3[1][0];
          m4[0][1] = m3[0][1] + m3[1][1];
          m4[0][2] = m3[0][2] + m3[1][2];
          m4[0][3] = m3[0][3] + m3[1][3];

          m4[1][0] = m3[0][0] - m3[1][0];
          m4[1][1] = m3[0][1] - m3[1][1];
          m4[1][2] = m3[0][2] - m3[1][2];
          m4[1][3] = m3[0][3] - m3[1][3];

          for (i = 0; i < 2; ++i)
          {
            m6[0] = m4[i][0] + m4[i][2];
            m6[1] = m4[i][0] - m4[i][2];
            m6[2] = m4[i][1] - m4[i][3];
            m6[3] = m4[i][1] + m4[i][3];

            imgcof[ 0][i<<2] = m6[0] + m6[3];
            imgcof[ 4][i<<2] = m6[1] + m6[2];
            imgcof[ 8][i<<2] = m6[1] - m6[2];
            imgcof[12][i<<2] = m6[0] - m6[3];
          }//for (i=0;i<2;++i)

          for(j = 0;j < p_Vid->mb_cr_size_y; j += BLOCK_SIZE)
          {
            for(i=0;i < p_Vid->mb_cr_size_x;i+=BLOCK_SIZE)
            {
              imgcof[j][i] = rshift_rnd_sf((imgcof[j][i] * InvLevelScale4x4[0][0]) << qp_per_uv_dc, 6);
            }
          }
        }
        else
        {
          for(j=0;j<4;++j)
          {
            for(i=0;i<2;++i)                
            {
              currSlice->cof[uv + 1][j<<2][i<<2] = m3[i][j];
            }
          }
        }

      }
    }//for (ll=0;ll<3;ll+=2)      
  }

  //========================== CHROMA AC ============================
  //-----------------------------------------------------------------
  // chroma AC coeff, all zero fram start_scan
  if (cbp>31)
  {
    currSE.context      = CHROMA_AC;

    if(currMB->is_lossless == FALSE)
    {          
      int b4, b8, uv, k;
      int **cof;
      CBPStructure  *s_cbp = &currMB->s_cbp[0];
      for (b8=0; b8 < p_Vid->num_blk8x8_uv; ++b8)
      {
        currMB->is_v_block = uv = (b8 > ((p_Vid->num_uv_blocks) - 1 ));
        InvLevelScale4x4 = intra ? currSlice->InvLevelScale4x4_Intra[uv + 1][qp_rem_uv[uv]] : currSlice->InvLevelScale4x4_Inter[uv + 1][qp_rem_uv[uv]];
        cof = currSlice->cof[uv + 1];

        for (b4 = 0; b4 < 4; ++b4)
        {
          int blk_x = cofuv_blk_x[yuv][b8][b4];
          int blk_y = cofuv_blk_y[yuv][b8][b4];

          currMB->subblock_y = subblk_offset_y[yuv][b8][b4];
          currMB->subblock_x = subblk_offset_x[yuv][b8][b4];

          pos_scan_4x4 = pos_scan4x4[1];
          level=1;

          for(k = 0; (k < 16) && (level != 0);++k)
          {
            readRunLevel_CABAC(currMB, &currSE, dep_dp);
            level = currSE.value1;

            if (level != 0)
            {
              s_cbp->blk |= i64_power2(cbp_blk_chroma[b8][b4]);
              pos_scan_4x4 += (currSE.value2 << 1);

              i0 = *pos_scan_4x4++;
              j0 = *pos_scan_4x4++;

              cof[(blk_y<<2) + j0][(blk_x<<2) + i0] = rshift_rnd_sf((level * InvLevelScale4x4[j0][i0])<<qp_per_uv[uv], 4);
            }
          }
        }
      }
    }
    else
    {
      CBPStructure  *s_cbp = &currMB->s_cbp[0];
      int b4, b8, k;
      int uv;
      for (b8=0; b8 < p_Vid->num_blk8x8_uv; ++b8)
      {
        currMB->is_v_block = uv = (b8 > ((p_Vid->num_uv_blocks) - 1 ));

        for (b4=0; b4 < 4; ++b4)
        {
          int blk_x = cofuv_blk_x[yuv][b8][b4];
          int blk_y = cofuv_blk_y[yuv][b8][b4];

          pos_scan_4x4 = pos_scan4x4[1];
          level=1;

          currMB->subblock_y = subblk_offset_y[yuv][b8][b4];
          currMB->subblock_x = subblk_offset_x[yuv][b8][b4];

          for(k=0;(k<16)&&(level!=0);++k)
          {
            readRunLevel_CABAC(currMB, &currSE, dep_dp);
            level = currSE.value1;

            if (level != 0)
            {
              s_cbp->blk |= i64_power2(cbp_blk_chroma[b8][b4]);
              pos_scan_4x4 += (currSE.value2 << 1);

              i0 = *pos_scan_4x4++;
              j0 = *pos_scan_4x4++;

              currSlice->cof[uv + 1][(blk_y<<2) + j0][(blk_x<<2) + i0] = level;
            }
          } 
        }
      } 
    }
  }
}

void set_read_CBP_and_coeffs_cabac(Slice *currSlice)
{
  switch (currSlice->p_Vid->active_sps->chroma_format_idc)
  {
  case YUV444:
    if (currSlice->p_Vid->separate_colour_plane_flag == 0)
    {
      currSlice->read_CBP_and_coeffs_from_NAL = read_CBP_and_coeffs_from_NAL_CABAC_444;
    }
    else
    {
      currSlice->read_CBP_and_coeffs_from_NAL = read_CBP_and_coeffs_from_NAL_CABAC_400;
    }
    break;
  case YUV422:
    currSlice->read_CBP_and_coeffs_from_NAL = read_CBP_and_coeffs_from_NAL_CABAC_422;
    break;
  case YUV420:
    currSlice->read_CBP_and_coeffs_from_NAL = read_CBP_and_coeffs_from_NAL_CABAC_420;
    break;
  case YUV400:
    currSlice->read_CBP_and_coeffs_from_NAL = read_CBP_and_coeffs_from_NAL_CABAC_400;
    break;
  default:
    assert (1);
    currSlice->read_CBP_and_coeffs_from_NAL = NULL;
    break;
  }
}
