#pragma once

#include <string>
#include <boost/asio.hpp>
#include <opencv2/core.hpp>
#include <atomic>
#include <condition_variable>

#include "decode_video.hpp"

class Copter
{
	public:
		struct TelemetryFrame
		{
			enum class Type
			{
				UNKNOWN1,
				UNKNOWN2,
				OTHER
			};
			
			double timestamp;
			Type type;
			uint32_t value;
		};
		struct VideoFrame
		{
			double timestamp;
			cv::Mat frame;
		};

		[[deprecated]] virtual bool initialize() = 0;

		/** Sends a steering command to the UAV. All values are in the range [-1; 1] */
		virtual void command(float roll, float pitch, float yaw, float height) = 0;
		virtual void takeoff() = 0;
		virtual void land() = 0;
		virtual void panic() = 0;

		/** Checks if new data is available, and if so, returns it.

		    Returns a list of VideoFrame and a list of TelemetryFrames.
		    If no data was available, both are empty.
		*/
		virtual std::pair< std::vector<VideoFrame>, std::vector<TelemetryFrame> > poll_data() = 0;
};

class SG500 : public Copter
{
    boost::asio::io_context io_context;
	boost::asio::ip::tcp::socket tcp_socket;
	boost::asio::ip::udp::socket udp_socket;

	std::string host;
	int udp_port, tcp_port;

	VideoTelemetryParser parser;

	std::chrono::steady_clock::time_point next_video_heartbeat;
	//DroneData data;

	public:
		SG500(std::string host = "172.16.10.1", int udp_port = 8080, int tcp_port = 8888);
		
		bool initialize();
		bool initialize(int n);

		/** Sends a steering command to the UAV. All values are in the range [-1; 1] */
		void command(float roll, float pitch, float yaw, float height);
		void takeoff();
		void land();
		void panic();

		/** Checks if new data is available, and if so, returns it.

		    Returns a list of VideoFrame and a list of TelemetryFrames.
		    If no data was available, both are empty.
		*/
		std::pair< std::vector<VideoFrame>, std::vector<TelemetryFrame> > poll_data();

	private:
		void command_thread_func();
		std::condition_variable condition_variable;
		std::mutex mutex;

		struct {float roll, pitch, yaw, height;} command_data;
		std::chrono::steady_clock::time_point takeoff_until;
		std::chrono::steady_clock::time_point land_until;
		std::chrono::steady_clock::time_point panic_until;
		bool update = false;

		std::thread command_thread; // must be the last member. (So it will be the first one to be destructed)
};
