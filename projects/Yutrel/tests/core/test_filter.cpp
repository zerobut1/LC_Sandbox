#include "ut/ut.hpp"

#include <concepts>
#include <limits>

#include "filters/box.h"
#include "filters/gaussian.h"
#include "filters/lanczos_sinc.h"
#include "filters/mitchell.h"
#include "filters/triangle.h"

using namespace boost::ut;
using namespace boost::ut::literals;
using namespace Yutrel;

static_assert(std::derived_from<BoxFilter, Filter>);
static_assert(std::derived_from<GaussianFilter, Filter>);
static_assert(std::derived_from<LanczosSincFilter, Filter>);
static_assert(std::derived_from<MitchellFilter, Filter>);
static_assert(std::derived_from<TriangleFilter, Filter>);

template <typename Spec>
void check_radius_validation()
{
    expect(!Spec{0.5f}.validate().has_value());
    expect(Spec{0.0f}.validate().has_value());
    expect(Spec{std::numeric_limits<float>::infinity()}.validate().has_value());
}

static auto test_filter_registration = []
{
    "filter_specs_validate_radius"_test = []
    {
        check_radius_validation<BoxFilterSpec>();
        check_radius_validation<GaussianFilterSpec>();
        check_radius_validation<LanczosSincFilterSpec>();
        check_radius_validation<MitchellFilterSpec>();
        check_radius_validation<TriangleFilterSpec>();
    };
    return 0;
}();

int main(int argc, char* argv[])
{
    boost::ut::detail::cfg::parse_arg_with_fallback(argc, const_cast<const char**>(argv));
}
