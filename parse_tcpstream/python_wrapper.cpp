#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/python/numpy.hpp>

#include "decode_video.hpp"

namespace np = boost::python::numpy;
namespace p = boost::python;

struct PyDroneData : public DroneDataBase
{
	virtual void add_video_frame(uint8_t* data, int y_stride, int width, int height)
	{
		p::tuple shape = p::make_tuple(height, width, 3);
		p::tuple stride = p::make_tuple(y_stride,3,1);

		p::object own;
		np::ndarray temp = np::from_data(data, np::dtype::get_builtin<uint8_t>(), shape, stride, own);
		np::ndarray result = temp.copy();
		video_frames.append(result);
	}

	boost::python::list video_frames;
};

struct PyVideoTelemetryParser
{
	PyDroneData consume_data(PyObject* bytes_obj)
	{
		char* buf;
		Py_ssize_t len;
		PyBytes_AsStringAndSize(bytes_obj, &buf, &len);

		PyDroneData dd;

		p.consume_data(reinterpret_cast<uint8_t*>(buf), len, &dd);

		return dd;
	}

	private:
		VideoTelemetryParser p;
};

using FrameList = std::vector<np::ndarray>;
using TelemetryList = std::vector<payload_t>;

BOOST_PYTHON_MODULE(libcopter)
{
	using namespace boost::python;

	//Py_Initialize(); // seems unneeded
	np::initialize();

	class_<FrameList>("FrameList")
        .def(vector_indexing_suite<FrameList>() );
	class_<TelemetryList>("TelemetryList")
        .def(vector_indexing_suite<TelemetryList>() );

	class_<payload_t>("payload_t")
		.def_readwrite("type", &payload_t::type)
		.def_readwrite("counter", &payload_t::counter)
		.def_readwrite("value", &payload_t::value)
		.def_readwrite("timestamp", &payload_t::timestamp)
		.def_readwrite("maybe_timestamp_high", &payload_t::maybe_timestamp_high);

	class_<PyDroneData>("PyDroneData")
		.def_readwrite("telemetry_batt", &PyDroneData::telemetry_batt)
		.def_readwrite("telemetry_alti", &PyDroneData::telemetry_alti)
		.def_readwrite("telemetry_other", &PyDroneData::telemetry_other)
		.def_readwrite("video_frames", &PyDroneData::video_frames);

    class_<PyVideoTelemetryParser>("PyVideoTelemetryParser")
        .def("consume_data", &PyVideoTelemetryParser::consume_data)
        ;
}
