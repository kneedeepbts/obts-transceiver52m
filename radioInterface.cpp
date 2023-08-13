#include "radioInterface.h"

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"

extern "C" {
#include "convert.h"
}

#define CHUNK 625
#define NUMCHUNKS 4

RadioInterface::RadioInterface(RadioDevice * radio, int recv_offset, int sps, GsmTime start_time) {
    m_radio = radio;
    m_receive_offset = recv_offset;
    m_sps_tx = sps;
    m_clock.set(start_time);
}

RadioInterface::~RadioInterface() {
    //close();
    delete m_send_buffer;
    delete m_recv_buffer;
    delete m_convert_send_buffer;
    delete m_convert_recv_buffer;

    m_send_buffer = nullptr;
    m_recv_buffer = nullptr;
    m_convert_send_buffer = nullptr;
    m_convert_recv_buffer = nullptr;
}

bool RadioInterface::init(int type) {
    if (type != RadioDevice::NORMAL) {
        // FIXME: Can the NORMAL and RESAMP versions of this class be integrated?
        SPDLOG_ERROR("Attempting to use the NORMAL radio interface when resampling is needed.");
        return false;
    }

    close();

    // FIXME: Put these defines somewhere sane, like a static variable in the class?
    m_send_buffer = new signalVector(CHUNK * m_sps_tx);
    m_recv_buffer = new signalVector(NUMCHUNKS * CHUNK * m_sps_rx);

    m_convert_send_buffer = new short[m_send_buffer->size() * 2];
    m_convert_recv_buffer = new short[m_recv_buffer->size() * 2];

    m_send_cursor = 0;
    m_recv_cursor = 0;

    return true;
}

void RadioInterface::close() {
    delete m_send_buffer;
    delete m_recv_buffer;
    delete m_convert_send_buffer;
    delete m_convert_recv_buffer;

    m_send_buffer = nullptr;
    m_recv_buffer = nullptr;
    m_convert_send_buffer = nullptr;
    m_convert_recv_buffer = nullptr;
}


double RadioInterface::fullScaleInputValue() {
    return m_radio->fullScaleInputValue();
}

double RadioInterface::fullScaleOutputValue() {
    return m_radio->fullScaleOutputValue();
}


void RadioInterface::setPowerAttenuation(double atten) {
    double rfGain, digAtten;

    rfGain = m_radio->setTxGain(m_radio->maxTxGain() - atten);
    digAtten = atten - m_radio->maxTxGain() + rfGain;

    if (digAtten < 1.0)
        m_power_scaling = 1.0;
    else
        m_power_scaling = 1.0 / sqrt(pow(10, (digAtten / 10.0)));
}

// FIXME: This appears to be just copying data from one data structure to another.  This
//        shouldn't use memset or memcpy, but instead should use a more modern method.
//        Really should let the compiler handle this.  Also, see if it's possible to not
//        need to do this copy in the first place.
int RadioInterface::radioifyVector(signalVector &wVector, float * retVector, bool zero) {
    if (zero) {
        memset(retVector, 0, wVector.size() * 2 * sizeof(float));
        return wVector.size();
    }

    memcpy(retVector, wVector.begin(), wVector.size() * 2 * sizeof(float));

    return wVector.size();
}

// FIXME: This appears to be just copying data from one data structure to another.  This
//        shouldn't use memset or memcopy, but instead should use a more modern method.
//        Really should let the compiler handle this.  Also, see if it's possible to not
//        need to do this copy in the first place.
int RadioInterface::unRadioifyVector(float * floatVector, signalVector &newVector) {
    signalVector::iterator itr = newVector.begin();

    if (newVector.size() > m_recv_cursor) {
        SPDLOG_ERROR("Insufficient number of samples in receive buffer");
        return -1;
    }

    for (int i = 0; i < newVector.size(); i++) {
        *itr++ = std::complex<float>(floatVector[2 * i + 0], floatVector[2 * i + 1]);
    }

    return newVector.size();
}

bool RadioInterface::tuneTx(double freq) {
    return m_radio->setTxFreq(freq);
}

bool RadioInterface::tuneRx(double freq) {
    return m_radio->setRxFreq(freq);
}


void RadioInterface::start() {
    SPDLOG_INFO("starting radio interface");
#ifdef USRP1
    mAlignRadioServiceLoopThread.start((void * (*)(void*))AlignRadioServiceLoopAdapter, (void*)this);
#endif
    m_write_timestamp = m_radio->initialWriteTimestamp();
    m_read_timestamp = m_radio->initialReadTimestamp();
    m_radio->start();
    SPDLOG_DEBUG("Radio started");
    m_radio->updateAlignment(m_write_timestamp - 10000);
    m_radio->updateAlignment(m_write_timestamp - 10000);

    m_radio_on = true;
}

// FIXME: Since the USRP1 isn't supported in this class, is this needed?  Or should
//        this code be updated to handle the USRP in the same executable?
#ifdef USRP1
void *AlignRadioServiceLoopAdapter(RadioInterface *radioInterface)
{
  while (1) {
    radioInterface->alignRadio();
    pthread_testcancel();
  }
  return NULL;
}

void RadioInterface::alignRadio() {
  sleep(60);
  mRadio->updateAlignment(writeTimestamp+ (TIMESTAMP) 10000);
}
#endif

