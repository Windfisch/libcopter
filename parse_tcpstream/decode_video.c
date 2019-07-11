// TCP stream splitter. warning, this is really dirty code

// based upon the decode_video.c libavcodec API example by Fabrice Bellard
// its license header is below:
/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/**
 * @file
 * video decoding with libavcodec API example
 *
 * @example decode_video.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#define bool int
#define false 0
#define true 1

#define INBUF_SIZE 4096

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  
  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
  
  // Close file
  fclose(pFile);
}
static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
                     char *filename)
{
    FILE *f;
    int i;

    f = fopen(filename,"w");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

static void decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt, AVFrame *frameRGB,
                   const char *filename)
{
    char buf[1024];
    int ret;
    static struct SwsContext* sws_ctx = NULL;

	printf("sending pkt\n");
    ret = avcodec_send_packet(dec_ctx, pkt);
	printf("sent pkt\n");
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }

        // TODO if first time, initialize swsparams
        if (sws_ctx == NULL)
        {
            sws_ctx =
                sws_getContext
                (
                 frame->width,
                 frame->height,
                 (enum AVPixelFormat) frame->format,
                 frame->width,
                 frame->height,
                 AV_PIX_FMT_RGB24,
                 SWS_BILINEAR,
                 NULL,
                 NULL,
                 NULL
                );
				printf("inited sws ctx\n");
        }

	printf("w/h = %d / %d\n", frame->width, frame->height);

        sws_scale
            (
             sws_ctx,
             (uint8_t const * const *)frame->data,
             frame->linesize,
             0,
             frame->height,
             frameRGB->data,
             frameRGB->linesize
            );
	
	// Save the frame to disk
	  SaveFrame(frameRGB, frame->width, frame->height, 
		    dec_ctx->frame_number);




        printf("saving frame %3d\n", dec_ctx->frame_number);
        fflush(stdout);

		switch (frame->colorspace)
		{
			case AVCOL_SPC_RGB: printf("RGB\n"); break;
			default: printf("%d\n", frame->colorspace); break;
		}

        /* the picture is allocated by the decoder. no need to
           free it */
        snprintf(buf, sizeof(buf), "%s-%d", filename, dec_ctx->frame_number);
        pgm_save(frame->data[0], frame->linesize[0],
                 frame->width, frame->height, buf);
    }
}


void do_parse(AVCodecParserContext* parser, AVCodecContext* c, AVPacket* pkt, uint8_t* data, int data_size, AVFrame* frame, AVFrame* rgbframe, const char* outfilename)
{
		/*printf("parsing %d bytes: ", data_size);
		for (int i=0; i<data_size; i++)
			printf("%2x ", data[i]);
		printf("\n");*/

		int iters = 0;
        while (data_size > 0) {
			iters++;

            int ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                                   data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
			printf("parsed\n");
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                exit(1);
            }
            data      += ret;
            data_size -= ret;

			if (pkt->size)
			{
				printf("got packet of size %d\n", pkt->size);
				//for (int i=0; i<pkt->size; i++)
				//	printf("%2x ", pkt->data[i]);
				//printf("\n");

				if (pkt->size > 44)
				{
					if (
						pkt->data[pkt->size-44] == 0 &&
						pkt->data[pkt->size-43] == 0 &&
						pkt->data[pkt->size-42] == 1 &&
						pkt->data[pkt->size-41] == 0xa0)
					{
						printf("got a0 frame\n");
					}

					if (
						pkt->data[pkt->size-44] == 0 &&
						pkt->data[pkt->size-43] == 0 &&
						pkt->data[pkt->size-42] == 1 &&
						pkt->data[pkt->size-41] == 0xa1)
					{
						printf("got a1 frame\n");
					}

				}
			}

            if (pkt->size)
                decode(c, frame, pkt, rgbframe, outfilename);
        }

		printf("do parse did %d iters\n", iters);
}


struct payload
{
	uint8_t type;
	uint8_t counter;
	uint32_t value;
	uint32_t timestamp;
	uint32_t maybe_timestamp_high;
};


