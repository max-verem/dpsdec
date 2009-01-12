#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "dps_io.h"

#define DPS_CHUNK_MAX_SIZE      1024
#define DPS_CHUNK_BODY_SIZE     0x00000000000000FFLL
#define DPS_CHUNK_ATTACH_SIZE   0xFFFFFFFFFFFFFF00LL

#define DPS_SIGN        0x30535044     /* 44 50 53 30 */
#define DPS_DATA_POS    0x0000000000000120LL
#define DPS_BLOCK_SIZE  512LL

static off_t read_chunk_body(FILE* res, unsigned char* data)
{
    off_t save_pos = ftell(res);
    off_t chunk_sizes = 0;

    /* clear data */
    memset(data, 0, DPS_CHUNK_MAX_SIZE);

    /* read chunk header */
    fread(&chunk_sizes, 1, sizeof(off_t), res);

#ifdef _DEBUG_
    fprintf
    (
        stderr,
        "dps_io(read_chunk): @%0.8llX chunk size %lld (0x%.2llX), attach size %lld (0x%.2llX)\n",
        save_pos,
        chunk_sizes & DPS_CHUNK_BODY_SIZE,
            chunk_sizes & DPS_CHUNK_BODY_SIZE,
        chunk_sizes & DPS_CHUNK_ATTACH_SIZE,
            chunk_sizes & DPS_CHUNK_ATTACH_SIZE
    );
#endif /* _DEBUG_ */

    /* read chunk data */
    fread(data, 1, (chunk_sizes & DPS_CHUNK_BODY_SIZE) - sizeof(off_t), res);

    /* seek back to pos */
//    fseek(save_pos);

    return chunk_sizes;
};


int dps_open(struct dps_info* dps, char* filename)
{
    unsigned char *chunk_body;
    off_t s;
    int32_t signature;
    int r, i;

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

    /* seek to data start */
    fseek(dps->res, DPS_DATA_POS, SEEK_SET);

    /* allocate chunk space */
    chunk_body = (char*)malloc(DPS_CHUNK_MAX_SIZE);

    /* read chunks */
    for(r = 1; r > 0; )
    {
        /* read chunks */
        s = read_chunk_body(dps->res, chunk_body);

        /* check result */
        if(s < 0)
            r = -1;
        else
        {
            switch(chunk_body[0])
            {
                case 0x00:                              /* video/audio description block */
                        switch(chunk_body[4])
                        {
                            case 0x00:                  /* video info (0) */

                                /* dims */
                                dps->width = ((int16_t*)(chunk_body + 8))[0];
                                dps->height = ((int16_t*)(chunk_body + 8))[1];

                                /* frames */
                                dps->frames_count = ((int32_t*)(chunk_body + 24))[0];

                                /* assume there are 2 fields */
                                dps->fields_count = 2;

                                break;
                            case 0x01:                  /* audio info (1) */
                                /* ignore that data */
                                break;
                            default:
                                r = -2;
                                break;
                        };
                    break;

                case 0x02:                              /* index block */
                        switch(chunk_body[4])
                        {
                            case 0x00:                  /* video (0) */
                                if(dps->frames_count)
                                {
                                    /* allocate index sizes */
                                    dps->frames_size = (int32_t*)malloc(s & DPS_CHUNK_ATTACH_SIZE);

                                    /* read index sizes table */
                                    fread(dps->frames_size, 1, s & DPS_CHUNK_ATTACH_SIZE, dps->res);
                                }
                                else
                                    r = -4;
                                break;
                            case 0x01:                  /* audio (1) */
                                /* ignore that data */
                                fseek(dps->res, s & DPS_CHUNK_ATTACH_SIZE, SEEK_CUR);
                                break;
                            default:                    /* error data */
                                r = -3;
                                break;
                        };
                    break;

                case 0x03:                              /* video frames data */
                        if((dps->frames_count) && (dps->frames_size))
                        {
                            /* allocate index positions */
                            dps->frames_pos = (off_t*)malloc(dps->fields_count * dps->frames_count * sizeof(off_t));

                            /* calc first frame pos */
                            dps->frames_pos[0] = ftell(dps->res);

#ifdef _DEBUG_
                            fprintf
                            (
                                stderr, "dps_io(dps_open): dps->frames_pos[0]=%.8llX\n",
                                dps->frames_pos[0], dps->frames_pos[0]
                            );
#endif /* _DEBUG_ */

                            /* calc rest table */
                            for(i = 1; i < (dps->frames_count * dps->fields_count); i++)
                            {
                                dps->frames_pos[i] = dps->frames_pos[i - 1] + dps->frames_size[i - 1] * DPS_BLOCK_SIZE;
#ifdef _DEBUG_
                                fprintf
                                (
                                    stderr, 
                                    "dps_io(dps_open): dps->frames_pos[%ld]=%.8llX, dps->frames_size[%ld - 1]=%d\n",
                                    i, dps->frames_pos[i], i, dps->frames_size[i - 1]
                                );
#endif /* _DEBUG_ */
                            };

                            /* no more */
                            r = 0;
                        }
                        else
                            r = -5;
                    break;
            };
        };
    };

    /* free chunk buffer */
    free(chunk_body);

    /* check results */
    if(0 == r)
        return 0;

    return -EFAULT;
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

#ifdef _DEBUG_
    fprintf(stderr, "dps_io(dps_frame_read): num=%d, pos=%lld, size=%d\n", num, dps->frames_pos[num], frame->size);
#endif /* _DEBUG_ */

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

