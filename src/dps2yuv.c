#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "dps_io.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

static AVCodec *pCodec;
static AVCodecContext *pCodecCtx;
static AVFrame *pFrame;
static AVFrame *pFrameA;
static AVFrame *pFrameB;


static int yuv_frame(struct dps_info* dps, size_t num, FILE* fout)
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
            for(i = 0; i<pCodecCtx->height; i++) /* Y */
            {
                fwrite(pFrameA->data[0] + pFrameA->linesize[0] * i, 1, pCodecCtx->width, fout);
                fwrite(pFrameB->data[0] + pFrameB->linesize[0] * i, 1, pCodecCtx->width, fout);
            };
            for(i = 0; i<pCodecCtx->height; i++) /* U */
            {
                fwrite(pFrameA->data[1] + pFrameA->linesize[1] * i, 1, pCodecCtx->width / 2, fout);
                fwrite(pFrameB->data[1] + pFrameB->linesize[1] * i, 1, pCodecCtx->width / 2, fout);
            };
            for(i = 0; i<pCodecCtx->height; i++) /* V */
            {
                fwrite(pFrameA->data[2] + pFrameA->linesize[2] * i, 1, pCodecCtx->width / 2, fout);
                fwrite(pFrameB->data[2] + pFrameB->linesize[2] * i, 1, pCodecCtx->width / 2, fout);
            };
        };

        /* free frames data */
        dps_frame_release(frameA);
        dps_frame_release(frameB);
    };

    return 0;
};


int main(int argc, char** argv)
{
    int r, i;
    struct dps_info dps;
    char dump_frame[1024];
    FILE* fout = stdout;

    /* check if filename is given */
    if(3 != argc)
    {
        fprintf(stderr, "ERROR: no arguments supplied!\nUsage:\n\tdps2jpgs <src.dps> <output file>\n");
        return 1;
    };

    /* try to read dps file */
    r = dps_open(&dps, argv[1]);

    /* check results */
    if(0 == r)
    {
        /* register all avliv */
        av_register_all();

        /* Find the decoder for the video frame */
        pCodec = avcodec_find_decoder(CODEC_ID_MJPEG);
        if(NULL == pCodec)
        {
            fprintf(stderr, "ERROR: Unsupported codec! for '%s'", "CODEC_ID_MJPEG");
            return -EINVAL;
        };

        /* Open codec */
        pCodecCtx = avcodec_alloc_context();
        if(avcodec_open(pCodecCtx, pCodec) < 0)
        {
            fprintf(stderr, "ERROR: Could not open codec for '%s'", "CODEC_ID_MJPEG");
            return -EINVAL;
        };


        /* Allocate video frame */
        pFrameA = avcodec_alloc_frame();
        pFrameB = avcodec_alloc_frame();
        pFrame = avcodec_alloc_frame();

        /* do all frames */
        for(i = 0; i < dps.frames_count; i++)
            yuv_frame(&dps, i, fout);

        r = 0;

        /* free frame */
        av_free(pFrame);
        av_free(pFrameA);
        av_free(pFrameB);

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
