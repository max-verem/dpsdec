#include <stdio.h>
#include <stdlib.h>
#include "dps_io.h"

static int frame2file(struct dps_info* dps, size_t num, char* filename)
{
    int r;
    FILE* f;
    struct dps_frame* frame;

    /* read frame */
    frame = dps_frame_read(dps, num);

    if(NULL == frame)
        r = -1;
    else
    {
        /* open output file */
        f = fopen(filename, "wb");

        if(NULL == f)
            r = -2;
        else
        {
            fwrite(frame->data, 1, frame->size, f);
            fclose(f);
        };

        /* free frame data */
        dps_frame_release(frame);
    };

    return r;
};

int main(int argc, char** argv)
{
    int r, i;
    struct dps_info dps;
    char dump_frame[1024];

    /* check if filename is given */
    if(3 != argc)
    {
        fprintf(stderr, "ERROR: no arguments supplied!\nUsage:\n\tdps2jpgs <src.dps> <dst_seq_prefix>\n");
        return 1;
    };


    /* try to read dps file */
    r = dps_open(&dps, argv[1]);

    /* check results */
    if(0 == r)
    {
        /* dump usefull info */
        printf("Frames count: %d\n", dps.frames_count);

        for(i = 0; i < dps.frames_count; i++)
        {
            /* interlaces? two fields data per on logical frame */
            if(1)
            {
                /* dump field A */
                sprintf(dump_frame, "%s%.04dA.jpg", argv[2], i);
                frame2file(&dps, 2*i + 0, dump_frame);

                /* dump field B */
                sprintf(dump_frame, "%s%.04dB.jpg", argv[2], i);
                frame2file(&dps, 2*i + 1, dump_frame);
            };
        };

        r = 0;
    }
    else
    {
        fprintf(stderr, "ERROR, dps_open(%s) failed with r=%d [%s]\n", argv[1], r, strerror(-r));
        r = -r;
    };

    /* close file */
    dps_close(&dps);

    return r;
};
