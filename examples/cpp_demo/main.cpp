#include "sg500.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>

using namespace std;

static float clamp(float val, float lo=-1.f, float hi=1.f)
{
	if (val < lo) return lo;
	if (val > hi) return hi;
	return val;
}

int main()
{
	SG500 drone;
	drone.initialize();

	float height=0, yaw=0, roll=0, pitch=0;

	while (true)
	{
		auto [vid, tel] = drone.poll_data();

		for (const auto& frame : vid)
			cv::imshow("frame", frame.frame);

		int key = cv::waitKey(30);

		switch(key)
		{
			case 's': pitch += 0.25; break;
			case 'w': pitch -= 0.25; break;
			case 'd': roll  += 0.25; break;
			case 'a': roll  -= 0.25; break;
			case 'e': yaw   += 0.25; break;
			case 'q': yaw   -= 0.25; break;
			case 'r': height+= 0.25; break;
			case 'f': height-= 0.25; break;
			case 'x': drone.takeoff(); break;
			case 'c': drone.panic(); break;
			case 'z': drone.land(); break;
			case '1':
			case '2':
			case '3': height=0; yaw=0; pitch=0; roll=0; break;
		}

		roll = clamp(roll);
		pitch = clamp(pitch);
		yaw = clamp(yaw);
		height = clamp(height);

		drone.command(roll, pitch, yaw, height);
	}
	return 0;
}
