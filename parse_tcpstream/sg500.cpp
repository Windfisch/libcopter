#include "sg500.hpp"

#include "decode_video.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <string>

using std::string_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""s;
using namespace std;

namespace ba = boost::asio;
using batcp = boost::asio::ip::tcp;
using baudp = boost::asio::ip::udp;

SG500::SG500(std::string host, int udp_port, int tcp_port) : 
	io_context(),
	tcp_socket(io_context),
	udp_socket(io_context),

	takeoff_until( std::chrono::steady_clock::now() ),
	land_until( std::chrono::steady_clock::now() ),
	panic_until( std::chrono::steady_clock::now() ),

	command_thread(&SG500::command_thread, this)
{
	batcp::resolver tcp_resolver(io_context);
	ba::connect(tcp_socket, tcp_resolver.resolve(host, to_string(tcp_port)));

	baudp::resolver udp_resolver(io_context);
	ba::connect(udp_socket, udp_resolver.resolve(host, to_string(udp_port)));
}


void SG500::command(float roll, float pitch, float yaw, float height)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		command_data = {roll, pitch, yaw, height};
		update = true;
	}
	condition_variable.notify_one();
}

void SG500::takeoff()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		takeoff_until = std::chrono::steady_clock::now() + 1000ms;
		update = true;
	}
	condition_variable.notify_one();
}

void SG500::land()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		land_until = std::chrono::steady_clock::now() + 1000ms;
		update = true;
	}
	condition_variable.notify_one();
}

void SG500::panic()
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		panic_until = std::chrono::steady_clock::now() + 1000ms;
		update = true;
	}
	condition_variable.notify_one();
}

void SG500::command_thread_func()
{
	while(true)
	{
		std::unique_lock<std::mutex> lock(mutex);
		condition_variable.wait_for(lock, 20ms, [this]{return update;});

		auto now = std::chrono::steady_clock::now();
		bool takeoff_flag = (now < takeoff_until);
		bool land_flag = (now < land_until);
		bool panic_flag = (now < panic_until);

		printf("%+2.4f %+2.4f %+2.4f %+2.4f %1d %1d %1d\n", command_data.roll, command_data.pitch, command_data.yaw, command_data.height, takeoff_flag, land_flag, panic_flag);

		update = false;
		lock.unlock();
	}
}


struct DroneDataHandler : public DroneDataInterface
{
	virtual void add_video_frame(uint8_t* data, int y_stride, int width, int height)
	{
		cv::Mat tmp(height, width, CV_8UC3, data, y_stride);
		video.push_back({-1, tmp.clone()}); // FIXME: proper timestamp

		if (telemetry.size() == video.size())
			video.back().timestamp = telemetry.back().timestamp;
		else
			std::cout << "ERROR: no telemetry frame preceding the video frame" << endl;
	}

	virtual void add_telemetry_data(const payload_t& payload)
	{
		SG500::TelemetryFrame::Type type;
		switch (payload.type)
		{
			case 0xA1: type = SG500::TelemetryFrame::Type::UNKNOWN1;
			case 0xA0: type = SG500::TelemetryFrame::Type::UNKNOWN2;
			default: type = SG500::TelemetryFrame::Type::OTHER;
		}
		telemetry.push_back({payload.timestamp/1000000., type, payload.value});
	}
	

	std::vector<SG500::VideoFrame> video;
	std::vector<SG500::TelemetryFrame> telemetry;
};

std::pair< std::vector<SG500::VideoFrame>, std::vector<SG500::TelemetryFrame> > SG500::poll_data()
{
	std::array<uint8_t, 1024> buf;
	uint8_t keepalive[] = {0,1,2,3,4,5,6,7,8,9,0x28,0x28};
	
	DroneDataHandler data_handler;

	while (tcp_socket.available())
	{
		// send the whole buffer, allow no short writes
		ba::write(tcp_socket, ba::buffer(keepalive));

		boost::system::error_code error;
		size_t len = tcp_socket.read_some(boost::asio::buffer(buf), error);

		if (error)
			throw boost::system::system_error(error);
		
		// TODO: parse buf
		parser.consume_data(buf.data(), len, &data_handler);
	}
	
	return std::make_pair( std::move(data_handler.video), std::move(data_handler.telemetry) );
}

