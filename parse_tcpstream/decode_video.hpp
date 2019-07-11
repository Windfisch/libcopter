#pragma once

#include <stdint.h>
#include <vector>

extern "C" // forward declare the ffmpeg pointer types
{
	struct AVCodec;
	struct AVCodecContext;
	struct AVCodecParserContext;
	struct AVFrame;
	struct AVPacket;
	struct SwsContext;
};

struct payload_t
{
	uint8_t type;
	uint8_t counter;
	uint32_t value;
	uint32_t timestamp;
	uint32_t maybe_timestamp_high;
};

struct DroneDataBase
{
	virtual void add_video_frame(uint8_t* data, int y_stride, int width, int height) = 0;
	virtual void add_telemetry_data(const payload_t& payload)
	{
		switch (payload.type)
		{
			case 0xA1: telemetry_alti.emplace_back(payload); break;
			case 0xA0: telemetry_batt.emplace_back(payload); break;
			default: telemetry_other.emplace_back(payload); break;
		}
	}

	std::vector<payload_t> telemetry_batt;
	std::vector<payload_t> telemetry_alti;
	std::vector<payload_t> telemetry_other;
};

struct VideoTelemetryParser
{
	VideoTelemetryParser();
	~VideoTelemetryParser();

	void consume_data(const uint8_t* data, size_t data_size, DroneDataBase* drone_data);
	
private:
	int parse_telemetry(const uint8_t* data, size_t data_size, DroneDataBase* drone_data, bool sync);
	int parse_video(const uint8_t* data, size_t data_size, DroneDataBase* drone_data);

	// these are initialized in the constructor
	const AVCodec* codec;
	AVCodecParserContext* parser;
	AVCodecContext* c;
	AVFrame* frame;
	AVFrame* rgbframe;
	AVPacket *pkt;

	// this can only be initialized when the frame size is known.
	// initialization is deferred to the decode step.
	SwsContext* sws_ctx = NULL;

	uint32_t parse_state = 0xdeadbeef;
	bool parser_suppress = false;

	static constexpr size_t TELEMETRY_BUFFER_LENGTH = 45;
	uint8_t telemetry_buffer[TELEMETRY_BUFFER_LENGTH];
	size_t telemetry_buf_idx = 0;
};

