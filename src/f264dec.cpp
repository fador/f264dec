
/*!
 ***********************************************************************
 *  \mainpage
 *     This is the H.264/AVC decoder reference software. For detailed documentation
 *     see the comments in each file.
 *
 *     The JM software web site is located at:
 *     http://iphome.hhi.de/suehring/tml
 *
 *     For bug reporting and known issues see:
 *     https://ipbt.hhi.fraunhofer.de
 *
 *  \author
 *     The main contributors are listed in contributors.h
 *
 *  \version
 *     JM 18.4 (FRExt)
 *
 *  \note
 *     tags are used for document system "doxygen"
 *     available at http://www.doxygen.org
 */
/*!
 *  \file
 *     ldecod.c
 *  \brief
 *     H.264/AVC reference decoder project main()
 *  \author
 *     Main contributors (see contributors.h for copyright, address and affiliation details)
 *     - Inge Lille-Langøy       <inge.lille-langoy@telenor.com>
 *     - Rickard Sjoberg         <rickard.sjoberg@era.ericsson.se>
 *     - Stephan Wenger          <stewe@cs.tu-berlin.de>
 *     - Jani Lainema            <jani.lainema@nokia.com>
 *     - Sebastian Purreiter     <sebastian.purreiter@mch.siemens.de>
 *     - Byeong-Moon Jeon        <jeonbm@lge.com>
 *     - Gabi Blaettermann
 *     - Ye-Kui Wang             <wyk@ieee.org>
 *     - Valeri George
 *     - Karsten Suehring
 *
 ***********************************************************************
 */

#include "contributors.h"

//#include <sys/stat.h>

#include "global.h"
#include "annexb.h"
#include "threading/frame_pipeline.h"
#include "image.h"
#include "memalloc.h"
#include "mc_prediction.h"
#include "mbuffer.h"
#include "fmo.h"
#include "output.h"
#include "cabac.h"
#include "parset.h"
#include "sei.h"
#include "erc_api.h"
#include "quant.h"
#include "block.h"
#include "nalu.h"
#include "loopfilter.h"
#include "h264decoder.h"
#include "strategies/strategyselector.h"
#include "threading/threadqueue.h"

#define LOGFILE     "log.dec"
#define DATADECFILE "dataDec.txt"
#define TRACEFILE   "trace_dec.txt"

// Decoder definition. This should be the only global variable in the entire
// software. Global variables should be avoided.
DecoderParams  *p_Dec;
char errortext[ET_SIZE];

// Prototypes of static functions
static void Report      (VideoParameters *p_Vid);
static void init        (VideoParameters *p_Vid);
void free_slice         (Slice *currSlice);

void init_frext(VideoParameters *p_Vid);

/*!
 ************************************************************************
 * \brief
 *    Error handling procedure. Print error message to stderr and exit
 *    with supplied code.
 * \param text
 *    Error message
 * \param code
 *    Exit code
 ************************************************************************
 */
void error(char *text, int code)
{
  if (p_Dec && p_Dec->p_Inp && p_Dec->p_Inp->error_cb)
  {
    p_Dec->p_Inp->error_cb(p_Dec->p_Inp->error_cb_user_data, code, text);
    return;
  }
  if (!p_Dec || !p_Dec->p_Inp || !p_Dec->p_Inp->silent)
  {
    fprintf(stderr, "%s\n", text);
    fflush(stderr);
  }
  if (p_Dec && p_Dec->p_Vid && p_Dec->p_Vid->p_Dpb_layer[0])
  {
    flush_dpb(p_Dec->p_Vid->p_Dpb_layer[0]);
  }
}

static void reset_dpb( VideoParameters *p_Vid, DecodedPictureBuffer *p_Dpb )
{
  p_Dpb->p_Vid = p_Vid;
  p_Dpb->init_done = 0;
}
/*!
 ***********************************************************************
 * \brief
 *    Allocate the Video Parameters structure
 * \par  Output:
 *    Video Parameters VideoParameters *p_Vid
 ***********************************************************************
 */
static int alloc_video_params( VideoParameters **p_Vid)
{
  int i;
  if ((*p_Vid   =  (VideoParameters *) calloc(1, sizeof(VideoParameters)))==NULL) 
  {
    no_mem_exit("alloc_video_params: p_Vid");
    return -1;
  }

  if (((*p_Vid)->old_slice = (OldSliceParams *) calloc(1, sizeof(OldSliceParams)))==NULL) 
  {
    no_mem_exit("alloc_video_params: p_Vid->old_slice");
    return -1;
  }

  if (((*p_Vid)->snr =  (SNRParameters *)calloc(1, sizeof(SNRParameters)))==NULL) 
  {
    no_mem_exit("alloc_video_params: p_Vid->snr");  
    return -1;
  }

  // Allocate new dpb buffer
  for (i = 0; i < MAX_NUM_DPB_LAYERS; i++)
  {
    if (((*p_Vid)->p_Dpb_layer[i] =  (DecodedPictureBuffer*)calloc(1, sizeof(DecodedPictureBuffer)))==NULL) 
    {
      no_mem_exit("alloc_video_params: p_Vid->p_Dpb_layer[i]");
      return -1;
    }
    (*p_Vid)->p_Dpb_layer[i]->layer_id = i;
    reset_dpb(*p_Vid, (*p_Vid)->p_Dpb_layer[i]);
    if(((*p_Vid)->p_EncodePar[i] = (CodingParameters *)calloc(1, sizeof(CodingParameters))) == NULL)
    {
      no_mem_exit("alloc_video_params:p_Vid->p_EncodePar[i]");
      return -1;
    }
    ((*p_Vid)->p_EncodePar[i])->layer_id = i;
    if(((*p_Vid)->p_LayerPar[i] = (LayerParameters *)calloc(1, sizeof(LayerParameters))) == NULL)
    {
      no_mem_exit("alloc_video_params:p_Vid->p_LayerPar[i]");
      return -1;
    }
    ((*p_Vid)->p_LayerPar[i])->layer_id = i;
  }
  (*p_Vid)->global_init_done[0] = (*p_Vid)->global_init_done[1] = 0;

  if (((*p_Vid)->seiToneMapping =  (ToneMappingSEI*)calloc(1, sizeof(ToneMappingSEI)))==NULL) 
  {
    no_mem_exit("alloc_video_params: (*p_Vid)->seiToneMapping");  
    return -1;
  }

  if(((*p_Vid)->ppSliceList = (Slice **) calloc(MAX_NUM_DECSLICES, sizeof(Slice *))) == NULL)
  {
    no_mem_exit("alloc_video_params: p_Vid->ppSliceList");
    return -1;
  }
  (*p_Vid)->iNumOfSlicesAllocated = MAX_NUM_DECSLICES;
  //(*p_Vid)->currentSlice = NULL;
  (*p_Vid)->pNextSlice = NULL;
  (*p_Vid)->nalu = AllocNALU(MAX_CODED_FRAME_SIZE);
  (*p_Vid)->pDecOuputPic = (DecodedPicList *)calloc(1, sizeof(DecodedPicList));
  (*p_Vid)->pNextPPS = AllocPPS();
  (*p_Vid)->first_sps = TRUE;
  return 0;
}


/*!
 ***********************************************************************
 * \brief
 *    Allocate the Input structure
 * \par  Output:
 *    Input Parameters InputParameters *p_Vid
 ***********************************************************************
 */
static int alloc_params( InputParameters **p_Inp )
{
  if ((*p_Inp = (InputParameters *) calloc(1, sizeof(InputParameters)))==NULL) 
  {
    no_mem_exit("alloc_params: p_Inp");
    return -1;
  }
  return 0;
}

  /*!
 ***********************************************************************
 * \brief
 *    Allocate the Decoder Structure
 * \par  Output:
 *    Decoder Parameters
 ***********************************************************************
 */
