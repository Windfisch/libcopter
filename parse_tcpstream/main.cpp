#include <stdio.h>
#include <string.h>
#include <fstream>

#include "decode_video.hpp"

struct DummyDroneData : public DroneDataBase
{
	virtual void add_video_frame(uint8_t* data, int y_stride, int width, int height)
	{
		FILE *pFile;
		char szFilename[32];
		int y;

		// Open file
		sprintf(szFilename, "frame%d.ppm", frame_count);
		pFile=fopen(szFilename, "wb");
		if(pFile==NULL)
			return;

		// Write header
		fprintf(pFile, "P6\n%d %d\n255\n", width, height);

		// Write pixel data
		for(y=0; y<height; y++)
			fwrite(data+y*y_stride, 1, width*3, pFile);

		// Close file
		fclose(pFile);
		
		frame_count++;
	}

	int frame_count = 0;
};

int main(int argc, const char** argv)
{
	using std::ifstream;

	constexpr int BUFSIZE = 1024;
	uint8_t buf[BUFSIZE];

	if (argc != 2)
	{
		printf("Usage: %s infile\n", argv[0]);
		exit(1);
	}

	VideoTelemetryParser parser;
	DummyDroneData dd;

	ifstream f(argv[1], std::ios::binary);

	while (f.good())
	{
		f.read(reinterpret_cast<char*>(buf), BUFSIZE);
		size_t n_read = f.gcount();

		parser.consume_data(buf, n_read, &dd);
	}
}
