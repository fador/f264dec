/*!
 ************************************************************************
 *  \file
 *     loopfilter.h
 *  \brief
 *     external deblocking filter interface
 ************************************************************************
 */

#ifndef _LOOPFILTER_H_
#define _LOOPFILTER_H_

#include "global.h"
#include "mbuffer.h"

extern void DeblockPicture(VideoParameters *p_Vid, StorablePicture *p) ;
extern void DeblockMbRows(VideoParameters *p_Vid, StorablePicture *p, int row0, int row1);

void  init_Deblock(VideoParameters *p_Vid, int mb_aff_frame_flag);
#endif //_LOOPFILTER_H_