static int alloc_decoder( DecoderParams **p_Dec)
{
  if ((*p_Dec = (DecoderParams *) calloc(1, sizeof(DecoderParams)))==NULL) 
  {
    fprintf(stderr, "alloc_decoder: p_Dec\n");
    return -1;
  }

  if (alloc_video_params(&((*p_Dec)->p_Vid)) != 0)
    return -1;
  if (alloc_params(&((*p_Dec)->p_Inp)) != 0)
    return -1;
  (*p_Dec)->p_Vid->p_Inp = (*p_Dec)->p_Inp;
  (*p_Dec)->p_trace = NULL;
  (*p_Dec)->bufferSize = 0;
  (*p_Dec)->bitcounter = 0;
  return 0;
}

/*!
 ***********************************************************************
 * \brief
 *    Free the Image structure
 * \par  Input:
 *    Image Parameters VideoParameters *p_Vid
 ***********************************************************************
 */
static void free_img( VideoParameters *p_Vid)
{
  int i;
  if (p_Vid != NULL)
  {
    if ( p_Vid->p_Inp->FileFormat == PAR_OF_ANNEXB )
    {
      free_annex_b (&p_Vid->annex_b);
    }
    if (p_Vid->seiToneMapping != NULL)
    {
      free (p_Vid->seiToneMapping);
      p_Vid->seiToneMapping = NULL;
    }

    // Free new dpb layers
    for (i = 0; i < MAX_NUM_DPB_LAYERS; i++)
    {
      if (p_Vid->p_Dpb_layer[i] != NULL)
      {
        free (p_Vid->p_Dpb_layer[i]);
        p_Vid->p_Dpb_layer[i] = NULL;
      }
      if(p_Vid->p_EncodePar[i])
      {
        free(p_Vid->p_EncodePar[i]);
        p_Vid->p_EncodePar[i] = NULL;
      }
      if(p_Vid->p_LayerPar[i])
      {
        free(p_Vid->p_LayerPar[i]);
        p_Vid->p_LayerPar[i] = NULL;
      }
    }    
    if (p_Vid->snr != NULL)
    {
      free (p_Vid->snr);
      p_Vid->snr = NULL;
    }
    if (p_Vid->old_slice != NULL)
    {
      free (p_Vid->old_slice);
      p_Vid->old_slice = NULL;
    }

    if(p_Vid->pNextSlice)
    {
      free_slice(p_Vid->pNextSlice);
      p_Vid->pNextSlice=NULL;
    }
    if(p_Vid->ppSliceList)
    {
      int i;
      for(i=0; i<p_Vid->iNumOfSlicesAllocated; i++)
        if(p_Vid->ppSliceList[i])
          free_slice(p_Vid->ppSliceList[i]);
      free(p_Vid->ppSliceList);
    }
    if(p_Vid->nalu)
    {
      FreeNALU(p_Vid->nalu);
      p_Vid->nalu=NULL;
    }
    //free memory;
    FreeDecPicList(p_Vid->pDecOuputPic);
    if(p_Vid->pNextPPS)
    {
      FreePPS(p_Vid->pNextPPS);
      p_Vid->pNextPPS = NULL;
    }

    // clear decoder statistics

    free (p_Vid);
    p_Vid = NULL;
  }
}

void FreeDecPicList(DecodedPicList *pDecPicList)
{
  while(pDecPicList)
  {
    DecodedPicList *pPicNext = pDecPicList->pNext;
    if(pDecPicList->pY)
    {
      free(pDecPicList->pY);
      pDecPicList->pY = NULL;
      pDecPicList->pU = NULL;
      pDecPicList->pV = NULL;
    }
    free(pDecPicList);
    pDecPicList = pPicNext;
  }
}

/*!
 ***********************************************************************
 * \brief
 *    Initilize some arrays
 ***********************************************************************
 */
static void init(VideoParameters *p_Vid)  //!< video parameters
{
  //int i;
  InputParameters *p_Inp = p_Vid->p_Inp;
  p_Vid->oldFrameSizeInMbs = (unsigned int) -1;

  p_Vid->imgY_ref  = NULL;
  p_Vid->imgUV_ref = NULL;

  p_Vid->recovery_point = 0;
  p_Vid->recovery_point_found = 0;
  p_Vid->recovery_poc = 0x7fffffff; /* set to a max value */

  p_Vid->idr_psnr_number = p_Inp->ref_offset;
  p_Vid->psnr_number=0;

  p_Vid->number = 0;
  p_Vid->type = I_SLICE;

  //p_Vid->dec_ref_pic_marking_buffer = NULL;

  p_Vid->g_nFrame = 0;
  // B pictures
  p_Vid->Bframe_ctr = p_Vid->snr->frame_ctr = 0;

  // time for total decoding session
  p_Vid->tot_time = 0;

  p_Vid->dec_picture = NULL;
  /*// reference flag initialization
  for(i=0;i<17;++i)
  {
  p_Vid->ref_flag[i] = 1;
  }*/

  p_Vid->MbToSliceGroupMap = NULL;
  p_Vid->MapUnitToSliceGroupMap = NULL;

  p_Vid->LastAccessUnitExists  = 0;
  p_Vid->NALUCount = 0;


  p_Vid->out_buffer = NULL;
  p_Vid->pending_output = NULL;
  p_Vid->pending_output_state = FRAME;
  p_Vid->recovery_flag = 0;


  init_tone_mapping_sei(p_Vid->seiToneMapping);


  p_Vid->newframe = 0;
  p_Vid->previous_frame_num = 0;

  p_Vid->iLumaPadX = MCBUF_LUMA_PAD_X;
  p_Vid->iLumaPadY = MCBUF_LUMA_PAD_Y;
  p_Vid->iChromaPadX = MCBUF_CHROMA_PAD_X;
  p_Vid->iChromaPadY = MCBUF_CHROMA_PAD_Y;

  p_Vid->iPostProcess = 0;
  p_Vid->bDeblockEnable = 0x3;
  p_Vid->last_dec_view_id = -1;
  p_Vid->last_dec_layer_id = -1;

}

/*!
 ***********************************************************************
 * \brief
 *    Initialize FREXT variables
 ***********************************************************************
 */
