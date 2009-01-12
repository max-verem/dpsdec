#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "dps_io.h"

#define DPS_SIGN        0x30535044     /* 44 50 53 30 */
#define DPS_DIM_POS     0x00000130
#define DPS_FRAMES_POS  0x00000140
#define DPS_INDEX_POS   0x00000158
#define DPS_BLOCK_SIZE  512

int dps_open(struct dps_info* dps, char* filename)
{
    int32_t signature;
    size_t index_blocks, i;

    /* clear data struct */
    memset(dps, 0, sizeof(struct dps_info));

    /* open file */
    dps->res = fopen(filename, "rb");
    if(NULL == dps->res)
        return -errno;

    /* read signature */
    fseek(dps->res, 0L, SEEK_SET);
    fread(&signature, 1, 4, dps->res);

    /* check signature */
    if(DPS_SIGN != signature)
        return -EMEDIUMTYPE;

    /* read frame dimentions */
    fseek(dps->res, DPS_DIM_POS, SEEK_SET);
    fread(&dps->width, 1, sizeof(int16_t), dps->res);
    fread(&dps->height, 1, sizeof(int16_t), dps->res);

    /* read frames number */
    fseek(dps->res, DPS_FRAMES_POS, SEEK_SET);
    fread(&dps->frames_count, 1, sizeof(int32_t), dps->res);

    /* assume there are 2 fields */
    dps->fields_count = 2;

    /* calc index table size in 512 blocks */
    index_blocks = dps->frames_count * dps->fields_count * sizeof(int32_t);
    index_blocks = (index_blocks / DPS_BLOCK_SIZE) +  ((index_blocks % DPS_BLOCK_SIZE)?1:0);

    /* allocate index sizes */
    dps->frames_size = (int*)malloc(index_blocks * DPS_BLOCK_SIZE);

    /* read index sizes table */
    fseek(dps->res, DPS_INDEX_POS, SEEK_SET);
    fread(dps->frames_size, DPS_BLOCK_SIZE, index_blocks, dps->res);

    /* allocate index positions */
    dps->frames_pos = (size_t*)malloc(dps->fields_count * dps->frames_count * sizeof(size_t));

    /* calc first frame pos */
    dps->frames_pos[0] = index_blocks * DPS_BLOCK_SIZE + 16 + DPS_INDEX_POS;

    /* calc whole table */
    for(i = 1; i < (dps->frames_count * dps->fields_count); i++)
        dps->frames_pos[i] = dps->frames_pos[i - 1] + dps->frames_size[i - 1] * DPS_BLOCK_SIZE;

    return 0;
};

int dps_close(struct dps_info* dps)
{
    if(NULL != dps->res)
        fclose(dps->res);

    if(NULL != dps->frames_pos)
        free(dps->frames_pos);

    if(NULL != dps->frames_size)
        free(dps->frames_size);

    return 0;
};

struct dps_frame* dps_frame_read(struct dps_info* dps, size_t num)
{
    struct dps_frame* frame;

    /* allocate frame struct */
    frame = (struct dps_frame*)malloc(sizeof(struct dps_frame));

    /* allocate frame data buffer */
    frame->size = dps->frames_size[num] * DPS_BLOCK_SIZE;
    frame->data = malloc(frame->size);

    /* seek */
    fseek(dps->res, dps->frames_pos[num], SEEK_SET);

    /* read */
    fread(frame->data, 1, frame->size, dps->res);

    return frame;
};

void dps_frame_release(struct dps_frame* frame)
{
    if(NULL != frame)
    {
        if(NULL != frame->data)
            free(frame->data);

        free(frame);
    };
};

