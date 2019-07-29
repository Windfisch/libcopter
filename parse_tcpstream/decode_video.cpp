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
#include <string.h>

#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "decode_video.hpp"


// #define DEBUG_PRINT // enable this for lots of debugging output

#ifdef DEBUG_PRINT
#define debugprintf(...) printf (__VA_ARGS__)
#else
#define debugprintf(...) do {} while(false);
#endif

VideoTelemetryParser::VideoTelemetryParser()
{
	pkt = av_packet_alloc();
	if (!pkt) throw std::runtime_error("Could not allocate an av packet");

	codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec) throw std::runtime_error("Could not find the H264 codec");

	parser = av_parser_init(codec->id);
	if (!parser) throw std::runtime_error("Could not initialize the H264 parser");

	c = avcodec_alloc_context3(codec);
	if (!c) throw std::runtime_error("Could not allocate the H264 codec context");

	if (avcodec_open2(c, codec, NULL) < 0) throw std::runtime_error("Could not open H264 codec");


	frame = av_frame_alloc();
	if (!frame) throw std::runtime_error("Could not allocate video frame");

	rgbframe = av_frame_alloc();
	if (!rgbframe) throw std::runtime_error("Could not allocate video rgbframe");

}

VideoTelemetryParser::~VideoTelemetryParser()
{
	av_parser_close(parser);
	avcodec_free_context(&c);
	av_frame_free(&frame);
	av_packet_free(&pkt);
}

void VideoTelemetryParser::consume_data(const uint8_t* data, size_t data_size, DroneDataInterface* drone_data)
{
	if (!data_size)
		return;

	debugprintf("-----------------------------------------\n");
	debugprintf("parsing\n");

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

	const uint8_t* last_data = data;
	for (size_t i=0; i<data_size; i++)
	{
		parse_state = (parse_state << 8) | data[i];

		if ((parse_state & 0xFFFFFF00) == 0x00000100) // we're at the beginning of a new NAL. we have already fed 00 00 01 into the parser, but not yet the nal type which follows
		{
			debugprintf("GOT FRAME %08X\n", parse_state);

			// feed the data so far, up to (including) 00 00 01 into the currently active parser.
			if (!parser_suppress)
				parse_video(last_data, &data[i] - last_data, drone_data); // feed everything up to excluding data[i] into the parser.
			else
				parse_telemetry(last_data, &data[i] - last_data, drone_data, true);

			last_data = &data[i];

			if (parse_state & 0x80) // invalid NAL type, do *not* feed the following data in the video parser
				parser_suppress = true;
			else
				parser_suppress = false; // good NAL type, do feed the following data into the video parser
		}
	}

	// feed the remaining data from the buffer into the active parser
	if (!parser_suppress)
		parse_video(last_data, &data[data_size] - last_data, drone_data); // feed everything up to excluding data[i] into the parser.
	else
		parse_telemetry(last_data, &data[data_size] - last_data, drone_data, false);
}