void init_frext(VideoParameters *p_Vid)  //!< video parameters
{
  //pel bitdepth init
  p_Vid->bitdepth_luma_qp_scale   = 6 * (p_Vid->bitdepth_luma - 8);

  if(p_Vid->bitdepth_luma > p_Vid->bitdepth_chroma || p_Vid->active_sps->chroma_format_idc == YUV400)
    p_Vid->pic_unit_bitsize_on_disk = (p_Vid->bitdepth_luma > 8)? 16:8;
  else
    p_Vid->pic_unit_bitsize_on_disk = (p_Vid->bitdepth_chroma > 8)? 16:8;
  p_Vid->dc_pred_value_comp[0]    = 1<<(p_Vid->bitdepth_luma - 1);
  p_Vid->max_pel_value_comp[0] = (1<<p_Vid->bitdepth_luma) - 1;
  p_Vid->mb_size[0][0] = p_Vid->mb_size[0][1] = MB_BLOCK_SIZE;

  if (p_Vid->active_sps->chroma_format_idc != YUV400)
  {
    //for chrominance part
    p_Vid->bitdepth_chroma_qp_scale = 6 * (p_Vid->bitdepth_chroma - 8);
    p_Vid->dc_pred_value_comp[1]    = (1 << (p_Vid->bitdepth_chroma - 1));
    p_Vid->dc_pred_value_comp[2]    = p_Vid->dc_pred_value_comp[1];
    p_Vid->max_pel_value_comp[1]    = (1 << p_Vid->bitdepth_chroma) - 1;
    p_Vid->max_pel_value_comp[2]    = (1 << p_Vid->bitdepth_chroma) - 1;
    p_Vid->num_blk8x8_uv = (1 << p_Vid->active_sps->chroma_format_idc) & (~(0x1));
    p_Vid->num_uv_blocks = (p_Vid->num_blk8x8_uv >> 1);
    p_Vid->num_cdc_coeff = (p_Vid->num_blk8x8_uv << 1);
    p_Vid->mb_size[1][0] = p_Vid->mb_size[2][0] = p_Vid->mb_cr_size_x  = (p_Vid->active_sps->chroma_format_idc==YUV420 || p_Vid->active_sps->chroma_format_idc==YUV422)?  8 : 16;
    p_Vid->mb_size[1][1] = p_Vid->mb_size[2][1] = p_Vid->mb_cr_size_y  = (p_Vid->active_sps->chroma_format_idc==YUV444 || p_Vid->active_sps->chroma_format_idc==YUV422)? 16 :  8;

    p_Vid->subpel_x    = p_Vid->mb_cr_size_x == 8 ? 7 : 3;
    p_Vid->subpel_y    = p_Vid->mb_cr_size_y == 8 ? 7 : 3;
    p_Vid->shiftpel_x  = p_Vid->mb_cr_size_x == 8 ? 3 : 2;
    p_Vid->shiftpel_y  = p_Vid->mb_cr_size_y == 8 ? 3 : 2;
    p_Vid->total_scale = p_Vid->shiftpel_x + p_Vid->shiftpel_y;
  }
  else
  {
    p_Vid->bitdepth_chroma_qp_scale = 0;
    p_Vid->max_pel_value_comp[1] = 0;
    p_Vid->max_pel_value_comp[2] = 0;
    p_Vid->num_blk8x8_uv = 0;
    p_Vid->num_uv_blocks = 0;
    p_Vid->num_cdc_coeff = 0;
    p_Vid->mb_size[1][0] = p_Vid->mb_size[2][0] = p_Vid->mb_cr_size_x  = 0;
    p_Vid->mb_size[1][1] = p_Vid->mb_size[2][1] = p_Vid->mb_cr_size_y  = 0;
    p_Vid->subpel_x      = 0;
    p_Vid->subpel_y      = 0;
    p_Vid->shiftpel_x    = 0;
    p_Vid->shiftpel_y    = 0;
    p_Vid->total_scale   = 0;
  }

  p_Vid->mb_cr_size = p_Vid->mb_cr_size_x * p_Vid->mb_cr_size_y;
  p_Vid->mb_size_blk[0][0] = p_Vid->mb_size_blk[0][1] = p_Vid->mb_size[0][0] >> 2;
  p_Vid->mb_size_blk[1][0] = p_Vid->mb_size_blk[2][0] = p_Vid->mb_size[1][0] >> 2;
  p_Vid->mb_size_blk[1][1] = p_Vid->mb_size_blk[2][1] = p_Vid->mb_size[1][1] >> 2;

  p_Vid->mb_size_shift[0][0] = p_Vid->mb_size_shift[0][1] = CeilLog2_sf (p_Vid->mb_size[0][0]);
  p_Vid->mb_size_shift[1][0] = p_Vid->mb_size_shift[2][0] = CeilLog2_sf (p_Vid->mb_size[1][0]);
  p_Vid->mb_size_shift[1][1] = p_Vid->mb_size_shift[2][1] = CeilLog2_sf (p_Vid->mb_size[1][1]);
}

/*!
 ************************************************************************
 * \brief
 *    Reports the gathered information to appropriate outputs
 *
 * \par Input:
 *    InputParameters *p_Inp,
 *    VideoParameters *p_Vid,
 *    struct snr_par *stat
 *
 * \par Output:
 *    None
 ************************************************************************
 */
static void Report(VideoParameters *p_Vid)
{
  if (!p_Vid || !p_Vid->p_Inp || p_Vid->p_Inp->silent)
    return;

  SNRParameters *snr = p_Vid->snr;
  p_Vid->tot_time = timenorm(p_Vid->tot_time);

  fprintf(stdout,"-------------------- Average SNR all frames ------------------------------\n");
  fprintf(stdout," SNR Y(dB)           : %5.2f\n",snr->snra[0]);
  fprintf(stdout," SNR U(dB)           : %5.2f\n",snr->snra[1]);
  fprintf(stdout," SNR V(dB)           : %5.2f\n",snr->snra[2]);
  fprintf(stdout," Total decoding time : %.3f sec (%.3f fps)[%d frm/%" FORMAT_OFF_T " ms]\n",p_Vid->tot_time*0.001,(snr->frame_ctr ) * 1000.0 / p_Vid->tot_time, snr->frame_ctr, p_Vid->tot_time);
  fprintf(stdout,"--------------------------------------------------------------------------\n");
  fprintf(stdout," Exit JM %s decoder, ver %s\n",JM, VERSION);
}

/*!
 ************************************************************************
 * \brief
 *    Allocates a stand-alone partition structure.  Structure should
 *    be freed by FreePartition();
 *    data structures
 *
 * \par Input:
 *    n: number of partitions in the array
 * \par return
 *    pointer to DataPartition Structure, zero-initialized
 ************************************************************************
 */

DataPartition *AllocPartition(int n)
{
  DataPartition *partArr, *dataPart;
  int i;

  partArr = (DataPartition *) calloc(n, sizeof(DataPartition));
  if (partArr == NULL)
  {
    snprintf(errortext, ET_SIZE, "AllocPartition: Memory allocation for Data Partition failed");
    error(errortext, 100);
    return NULL;
  }

  for (i = 0; i < n; ++i) // loop over all data partitions
  {
    dataPart = &(partArr[i]);
    dataPart->bitstream = (Bitstream *) calloc(1, sizeof(Bitstream));
    if (dataPart->bitstream == NULL)
    {
      snprintf(errortext, ET_SIZE, "AllocPartition: Memory allocation for Bitstream failed");
      error(errortext, 100);
      FreePartition(partArr, i);
      return NULL;
    }
    dataPart->bitstream->streamBuffer = (byte *) calloc(MAX_CODED_FRAME_SIZE, sizeof(byte));
    if (dataPart->bitstream->streamBuffer == NULL)
    {
      snprintf(errortext, ET_SIZE, "AllocPartition: Memory allocation for streamBuffer failed");
      error(errortext, 100);
      FreePartition(partArr, i + 1);
      return NULL;
    }
  }
  return partArr;
}




/*!
 ************************************************************************
 * \brief
 *    Frees a partition structure (array).
 *
 * \par Input:
 *    Partition to be freed, size of partition Array (Number of Partitions)
 *
 * \par return
 *    None
 *
 * \note
 *    n must be the same as for the corresponding call of AllocPartition
 ************************************************************************
 */
void FreePartition (DataPartition *dp, int n)
{
  int i;

  if (!dp) return;
  for (i=0; i<n; ++i)
  {
    if (dp[i].bitstream)
    {
      if (dp[i].bitstream->streamBuffer)
        free (dp[i].bitstream->streamBuffer);
      free (dp[i].bitstream);
    }
  }
  free (dp);
}


/*!
 ************************************************************************
 * \brief
 *    Allocates the slice structure along with its dependent
 *    data structures
 *
 * \par Input:
 *    Input Parameters InputParameters *p_Inp,  VideoParameters *p_Vid
 ************************************************************************
 */
