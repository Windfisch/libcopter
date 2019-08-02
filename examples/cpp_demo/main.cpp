#include "sg500.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>

using namespace std;

int main()
{
	SG500 drone;
	drone.initialize();

	while (true)
	{
		auto [vid, tel] = drone.poll_data();

		cout << vid.size() << " / " << tel.size() << endl;
		for (const auto& frame : vid)
		{
			cv::imshow("frame", frame.frame);
		}
		cv::waitKey(40);
	}
	return 0;
}