void RadioInterface::driveTransmitRadio(signalVector &radioBurst, bool zeroBurst) {
    if (!m_radio_on) {
        return;
    }

    radioifyVector(radioBurst, (float *) (m_send_buffer->begin() + m_send_cursor), zeroBurst);
    m_send_cursor += radioBurst.size();
    pushBuffer();
}

// FIXME: From GSMTransfer.h
static const unsigned gSlotLen = 148;	///< number of symbols per slot, not counting guard periods

void RadioInterface::driveReceiveRadio() {
    if (!m_radio_on) {
        return;
    }

    // FIXME: Why is there a FIFO, a recv_buffer, and a convert_recv_buffer?  Seems
    //        like a lot of copying for a bit of data.
    if (m_receive_fifo.size() > 8) {
        return;
    }
    pullBuffer();

    GsmTime rcvClock = m_clock.get();
    rcvClock.decTN(m_receive_offset);
    unsigned tN = rcvClock.TN();
    int rcvSz = m_recv_cursor;
    int readSz = 0;
    const int symbolsPerSlot = gSlotLen + 8;

    // while there's enough data in receive buffer, form received
    //    GSM bursts and pass up to Transceiver
    // Using the 157-156-156-156 symbols per timeslot format.
    while (rcvSz > (symbolsPerSlot + (tN % 4 == 0)) * m_sps_rx) {
        signalVector rxVector((symbolsPerSlot + (tN % 4 == 0)) * m_sps_rx);
        unRadioifyVector((float *) (m_recv_buffer->begin() + readSz), rxVector);
        GsmTime tmpTime = rcvClock;
        if (rcvClock.FN() >= 0) {
            //LOG(DEBUG) << "FN: " << rcvClock.FN();
            radioVector * rxBurst = nullptr;
            if (!m_load_test)
                rxBurst = new radioVector(rxVector, tmpTime);
            else {
                // FIXME: Should there be load test code here?
                if (tN % 4 == 0)
                    rxBurst = new radioVector(*m_final_vec9, tmpTime);
                else
                    rxBurst = new radioVector(*m_final_vec, tmpTime);
            }
            m_receive_fifo.put(rxBurst);
        }
        m_clock.incTN();
        rcvClock.incTN();
        readSz += (symbolsPerSlot + (tN % 4 == 0)) * m_sps_rx;
        rcvSz -= (symbolsPerSlot + (tN % 4 == 0)) * m_sps_rx;

        tN = rcvClock.TN();
    }

    if (readSz > 0) {
        // FIXME: Again, need to do this in a more modern way, one that's safer and the compiler will optimize.
        memmove(m_recv_buffer->begin(), m_recv_buffer->begin() + readSz, (m_recv_cursor - readSz) * 2 * sizeof(float));

        m_recv_cursor -= readSz;
    }
}

bool RadioInterface::isUnderrun() {
    bool retVal = m_underrun;
    m_underrun = false; // FIXME: Why would this clear the underrun flag?

    return retVal;
}

double RadioInterface::setRxGain(double dB) {
    if (m_radio) {
        return m_radio->setRxGain(dB);
    }
    return -1;
}

double RadioInterface::getRxGain() {
    if (m_radio) {
        return m_radio->getRxGain();
    }
    return -1;
}

/* Receive a timestamped chunk from the device */
void RadioInterface::pullBuffer() {
    bool local_underrun;
    int num_recv;
    float * output;

    if (m_recv_cursor > m_recv_buffer->size() - CHUNK) {
        return;
    }

    /* Outer buffer access size is fixed */
    SPDLOG_DEBUG("Just before readSamples");
    num_recv = m_radio->readSamples(m_convert_recv_buffer, CHUNK, &m_overrun, m_read_timestamp, &local_underrun);
    if (num_recv != CHUNK) {
        SPDLOG_ERROR("Receive error {}", num_recv);
        return;
    }

    SPDLOG_DEBUG("Just before output");
    output = (float *) (m_recv_buffer->begin() + m_recv_cursor);
    SPDLOG_DEBUG("About to convert_short_float, output: {}", *output);
    convert_short_float(output, m_convert_recv_buffer, 2 * num_recv);
    SPDLOG_DEBUG("After convert_short_float");


    m_underrun |= local_underrun;

    m_read_timestamp += num_recv;
    m_recv_cursor += num_recv;
    SPDLOG_DEBUG("End PullBuffer, m_underrun: {}, m_read_timestamp: {}, m_recv_cursor:{}", m_underrun, m_read_timestamp, m_recv_cursor);
}

/* Send timestamped chunk to the device with arbitrary size */
void RadioInterface::pushBuffer() {
    int num_sent;

    if (m_send_cursor < CHUNK) {
        return;
    }

    if (m_send_cursor > m_send_buffer->size()) {
        SPDLOG_ERROR("Send buffer overflow");
    }

    convert_float_short(m_convert_send_buffer, (float *) m_send_buffer->begin(), m_power_scaling, 2 * m_send_cursor);

    /* Send the all samples in the send buffer */
    num_sent = m_radio->writeSamples(m_convert_send_buffer, m_send_cursor, &m_underrun, m_write_timestamp);
    if (num_sent != m_send_cursor) {
        SPDLOG_ERROR("Transmit error {}", num_sent);
    }

    m_write_timestamp += num_sent;
    m_send_cursor = 0;
}
