#include "radioInterface.h"

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"


#include "Resampler.h"

extern "C" {
#include "convert.h"
}

/* Resampling parameters for 64 MHz clocking */
#define RESAMP_64M_INRATE            65
#define RESAMP_64M_OUTRATE            96

/* Resampling parameters for 100 MHz clocking */
#define RESAMP_100M_INRATE            52
#define RESAMP_100M_OUTRATE            75

/* Universal resampling parameters */
#define NUMCHUNKS                24

/*
 * Resampling filter bandwidth scaling factor
 *   This narrows the filter cutoff relative to the output bandwidth
 *   of the polyphase resampler. At 4 samples-per-symbol using the
 *   2 pulse Laurent GMSK approximation gives us below 0.5 degrees
 *   RMS phase error at the resampler output.
 */
#define RESAMP_TX4_FILTER        0.45

// FIXME: Make these globals go away at some point in the future.
static Resampler * upsampler = nullptr;
static Resampler * dnsampler = nullptr;
static int resamp_inrate = 0;
static int resamp_inchunk = 0;
static int resamp_outrate = 0;
static int resamp_outchunk = 0;

short * convertRecvBuffer = nullptr;
short * convertSendBuffer = nullptr;

RadioInterfaceResamp::RadioInterfaceResamp(RadioDevice * radio, int recv_offset, int sps, GsmTime start_time)
        : RadioInterface(radio, recv_offset, sps, start_time) {}

RadioInterfaceResamp::~RadioInterfaceResamp() {
    delete m_inner_send_buffer;
    delete m_outer_send_buffer;
    delete m_inner_recv_buffer;
    delete m_outer_recv_buffer;

    delete upsampler;
    delete dnsampler;

    m_inner_send_buffer = nullptr;
    m_outer_send_buffer = nullptr;
    m_inner_recv_buffer = nullptr;
    m_outer_recv_buffer = nullptr;
    m_send_buffer = nullptr;
    m_recv_buffer = nullptr;

    upsampler = nullptr;
    dnsampler = nullptr;
}

void RadioInterfaceResamp::close() {
    delete m_inner_send_buffer;
    delete m_outer_send_buffer;
    delete m_inner_recv_buffer;
    delete m_outer_recv_buffer;

    delete upsampler;
    delete dnsampler;

    m_inner_send_buffer = nullptr;
    m_outer_send_buffer = nullptr;
    m_inner_recv_buffer = nullptr;
    m_outer_recv_buffer = nullptr;
    m_send_buffer = nullptr;
    m_recv_buffer = nullptr;

    upsampler = nullptr;
    dnsampler = nullptr;

    RadioInterface::close();
}

/* Initialize I/O specific objects */
bool RadioInterfaceResamp::init(int type) {
    float cutoff = 1.0f;

    close();

    switch (type) {
        case RadioDevice::RESAMP_64M:
            resamp_inrate = RESAMP_64M_INRATE;
            resamp_outrate = RESAMP_64M_OUTRATE;
            break;
        case RadioDevice::RESAMP_100M:
            resamp_inrate = RESAMP_100M_INRATE;
            resamp_outrate = RESAMP_100M_OUTRATE;
            break;
        case RadioDevice::NORMAL:
        default:
            SPDLOG_ERROR("Invalid device configuration");
            return false;
    }

    resamp_inchunk = resamp_inrate * 4;
    resamp_outchunk = resamp_outrate * 4;

    if (resamp_inchunk * NUMCHUNKS < 157 * m_sps_tx * 2) {
        SPDLOG_ERROR("Invalid inner chunk size {}", resamp_inchunk);
        return false;
    }

    if (m_sps_tx == 4) {
        cutoff = RESAMP_TX4_FILTER;
    }

    dnsampler = new Resampler(resamp_inrate, resamp_outrate);
    if (!dnsampler->init()) {
        SPDLOG_ERROR("Rx resampler failed to initialize");
        return false;
    }

    upsampler = new Resampler(resamp_outrate, resamp_inrate);
    if (!upsampler->init(cutoff)) {
        SPDLOG_ERROR("Tx resampler failed to initialize");
        return false;
    }

    /*
     * Allocate high and low rate buffers. The high rate receive
     * buffer and low rate transmit vectors feed into the resampler
     * and requires headroom equivalent to the filter length. Low
     * rate buffers are allocated in the main radio interface code.
     */
    m_inner_send_buffer = new signalVector(NUMCHUNKS * resamp_inchunk, upsampler->len());
    m_outer_send_buffer = new signalVector(NUMCHUNKS * resamp_outchunk);
    m_inner_recv_buffer = new signalVector(NUMCHUNKS * resamp_inchunk / m_sps_tx);
    m_outer_recv_buffer = new signalVector(resamp_outchunk, dnsampler->len());


    convertSendBuffer = new short[m_outer_send_buffer->size() * 2];
    convertRecvBuffer = new short[m_outer_recv_buffer->size() * 2];

    m_send_buffer = m_inner_send_buffer;
    m_recv_buffer = m_inner_recv_buffer;

    return true;
}

