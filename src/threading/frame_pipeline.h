#ifndef F264_FRAME_PIPELINE_H_
#define F264_FRAME_PIPELINE_H_

#include "global.h"
#include "mbuffer.h"
#include "threading/threadqueue.h"
#include <vector>

struct FrameWorkerSlot {
  int slot_id{0};
  VideoParameters *p_Vid{nullptr};
  Macroblock *mb_data{nullptr};
  char *intra_block{nullptr};
  byte **ipredmode{nullptr};
  byte ****nz_coeff{nullptr};
  int **siblock{nullptr};
  Slice **ppSliceList{nullptr};
  int iSliceNumOfCurrPic{0};
  int iNumOfSlicesAllocated{0};
  threadqueue_job_t *job{nullptr};
  StorablePicture *dec_picture{nullptr};
  int allocated_FrameSizeInMbs{0};
  int allocated_FrameHeightInMbs{0};
  int allocated_PicWidthInMbs{0};
  std::vector<StorablePicture*> referenced_pics;
};

struct FramePipeline {
  int num_slots{0};
  int next_slot_idx{0};
  std::vector<FrameWorkerSlot> slots;
};

FramePipeline* f264_frame_pipeline_init(int num_slots, VideoParameters *master_Vid);
void f264_frame_pipeline_free(FramePipeline *pipeline);
void f264_frame_pipeline_flush(FramePipeline *pipeline, threadqueue_queue_t *tq);
void f264_frame_pipeline_ensure_buffers(FrameWorkerSlot *slot, VideoParameters *p_Vid);

#endif // F264_FRAME_PIPELINE_H_
