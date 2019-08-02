#include "sg500.hpp"

#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/numpy.hpp>

#include <opencv2/core.hpp>

namespace np = boost::python::numpy;
namespace p = boost::python;

struct PyVideoFrame
{
	double timestamp;
	np::ndarray frame;
};

struct opencv_owner
{
	cv::Mat mat;
};

p::object opencv_owner_py;

/** Converts a cv::Mat to numpy array without performing a full copy */
static np::ndarray mat2numpy(cv::Mat mat)
{
	if (mat.dims != 2)
		throw std::runtime_error("Error: Can only convert two-dimensional matrices from OpenCV to numpy, but dimensions are "+std::to_string(mat.dims));

	// create a python object that holds the cv::Mat.
	p::object own_py = opencv_owner_py();
	opencv_owner& own = p::extract<opencv_owner&>(own_py);
	own.mat = mat; // this is cheap, no deep copy of the array data involved
	
	cv::Mat& m = own.mat;
	
	boost::python::numpy::dtype dtype = np::dtype::get_builtin<uint8_t>(); // we must choose a default value. this is overridden anyway.
	switch (m.depth())
	{
		case CV_8U:  dtype = np::dtype::get_builtin< uint8_t>(); break;
		case CV_8S:  dtype = np::dtype::get_builtin<  int8_t>(); break;
		case CV_16U: dtype = np::dtype::get_builtin<uint16_t>(); break;
		case CV_16S: dtype = np::dtype::get_builtin< int16_t>(); break;
		case CV_32S: dtype = np::dtype::get_builtin< int32_t>(); break;
		case CV_32F: dtype = np::dtype::get_builtin<   float>(); break;
		case CV_64F: dtype = np::dtype::get_builtin<  double>(); break;
		default: throw std::runtime_error("unknown depth " + std::to_string(m.depth()));
	}

	p::tuple shape;
	p::tuple stride;
	
	if (m.channels() > 1)
	{
		shape = p::make_tuple(m.rows, m.cols, m.channels());
		stride = p::make_tuple(size_t(m.step), m.elemSize(), m.elemSize1());
	}
	else
	{
		shape = p::make_tuple(m.rows, m.cols);
		stride = p::make_tuple(size_t(m.step), m.elemSize());
	}

	// this np::array does not own its data, but own_py owns it. If the np::array is freed,
	// then own_py's refcount is decremented to zero, and own_py will be destructed. This causes
	// the cv::Mat inside own to be freed, causing no memory leaks.
	return np::from_data(m.data, dtype, shape, stride, own_py);
}


// calls c->poll_data, and converts it as follows:
// std::pair becomes a python tuple
// std::vector becomes a python list
// structs become python classes
// cv::Mat becomes numpy::array
boost::python::tuple py_poll_data(SG500& c)
{
	auto [video_frames, telemetry_frames] = c.poll_data();

	boost::python::list py_video_frames, py_telemetry_frames;
	
	for (const Copter::TelemetryFrame& t : telemetry_frames)
		py_telemetry_frames.append(t);

	for (const Copter::VideoFrame& v : video_frames)
		py_video_frames.append(PyVideoFrame{v.timestamp, mat2numpy(v.frame)});

	return boost::python::make_tuple(py_video_frames, py_telemetry_frames);
}

BOOST_PYTHON_MODULE(libsg500)
{
	using namespace boost::python;

	np::initialize();

	bool (SG500::*init1)() = &SG500::initialize;
	bool (SG500::*init2)(int) = &SG500::initialize;

	opencv_owner_py = p::class_<opencv_owner>("opencv_owner");

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

	class_<PyVideoFrame>("VideoFrame", p::no_init)
		.def_readwrite("timestamp", &PyVideoFrame::timestamp)
		.def_readwrite("frame", &PyVideoFrame::frame)
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
