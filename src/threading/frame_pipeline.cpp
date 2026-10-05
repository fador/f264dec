#include "frame_pipeline.h"
#include "memalloc.h"
#include <cstdlib>
#include <cstring>

FramePipeline* f264_frame_pipeline_init(int num_slots, VideoParameters *master_Vid)
{
  if (num_slots <= 0) return nullptr;
  auto *pipeline = new FramePipeline();
  pipeline->num_slots = num_slots;
  pipeline->next_slot_idx = 0;
  pipeline->slots.resize(num_slots);

  for (int i = 0; i < num_slots; i++) {
    auto &slot = pipeline->slots[i];
    slot.slot_id = i;
    slot.p_Vid = (VideoParameters*)calloc(1, sizeof(VideoParameters));
    memcpy(slot.p_Vid, master_Vid, sizeof(VideoParameters));
    slot.ppSliceList = (Slice**)calloc(MAX_NUM_DECSLICES, sizeof(Slice*));
    slot.iNumOfSlicesAllocated = MAX_NUM_DECSLICES;
    slot.p_Vid->ppSliceList = slot.ppSliceList;
    slot.p_Vid->iNumOfSlicesAllocated = MAX_NUM_DECSLICES;
    slot.allocated_FrameSizeInMbs = 0;
    slot.allocated_FrameHeightInMbs = 0;
    slot.allocated_PicWidthInMbs = 0;
  }
  return pipeline;
}

void f264_frame_pipeline_ensure_buffers(FrameWorkerSlot *slot, VideoParameters *p_Vid)
{
  if (slot->allocated_FrameSizeInMbs == (int)p_Vid->FrameSizeInMbs &&
      slot->allocated_FrameHeightInMbs == (int)p_Vid->FrameHeightInMbs &&
      slot->allocated_PicWidthInMbs == (int)p_Vid->PicWidthInMbs) {
    return;
  }

  // Free existing buffers if any
  if (slot->mb_data) {
    free(slot->mb_data);
    slot->mb_data = nullptr;
  }
  if (slot->intra_block) {
    free(slot->intra_block);
    slot->intra_block = nullptr;
  }
  if (slot->ipredmode) {
    free_mem2D(slot->ipredmode);
    slot->ipredmode = nullptr;
  }
  if (slot->nz_coeff) {
    free_mem4D(slot->nz_coeff);
    slot->nz_coeff = nullptr;
  }
  if (slot->siblock) {
    free_mem2Dint(slot->siblock);
    slot->siblock = nullptr;
  }
  if (slot->mb_to_slice_group_map) {
    free(slot->mb_to_slice_group_map);
    slot->mb_to_slice_group_map = nullptr;
    slot->allocated_map_size = 0;
  }

  // Allocate new buffers
  slot->mb_data = (Macroblock*)calloc(p_Vid->FrameSizeInMbs, sizeof(Macroblock));
  slot->intra_block = (char*)calloc(p_Vid->FrameSizeInMbs, sizeof(char));
  get_mem2D(&(slot->ipredmode), 4 * p_Vid->FrameHeightInMbs, 4 * p_Vid->PicWidthInMbs);
  get_mem4D(&(slot->nz_coeff), p_Vid->FrameSizeInMbs, 3, BLOCK_SIZE, BLOCK_SIZE);
  get_mem2Dint(&(slot->siblock), p_Vid->FrameHeightInMbs, p_Vid->PicWidthInMbs);
  slot->mb_to_slice_group_map = (int*)calloc(p_Vid->FrameSizeInMbs, sizeof(int));
  slot->allocated_map_size = p_Vid->FrameSizeInMbs;

  slot->allocated_FrameSizeInMbs = p_Vid->FrameSizeInMbs;
  slot->allocated_FrameHeightInMbs = p_Vid->FrameHeightInMbs;
  slot->allocated_PicWidthInMbs = p_Vid->PicWidthInMbs;

  slot->p_Vid->mb_data = slot->mb_data;
  slot->p_Vid->intra_block = slot->intra_block;
  slot->p_Vid->ipredmode = slot->ipredmode;
  slot->p_Vid->nz_coeff = slot->nz_coeff;
  slot->p_Vid->siblock = slot->siblock;
}

void f264_frame_pipeline_flush(FramePipeline *pipeline, threadqueue_queue_t *tq)
{
  if (!pipeline || !tq) return;
  for (int i = 0; i < pipeline->num_slots; i++) {
    auto &slot = pipeline->slots[i];
    if (slot.job) {
      f264_threadqueue_waitfor(tq, slot.job);
      f264_threadqueue_free_job(&slot.job);
    }
  }
}

void f264_frame_pipeline_free(FramePipeline *pipeline)
{
  if (!pipeline) return;
  for (int i = 0; i < pipeline->num_slots; i++) {
    auto &slot = pipeline->slots[i];
    if (slot.job) {
      f264_threadqueue_free_job(&slot.job);
    }
    for (int j = 0; j < slot.iSliceNumOfCurrPic; j++) {
      if (slot.ppSliceList && slot.ppSliceList[j]) {
        free_slice(slot.ppSliceList[j]);
        slot.ppSliceList[j] = nullptr;
      }
    }
    if (slot.ppSliceList) {
      free(slot.ppSliceList);
      slot.ppSliceList = nullptr;
    }
    if (slot.mb_data) {
      free(slot.mb_data);
      slot.mb_data = nullptr;
    }
    if (slot.intra_block) {
      free(slot.intra_block);
      slot.intra_block = nullptr;
    }
    if (slot.ipredmode) {
      free_mem2D(slot.ipredmode);
      slot.ipredmode = nullptr;
    }
    if (slot.nz_coeff) {
      free_mem4D(slot.nz_coeff);
      slot.nz_coeff = nullptr;
    }
    if (slot.siblock) {
      free_mem2Dint(slot.siblock);
      slot.siblock = nullptr;
    }
    if (slot.mb_to_slice_group_map) {
      free(slot.mb_to_slice_group_map);
      slot.mb_to_slice_group_map = nullptr;
    }
    if (slot.p_Vid) {
      free(slot.p_Vid);
      slot.p_Vid = nullptr;
    }
  }
  delete pipeline;
}
