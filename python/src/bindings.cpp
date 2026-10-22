#include "discretization/discretization.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace {

// `forcecast | c_style`: a contiguous float32 array (e.g. from NumPy or
// `pandas.Series.to_numpy(np.float32)`) is used in place with no copy; any
// other array-like (lists, float64, strided views) is converted once.
using FloatArray = py::array_t<float, py::array::c_style | py::array::forcecast>;

int calculate_discretization_factor(const FloatArray &values, float epsilon) {
  if (values.ndim() != 1)
    throw py::value_error("values must be one-dimensional");
  const float *data = values.data();
  const auto size = static_cast<std::size_t>(values.size());
  py::gil_scoped_release release;
  return discretization::calculate_discretization_factor(data, size, epsilon);
}

py::array_t<int> discretize(const FloatArray &values, float epsilon) {
  if (values.ndim() != 1)
    throw py::value_error("values must be one-dimensional");
  py::array_t<int> out(values.size());
  const float *data = values.data();
  int *out_data = out.mutable_data();
  const auto size = static_cast<std::size_t>(values.size());
  {
    py::gil_scoped_release release;
    discretization::discretize(data, size, epsilon, out_data);
  }
  return out;
}

} // namespace

PYBIND11_MODULE(_core, m) {
  m.doc() = "C++ discretization routines";
  m.def("calculate_discretization_factor",
        &calculate_discretization_factor,
        py::arg("values"),
        py::arg("epsilon"),
        "Return the factor needed to discretize `values` within `epsilon`.");
  m.def("discretize",
        &discretize,
        py::arg("values"),
        py::arg("epsilon"),
        "Return `values` discretized to an int32 NumPy array.");
}
