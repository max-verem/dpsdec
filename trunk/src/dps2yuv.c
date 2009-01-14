#define PROGRAM "dps2yuv"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "dps_io.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "dps2yuv_out_pframes.h"

#include "svnversion.h"

static AVCodec *pCodec;
static AVCodecContext *pCodecCtx;
static AVFrame *pFrame;
static AVFrame *pFrameA;
static AVFrame *pFrameB;

static int yuv_frame(struct dps_info* dps, size_t num, FILE* fout, int shift)
{
    int r = 0;
    int i, r1, r2, c1, c2;
    struct dps_frame *frame;
    struct dps_frame *frameA;
    struct dps_frame *frameB;

    /* interlaces? two fields data per on logical frame */
    if(1)
    {
        /* read frames */
        frameA = dps_frame_read(dps, num*2);
        frameB = dps_frame_read(dps, num*2 + 1);

        /* Decode video frame */
        r1 = avcodec_decode_video(pCodecCtx, pFrameA, &c1, frameA->data, frameA->size);
        r2 = avcodec_decode_video(pCodecCtx, pFrameB, &c2, frameB->data, frameB->size);

        /* check error condition */
        if((r1 <= 0) || (r2 <= 0))
        {
            r = -1;
            if(r1 <= 0)
                fprintf(stderr, PROGRAM ": ERROR! avcodec_decode_video(%d) failed, r=%d, l=%d\n", num * 2, r1, c1);
            if(r2 <= 0)
                fprintf(stderr, PROGRAM ": ERROR! avcodec_decode_video(%d) failed, r=%d, l=%d\n", num * 2 + 1, r2, c2);
        }
        else
            r = 0;

        if(0 == r)
        {
            /* output frame */
            out_pframes(pFrameA, pFrameB, pCodecCtx->width, pCodecCtx->height, shift, fout);
        };

        /* free frames data */
        dps_frame_release(frameA);
        dps_frame_release(frameB);
    };

    return r;
};

static void usage()
{
    fprintf
    (
        stderr,
        "Usage:\n"
        "    dps2jpgs <input file> <pixel format> <field order> <output file>\n"
        "Where:\n"
        "    <input file>      - input DPS file name\n"
        "    <pixel format>    - color space output, supported values: yuv422p,yuv420p,yuv411p\n"
        "    <field order>     - output field ourder, supported value: U,L\n"
        "    <output file>     - output file name or - for stdout\n\n"
    );
};

int main(int argc, char** argv)
{
    int r, i, f;
    int shift;
    struct dps_info dps;
    char dump_frame[1024];
    FILE* fout;

    /* output info */
    fprintf(stderr, PROGRAM "-r" SVNVERSION " Copyright by Maksym Veremeyenko, 2009\n");

    /* check if filename is given */
    if(5 != argc)
    {
        fprintf(stderr, PROGRAM ": ERROR! no arguments supplied!\n");
        usage();
        return 1;
    };

    /* check colorspace */
    if(0 != strcasecmp("yuv422p", argv[2]))
    {
        fprintf(stderr, PROGRAM ": ERROR! Pixel format [%s] not supported, (possible yet!)\n", argv[2]);
        usage();
        return 1;
    };

    /* check fields output order */
    if(0 == strcasecmp("U", argv[3]))
        f = 0;
    else if (0 == strcasecmp("L", argv[3]))
        f = 1;
    else
    {
        fprintf(stderr, PROGRAM ": ERROR! Fields order not supported [%s] not supported\n", argv[3]);
        usage();
        return 1;
    };

    /* register all avliv */
    av_register_all();

    /* Find the decoder for the video frame */
    pCodec = avcodec_find_decoder(CODEC_ID_MJPEG);
    if(NULL == pCodec)
    {
        fprintf(stderr, PROGRAM ": ERROR! Unsupported codec! for '%s'\n", "CODEC_ID_MJPEG");
        return 2;
    };

    /* Open codec */
    pCodecCtx = avcodec_alloc_context();
    if(avcodec_open(pCodecCtx, pCodec) < 0)
    {
        fprintf(stderr, PROGRAM ": ERROR! Could not open codec for '%s'\n", "CODEC_ID_MJPEG");
        return 2;
    };

    /* try to read dps file */
    r = dps_open(&dps, argv[1]);

    /* check results */
    if(0 == r)
    {
        /* output info */
        fprintf
        (
            stderr,
            PROGRAM ": INFO: frames: %d\n"
            PROGRAM ": INFO: size: %dx%d\n",
            dps.frames_count,
            dps.width, dps.height
        );

        /* open output file */
        if(0 != strcasecmp("-", argv[4]))
            fout = fopen(argv[4], "wb");
        else
            fout = stdout;

        if(NULL == fout)
        {
            r = errno;
            fprintf(stderr, PROGRAM ": ERROR! fopen(%s) failed with r=%d [%s]\n", argv[4], r, strerror(r));
        }
        else
        {
            /* Allocate video frame */
            pFrameA = avcodec_alloc_frame();
            pFrameB = avcodec_alloc_frame();
            pFrame = avcodec_alloc_frame();

            /* do all frames */
            for(i = 0; i < dps.frames_count; i++)
                yuv_frame(&dps, i, fout, /* we assume input is UFF */ f);

            /* free frame */
            av_free(pFrame);
            av_free(pFrameA);
            av_free(pFrameB);

            /* close output file */
            fclose(fout);
        };
    }
    else
    {
        fprintf(stderr, PROGRAM ": ERROR! dps_open(%s) failed with r=%d [%s]\n", argv[1], r, strerror(-r));
        r = -r;
    };

    /* close file */
    dps_close(&dps);

    return r;
};