static std::vector<uint8_t> make_raw_command(uint8_t height, uint8_t yaw, uint8_t pitch, uint8_t roll, bool launch, bool panic, bool land, bool recalibrate, bool auto_altitude=true, uint8_t yaw_trim=0x10, uint8_t pitch_trim=0x10, uint8_t roll_trim=0x10, bool compass=false, uint8_t percent_raw=0)
{
	std::vector<uint8_t> command(11, 0);
	command[0] = 0xFF;
	command[1] = 0x08;
	command[2] = height & 0xFF;
	command[3] = yaw & 0x7F;
	command[4] = pitch & 0x7F;
	command[5] = roll & 0x7F;
	
	if (auto_altitude) command[6] |= 0x80;
	if (recalibrate)   command[6] |= 0x40;

	command[6] |= yaw_trim & 0x3F;
	command[7] = pitch_trim & 0x3F;
	command[8] = roll_trim & 0x3F;

	if (launch)  command[9] |= 0x40;
	if (panic)   command[9] |= 0x20;
	if (land)    command[9] |= 0x80;
	if (compass) command[9] |= 0x10;

	command[9] |= percent_raw & 0x03;

	// checksum
	uint8_t sum = 0;
	for (int i=1; i<10; i++) sum += uint8_t(command[i]);
	command[10] = 0xFF - sum;

	return command;
}

static std::vector<uint8_t> make_command(float height, float yaw, float pitch, float roll, bool launch=false, bool panic=false, bool land=false, bool recalibrate=false)
{
	height = max(min(height, 1.f), -1.f);
	yaw =    max(min(yaw,    1.f), -1.f);
	pitch =  max(min(pitch,  1.f), -1.f);
	roll =   max(min(roll,   1.f), -1.f);

	return make_raw_command(
		uint8_t(height*0x7E + 0x7E),
		uint8_t(yaw   *0x3F + 0x3F),
		uint8_t(pitch *0x3F + 0x40),
		uint8_t(roll  *0x3F + 0x3F),
		launch, panic, land, recalibrate
	);
}

static std::string udp_recv(ba::ip::udp::socket& udp_socket)
{
	std::array<char,128> recv_buf;

	if (udp_socket.available())
	{
		size_t len = udp_socket.receive(boost::asio::buffer(recv_buf));
		return std::string(recv_buf.data(), len);
	}
	else
	{
		return ""s;
	}
}

static std::string udp_recv(ba::ip::udp::socket& udp_socket, std::chrono::milliseconds timeout)
{
	while(1)
	{
		string reply = udp_recv(udp_socket);

		if (reply.empty())
			std::this_thread::sleep_for(10ms);
		else
			return reply;
	}
}

bool SG500::initialize() { return initialize(10); }

bool SG500::initialize(int n)
{
	if (n==0)
		return false;

	cerr << "resetting the drone" << endl;
	ba::write(udp_socket, ba::buffer({0x0F})); // reset

	string drone_type, drone_version;

	while(1)
	{
		ba::write(udp_socket, ba::buffer({0x28})); // request UDP720P message
		string reply = udp_recv(udp_socket, 500ms);
		if (reply.substr(0,3) == "UDP")
		{
			drone_type = reply;
			break;
		}
		else if (!reply.empty())
		{
			cerr << "got unexpected reply to 0x28 initialization command: '" << reply << "'" << endl;
			return initialize(n-1);
		}
	}

	cout << "Drone type is '" << drone_type << "'" << endl;


	while(1)
	{
		ba::write(udp_socket, ba::buffer({0x28})); // request version message
		string reply = udp_recv(udp_socket, 500ms);
		if (reply == drone_type)
			cerr << "ignoring duplicate '" << drone_type << "' message when waiting for version" << endl;
		else if (reply.substr(0,1) == "V")
		{
			drone_version = reply;
			break;
		}
		else if (!reply.empty())
		{
			cerr << "got unexpected reply to 0x28 initialization command: '" << reply << "'" << endl;
			return initialize(n-1);
		}
	}

	cout << "Drone version is '" << drone_version << "'" << endl;

sock.sendto(bytes([0x0f]), (IP, PORT))
sock.sendto(bytes([0x28]), (IP, PORT)) # request UDP720P message
print(sock.recvfrom(100))
sock.sendto(bytes([0x28]), (IP, PORT)) # request version message
print(sock.recvfrom(100))
sock.sendto(bytes([0x42]), (IP, PORT))

for i in range(100):
	msg1, msg2 = command.make_date_messages() # todo: check for 'timeok'
	sock.sendto(msg1, (IP, PORT))
	sock.sendto(msg2, (IP, PORT))
	time.sleep(0.01)

sock.sendto(bytes([0x2c]), (IP, PORT)) # causes "ok\x02"
#sock.sendto(bytes([0x27]), (IP, PORT)) # the app does this, but it seems unneeded
#sock.sendto(bytes([0x27]), (IP, PORT))
print(sock.recvfrom(100))

print("initialization complete, now opening the video+telemetry TCP socket")

}