int VideoTelemetryParser::parse_video(const uint8_t* data, size_t data_size, DroneDataInterface* drone_data)
{
	int n_frames = 0;
	while (data_size > 0)
	{
		int ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
				data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
		debugprintf("parsed\n");
		if (ret < 0) {
			fprintf(stderr, "Error while parsing\n");
			exit(1);
		}
		data      += ret;
		data_size -= ret;

		if (pkt->size)
		{
			int ret;

			debugprintf("sending pkt\n");
			ret = avcodec_send_packet(c, pkt);
			debugprintf("sent pkt\n");
			if (ret < 0) // oops. this should not happen but sometimes does. just ignore it and live with some video corruption
				break;

			while (ret >= 0)
			{
				ret = avcodec_receive_frame(c, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					break;
				else if (ret < 0)
					throw std::runtime_error("Error at avcodec_receive_frame");

				// FIXME if first time, initialize swsparams and rgbframe
				if (sws_ctx == NULL)
				{
					sws_ctx = sws_getContext(
						frame->width, frame->height, (enum AVPixelFormat) frame->format,
						frame->width, frame->height, AV_PIX_FMT_BGR24,
						 SWS_BILINEAR, NULL, NULL, NULL
					);
					if (!sws_ctx) throw std::runtime_error("Could not initialize software scaler");
					debugprintf("inited sws ctx\n");
					
					// FIXME: error checking?
					av_image_alloc(rgbframe->data, rgbframe->linesize, frame->width, frame->height, AV_PIX_FMT_BGR24, 1);
				}

				debugprintf("w/h = %d / %d\n", frame->width, frame->height);

				// convert the frame to rgb
				sws_scale(
					sws_ctx,
					(uint8_t const * const *)frame->data, frame->linesize, 0, frame->height,
					rgbframe->data, rgbframe->linesize
				);


				drone_data->add_video_frame(rgbframe->data[0], rgbframe->linesize[0], frame->width, frame->height);
				n_frames++;
			}
		}
	}
	
	return n_frames;
}


// parses the payload. returns -1 on error, or the amount of payloads
// that have been parsed. the last payload is written to pl.
// the value of pl is unchanged when 0 is returned.
int VideoTelemetryParser::parse_telemetry(const uint8_t* data, size_t len, DroneDataInterface* drone_data, bool sync)
{
	int payload_cnt = 0;

	debugprintf("ALTERNATIVE PARSE:\n");

	payload_t pl;

	while (len > 0)
	{
		size_t copy_len = len < TELEMETRY_BUFFER_LENGTH-telemetry_buf_idx ? len : TELEMETRY_BUFFER_LENGTH-telemetry_buf_idx;

		memcpy(telemetry_buffer+telemetry_buf_idx, data, copy_len);
		telemetry_buf_idx += copy_len;

		if (telemetry_buf_idx == TELEMETRY_BUFFER_LENGTH)
		{
			for (size_t i=0; i<TELEMETRY_BUFFER_LENGTH; i++)
				debugprintf("%2x ", telemetry_buffer[i]);
			debugprintf("\n");
			for (size_t i=0; i<TELEMETRY_BUFFER_LENGTH; i++)
				debugprintf("%2zu ", i);
			debugprintf("\n");

			if (!(telemetry_buffer[42] == 0 && telemetry_buffer[43] == 0 && telemetry_buffer[44] == 1))
			{
				fprintf(stderr, "ERROR: the payload is not aligned properly, all payloads must have exactly 45 bytes (including the NALU header)\n");
				return -1;
			}

			pl.type = telemetry_buffer[0];
			pl.counter = telemetry_buffer[7];
			pl.value = telemetry_buffer[13] | (telemetry_buffer[14]<<8) | (telemetry_buffer[15]<<16) | (telemetry_buffer[16]<<24);
			pl.timestamp = telemetry_buffer[29] | (telemetry_buffer[30]<<8) | (telemetry_buffer[31]<<16) | (telemetry_buffer[32]<<24);
			pl.maybe_timestamp_high = telemetry_buffer[33] | (telemetry_buffer[34]<<8) | (telemetry_buffer[35]<<16) | (telemetry_buffer[36]<<24);

			debugprintf("alternative parse: type = %02x, counter = %02x, value = %9d, timestamp = %9d, timestamp_hi = %9d\n", (int)pl.type, (int)pl.counter, pl.value, pl.timestamp, pl.maybe_timestamp_high);

			drone_data->add_telemetry_data(pl);
			payload_cnt++;

			telemetry_buf_idx = 0;
		}

		data += copy_len;
		len -= copy_len;
	}

	if (sync)
	{
		if (telemetry_buf_idx != 0)
			fprintf(stderr, "ERROR: got incomplete payload frame, ignoring.\n");
		telemetry_buffer[TELEMETRY_BUFFER_LENGTH-1] = 0xFF; // force the sanity check to fail unless the telemetry_buffer gets fully overwritten
		telemetry_buf_idx = 0;
	}

	return payload_cnt;
}

