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

using bytes = std::vector<uint8_t>;

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

static void put_int(uint8_t buf[4], uint32_t val)
{
	buf[0] = (val      ) & 0xFF;
	buf[1] = (val >>  8) & 0xFF;
	buf[2] = (val >> 16) & 0xFF;
	buf[3] = (val >> 24) & 0xFF;
}

static std::pair< std::vector<uint8_t>, std::string > make_date_messages(std::chrono::system_clock::time_point now = std::chrono::system_clock::now())
{
	time_t tt = std::chrono::system_clock::to_time_t(now);
	tm t = *localtime(&tt); 

	vector<uint8_t> msg1(29,0);
	msg1[0] = 0x26;
	put_int(&msg1[ 1], t.tm_year + 1900);
	put_int(&msg1[ 5], t.tm_mon + 1);
	put_int(&msg1[ 9], t.tm_mday);
	put_int(&msg1[13], t.tm_wday);
	put_int(&msg1[17], t.tm_hour);
	put_int(&msg1[21], t.tm_min);
	put_int(&msg1[25], t.tm_sec);


	char msg2_buf[32];
	std::snprintf(msg2_buf, sizeof(msg2_buf), "date -s \"%4d-%02d-%02d %02d:%02d:%02d\"", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

	return make_pair(msg1, string(msg2_buf));
}


SG500::SG500(std::string host_, int udp_port_, int tcp_port_) : 
	io_context(),
	tcp_socket(io_context),
	udp_socket(io_context),

	host(host_), udp_port(udp_port_), tcp_port(tcp_port_),

	takeoff_until( std::chrono::steady_clock::now() ),
	land_until( std::chrono::steady_clock::now() ),
	panic_until( std::chrono::steady_clock::now() ),
	next_video_heartbeat( std::chrono::steady_clock::now() )
{
	cout << "SG500 ctor" << endl;

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
		condition_variable.wait_for(lock, 50ms, [this]{return update;});

		auto now = std::chrono::steady_clock::now();
		bool takeoff_flag = (now < takeoff_until);
		bool land_flag = (now < land_until);
		bool panic_flag = (now < panic_until);

		printf("%+2.4f %+2.4f %+2.4f %+2.4f %1d %1d %1d\n", command_data.roll, command_data.pitch, command_data.yaw, command_data.height, takeoff_flag, land_flag, panic_flag);
		std::vector<uint8_t> buf = make_command(command_data.height, command_data.yaw, command_data.pitch, command_data.roll, takeoff_flag, panic_flag, land_flag);
		udp_socket.send(ba::buffer(buf));

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

	auto now = chrono::steady_clock::now();
	if (now > next_video_heartbeat)
	{
		ba::write(tcp_socket, ba::buffer(bytes{0,1,2,3,4,5,6,7,8,9,0x28,0x28})); // send the whole buffer, allow no short writes
		next_video_heartbeat = now + 800ms; // the app waits one second. better be safe than sorry.
	}
	
	DroneDataHandler data_handler;

	while (tcp_socket.available())
	{
		boost::system::error_code error;
		size_t len = tcp_socket.read_some(boost::asio::buffer(buf), error);

		if (error)
			throw boost::system::system_error(error);
		
		parser.consume_data(buf.data(), len, &data_handler);
	}
	
	return std::make_pair( std::move(data_handler.video), std::move(data_handler.telemetry) );
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
	auto start = std::chrono::steady_clock::now();
	while(1)
	{
		string reply = udp_recv(udp_socket);

		if (reply.empty())
			std::this_thread::sleep_for(10ms);
		else
			return reply;

		if (std::chrono::steady_clock::now() > start + timeout)
			return "";
	}
}

bool SG500::initialize() { return initialize(10); }


/** Repeatedly send `commands`, and wait for a reply for which `predicate` returns true.
    At most `tries` attempts are made, and each recv waits for at least `recv_timeout`. */
optional<string> expect(baudp::socket& udp_socket, vector<ba::const_buffer> commands, function<bool(string)> predicate, int tries = 5, std::chrono::milliseconds recv_timeout = 30ms)
{
	for (int i=0; i<tries; i++)
	{
		cout << "." << flush;
		for (const auto& command : commands)
			udp_socket.send(ba::buffer(command));

		string reply = udp_recv(udp_socket, recv_timeout);

		if (predicate(reply))
			return reply;
		else if (!reply.empty())
			cout << "?('" << reply << "') " << flush;
	}
	return nullopt;
}

/** Repeatedly send `command`, and wait for a reply for which `predicate` returns true.
    At most `tries` attempts are made, and each recv waits for at least `recv_timeout`. */
optional<string> expect(baudp::socket& udp_socket, ba::const_buffer command, function<bool(string)> predicate, int tries = 5, std::chrono::milliseconds recv_timeout = 30ms)
{
	return expect(udp_socket, vector<ba::const_buffer>{command}, predicate, tries, recv_timeout);
}


bool SG500::initialize(int n)
{
	cout << "connecting udp" << endl;
	baudp::resolver udp_resolver(io_context);
	ba::connect(udp_socket, udp_resolver.resolve( baudp::resolver::query(host, to_string(udp_port))));
	for (int i=0; i<n; i++)
	{
		if (i!=0) cout << " :(" << endl;
		cout << "resetting the drone" << endl;
		udp_socket.send(ba::buffer(bytes{0x0F}));

		string drone_type, drone_version;

		cout << "requesting UDP720P message (via command 0x28)" << flush;
		if (optional<string> result = expect(udp_socket, ba::buffer(bytes{0x28}), [](string s) { return s.substr(0,3)=="UDP"; }))
			drone_type = *result;
		else
			continue;
		cout << " -> '" << drone_type << "'" << endl;

		cout << "requesting version message" << flush;
		if (optional<string> result = expect(udp_socket, ba::buffer(bytes{0x28}), [](string s) { return s.substr(0,1)=="V"; }))
			drone_version = *result;
		else
			continue;
		cout << " -> '" << drone_version << "'" << endl;

		cout << "setting the time" << flush;
		auto [msg1, msg2] = make_date_messages();
		if (optional<string> result = expect(udp_socket, {ba::buffer(bytes{0x42}), ba::buffer(msg1), ba::buffer(msg2)}, [](string s) { return s.substr(0,6)=="timeok"; }))
			cout << " -> timeok" << endl;
		else
			cout << " -> no timeok :(" << endl;

		cout << "finalizing" << flush;
		if (optional<string> result = expect(udp_socket, ba::buffer(bytes{0x2c}), [](string s) { return s.substr(0,2)=="ok"; }))
		{
			cout << " -> initialization complete :)" << endl;
			command_thread = std::thread(&SG500::command_thread_func, this); // launch the command thread
			cout << "connecting tcp to port " << tcp_port << endl;
			batcp::resolver tcp_resolver(io_context);
			ba::connect(tcp_socket, tcp_resolver.resolve(batcp::resolver::query(host, to_string(tcp_port))));
			return true;
		}
		else
			continue;
	}

	return false;
}
