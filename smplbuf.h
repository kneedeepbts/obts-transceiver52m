#ifndef OBTS_TRANSCEIVER52M_SMPLBUF_H
#define OBTS_TRANSCEIVER52M_SMPLBUF_H

#include <cstdint>
#include <uhd/types/time_spec.hpp>
#include "radioDevice.h"

/*
    Sample Buffer - Allows reading and writing of timed samples using OpenBTS
                    or UHD style timestamps.
*/
// FIXME: Should this be templated and put into its own file?
class smpl_buf {
public:
    enum err_code {
        ERROR_TIMESTAMP = -1,
        ERROR_READ = -2,
        ERROR_WRITE = -3,
        ERROR_OVERFLOW = -4
    };

    /** Sample buffer constructor
        @param len number of 32-bit samples the buffer should hold
        @param rate sample clockrate
        @param timestamp
    */
    smpl_buf(std::size_t len, double rate);

    ~smpl_buf();

    /** Query number of samples available for reading
        @param timestamp time of first sample
        @return number of available samples or error
    */
    [[nodiscard]] ssize_t avail_smpls(TIMESTAMP timestamp) const;

    [[nodiscard]] ssize_t avail_smpls(uhd::time_spec_t timestamp) const;

    /** Read and write
        @param buf pointer to buffer
        @param len number of samples desired to read or write
        @param timestamp time of first stample
        @return number of actual samples read or written or error
    */
    ssize_t read(void * buf, size_t len, TIMESTAMP timestamp);

    ssize_t read(void * buf, size_t len, uhd::time_spec_t timestamp);

    ssize_t write(void * buf, size_t len, TIMESTAMP timestamp);

    ssize_t write(void * buf, size_t len, uhd::time_spec_t timestamp);

    /** Buffer status string
        @return a formatted string describing internal buffer state
    */
    [[nodiscard]] std::string str_status() const;

    /** Formatted error string
        @param code an error code
        @return a formatted error string
    */
    static std::string str_code(ssize_t code);

private:
    uint32_t * m_data;
    size_t m_buf_len;
    double m_clk_rt;

    TIMESTAMP m_time_start = 0;
    TIMESTAMP m_time_end = 0;

    size_t m_data_start = 0;
    size_t m_data_end = 0;
};

#endif //OBTS_TRANSCEIVER52M_SMPLBUF_H
