#define PROGRAM "dps2yuv_m"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include "dps_io.h"

#include <pthread.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "dps2yuv_out_pframes.h"

#include "svnversion.h"

#define CELL_YUV_PRESENT        1
#define CELL_JPEG_PRESENT       2

struct dps2yuv_instance;
struct frames_pipe_cell
{
    size_t num;
    int is_present;

    /* non-interlaces */
    AVFrame *pFrame;
    struct dps_frame *frame;

    /* interlaced */
    AVFrame *pFrameA;
    AVFrame *pFrameB;
    struct dps_frame *frameA;
    struct dps_frame *frameB;

    /* parent info */
    int id;
    struct dps2yuv_instance* parent;

    pthread_t thread;
    pthread_cond_t trig_decoder;
};

struct dps2yuv_instance
{
    int f_exit, A, B, cells_count, shift;
    FILE* fout;
    struct dps_info dps;
    struct frames_pipe_cell* frames_pipe;
    pthread_mutex_t lock;
    pthread_cond_t trig_reader;
    pthread_cond_t trig_writer;
    pthread_t thread_reader;
    pthread_t thread_writer;
};

static int get_empty_cell(struct dps2yuv_instance* desc, int flag, int num)
{
    int i;

    for(i = 0; i < desc->cells_count; i++)
        if((-1 == num) || (desc->frames_pipe[i].num == num))
            if(desc->frames_pipe[i].is_present == flag)
                return i;

    return -1;
};

static void* frames_pipe_reader(void* p)
{
    int i, j;
    struct dps2yuv_instance* desc = (struct dps2yuv_instance*)p;

    /* read all frames */
    for(i = desc->A; (i < desc->B) && (0 == desc->f_exit); )
    {
        /* lock mutex */
        pthread_mutex_lock(&desc->lock);

        /* get free slot */
        j = get_empty_cell(desc, 0, -1);

        /* check if free slot exist */
        if(-1 == j)
        {
            /* wait for signal from writer */
            pthread_cond_wait(&desc->trig_reader, &desc->lock);

            /* get free slot */
            j = get_empty_cell(desc, 0, -1);
        };

        /* unlock mutex */
        pthread_mutex_unlock(&desc->lock);

        /* start read */
        if(-1 != j)
        {
#ifdef _DEBUG_
            fprintf(stderr, PROGRAM "(frames_pipe_reader): START #%3d [%d]\n", i, j);
#endif /* _DEBUG_ */

            /* read frames */
            desc->frames_pipe[j].frameA = dps_frame_read(&desc->dps, i * 2);
            desc->frames_pipe[j].frameB = dps_frame_read(&desc->dps, i * 2 + 1);

            /* lock mutex */
            pthread_mutex_lock(&desc->lock);

            desc->frames_pipe[j].num = i;
            desc->frames_pipe[j].is_present = CELL_JPEG_PRESENT;

#ifdef _DEBUG_
            fprintf(stderr, PROGRAM "(frames_pipe_reader): DONE  #%3d [%d]\n", i, j);
#endif /* _DEBUG_ */

            /* start encoding */
            pthread_cond_signal(&desc->frames_pipe[j].trig_decoder);

            /* unlock mutex */
            pthread_mutex_unlock(&desc->lock);

            /* increment readed frames */
            i++;
        };
    };

    return NULL;
};

static void* frames_pipe_writer(void* p)
{
    int i, j;
    struct dps2yuv_instance* desc = (struct dps2yuv_instance*)p;

    /* read all frames */
    for(i = desc->A; (i < desc->B) && (0 == desc->f_exit); )
    {
        /* lock mutex */
        pthread_mutex_lock(&desc->lock);

        /* get free slot */
        j = get_empty_cell(desc, CELL_YUV_PRESENT | CELL_JPEG_PRESENT, i);

        /* check if free slot exist */
        if(-1 == j)
        {
            /* wait for signal from writer */
            pthread_cond_wait(&desc->trig_writer, &desc->lock);

            /* get free slot */
            j = get_empty_cell(desc, CELL_YUV_PRESENT | CELL_JPEG_PRESENT, i);
        };

        /* unlock mutex */
        pthread_mutex_unlock(&desc->lock);

#ifdef _DEBUG_
        fprintf(stderr, PROGRAM "(frames_pipe_writer): START #%d [%d]\n", i, j);
#endif /* _DEBUG_ */

        /* start writes */
        if(-1 != j)
        {

            /* output frame */
            if(1)
                out_pframes
                (
                    desc->frames_pipe[j].pFrameA,
                    desc->frames_pipe[j].pFrameB,
                    desc->dps.width,
                    desc->dps.height / 2,
                    1,
                    desc->fout
                );
            else
            {
            };

            /* lock mutex */
            pthread_mutex_lock(&desc->lock);

            /* read frames */
            desc->frames_pipe[j].is_present = 0;

#ifdef _DEBUG_
            fprintf(stderr, PROGRAM "(frames_pipe_writer): DONE  #%d [%d]\n", i, j);
#endif /* _DEBUG_ */

            /* start encoding */
            pthread_cond_signal(&desc->trig_reader);

            /* unlock mutex */
            pthread_mutex_unlock(&desc->lock);

            /* increment written frames */
            i++;
        };
    };

    return NULL;
};