/* Receive a timestamped chunk from the device */
void RadioInterfaceResamp::pullBuffer() {
    bool local_underrun;
    int rc, num_recv;

    if (m_recv_cursor > m_inner_recv_buffer->size() - resamp_inchunk) {
        return;
    }

    /* Outer buffer access size is fixed */
    num_recv = m_radio->readSamples(convertRecvBuffer, resamp_outchunk, &m_overrun, m_read_timestamp, &local_underrun);
    if (num_recv != resamp_outchunk) {
        SPDLOG_ERROR("Receive error {}", num_recv);
        return;
    }

    convert_short_float((float *) m_outer_recv_buffer->begin(), convertRecvBuffer, 2 * resamp_outchunk);

    m_underrun |= local_underrun;
    m_read_timestamp += (TIMESTAMP) resamp_outchunk;

    /* Write to the end of the inner receive buffer */
    rc = dnsampler->rotate((float *) m_outer_recv_buffer->begin(), resamp_outchunk, (float *) (m_inner_recv_buffer->begin() + m_recv_cursor), resamp_inchunk);
    if (rc < 0) {
        SPDLOG_ERROR("Sample rate upsampling error");
    }

    m_recv_cursor += resamp_inchunk;
}

/* Send a timestamped chunk to the device */
void RadioInterfaceResamp::pushBuffer() {
    int rc, chunks, num_sent;
    int inner_len, outer_len;

    if (m_send_cursor < resamp_inchunk) {
        return;
    }

    if (m_send_cursor > m_inner_send_buffer->size()) {
        SPDLOG_ERROR("Send buffer overflow");
    }

    chunks = m_send_cursor / resamp_inchunk;

    inner_len = chunks * resamp_inchunk;
    outer_len = chunks * resamp_outchunk;

    /* Always send from the beginning of the buffer */
    rc = upsampler->rotate((float *) m_inner_send_buffer->begin(), inner_len, (float *) m_outer_send_buffer->begin(), outer_len);
    if (rc < 0) {
        SPDLOG_ERROR("Sample rate downsampling error");
    }

    convert_float_short(convertSendBuffer, (float *) m_outer_send_buffer->begin(), m_power_scaling, 2 * outer_len);

    num_sent = m_radio->writeSamples(convertSendBuffer, outer_len, &m_underrun, m_write_timestamp);
    if (num_sent != outer_len) {
        SPDLOG_ERROR("Transmit error {}", num_sent);
    }

    /* Shift remaining samples to beginning of buffer */
    memmove(m_inner_send_buffer->begin(), m_inner_send_buffer->begin() + inner_len, (m_send_cursor - inner_len) * 2 * sizeof(float));

    m_write_timestamp += outer_len;
    m_send_cursor -= inner_len;
    assert(m_send_cursor >= 0);
}