Slice *malloc_slice(InputParameters *p_Inp, VideoParameters *p_Vid)
{
  int i, j, memory_size = 0;
  Slice *currSlice;

  currSlice = (Slice *) calloc(1, sizeof(Slice));
  if ( currSlice  == NULL)
  {
    snprintf(errortext, ET_SIZE, "Memory allocation for Slice datastruct in NAL-mode %d failed", p_Inp->FileFormat);
    error(errortext,100);
  }

  // create all context models
  currSlice->mot_ctx = create_contexts_MotionInfo();
  currSlice->tex_ctx = create_contexts_TextureInfo();

  currSlice->max_part_nr = 3;  //! assume data partitioning (worst case) for the following mallocs()
  currSlice->partArr = AllocPartition(currSlice->max_part_nr);

  memory_size += get_mem2Dwp (&(currSlice->wp_params), 2, MAX_REFERENCE_PICTURES);

  memory_size += get_mem3Dint(&(currSlice->wp_weight), 2, MAX_REFERENCE_PICTURES, 3);
  memory_size += get_mem3Dint(&(currSlice->wp_offset), 6, MAX_REFERENCE_PICTURES, 3);
  memory_size += get_mem4Dint(&(currSlice->wbp_weight), 6, MAX_REFERENCE_PICTURES, MAX_REFERENCE_PICTURES, 3);

  memory_size += get_mem3Dpel(&(currSlice->mb_pred), MAX_PLANE, MB_BLOCK_SIZE, MB_BLOCK_SIZE);
  memory_size += get_mem3Dpel(&(currSlice->mb_rec ), MAX_PLANE, MB_BLOCK_SIZE, MB_BLOCK_SIZE);
  memory_size += get_mem3Dint(&(currSlice->mb_rres), MAX_PLANE, MB_BLOCK_SIZE, MB_BLOCK_SIZE);
  memory_size += get_mem3Dint(&(currSlice->cof    ), MAX_PLANE, MB_BLOCK_SIZE, MB_BLOCK_SIZE);
  //  memory_size += get_mem3Dint(&(currSlice->fcf    ), MAX_PLANE, MB_BLOCK_SIZE, MB_BLOCK_SIZE);
  allocate_pred_mem(currSlice);
  // reference flag initialization
  for(i=0;i<17;++i)
  {
    currSlice->ref_flag[i] = 1;
  }
  for (i = 0; i < 6; i++)
  {
    currSlice->listX[i] = (StorablePicture**)calloc(MAX_LIST_SIZE, sizeof (StorablePicture*)); // +1 for reordering
    if (NULL==currSlice->listX[i])
      no_mem_exit("malloc_slice: currSlice->listX[i]");
  }
  for (j = 0; j < 6; j++)
  {
    for (i = 0; i < MAX_LIST_SIZE; i++)
    {
      currSlice->listX[j][i] = NULL;
    }
    currSlice->listXsize[j]=0;
  }

  return currSlice;
}


/*!
 ************************************************************************
 * \brief
 *    Memory frees of the Slice structure and of its dependent
 *    data structures
 *
 * \par Input:
 *    Input Parameters Slice *currSlice
 ************************************************************************
 */
void free_slice(Slice *currSlice)
{
  int i;

  if (currSlice->slice_type != I_SLICE && currSlice->slice_type != SI_SLICE)
  free_ref_pic_list_reordering_buffer(currSlice);
  free_pred_mem(currSlice);
  free_mem3Dint(currSlice->cof    );
  free_mem3Dint(currSlice->mb_rres);
  free_mem3Dpel(currSlice->mb_rec );
  free_mem3Dpel(currSlice->mb_pred);

  free_mem2Dwp (currSlice->wp_params );
  free_mem3Dint(currSlice->wp_weight );
  free_mem3Dint(currSlice->wp_offset );
  free_mem4Dint(currSlice->wbp_weight);

  FreePartition (currSlice->partArr, 3);

  //if (1)
  {
    // delete all context models
    delete_contexts_MotionInfo (currSlice->mot_ctx);
    delete_contexts_TextureInfo(currSlice->tex_ctx);
  }

  for (i=0; i<6; i++)
  {
    if (currSlice->listX[i])
    {
      free (currSlice->listX[i]);
      currSlice->listX[i] = NULL;
    }
  }
  while (currSlice->dec_ref_pic_marking_buffer)
  {
    DecRefPicMarking_t *tmp_drpm=currSlice->dec_ref_pic_marking_buffer;
    currSlice->dec_ref_pic_marking_buffer=tmp_drpm->Next;
    free (tmp_drpm);
  }

  free(currSlice);
  currSlice = NULL;
}

/*!
 ************************************************************************
 * \brief
 *    Dynamic memory allocation of frame size related global buffers
 *    buffers are defined in global.h, allocated memory must be freed in
 *    void free_global_buffers()
 *
 *  \par Input:
 *    Input Parameters VideoParameters *p_Vid
 *
 *  \par Output:
 *     Number of allocated bytes
 ***********************************************************************
 */
int init_global_buffers(VideoParameters *p_Vid, int layer_id)
{
  int memory_size=0;
  int i;
  CodingParameters *cps = p_Vid->p_EncodePar[layer_id];
  BlockPos* PicPos;

  if (p_Vid->global_init_done[layer_id])
  {
    free_layer_buffers(p_Vid, layer_id);
  }

  // allocate memory for reference frame in find_snr
  memory_size += get_mem2Dpel(&cps->imgY_ref, cps->height, cps->width);
  if (cps->yuv_format != YUV400)
  {
    memory_size += get_mem3Dpel(&cps->imgUV_ref, 2, cps->height_cr, cps->width_cr);
  }
  else
    cps->imgUV_ref = NULL;

  // allocate memory in structure p_Vid
  if( (cps->separate_colour_plane_flag != 0) )
  {
    for( i=0; i<MAX_PLANE; ++i )
    {
      if(((cps->mb_data_JV[i]) = (Macroblock *) calloc(cps->FrameSizeInMbs, sizeof(Macroblock))) == NULL)
        no_mem_exit("init_global_buffers: cps->mb_data_JV");
    }
    cps->mb_data = NULL;
  }
  else
  {
    if(((cps->mb_data) = (Macroblock *) calloc(cps->FrameSizeInMbs, sizeof(Macroblock))) == NULL)
      no_mem_exit("init_global_buffers: cps->mb_data");
  }
  if( (cps->separate_colour_plane_flag != 0) )
  {
    for( i=0; i<MAX_PLANE; ++i )
    {
      if(((cps->intra_block_JV[i]) = (char*) calloc(cps->FrameSizeInMbs, sizeof(char))) == NULL)
        no_mem_exit("init_global_buffers: cps->intra_block_JV");
    }
    cps->intra_block = NULL;
  }
  else
  {
    if(((cps->intra_block) = (char*) calloc(cps->FrameSizeInMbs, sizeof(char))) == NULL)
      no_mem_exit("init_global_buffers: cps->intra_block");
  }


  //memory_size += get_mem2Dint(&PicPos,p_Vid->FrameSizeInMbs + 1,2);  //! Helper array to access macroblock positions. We add 1 to also consider last MB.
  if(((cps->PicPos) = (BlockPos*) calloc(cps->FrameSizeInMbs + 1, sizeof(BlockPos))) == NULL)
    no_mem_exit("init_global_buffers: PicPos");

  PicPos = cps->PicPos;
  for (i = 0; i < (int) cps->FrameSizeInMbs + 1;++i)
  {
    PicPos[i].x = (short) (i % cps->PicWidthInMbs);
    PicPos[i].y = (short) (i / cps->PicWidthInMbs);
  }

  if( (cps->separate_colour_plane_flag != 0) )
  {
    for( i=0; i<MAX_PLANE; ++i )
    {
      get_mem2D(&(cps->ipredmode_JV[i]), 4*cps->FrameHeightInMbs, 4*cps->PicWidthInMbs);
    }
    cps->ipredmode = NULL;
  }
  else
   memory_size += get_mem2D(&(cps->ipredmode), 4*cps->FrameHeightInMbs, 4*cps->PicWidthInMbs);

  // CAVLC mem
  memory_size += get_mem4D(&(cps->nz_coeff), cps->FrameSizeInMbs, 3, BLOCK_SIZE, BLOCK_SIZE);
  if( (cps->separate_colour_plane_flag != 0) )
  {
    for( i=0; i<MAX_PLANE; ++i )
    {
      get_mem2Dint(&(cps->siblock_JV[i]), cps->FrameHeightInMbs, cps->PicWidthInMbs);
      if(cps->siblock_JV[i]== NULL)
        no_mem_exit("init_global_buffers: p_Vid->siblock_JV");
    }
    cps->siblock = NULL;
  }
  else
  {
    memory_size += get_mem2Dint(&(cps->siblock), cps->FrameHeightInMbs, cps->PicWidthInMbs);
  }
  init_qp_process(cps);
  cps->oldFrameSizeInMbs = cps->FrameSizeInMbs;

  if(layer_id == 0 )
    init_output(cps, ((cps->pic_unit_bitsize_on_disk+7) >> 3));
  else
    cps->img2buf = p_Vid->p_EncodePar[0]->img2buf;
  p_Vid->global_init_done[layer_id] = 1;

  return (memory_size);
}

