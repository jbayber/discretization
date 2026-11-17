#include "discretization/discretization.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <stdexcept>
#include <string>

namespace py = pybind11;

namespace {

using discretization::UnitSystem;

// `forcecast | c_style`: a contiguous float32 array (e.g. from NumPy or
// `pandas.Series.to_numpy(np.float32)`) is used in place with no copy; any
// other array-like (lists, float64, strided views) is converted once.
using FloatArray = py::array_t<float, py::array::c_style | py::array::forcecast>;

// Accepts a `UnitSystem` or a case-insensitive "metric" / "imperial" string.
UnitSystem parse_unit_system(const py::object &unit_system) {
  if (py::isinstance<UnitSystem>(unit_system))
    return unit_system.cast<UnitSystem>();
  if (py::isinstance<py::str>(unit_system)) {
    const auto name = unit_system.attr("lower")().cast<std::string>();
    if (name == "metric")
      return UnitSystem::Metric;
    if (name == "imperial")
      return UnitSystem::Imperial;
  }
  throw py::value_error("unit_system must be 'metric', 'imperial' or a discretization.UnitSystem, got " +
                        py::repr(unit_system).cast<std::string>());
}

int calculate_discretization_factor(const FloatArray &values, float epsilon, const py::object &unit_system) {
  if (values.ndim() != 1)
    throw py::value_error("values must be one-dimensional");
  const UnitSystem system = parse_unit_system(unit_system);
  const float *data = values.data();
  const auto size = static_cast<std::size_t>(values.size());
  try {
    py::gil_scoped_release release;
    return discretization::calculate_discretization_factor(data, size, epsilon, system);
  } catch (const std::out_of_range &e) {
    // pybind11 would map std::out_of_range to IndexError.
    throw py::value_error(e.what());
  }
}

py::array_t<int> discretize(const FloatArray &values, float epsilon, const py::object &unit_system) {
  if (values.ndim() != 1)
    throw py::value_error("values must be one-dimensional");
  const UnitSystem system = parse_unit_system(unit_system);
  py::array_t<int> out(values.size());
  const float *data = values.data();
  int *out_data = out.mutable_data();
  const auto size = static_cast<std::size_t>(values.size());
  try {
    py::gil_scoped_release release;
    discretization::discretize(data, size, epsilon, out_data, system);
  } catch (const std::out_of_range &e) {
    // pybind11 would map std::out_of_range to IndexError.
    throw py::value_error(e.what());
  }
  return out;
}

} // namespace

PYBIND11_MODULE(_core, m) {
  m.doc() = "C++ discretization routines";
  py::enum_<UnitSystem>(m, "UnitSystem", "Family of candidate discretization factors.")
      .value("METRIC", UnitSystem::Metric, "1, 10, 100, ..., 10^9")
      .value("IMPERIAL", UnitSystem::Imperial, "Every 2^a * 3^b that fits in int32");
  m.def("calculate_discretization_factor",
        &calculate_discretization_factor,
        py::arg("values"),
        py::arg("epsilon"),
        py::arg("unit_system") = "metric",
        "Return the smallest factor of `unit_system` that discretizes `values` within `epsilon`.");
  m.def("discretize",
        &discretize,
        py::arg("values"),
        py::arg("epsilon"),
        py::arg("unit_system") = "metric",
        "Return `values` scaled by the discretization factor and rounded, as an int32 NumPy array.");
}
