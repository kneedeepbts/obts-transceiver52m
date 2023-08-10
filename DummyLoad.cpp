/*
 * Compilation Flags
 *    SWLOOPBACK - compile for software loopback testing
 */

#include <string>

#include "Threads.h"
#include "DummyLoad.h"

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"

using namespace std;

DummyLoad::DummyLoad(double desired_sample_rate) {
    SPDLOG_INFO("Creating Dummy USRP device");
    m_sample_rate = desired_sample_rate;
}

int DummyLoad::loadBurst(short * dummy_burst, int length) {
    dummy_burst = dummy_burst;
    m_dummy_burst_size = length;
    return 0;
}

void DummyLoad::updateTime() {
    gettimeofday(&m_current_time, nullptr);
    double timeElapsed = (m_current_time.tv_sec - m_start_time.tv_sec) * 1.0e6 + (m_current_time.tv_usec - m_start_time.tv_usec);
    m_current_timestamp = (TIMESTAMP) floor(timeElapsed / (1.0e6 / m_sample_rate));
}

bool DummyLoad::make(bool skip_rx) {
    m_samples_read = 0;
    m_samples_written = 0;
    return true;
}

bool DummyLoad::start() {
    SPDLOG_INFO("Starting Dummy USRP");
    m_underrun = false;
    gettimeofday(&m_start_time, nullptr);
    m_dummy_burst_cursor = 0;
    return true;
}

bool DummyLoad::stop() {
    SPDLOG_INFO("Stopping Dummy USRP");
    return true;
}


// NOTE: Assumes sequential reads
int DummyLoad::readSamples(short * buf, int len, bool * overrun, TIMESTAMP timestamp, bool * underrun, unsigned * rssi) {
    updateTime();

    m_underrun_lock.lock();
    * underrun = m_underrun;
    m_underrun_lock.unlock();

    // FIXME: This section seems like it could use some cleanup.
    if (m_current_timestamp + len < timestamp) {
        usleep(100);
        return 0;
    } else if (m_current_timestamp < timestamp) {
        usleep(100);
        return 0;
    } else if (timestamp + len < m_current_timestamp) {
        memcpy(buf, m_dummy_burst + m_dummy_burst_cursor * 2, sizeof(short) * 2 * (m_dummy_burst_size - m_dummy_burst_cursor));
        int retVal = m_dummy_burst_size - m_dummy_burst_cursor;
        m_dummy_burst_cursor = 0;
        return retVal;
    } else if (timestamp + len > m_current_timestamp) {
        int amount = timestamp + len - m_current_timestamp;
        if (amount < m_dummy_burst_size - m_dummy_burst_cursor) {
            memcpy(buf, m_dummy_burst + m_dummy_burst_cursor * 2, sizeof(short) * 2 * amount);
            m_dummy_burst_cursor += amount;
            return amount;
        } else {
            memcpy(buf, m_dummy_burst + m_dummy_burst_cursor * 2, sizeof(short) * 2 * (m_dummy_burst_size - m_dummy_burst_cursor));
            int retVal = m_dummy_burst_size - m_dummy_burst_cursor;
            m_dummy_burst_cursor = 0;
            return retVal;
        }
    }
    return 0;
}

int DummyLoad::writeSamples(short * buf, int len, bool * underrun, TIMESTAMP timestamp, bool is_control) {
    updateTime();
    m_underrun_lock.lock();
    m_underrun |= (m_current_timestamp + len < timestamp);
    m_underrun_lock.unlock();
    return len;
}

bool DummyLoad::updateAlignment(TIMESTAMP timestamp) {
    return true;
}

bool DummyLoad::setTxFreq(double wFreq) {
    return true;
}

bool DummyLoad::setRxFreq(double wFreq) {
    return true;
}