/*!
 ************************************************************************
 * \brief
 *    Free allocated memory of frame size related global buffers
 *    buffers are defined in global.h, allocated memory is allocated in
 *    int init_global_buffers()
 *
 * \par Input:
 *    Input Parameters VideoParameters *p_Vid
 *
 * \par Output:
 *    none
 *
 ************************************************************************
 */
void free_layer_buffers(VideoParameters *p_Vid, int layer_id)
{  
  CodingParameters *cps = p_Vid->p_EncodePar[layer_id];
  
  if(!p_Vid->global_init_done[layer_id])
    return;

  if (cps->imgY_ref)
  {
    free_mem2Dpel (cps->imgY_ref);
    cps->imgY_ref = NULL;
  }
  if (cps->imgUV_ref)
  {
    free_mem3Dpel (cps->imgUV_ref);
    cps->imgUV_ref = NULL;
  }
  // CAVLC free mem
  if (cps->nz_coeff)
  {
    free_mem4D(cps->nz_coeff);
    cps->nz_coeff = NULL;
  }

  // free mem, allocated for structure p_Vid
  if( (cps->separate_colour_plane_flag != 0) )
  {
    int i;
    for(i=0; i<MAX_PLANE; i++)
    {
      free(cps->mb_data_JV[i]);
      cps->mb_data_JV[i] = NULL;
      free_mem2Dint(cps->siblock_JV[i]);
      cps->siblock_JV[i] = NULL;
      free_mem2D(cps->ipredmode_JV[i]);
      cps->ipredmode_JV[i] = NULL;
      free (cps->intra_block_JV[i]);
      cps->intra_block_JV[i] = NULL;
    }   
  }
  else
  {
    if (cps->mb_data != NULL)
    {
      free(cps->mb_data);
      cps->mb_data = NULL;
    }
    if(cps->siblock)
    {
      free_mem2Dint(cps->siblock);
      cps->siblock = NULL;
    }
    if(cps->ipredmode)
    {
      free_mem2D(cps->ipredmode);
      cps->ipredmode = NULL;
    }
    if(cps->intra_block)
    {
      free (cps->intra_block);
      cps->intra_block = NULL;
    }
  }
  if(cps->PicPos)
  {
    free(cps->PicPos);
    cps->PicPos = NULL;
  }

  free_qp_matrices(cps);


  p_Vid->global_init_done[layer_id] = 0;
}

void free_global_buffers(VideoParameters *p_Vid)
{
  if(p_Vid->dec_picture)
  {
    free_storable_picture(p_Vid->dec_picture);
    p_Vid->dec_picture = NULL;
  }
}

void report_stats_on_error(void)
{
  //free_encoder_memory(p_Vid);
}

void ClearDecPicList(VideoParameters *p_Vid)
{
  DecodedPicList *pPic = p_Vid->pDecOuputPic, *pPrior = NULL;
  //find the head first;
  while(pPic && !pPic->bValid)
  {
    pPrior = pPic;
    pPic = pPic->pNext;
  }

  if(pPic && (pPic != p_Vid->pDecOuputPic))
  {
    //move all nodes before pPic to the end;
    DecodedPicList *pPicTail = pPic;
    while(pPicTail->pNext)
      pPicTail = pPicTail->pNext;

    pPicTail->pNext = p_Vid->pDecOuputPic;
    p_Vid->pDecOuputPic = pPic;
    pPrior->pNext = NULL;
  }
}