// parses the payload. returns -1 on error, or the amount of payloads
// that have been parsed. the last payload is written to pl.
// the value of pl is unchanged when 0 is returned.
int do_alternative_parse(uint8_t* data, int len, struct payload* pl, bool sync)
{
	#define BUFLEN 45
	static uint8_t buffer[BUFLEN];
	static int buf_idx = 0;

	int payload_cnt = 0;

	printf("ALTERNATIVE PARSE:\n");

	while (len > 0)
	{
		int copy_len = len < BUFLEN-buf_idx ? len : BUFLEN-buf_idx;

		memcpy(buffer+buf_idx, data, copy_len);
		buf_idx += copy_len;

		if (buf_idx == BUFLEN)
		{
			for (int i=0; i<BUFLEN; i++)
				printf("%2x ", buffer[i]);
			printf("\n");
			for (int i=0; i<BUFLEN; i++)
				printf("%2d ", i);
			printf("\n");

			if (!(buffer[42] == 0 && buffer[43] == 0 && buffer[44] == 1))
			{
				fprintf(stderr, "ERROR: the payload is not aligned properly, all payloads must have exactly 45 bytes (including the NALU header)\n");
				return -1;
			}

			pl->type = buffer[0];
			pl->counter = buffer[7];
			pl->value = buffer[13] | (buffer[14]<<8) | (buffer[15]<<16) | (buffer[16]<<24);
			pl->timestamp = buffer[29] | (buffer[30]<<8) | (buffer[31]<<16) | (buffer[32]<<24);
			pl->maybe_timestamp_high = buffer[33] | (buffer[34]<<8) | (buffer[35]<<16) | (buffer[36]<<24);

			printf("alternative parse: type = %02x, counter = %02x, value = %9d, timestamp = %9d, timestamp_hi = %9d\n", (int)pl->type, (int)pl->counter, pl->value, pl->timestamp, pl->maybe_timestamp_high);

			payload_cnt++;

			buf_idx = 0;
		}

		data += copy_len;
		len -= copy_len;
	}

	if (sync)
	{
		if (buf_idx != 0)
			fprintf(stderr, "ERROR: got incomplete payload frame, ignoring.\n");
		buffer[BUFLEN-1] = 0xFF; // force the sanity check to fail unless the buffer gets fully overwritten
		buf_idx = 0;
	}

	return payload_cnt;
}

int main(int argc, char **argv)
{
    const char *filename, *outfilename;
    const AVCodec *codec;
    AVCodecParserContext *parser;
    AVCodecContext *c= NULL;
    FILE *f;
    AVFrame *frame, *rgbframe;
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t   data_size;
    int ret;
    AVPacket *pkt;

    if (argc <= 2) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(0);
    }
    filename    = argv[1];
    outfilename = argv[2];

    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);

    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    /* find the MPEG-1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "parser not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }


    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

	rgbframe = av_frame_alloc();
    if (!rgbframe) {
        fprintf(stderr, "Could not allocate video rgbframe\n");
        exit(1);
    }

  av_image_alloc(rgbframe->data, rgbframe->linesize, 1280, 720, AV_PIX_FMT_RGB24, 1);


	uint32_t parse_state = 0xdeadbeef;
	bool parser_suppress = false;

    while (!feof(f)) {
        /* read raw data from the input file */
        data_size = fread(inbuf, 1, INBUF_SIZE, f);
        if (!data_size)
            break;

        /* use the parser to split the data into frames */
        data = inbuf;

		printf("-----------------------------------------\n");
		printf("parsing\n");

		// Split the stream at NALU boundaries. we need to do this on our own,
		// because the stream contains invalid NALUs which would confuse FFMPEG.
		// Note that this code does not split exactly on NALU boundaries (like
		// ca fe | 00 00 01 42 de ad be ef | 00 00 01 a0 ba ad f0 0d | 00 ...)
		// but instead after the 00 00 01, i.e. like this:
		// ca fe 00 00 01 | 42 de ad be ef 00 00 01 | a0 ba ad f0 0d 00 ...
		// This is fine, because av_parser_parse2() implements its own buffering
		// and thus gracefully deals with this. While we actually are supposed to
		// cut out the "00 00 01 a0 ba ad f0 0d" part in the example stream, we
		// are actually cutting out "a0 ba ad f0 0d 00 00 01", which ultimately
		// leads to the same result. We just must watch out that do_alternative_parse()
		// will receive "a0 ba ad f0 0d 00 00 01", so when interpreting these
		// packets, offsets need to be corrected.

		uint8_t* last_data = data;
		struct payload payload;
		for (size_t i=0; i<data_size; i++)
		{
			parse_state = (parse_state << 8) | data[i];

			if ((parse_state & 0xFFFFFF00) == 0x00000100) // we're at the beginning of a new NAL. we have already fed 00 00 01 into the parser, but not yet the nal type which follows
			{
				printf("GOT FRAME %08X\n", parse_state);

				// feed the data so far, up to (including) 00 00 01 into the currently active parser.
				if (!parser_suppress)
					do_parse(parser, c, pkt, last_data, &data[i] - last_data, frame, rgbframe, outfilename); // feed everything up to excluding data[i] into the parser.
				else
					do_alternative_parse(last_data, &data[i] - last_data, &payload, true);

				last_data = &data[i];

				if (parse_state & 0x80) // invalid NAL type, do *not* feed the following data in the video parser
					parser_suppress = true;
				else
					parser_suppress = false; // good NAL type, do feed the following data into the video parser
			}
		}

		// feed the remaining data from the buffer into the active parser
		if (!parser_suppress)
			do_parse(parser, c, pkt, last_data, &data[data_size] - last_data, frame, rgbframe, outfilename); // feed everything up to excluding data[i] into the parser.
		else
			do_alternative_parse(last_data, &data[data_size] - last_data, &payload, false);


    }

    /* flush the decoder */
    decode(c, frame, NULL, rgbframe, outfilename);

    fclose(f);

    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);

    return 0;
}
