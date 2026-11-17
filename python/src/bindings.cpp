#include "discretization/discretization.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace py = pybind11;

namespace {

using discretization::FactorResult;
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

FactorResult best_discretization_factor(const FloatArray &values, const py::object &unit_system,
                                        std::int64_t max_factor, int num_threads) {
  if (values.ndim() != 1)
    throw py::value_error("values must be one-dimensional");
  const std::optional<UnitSystem> system =
      unit_system.is_none() ? std::nullopt : std::optional<UnitSystem>(parse_unit_system(unit_system));
  if (max_factor < 1)
    throw py::value_error("max_factor must be >= 1");
  // Every candidate is <= INT32_MAX, so a larger budget means "no budget".
  const int budget = static_cast<int>(std::min<std::int64_t>(max_factor, std::numeric_limits<int>::max()));
  const float *data = values.data();
  const auto size = static_cast<std::size_t>(values.size());
  try {
    py::gil_scoped_release release;
    return discretization::best_discretization_factor(data, size, system, budget, num_threads);
  } catch (const std::out_of_range &e) {
    // pybind11 would map std::out_of_range to IndexError.
    throw py::value_error(e.what());
  }
}

} // namespace

PYBIND11_MODULE(_core, m) {
  m.doc() = "C++ discretization routines";
  py::enum_<UnitSystem>(m, "UnitSystem", "Family of candidate discretization factors.")
      .value("METRIC", UnitSystem::Metric, "1, 10, 100, ..., 10^9")
      .value("IMPERIAL", UnitSystem::Imperial, "Every 2^a * 3^b that fits in int32");
  py::class_<FactorResult>(m, "FactorResult", "Result of best_discretization_factor (read-only).")
      .def_readonly("factor", &FactorResult::factor, "The factor with the lowest mean absolute error.")
      .def_readonly("mean_abs_error", &FactorResult::mean_abs_error, "Mean absolute error of `factor`.")
      .def_readonly("unit_system", &FactorResult::unit_system, "The candidate list `factor` comes from.")
      .def("__repr__", [](const FactorResult &r) {
        return "FactorResult(factor=" + std::to_string(r.factor) +
               ", mean_abs_error=" + py::repr(py::float_(r.mean_abs_error)).cast<std::string>() +
               ", unit_system=" + py::str(py::cast(r.unit_system)).cast<std::string>() + ")";
      });
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
  m.def("best_discretization_factor",
        &best_discretization_factor,
        py::arg("values"),
        py::arg("unit_system") = py::none(),
        py::arg("max_factor") = std::numeric_limits<int>::max(),
        py::arg("num_threads") = 0,
        "Return the candidate factor (up to `max_factor`) with the lowest mean absolute error, as a FactorResult.\n"
        "`unit_system`: None (both lists), 'metric', 'imperial' or a UnitSystem. `num_threads`: 0 = auto.");
}