DecodedPicList *get_one_avail_dec_pic_from_list(DecodedPicList **ppDecPicList, int b3D, int view_id)
{
  if (!ppDecPicList) return NULL;
  DecodedPicList *pPic = *ppDecPicList, *pPrior = NULL;
  if(b3D)
  {
    while(pPic && (pPic->bValid &(1<<view_id)))
    {
      pPrior = pPic;
      pPic = pPic->pNext;
    }
  }
  else
  {
    while(pPic && (pPic->bValid))
    {
      pPrior = pPic;
      pPic = pPic->pNext;
    }
  }

  if(!pPic)
  {
    pPic = (DecodedPicList *)calloc(1, sizeof(*pPic));
    if (pPrior)
    {
      pPrior->pNext = pPic;
    }
    else
    {
      *ppDecPicList = pPic;
    }
  }

  return pPic;
}
/************************************
Interface: OpenDecoder
Return: 
       0: NOERROR;
       <0: ERROR;
************************************/
int OpenDecoder(InputParameters *p_Inp)
{
  int iRet;
  DecoderParams *pDecoder;
  
  iRet = alloc_decoder(&p_Dec);
  if(iRet)
  {
    return (iRet|DEC_ERRMASK);
  }
  init_time();

  pDecoder = p_Dec;
  pDecoder->p_Vid->dpb_flushed = 0;
  pDecoder->p_Vid->dec_eos_reached = 0;
  memcpy(pDecoder->p_Inp, p_Inp, sizeof(InputParameters));
  if (pDecoder->p_Inp->poc_scale <= 0) pDecoder->p_Inp->poc_scale = 2;
  if (pDecoder->p_Inp->ref_poc_gap <= 0) pDecoder->p_Inp->ref_poc_gap = 2;
  if (pDecoder->p_Inp->poc_gap <= 0) pDecoder->p_Inp->poc_gap = 2;
  pDecoder->p_Inp->write_uv = 1;
  pDecoder->p_Inp->intra_profile_deblocking = 1;
  if (pDecoder->p_Inp->dpb_plus[0] == 0) pDecoder->p_Inp->dpb_plus[0] = 1;
  pDecoder->p_Vid->conceal_mode = pDecoder->p_Inp->conceal_mode;
  pDecoder->p_Vid->ref_poc_gap = pDecoder->p_Inp->ref_poc_gap;
  pDecoder->p_Vid->poc_gap = pDecoder->p_Inp->poc_gap;

  f264_strategyselector_init(p_Inp->cpuid, 8, p_Inp->silent ? 0 : 1);
  int nthreads = pDecoder->p_Inp->threads > 0 ? pDecoder->p_Inp->threads : f264_g_hardware_flags.logical_cpu_count;
  pDecoder->thread_queue = f264_threadqueue_init(nthreads > 1 ? nthreads : 0);
  pDecoder->p_Vid->thread_queue = pDecoder->thread_queue;
  pDecoder->frame_pipeline = NULL;
  if (pDecoder->thread_queue && nthreads > 1) {
    int num_slots = nthreads > 4 ? 4 : nthreads;
    if (num_slots < 2) num_slots = 2;
    pDecoder->frame_pipeline = f264_frame_pipeline_init(num_slots, pDecoder->p_Vid);
    // Dependent frames spin-wait on reference rows; keep spare workers so the
    // oldest in-flight job can always make progress.
    ((FramePipeline*)pDecoder->frame_pipeline)->row_overlap = (nthreads >= num_slots + 2);
  }

  if((strcasecmp(p_Inp->outfile, "\"\"")!=0) && (strlen(p_Inp->outfile)>0))
  {
    if ((pDecoder->p_Vid->p_out = open(p_Inp->outfile, OPENFLAGS_WRITE, OPEN_PERMISSIONS))==-1)
    {
      snprintf(errortext, ET_SIZE, "Error open file %s ",p_Inp->outfile);
      error(errortext,500);
      return (DEC_ERRMASK | 1);
    }
  }
  else
    pDecoder->p_Vid->p_out = -1;


  if(strlen(pDecoder->p_Inp->reffile)>0 && strcmp(pDecoder->p_Inp->reffile, "\"\""))
  {
   if ((pDecoder->p_Vid->p_ref = open(pDecoder->p_Inp->reffile, OPENFLAGS_READ))==-1)
   {
    if (!pDecoder->p_Inp->silent)
    {
      fprintf(stdout," Input reference file                   : %s does not exist \n",pDecoder->p_Inp->reffile);
      fprintf(stdout,"                                          SNR values are not available\n");
    }
   }
  }
  else
    pDecoder->p_Vid->p_ref = -1;

  if (malloc_annex_b(pDecoder->p_Vid, &pDecoder->p_Vid->annex_b) != 0)
  {
    return (DEC_ERRMASK | 1);
  }
  if (pDecoder->p_Inp->memory_input || strlen(pDecoder->p_Inp->infile) == 0)
  {
    open_annex_b_memory(pDecoder->p_Vid->annex_b);
  }
  else
  {
    if (open_annex_b(pDecoder->p_Inp->infile, pDecoder->p_Vid->annex_b) != 0)
    {
      return (DEC_ERRMASK | 1);
    }
  }
  
  // Allocate Slice data struct
  //pDecoder->p_Vid->currentSlice = NULL; //malloc_slice(pDecoder->p_Inp, pDecoder->p_Vid);
  
  init_old_slice(pDecoder->p_Vid->old_slice);

  init(pDecoder->p_Vid);
 
  init_out_buffer(pDecoder->p_Vid);



#if _FLTDBG_
  pDecoder->p_Vid->fpDbg = fopen("c:/fltdbg.txt", "a");
  fprintf(pDecoder->p_Vid->fpDbg, "\ndecoder is opened.\n");
#endif

  return DEC_OPEN_NOERR;
}

/************************************
Interface: DecodeOneFrame
Return: 
       0: NOERROR;
       1: Finished decoding;
       others: Error Code;
************************************/
int DecodeOneFrame(DecodedPicList **ppDecPicList)
{
  int iRet;
  DecoderParams *pDecoder = p_Dec;
  ClearDecPicList(pDecoder->p_Vid);
  iRet = decode_one_frame(pDecoder);
  if(iRet == SOP)
  {
    iRet = DEC_SUCCEED;
  }
  else if(iRet == EOS)
  {
    iRet = DEC_EOS;
  }
  else
  {
    iRet |= DEC_ERRMASK;
  }

  *ppDecPicList = pDecoder->p_Vid->pDecOuputPic;
  return iRet;
}

int FinitDecoder(DecodedPicList **ppDecPicList)
{
  DecoderParams *pDecoder = p_Dec;
  if(!pDecoder)
    return DEC_GEN_NOERR;
  if (pDecoder->frame_pipeline && pDecoder->thread_queue) {
    f264_frame_pipeline_flush((FramePipeline*)pDecoder->frame_pipeline, pDecoder->thread_queue);
    if (pDecoder->p_Vid->snr->frame_ctr > 0 && pDecoder->p_Vid->tot_time == 0) {
      gettime(&(pDecoder->p_Vid->end_time));
      pDecoder->p_Vid->tot_time = timediff(&(pDecoder->p_Vid->start_time), &(pDecoder->p_Vid->end_time));
    }
  }
  ClearDecPicList(pDecoder->p_Vid);
  flush_dpb(pDecoder->p_Vid->p_Dpb_layer[0]);
  if (pDecoder->p_Inp->FileFormat == PAR_OF_ANNEXB)
  {
    reset_annex_b(pDecoder->p_Vid->annex_b); 
  }
  pDecoder->p_Vid->newframe = 0;
  pDecoder->p_Vid->previous_frame_num = 0;
  *ppDecPicList = pDecoder->p_Vid->pDecOuputPic;
  return DEC_GEN_NOERR;
}

int CloseDecoder()
{
  int i;

  DecoderParams *pDecoder = p_Dec;
  if(!pDecoder)
    return DEC_CLOSE_NOERR;

  if (pDecoder->frame_pipeline) {
    // Wait for the frame jobs before freeing their slices and buffers. The
    // decoder may be closed with pictures still in flight (a player resets it
    // to seek), and those workers read the very structures freed below.
    if (pDecoder->thread_queue) {
      f264_frame_pipeline_flush((FramePipeline*)pDecoder->frame_pipeline, pDecoder->thread_queue);
    }
    f264_frame_pipeline_free((FramePipeline*)pDecoder->frame_pipeline);
    pDecoder->frame_pipeline = NULL;
  }

  if (pDecoder->thread_queue) {
    f264_threadqueue_free(pDecoder->thread_queue);
    pDecoder->thread_queue = NULL;
    if (pDecoder->p_Vid) pDecoder->p_Vid->thread_queue = NULL;
  }
  
  if (pDecoder->p_Vid) {
    Report  (pDecoder->p_Vid);
    FmoFinit(pDecoder->p_Vid);
    free_layer_buffers(pDecoder->p_Vid, 0);
    free_layer_buffers(pDecoder->p_Vid, 1);
    free_global_buffers(pDecoder->p_Vid);
    if (pDecoder->p_Vid->annex_b) {
      close_annex_b(pDecoder->p_Vid->annex_b);
    }

    if(pDecoder->p_Vid->p_out >=0)
    {
      close(pDecoder->p_Vid->p_out);
      pDecoder->p_Vid->p_out = -1;
    }

    if (pDecoder->p_Vid->p_ref != -1)
    {
      close(pDecoder->p_Vid->p_ref);
      pDecoder->p_Vid->p_ref = -1;
    }

    if (pDecoder->p_Vid->erc_errorVar) {
      ercClose(pDecoder->p_Vid, pDecoder->p_Vid->erc_errorVar);
      pDecoder->p_Vid->erc_errorVar = NULL;
    }

    CleanUpPPS(pDecoder->p_Vid);

    for(i=0; i<MAX_NUM_DPB_LAYERS; i++)
    {
      if (pDecoder->p_Vid->p_Dpb_layer[i]) {
        free_dpb(pDecoder->p_Vid->p_Dpb_layer[i]);
      }
    }

    uninit_out_buffer(pDecoder->p_Vid);
#if _FLTDBG_
    if(pDecoder->p_Vid->fpDbg)
    {
      fprintf(pDecoder->p_Vid->fpDbg, "decoder is closed.\n");
      fclose(pDecoder->p_Vid->fpDbg);
      pDecoder->p_Vid->fpDbg = NULL;
    }
#endif
  }

  if (pDecoder->pic_wrapper) {
    free(pDecoder->pic_wrapper);
    pDecoder->pic_wrapper = NULL;
  }

  if (pDecoder->p_Vid) {
    free_img (pDecoder->p_Vid);
    pDecoder->p_Vid = NULL;
  }
  if (pDecoder->p_Inp) {
    free (pDecoder->p_Inp);
    pDecoder->p_Inp = NULL;
  }
  free(pDecoder);

  p_Dec = NULL;
  return DEC_CLOSE_NOERR;
}