static pthread_mutex_t avlock;

static void* frames_pipe_decoder(void* p)
{
    int r, c1, c2, r1, r2;

    /* avlib data */
    AVCodec *pCodec;
    AVCodecContext *pCodecCtx;

    struct frames_pipe_cell* cell = (struct frames_pipe_cell*)p;
    struct dps2yuv_instance* desc = cell->parent;

    /* Find the decoder for the video frame */
    pthread_mutex_lock(&avlock);
    pCodec = avcodec_find_decoder(CODEC_ID_MJPEG);
    pthread_mutex_unlock(&avlock);
    if(NULL == pCodec)
    {
        fprintf(stderr, PROGRAM ": ERROR! Unsupported codec! for '%s'\n", "CODEC_ID_MJPEG");
        return NULL;
    };

    /* Open codec */
    pthread_mutex_lock(&avlock);
    pCodecCtx = avcodec_alloc_context();
    r = avcodec_open(pCodecCtx, pCodec);
    pthread_mutex_unlock(&avlock);
    if(r < 0)
    {
        fprintf(stderr, PROGRAM ": ERROR! Could not open codec for '%s'\n", "CODEC_ID_MJPEG");
        return NULL;
    };

#ifdef _DEBUG_
    fprintf(stderr, PROGRAM "(frames_pipe_decoder[%d]): started\n", cell->id);
#endif /* _DEBUG_ */

    while(0 == desc->f_exit)
    {
        /* lock mutex */
        pthread_mutex_lock(&desc->lock);

#ifdef _DEBUG_
        fprintf(stderr, PROGRAM "(frames_pipe_decoder[%d]): pthread_cond_wait\n", cell->id);
#endif /* _DEBUG_ */

        if(cell->is_present != CELL_JPEG_PRESENT)
            /* wait for signal from writer */
            pthread_cond_wait(&cell->trig_decoder, &desc->lock);

        /* unlock mutex */
        pthread_mutex_unlock(&desc->lock);

#ifdef _DEBUG_
        fprintf(stderr, PROGRAM "(frames_pipe_decoder[%d]): #%d, is_present=%d\n", cell->id, cell->num, cell->is_present);
#endif /* _DEBUG_ */

        /* check for exit status */
        if((0 == desc->f_exit) && (0 != (cell->is_present & CELL_JPEG_PRESENT)))
        {
            /* Decode video frame */
            if(1)
            {
                r1 = avcodec_decode_video(pCodecCtx, cell->pFrameA, &c1, cell->frameA->data, cell->frameA->size);
                r2 = avcodec_decode_video(pCodecCtx, cell->pFrameB, &c2, cell->frameB->data, cell->frameB->size);

                /* free frames data */
                dps_frame_release(cell->frameA);
                dps_frame_release(cell->frameB);
            };

            /* lock mutex */
            pthread_mutex_lock(&desc->lock);

            /* mark frame decoded */
            cell->is_present |= CELL_YUV_PRESENT;

#ifdef _DEBUG_
            fprintf(stderr, PROGRAM "(frames_pipe_decoder[%d]): #%d, DONE\n", cell->id, cell->num);
#endif /* _DEBUG_ */

            /* start writer */
            pthread_cond_signal(&desc->trig_writer);

            /* unlock mutex */
            pthread_mutex_unlock(&desc->lock);
        };
    };

    return NULL;
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

static struct dps2yuv_instance instance;

static void sighandler (int sig)
{
    int i;

    /* rise exit flag */
    instance.f_exit = 1;

    /* notify */
    fprintf(stderr, PROGRAM ": Interrupts signal catched!\n");

    /* lock mutex */
    pthread_mutex_lock(&instance.lock);

    /* signal */
    pthread_cond_signal(&instance.trig_writer);
    pthread_cond_signal(&instance.trig_reader);
    for(i = 0; i<instance.cells_count; i++)
        pthread_cond_signal(&instance.frames_pipe[i].trig_decoder);

    /* unlock mutex */
    pthread_mutex_unlock(&instance.lock);
};

static void signals()
{
    /* setup handlers */
    signal (SIGINT, sighandler);
    signal (SIGTERM, sighandler);
    signal (SIGPIPE, sighandler);
    signal (SIGHUP, sighandler);
    signal (SIGALRM, sighandler);
};

int main(int argc, char** argv)
{
    int r, i;
    void* p;

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
        instance.shift = 0;
    else if (0 == strcasecmp("L", argv[3]))
        instance.shift = 1;
    else
    {
        fprintf(stderr, PROGRAM ": ERROR! Fields order not supported [%s] not supported\n", argv[3]);
        usage();
        return 1;
    };

    /* register all avliv */
    av_register_all();

    /* try to read dps file */
    r = dps_open(&instance.dps, argv[1]);

    /* check results */
    if(0 == r)
    {
        /* output info */
        fprintf
        (
            stderr,
            PROGRAM ": INFO: frames: %d\n"
            PROGRAM ": INFO: size: %dx%d\n",
            instance.dps.frames_count,
            instance.dps.width, instance.dps.height
        );

        /* open output file */
        if(0 != strcasecmp("-", argv[4]))
            instance.fout = fopen(argv[4], "wb");
        else
            instance.fout = stdout;

        if(NULL == instance.fout)
        {
            r = errno;
            fprintf(stderr, PROGRAM ": ERROR! fopen(%s) failed with r=%d [%s]\n", argv[4], r, strerror(r));
        }
        else
        {
            /* assume 4 threads */
            instance.cells_count = 8;

            /* frame range */
            instance.A = 0;
            instance.B = instance.dps.frames_count;

            /* init locks */
            pthread_mutex_init(&instance.lock, NULL);
            pthread_mutex_init(&avlock, NULL);


            /* prepare pipes */
            instance.frames_pipe = (struct frames_pipe_cell*)malloc(sizeof(struct frames_pipe_cell)*instance.cells_count);
            memset(instance.frames_pipe, 0, sizeof(struct frames_pipe_cell) *  instance.cells_count);
            for(i = 0; i<instance.cells_count; i++)
            {
                /* init cond */
                pthread_cond_init(&instance.frames_pipe[i].trig_decoder, NULL);

                /* Allocate video frame */
                instance.frames_pipe[i].pFrameA = avcodec_alloc_frame();
                instance.frames_pipe[i].pFrameB = avcodec_alloc_frame();
                instance.frames_pipe[i].pFrame = avcodec_alloc_frame();

                /* other */
                instance.frames_pipe[i].id = i;
                instance.frames_pipe[i].parent = &instance;
            };

            /* setup signals */
            signals();

            /* start decoder threads */
            for(i = 0; i<instance.cells_count; i++)
                pthread_create
                (
                    &instance.frames_pipe[i].thread,
                    NULL,
                    frames_pipe_decoder,
                    &instance.frames_pipe[i]
                );
            /* start reader/writer */
            pthread_create
            (
                &instance.thread_writer,
                NULL,
                frames_pipe_writer,
                &instance
            );
            pthread_create
            (
                &instance.thread_reader,
                NULL,
                frames_pipe_reader,
                &instance
            );



            /* wait for thread finish */
            pthread_join(instance.thread_reader, &p);
            pthread_join(instance.thread_writer, &p);

#ifdef _DEBUG_
            fprintf(stderr, PROGRAM "(main): exiting, waiting for decoders\n");
#endif /* _DEBUG_ */

            instance.f_exit = 1;

            /* release pipes */
            for(i = 0; i<instance.cells_count; i++)
            {
                /* signal them */
                pthread_cond_signal(&instance.frames_pipe[i].trig_decoder);

                /* wait for thread finish */
                pthread_join(instance.frames_pipe[i].thread, &p);

                /* init cond */
                pthread_cond_destroy(&instance.frames_pipe[i].trig_decoder);

                /* free frame */
                av_free(instance.frames_pipe[i].pFrame);
                av_free(instance.frames_pipe[i].pFrameA);
                av_free(instance.frames_pipe[i].pFrameB);
            };
            pthread_cond_destroy(&instance.trig_reader);
            pthread_cond_destroy(&instance.trig_writer);

            /* destroy lock */
            pthread_mutex_destroy(&instance.lock);
            pthread_mutex_destroy(&avlock);

            /* close output file */
            fclose(instance.fout);
        };
    }
    else
    {
        fprintf(stderr, PROGRAM ": ERROR! dps_open(%s) failed with r=%d [%s]\n", argv[1], r, strerror(-r));
        r = -r;
    };

    /* close file */
    dps_close(&instance.dps);

    return r;
};
