#include "sg500.hpp"

#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/python/numpy.hpp>

#include <opencv2/core.hpp>

#include <opencv2/

#include <iostream>
using std::cout, std::endl;

namespace np = boost::python::numpy;
namespace p = boost::python;

// calls c->poll_data, and converts it as follows:
// std::pair becomes a python tuple
// std::vector becomes a python list
// structs become python classes
// cv::Mat becomes numpy::array
boost::python::tuple py_poll_data(SG500& c)
{
	cout << "py_poll_data" << endl;
	//auto [video_frames, telemetry_frames] = c.poll_data();
	std::vector<Copter::VideoFrame> video_frames;
	std::vector<Copter::TelemetryFrame> telemetry_frames;

	video_frames.push_back( {42.0, cv::Mat::zeros(100,100, CV_8UC3)} );
	telemetry_frames.push_back( {13.37, Copter::TelemetryFrame::Type::UNKNOWN2, 42} );

	cout << "polled" << endl;
	boost::python::list py_video_frames, py_telemetry_frames;
	
	cout << "converting" << endl;

	for (const Copter::TelemetryFrame& t : telemetry_frames)
		py_telemetry_frames.append(t);

	cout << "returning" << endl;
	return boost::python::make_tuple(py_video_frames, py_telemetry_frames);
}

BOOST_PYTHON_MODULE(libsg500)
{
	using namespace boost::python;

	np::initialize();

	bool (SG500::*init1)() = &SG500::initialize;
	bool (SG500::*init2)(int) = &SG500::initialize;

	class_<Copter::TelemetryFrame>("TelemetryFrame")
		.def_readwrite("timestamp", &Copter::TelemetryFrame::timestamp)
		.def_readwrite("type", &Copter::TelemetryFrame::type)
		.def_readwrite("value", &Copter::TelemetryFrame::value)
	;

	enum_<Copter::TelemetryFrame::Type>("TelemetryFrameType")
		.value("UNKNOWN1", Copter::TelemetryFrame::Type::UNKNOWN1)
		.value("UNKNOWN2", Copter::TelemetryFrame::Type::UNKNOWN2)
		.value("OTHER",     Copter::TelemetryFrame::Type::OTHER)
	;

	class_<SG500, boost::noncopyable>("SG500")
		.def(init< optional<std::string, int, int> >())
		.def("command", &SG500::command)
		.def("takeoff", &SG500::takeoff)
		.def("land", &SG500::land)
		.def("panic", &SG500::panic)
		.def("initialize", init1)
		.def("initialize", init2)
		.def("poll_data", py_poll_data)
	;
}