void set_global_coding_par(VideoParameters *p_Vid, CodingParameters *cps)
{
    p_Vid->bitdepth_chroma = 0;
    p_Vid->width_cr        = 0;
    p_Vid->height_cr       = 0;
    p_Vid->lossless_qpprime_flag   = cps->lossless_qpprime_flag;
    p_Vid->max_vmv_r = cps->max_vmv_r;

    // Fidelity Range Extensions stuff (part 1)
    p_Vid->bitdepth_luma       = cps->bitdepth_luma;
    p_Vid->bitdepth_scale[0]   = cps->bitdepth_scale[0];
    p_Vid->bitdepth_chroma = cps->bitdepth_chroma;
    p_Vid->bitdepth_scale[1] = cps->bitdepth_scale[1];

    p_Vid->max_frame_num = cps->max_frame_num;
    p_Vid->PicWidthInMbs = cps->PicWidthInMbs;
    p_Vid->PicHeightInMapUnits = cps->PicHeightInMapUnits;
    p_Vid->FrameHeightInMbs = cps->FrameHeightInMbs;
    p_Vid->FrameSizeInMbs = cps->FrameSizeInMbs;

    p_Vid->yuv_format = cps->yuv_format;
    p_Vid->separate_colour_plane_flag = cps->separate_colour_plane_flag;
    p_Vid->ChromaArrayType = cps->ChromaArrayType;

    p_Vid->width = cps->width;
    p_Vid->height = cps->height;
    p_Vid->iLumaPadX = MCBUF_LUMA_PAD_X;
    p_Vid->iLumaPadY = MCBUF_LUMA_PAD_Y;
    p_Vid->iChromaPadX = MCBUF_CHROMA_PAD_X;
    p_Vid->iChromaPadY = MCBUF_CHROMA_PAD_Y;
    if (p_Vid->yuv_format == YUV420)
    {
      p_Vid->width_cr  = (p_Vid->width  >> 1);
      p_Vid->height_cr = (p_Vid->height >> 1);
    }
    else if (p_Vid->yuv_format == YUV422)
    {
      p_Vid->width_cr  = (p_Vid->width >> 1);
      p_Vid->height_cr = p_Vid->height;
      p_Vid->iChromaPadY = MCBUF_CHROMA_PAD_Y*2;
    }
    else if (p_Vid->yuv_format == YUV444)
    {
      //YUV444
      p_Vid->width_cr = p_Vid->width;
      p_Vid->height_cr = p_Vid->height;
      p_Vid->iChromaPadX = p_Vid->iLumaPadX;
      p_Vid->iChromaPadY = p_Vid->iLumaPadY;
    }

    init_frext(p_Vid);
}

/*****************************************************************************
 * Kvazaar-Normalized f264dec API Implementation
 *****************************************************************************/

f264_config * f264_config_alloc(void)
{
  f264_config *cfg = (f264_config *)calloc(1, sizeof(f264_config));
  if (cfg) {
    f264_config_init(cfg);
  }
  return cfg;
}

void f264_config_destroy(f264_config *cfg)
{
  if (cfg) {
    free(cfg);
  }
}

int f264_config_init(f264_config *cfg)
{
  if (!cfg) return 0;
  memset(cfg, 0, sizeof(f264_config));
  cfg->threads = 0;          // Auto-detect
  cfg->cpuid = 1;            // SIMD enabled
  cfg->max_frames = 0;       // Decode all
  cfg->silent = 0;           // Verbose
  cfg->deblock_enable = 1;
  cfg->file_format = 0;      // PAR_OF_ANNEXB
  cfg->memory_input = 0;
  cfg->error_cb = NULL;
  cfg->error_cb_user_data = NULL;
  return 1;
}

int f264_config_parse(f264_config *cfg, const char *name, const char *value)
{
  if (!cfg || !name || !value) return 0;
  if (strcmp(name, "threads") == 0) {
    cfg->threads = atoi(value);
    return 1;
  }
  if (strcmp(name, "cpuid") == 0) {
    cfg->cpuid = atoi(value);
    return 1;
  }
  if (strcmp(name, "frames") == 0 || strcmp(name, "max_frames") == 0) {
    cfg->max_frames = atoi(value);
    return 1;
  }
  if (strcmp(name, "silent") == 0) {
    cfg->silent = atoi(value);
    return 1;
  }
  if (strcmp(name, "input") == 0 || strcmp(name, "infile") == 0) {
    strncpy(cfg->infile, value, sizeof(cfg->infile) - 1);
    return 1;
  }
  if (strcmp(name, "output") == 0 || strcmp(name, "outfile") == 0) {
    strncpy(cfg->outfile, value, sizeof(cfg->outfile) - 1);
    return 1;
  }
  if (strcmp(name, "ref") == 0 || strcmp(name, "reffile") == 0) {
    strncpy(cfg->reffile, value, sizeof(cfg->reffile) - 1);
    return 1;
  }
  if (strcmp(name, "memory_input") == 0) {
    cfg->memory_input = atoi(value);
    return 1;
  }
  if (strcmp(name, "deblock_enable") == 0 || strcmp(name, "deblock") == 0) {
    cfg->deblock_enable = atoi(value);
    return 1;
  }
  if (strcmp(name, "file_format") == 0) {
    cfg->file_format = atoi(value);
    return 1;
  }
  return 0;
}

f264_picture * f264_picture_alloc(int32_t width, int32_t height)
{
  return f264_picture_alloc_csp(F264_CSP_420, width, height);
}

f264_picture * f264_picture_alloc_csp(f264_chroma_format csp, int32_t width, int32_t height)
{
  f264_picture *pic = (f264_picture *)calloc(1, sizeof(f264_picture));
  if (!pic) return NULL;

  pic->width = width;
  pic->height = height;
  pic->stride = width;
  pic->chroma_format = csp;
  pic->bit_depth = 8;

  int w_c = (csp == F264_CSP_420 || csp == F264_CSP_422) ? (width >> 1) : (csp == F264_CSP_400 ? 0 : width);
  int h_c = (csp == F264_CSP_420) ? (height >> 1) : (csp == F264_CSP_400 ? 0 : height);
  pic->width_c = w_c;
  pic->height_c = h_c;
  pic->stride_c = w_c;

  size_t y_size = (size_t)width * height;
  size_t c_size = (size_t)w_c * h_c;
  size_t total = y_size + 2 * c_size;

  pic->fulldata_buf = (f264_pixel *)malloc(total);
  if (!pic->fulldata_buf) {
    free(pic);
    return NULL;
  }
  pic->fulldata = pic->fulldata_buf;
  pic->y = pic->fulldata;
  pic->u = (c_size > 0) ? pic->y + y_size : NULL;
  pic->v = (c_size > 0) ? pic->u + c_size : NULL;
  pic->data[0] = pic->y;
  pic->data[1] = pic->u;
  pic->data[2] = pic->v;

  return pic;
}

