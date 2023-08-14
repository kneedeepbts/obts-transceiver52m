#ifndef OBTS_TRANSCEIVER52M_GSMTIME_H
#define OBTS_TRANSCEIVER52M_GSMTIME_H

#include <cassert>
#include <cstdint>
#include <ostream>


/** Get a clock difference, within the modulus, v1-v2. */
std::int32_t FNDelta(std::uint32_t v1, std::uint32_t v2);

/** Compare two frame clock values.
 *  return 1 if v1>v2, -1 if v1<v2, 0 if v1==v2
 */
std::int32_t FNCompare(std::uint32_t v1, std::uint32_t v2);

/**
	GSM frame clock value. GSM 05.02 4.3
	No internal thread sync.
*/
class GsmTime {
public:
    /** There can only be eight timeslots (0 to 7) per GSM 05.02 4.3.2 */
    static const std::uint8_t g_max_timeslots = 8;

    /** Frame numbers cycle from 0 to 2715647 ((26 x 51 x 2048) - 1) per GSM 05.02 4.3.3 */
    static const std::uint32_t g_max_frames = (26 * 51 * 2048);

    /** The GSM hyperframe is largest time period in the GSM system, GSM 05.02 4.3.3. */
    // It is 2715648 or 3 hours, 28 minutes, 53 seconds
    // NOTE: The "hyperframe" is a group of frames that includes all of the frame numbers.
    //       hyperframe = 2048 superframes
    //       superframe = 26 x 51 (TDMA) frames
    //       superframe = 51 traffic multiframes (26 frame multiframe)
    //       superframe = 26 broadcast multiframes (51 frame multiframe)
    //static const std::uint32_t g_hyperframe = 2048UL * 26UL * 51UL;

    // FIXME: What's the best way to validate input values in the constructor?
    explicit GsmTime(std::uint32_t frame_num = 0, std::uint8_t timeslot_num = 0) {
        assert(frame_num < g_max_frames);
        m_frame_number = frame_num;
        assert(timeslot_num < g_max_timeslots);
        m_timeslot_number = timeslot_num;
    }

    /** Move the time forward to a given position in a given modulus. */
    [[maybe_unused]] void rollForward(std::uint32_t frame_num, std::uint32_t modulus) {
        assert(modulus < g_max_frames);
        while ((m_frame_number % modulus) != frame_num) {
            m_frame_number = (m_frame_number + 1) % g_max_frames;
        }
    }

    [[nodiscard]] std::uint32_t FN() const {
        return m_frame_number;
    }

    void FN(std::uint32_t frame_num) {
        assert(frame_num < g_max_frames);
        m_frame_number = frame_num;
    }

    [[nodiscard]] std::uint32_t TN() const {
        return m_timeslot_number;
    }

    void TN(std::uint32_t timeslot_num) {
        assert(timeslot_num < g_max_timeslots);
        m_timeslot_number = timeslot_num;
    }


    GsmTime &operator++() {
        m_frame_number = (m_frame_number + 1) % g_max_frames;
        return *this;
    }

    GsmTime &decTN(std::uint32_t step = 1) {
        assert(step <= 8);
        m_timeslot_number -= step;
        if (m_timeslot_number < 0) {
            m_timeslot_number += 8;
            m_frame_number -= 1;
            if (m_frame_number < 0) {
                m_frame_number += g_max_frames;
            }
        }
        return *this;
    }

    GsmTime &incTN(std::uint32_t step = 1) {
        assert(step <= 8);
        m_timeslot_number += step;
        if (m_timeslot_number > 7) {
            m_timeslot_number -= 8;
            m_frame_number = (m_frame_number + 1) % g_max_frames;
        }
        return *this;
    }

    GsmTime &operator+=(std::uint32_t step) {
        // Remember the step might be negative.
        m_frame_number += step;
        if (m_frame_number < 0) m_frame_number += g_max_frames;
        m_frame_number = m_frame_number % g_max_frames;
        return *this;
    }

    GsmTime operator-(std::uint32_t step) const {
        return operator+(-step);
    }

    GsmTime operator+(std::uint32_t step) const {
        GsmTime newVal = *this;
        newVal += step;
        return newVal;
    }

    // (pat) Notice that + and - are different.
    GsmTime operator+(const GsmTime &other) const {
        std::uint32_t newTN = (m_timeslot_number + other.m_timeslot_number) % 8;
        std::uint32_t newFN = (m_frame_number + other.m_frame_number + (m_timeslot_number + other.m_timeslot_number) / 8) % g_max_frames;
        return GsmTime(newFN, newTN);
    }

    int operator-(const GsmTime &other) const {
        return FNDelta(m_frame_number, other.m_frame_number);
    }


    bool operator<(const GsmTime &other) const {
        if (m_frame_number == other.m_frame_number) return (m_timeslot_number < other.m_timeslot_number);
        return FNCompare(m_frame_number, other.m_frame_number) < 0;
    }

    bool operator>(const GsmTime &other) const {
        if (m_frame_number == other.m_frame_number) return (m_timeslot_number > other.m_timeslot_number);
        return FNCompare(m_frame_number, other.m_frame_number) > 0;
    }

    bool operator<=(const GsmTime &other) const {
        if (m_frame_number == other.m_frame_number) return (m_timeslot_number <= other.m_timeslot_number);
        return FNCompare(m_frame_number, other.m_frame_number) <= 0;
    }

    bool operator>=(const GsmTime &other) const {
        if (m_frame_number == other.m_frame_number) return (m_timeslot_number >= other.m_timeslot_number);
        return FNCompare(m_frame_number, other.m_frame_number) >= 0;
    }

    bool operator==(const GsmTime &other) const {
        return (m_frame_number == other.m_frame_number) && (m_timeslot_number == other.m_timeslot_number);
    }

    /** GSM 05.02 3.3.2.2.1 */
    [[nodiscard]] std::uint32_t SFN() const {
        return m_frame_number / (26 * 51);
    }

    /** GSM 05.02 3.3.2.2.1 */
    [[nodiscard]] std::uint32_t T1() const {
        return SFN() % 2048;
    }

    /** GSM 05.02 3.3.2.2.1 */
    [[nodiscard]] std::uint32_t T2() const {
        return m_frame_number % 26;
    }

    /** GSM 05.02 3.3.2.2.1 */
    [[nodiscard]] std::uint32_t T3() const {
        return m_frame_number % 51;
    }

    /** GSM 05.02 3.3.2.2.1. */
    [[nodiscard]] std::uint32_t T3p() const {
        return (T3() - 1) / 10;
    }

    /** GSM 05.02 6.3.1.3. */
    [[nodiscard]] std::uint32_t TC() const {
        return (FN() / 51) % 8;
    }

    /** GSM 04.08 10.5.2.30. */
    [[nodiscard]] std::uint32_t T1p() const {
        return SFN() % 32;
    }

    /** GSM 05.02 6.2.3 */
    [[nodiscard]] std::uint32_t T1R() const {
        return T1() % 64;
    }

private:
    std::uint32_t m_frame_number; // frame number in the hyperframe
    std::uint8_t m_timeslot_number; // timeslot number (0 to 7)

};

std::ostream &operator<<(std::ostream &os, const GsmTime &ts);

#endif //OBTS_TRANSCEIVER52M_GSMTIME_H
