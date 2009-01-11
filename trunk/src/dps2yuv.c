#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "dps_io.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "svnversion.h"

static AVCodec *pCodec;
static AVCodecContext *pCodecCtx;
static AVFrame *pFrame;
static AVFrame *pFrameA;
static AVFrame *pFrameB;

static int yuv_frame(struct dps_info* dps, size_t num, FILE* fout, int shift)
{
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

        if(1)
        {
            /* output frame */

            if(shift)
                fwrite(pFrameA->data[0], 1, pCodecCtx->width, fout);

            for(i = 0; i<pCodecCtx->height - shift; i++) /* Y */
            {
                fwrite(pFrameA->data[0] + pFrameA->linesize[0] * i, 1, pCodecCtx->width, fout);
                fwrite(pFrameB->data[0] + pFrameB->linesize[0] * i, 1, pCodecCtx->width, fout);
            };

            if(shift)
            {
                fwrite(pFrameA->data[0] + pFrameA->linesize[0] * (pCodecCtx->height - 1), 1, pCodecCtx->width, fout);
                fwrite(pFrameA->data[1], 1, pCodecCtx->width / 2, fout);
            };

            for(i = 0; i<pCodecCtx->height - shift; i++) /* U */
            {
                fwrite(pFrameA->data[1] + pFrameA->linesize[1] * i, 1, pCodecCtx->width / 2, fout);
                fwrite(pFrameB->data[1] + pFrameB->linesize[1] * i, 1, pCodecCtx->width / 2, fout);
            };

            if(shift)
            {
                fwrite(pFrameA->data[1] + pFrameA->linesize[1] * ((pCodecCtx->height - 1)), 1, pCodecCtx->width / 2, fout);
                fwrite(pFrameA->data[2], 1, pCodecCtx->width / 2, fout);
            };

            for(i = 0; i<pCodecCtx->height - shift; i++) /* V */
            {
                fwrite(pFrameA->data[2] + pFrameA->linesize[2] * i, 1, pCodecCtx->width / 2, fout);
                fwrite(pFrameB->data[2] + pFrameB->linesize[2] * i, 1, pCodecCtx->width / 2, fout);
            };

            if(shift)
                fwrite(pFrameA->data[2] + pFrameA->linesize[2] * ((pCodecCtx->height - 1)), 1, pCodecCtx->width / 2, fout);
        };

        /* free frames data */
        dps_frame_release(frameA);
        dps_frame_release(frameB);
    };

    return 0;
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
    fprintf(stderr, "dps2yuv-r" SVNVERSION " Copyright by Maksym Veremeyenko, 2009\n");

    /* check if filename is given */
    if(5 != argc)
    {
        fprintf(stderr, "dps2yuv: ERROR! no arguments supplied!\n");
        usage();
        return 1;
    };

    /* check colorspace */
    if(0 != strcasecmp("yuv422p", argv[2]))
    {
        fprintf(stderr, "dps2yuv: ERROR! Pixel format [%s] not supported, (possible yet!)\n", argv[2]);
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
        fprintf(stderr, "dps2yuv: ERROR! Fields order not supported [%s] not supported\n", argv[3]);
        usage();
        return 1;
    };

    /* register all avliv */
    av_register_all();

    /* Find the decoder for the video frame */
    pCodec = avcodec_find_decoder(CODEC_ID_MJPEG);
    if(NULL == pCodec)
    {
        fprintf(stderr, "dps2yuv: ERROR! Unsupported codec! for '%s'\n", "CODEC_ID_MJPEG");
        return 2;
    };

    /* Open codec */
    pCodecCtx = avcodec_alloc_context();
    if(avcodec_open(pCodecCtx, pCodec) < 0)
    {
        fprintf(stderr, "dps2yuv: ERROR! Could not open codec for '%s'\n", "CODEC_ID_MJPEG");
        return 2;
    };

    /* try to read dps file */
    r = dps_open(&dps, argv[1]);

    /* check results */
    if(0 == r)
    {
        /* open output file */
        if(0 != strcasecmp("-", argv[4]))
            fout = fopen(argv[4], "wb");
        else
            fout = stdout;

        if(NULL == fout)
        {
            r = errno;
            fprintf(stderr, "dps2yuv: ERROR! fopen(%s) failed with r=%d [%s]\n", argv[4], r, strerror(r));
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
        fprintf(stderr, "dps2yuv: ERROR! dps_open(%s) failed with r=%d [%s]\n", argv[1], r, strerror(-r));
        r = -r;
    };

    /* close file */
    dps_close(&dps);

    return r;
};