void f264_picture_free(f264_picture *pic)
{
  if (pic) {
    if (pic->fulldata_buf) {
      free(pic->fulldata_buf);
    }
    free(pic);
  }
}

static void fill_f264_picture_from_dec_pic(f264_picture *out, DecodedPicList *dec_pic)
{
  if (!out || !dec_pic) return;
  out->y = (f264_pixel*)dec_pic->pY;
  out->u = (f264_pixel*)dec_pic->pU;
  out->v = (f264_pixel*)dec_pic->pV;
  out->data[0] = out->y;
  out->data[1] = out->u;
  out->data[2] = out->v;
  out->width = dec_pic->iWidth;
  out->height = dec_pic->iHeight;
  out->stride = dec_pic->iYBufStride;
  out->bit_depth = dec_pic->iBitDepth;
  out->poc = dec_pic->iPOC;
  out->chroma_format = (f264_chroma_format)dec_pic->iYUVFormat;

  if (dec_pic->iYUVFormat == 1) { // 4:2:0
    out->width_c = dec_pic->iWidth >> 1;
    out->height_c = dec_pic->iHeight >> 1;
    out->stride_c = dec_pic->iUVBufStride;
  } else if (dec_pic->iYUVFormat == 2) { // 4:2:2
    out->width_c = dec_pic->iWidth >> 1;
    out->height_c = dec_pic->iHeight;
    out->stride_c = dec_pic->iUVBufStride;
  } else if (dec_pic->iYUVFormat == 3) { // 4:4:4
    out->width_c = dec_pic->iWidth;
    out->height_c = dec_pic->iHeight;
    out->stride_c = dec_pic->iUVBufStride;
  } else { // 4:0:0
    out->width_c = 0;
    out->height_c = 0;
    out->stride_c = 0;
  }
}

f264_decoder * f264_decoder_open(const f264_config *cfg)
{
  if (!cfg) return NULL;
  if (p_Dec != NULL) {
    fprintf(stderr, "f264dec error: only one active decoder instance is supported per process\n");
    return NULL;
  }
  InputParameters inp;
  memset(&inp, 0, sizeof(inp));
  strncpy(inp.infile, cfg->infile, sizeof(inp.infile) - 1);
  strncpy(inp.outfile, cfg->outfile, sizeof(inp.outfile) - 1);
  strncpy(inp.reffile, cfg->reffile, sizeof(inp.reffile) - 1);
  inp.threads = cfg->threads;
  inp.iDecFrmNum = cfg->max_frames;
  inp.cpuid = cfg->cpuid;
  inp.silent = cfg->silent;
  inp.FileFormat = PAR_OF_ANNEXB;
  inp.memory_input = cfg->memory_input;
  inp.error_cb = cfg->error_cb;
  inp.error_cb_user_data = cfg->error_cb_user_data;

  int ret = OpenDecoder(&inp);
  if (ret != DEC_OPEN_NOERR || !p_Dec) {
    if (p_Dec) {
      CloseDecoder();
    }
    return NULL;
  }

  if (!p_Dec->pic_wrapper) {
    p_Dec->pic_wrapper = calloc(1, sizeof(f264_picture));
  }
  return (f264_decoder *)p_Dec;
}

void f264_decoder_close(f264_decoder *dec)
{
  (void)dec;
  CloseDecoder();
}

int f264_decoder_push(f264_decoder *dec, const uint8_t *data, size_t size)
{
  if (!dec || !data || size == 0) return 0;
  DecoderParams *pDecoder = (DecoderParams *)dec;
  if (!pDecoder->p_Vid || !pDecoder->p_Vid->annex_b) return -1;
  return annex_b_push(pDecoder->p_Vid->annex_b, (const byte *)data, size);
}

f264_picture * f264_decoder_get_picture(f264_decoder *dec)
{
  if (!dec) return NULL;
  DecoderParams *pDecoder = (DecoderParams *)dec;
  if (!pDecoder->p_Vid) return NULL;

  DecodedPicList *pPic = pDecoder->p_Vid->pDecOuputPic;
  while (pPic)
  {
    if (pPic->bValid)
    {
      if (!pDecoder->pic_wrapper)
      {
        pDecoder->pic_wrapper = calloc(1, sizeof(f264_picture));
      }
      f264_picture *wrap = (f264_picture *)pDecoder->pic_wrapper;
      fill_f264_picture_from_dec_pic(wrap, pPic);
      pPic->bValid = 0; // Consume the picture node so it can be reused
      return wrap;
    }
    pPic = pPic->pNext;
  }
  return NULL;
}

int f264_decoder_decode(f264_decoder *dec, f264_picture **pic_out)
{
  if (!dec) return F264_ERR;
  if (pic_out) *pic_out = NULL;

  DecoderParams *pDecoder = (DecoderParams *)dec;
  if (!pDecoder || !pDecoder->p_Vid) return F264_ERR;

  // Check if there is already an unconsumed picture available
  if (pic_out)
  {
    f264_picture *pending = f264_decoder_get_picture(dec);
    if (pending)
    {
      *pic_out = pending;
      return F264_OK;
    }
  }

  // If EOS was already reached, do not call DecodeOneFrame again
  if (pDecoder->p_Vid->dec_eos_reached)
  {
    return F264_EOS;
  }

  DecodedPicList *pic_list = NULL;
  int ret = DecodeOneFrame(&pic_list);

  if (ret == DEC_EOS)
  {
    pDecoder->p_Vid->dec_eos_reached = 1;
  }

  if (pic_out)
  {
    *pic_out = f264_decoder_get_picture(dec);
    if (*pic_out != NULL)
    {
      return F264_OK;
    }
  }

  if (ret == DEC_SUCCEED) return F264_OK;
  if (ret == DEC_EOS) return F264_EOS;
  return F264_ERR;
}

int f264_decoder_flush(f264_decoder *dec, f264_picture **pic_out)
{
  if (!dec) return F264_ERR;
  if (pic_out) *pic_out = NULL;

  DecoderParams *pDecoder = (DecoderParams *)dec;
  if (!pDecoder || !pDecoder->p_Vid) return F264_ERR;

  // On the first flush call, flush the DPB to output all remaining buffered pictures
  if (!pDecoder->p_Vid->dpb_flushed)
  {
    pDecoder->p_Vid->dpb_flushed = 1;
    DecodedPicList *pic_list = NULL;
    FinitDecoder(&pic_list);
  }

  // Drain next picture from output list
  f264_picture *pending = f264_decoder_get_picture(dec);
  if (pending)
  {
    if (pic_out) *pic_out = pending;
    return F264_OK;
  }

  return F264_EOS;
}

static const f264_api g_f264_api = {
  f264_config_alloc,
  f264_config_destroy,
  f264_config_init,
  f264_config_parse,
  f264_decoder_open,
  f264_decoder_close,
  f264_decoder_decode,
  f264_decoder_flush,
  f264_decoder_push,
  f264_decoder_get_picture,
  f264_picture_alloc,
  f264_picture_alloc_csp,
  f264_picture_free
};

const f264_api * f264_api_get(int bit_depth)
{
  (void)bit_depth;
  return &g_f264_api;
}

const f264_api * f264dec_api_get(int bit_depth)
{
  return f264_api_get(bit_depth);
}

const char * f264_get_version_string(void)
{
  return "1.0.0";
}

int f264_get_version_major(void) { return F264_VERSION_MAJOR; }
int f264_get_version_minor(void) { return F264_VERSION_MINOR; }
int f264_get_version_revision(void) { return F264_VERSION_REV; }

