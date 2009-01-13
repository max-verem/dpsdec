#include <stdio.h>
#include <stdlib.h>
#include "dps_io.h"

#include "svnversion.h"

#define NUM_SUFF "%s%.06d"

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

static void usage()
{
    fprintf
    (
        stderr,
        "Usage:\n"
        "    dps2jpgs <src.dps> <dst_seq_prefix> [<start frame> <count>]\n"
        "Where:\n"
        "    <src.dps>         - input DPS file\n"
        "    <dst_seq_prefix>  - output sequence prefix, i.e. /tmp/test_seq_ number and extension will be added automaticaly\n"
    );
};

int main(int argc, char** argv)
{
    int r, i, c;
    struct dps_info dps;
    char dump_frame[1024];

    /* output info */
    fprintf(stderr, "dps2jpgs-r" SVNVERSION " Copyright by Maksym Veremeyenko, 2009\n");

    /* check if filename is given */
    if((3 != argc) && (5 != argc))
    {
        fprintf(stderr, "dps2jpgs: ERROR! no arguments supplied!\n");
        usage();
        return 1;
    };

    /* try to read dps file */
    r = dps_open(&dps, argv[1]);

    /* check results */
    if(0 == r)
    {
        /* dump usefull info */
        fprintf(stderr, "dps2jpgs: Frames count: %d\n", dps.frames_count);

        /* setup frames range */
        if(5 == argc)
        {
            i = atol(argv[3]);
            c = atol(argv[4]);
        }
        else
        {
            i = 0;
            c = dps.frames_count;
        };

        for(c = i + c; i < c; i++)
        {
            /* interlaces? two fields data per on logical frame */
            if(1)
            {
                /* dump field A */
                sprintf(dump_frame, NUM_SUFF "A.jpg", argv[2], i);
                frame2file(&dps, 2*i + 0, dump_frame);

                /* dump field B */
                sprintf(dump_frame, NUM_SUFF "B.jpg", argv[2], i);
                frame2file(&dps, 2*i + 1, dump_frame);
            };
        };

        r = 0;
    }
    else
    {
        fprintf(stderr, "dps2jpgs: ERROR! dps_open(%s) failed with r=%d [%s]\n", argv[1], r, strerror(-r));
        r = -r;
    };

    /* close file */
    dps_close(&dps);

    return r;
};
