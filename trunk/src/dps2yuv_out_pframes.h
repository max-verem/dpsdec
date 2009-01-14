static void out_pframes(AVFrame *pFrameA, AVFrame *pFrameB, int width, int height, int shift, FILE* fout)
{
    int i;

    /* output frame */

    if(shift)
        fwrite(pFrameA->data[0], 1, width, fout);

    for(i = 0; i< (height - shift); i++) /* Y */
    {
        fwrite(pFrameA->data[0] + pFrameA->linesize[0] * i, 1, width, fout);
        fwrite(pFrameB->data[0] + pFrameB->linesize[0] * i, 1, width, fout);
    };

    if(shift)
    {
        fwrite(pFrameA->data[0] + pFrameA->linesize[0] * (height - 1), 1, width, fout);
        fwrite(pFrameA->data[1], 1, width / 2, fout);
    };

    for(i = 0; i < (height - shift); i++) /* U */
    {
        fwrite(pFrameA->data[1] + pFrameA->linesize[1] * i, 1, width / 2, fout);
        fwrite(pFrameB->data[1] + pFrameB->linesize[1] * i, 1, width / 2, fout);
    };

    if(shift)
    {
        fwrite(pFrameA->data[1] + pFrameA->linesize[1] * (height - 1), 1, width / 2, fout);
        fwrite(pFrameA->data[2], 1, width / 2, fout);
    };

    for(i = 0; i< (height - shift); i++) /* V */
    {
        fwrite(pFrameA->data[2] + pFrameA->linesize[2] * i, 1, width / 2, fout);
        fwrite(pFrameB->data[2] + pFrameB->linesize[2] * i, 1, width / 2, fout);
    };

    if(shift)
        fwrite(pFrameA->data[2] + pFrameA->linesize[2] * (height - 1), 1, width / 2, fout);
};
