#include "smplbuf.h"

#include <sstream>
#include <cstring>

smpl_buf::smpl_buf(size_t len, double rate) : m_buf_len(len), m_clk_rt(rate) {
    m_data = new uint32_t[len];
}

smpl_buf::~smpl_buf() {
    delete[] m_data;
}

ssize_t smpl_buf::avail_smpls(TIMESTAMP timestamp) const {
    if (timestamp < m_time_start)
        return ERROR_TIMESTAMP;
    else if (timestamp >= m_time_end)
        return 0;
    else
        return m_time_end - timestamp;
}

ssize_t smpl_buf::avail_smpls(uhd::time_spec_t ts) const {
    return avail_smpls(ts.to_ticks(m_clk_rt));
}

// FIXME: Not a fan of using a bunch of `memcpy`.  Should make this use more modern data structures in the future.
ssize_t smpl_buf::read(void *buf, size_t len, TIMESTAMP timestamp) {
    int type_sz = 2 * sizeof(short);

    // Check for valid read
    if (timestamp < m_time_start)
        return ERROR_TIMESTAMP;
    if (timestamp >= m_time_end)
        return 0;
    if (len >= m_buf_len)
        return ERROR_READ;

    // How many samples should be copied
    size_t num_smpls = m_time_end - timestamp;
    if (num_smpls > len)
        num_smpls = len;

    // Starting index
    size_t read_start = m_data_start + (timestamp - m_time_start);

    // Read it
    if (read_start + num_smpls < m_buf_len) {
        size_t numBytes = len * type_sz;
        memcpy(buf, m_data + read_start, numBytes);
    } else {
        size_t first_cp = (m_buf_len - read_start) * type_sz;
        size_t second_cp = len * type_sz - first_cp;

        memcpy(buf, m_data + read_start, first_cp);
        memcpy((char *) buf + first_cp, m_data, second_cp);
    }

    m_data_start = (read_start + len) % m_buf_len;
    m_time_start = timestamp + len;

    if (m_time_start > m_time_end)
        return ERROR_READ;
    else
        return num_smpls;
}

ssize_t smpl_buf::read(void *buf, size_t len, uhd::time_spec_t ts) {
    return read(buf, len, ts.to_ticks(m_clk_rt));
}

ssize_t smpl_buf::write(void *buf, size_t len, TIMESTAMP timestamp) {
    int type_sz = 2 * sizeof(short);

    // Check for valid write
    if ((len == 0) || (len >= m_buf_len))
        return ERROR_WRITE;
    if ((timestamp + len) <= m_time_end)
        return ERROR_TIMESTAMP;

    // Starting index
    size_t write_start = (m_data_start + (timestamp - m_time_start)) % m_buf_len;

    // Write it
    if ((write_start + len) < m_buf_len) {
        size_t numBytes = len * type_sz;
        memcpy(m_data + write_start, buf, numBytes);
    } else {
        size_t first_cp = (m_buf_len - write_start) * type_sz;
        size_t second_cp = len * type_sz - first_cp;

        memcpy(m_data + write_start, buf, first_cp);
        memcpy(m_data, (char *) buf + first_cp, second_cp);
    }

    m_data_end = (write_start + len) % m_buf_len;
    m_time_end = timestamp + len;

    if (((write_start + len) > m_buf_len) && (m_data_end > m_data_start))
        return ERROR_OVERFLOW;
    else if (m_time_end <= m_time_start)
        return ERROR_WRITE;
    else
        return len;
}

ssize_t smpl_buf::write(void *buf, size_t len, uhd::time_spec_t ts) {
    return write(buf, len, ts.to_ticks(m_clk_rt));
}

// FIXME: Seems like string formatting would be better here.
std::string smpl_buf::str_status() const {
    std::ostringstream ost("Sample buffer: ");

    ost << "length = " << m_buf_len;
    ost << ", time_start = " << m_time_start;
    ost << ", time_end = " << m_time_end;
    ost << ", data_start = " << m_data_start;
    ost << ", data_end = " << m_data_end;

    return ost.str();
}

std::string smpl_buf::str_code(ssize_t code) {
    switch (code) {
        case ERROR_TIMESTAMP:
            return "Sample buffer: Requested timestamp is not valid";
        case ERROR_READ:
            return "Sample buffer: Read error";
        case ERROR_WRITE:
            return "Sample buffer: Write error";
        case ERROR_OVERFLOW:
            return "Sample buffer: Overrun";
        default:
            return "Sample buffer: Unknown error";
    }
}

