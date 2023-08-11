#include "gsmtime.h"

/** Get a clock difference, within the modulus, v1-v2. */
std::int32_t FNDelta(std::uint32_t v1, std::uint32_t v2) {
    static const int32_t half_modulus = GsmTime::g_hyperframe / 2;
    std::int32_t delta = v1 - v2;
    if (delta >= half_modulus) delta -= GsmTime::g_hyperframe;
    else if (delta < -half_modulus) delta += GsmTime::g_hyperframe;
    return delta;
}

std::int32_t FNCompare(std::uint32_t v1, std::uint32_t v2) {
    std::int32_t delta = FNDelta(v1, v2);
    if (delta > 0) {
        return 1;
    }
    if (delta < 0) {
        return -1;
    }
    return 0;
}

std::ostream &operator<<(std::ostream &os, const GsmTime &ts) {
    os << ts.TN() << ":" << ts.FN();
    return os;
}
