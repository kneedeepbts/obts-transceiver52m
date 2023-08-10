#ifndef OBTS_TRANSCEIVER52M_DUMMYLOAD_H
#define OBTS_TRANSCEIVER52M_DUMMYLOAD_H

#include <cstdint>
#include <sys/time.h>
#include <math.h>
#include <string>
#include <iostream>

#include "radioDevice.h"

/** A class to handle a USRP rev 4, with a two RFX900 daughterboards */
class DummyLoad : public RadioDevice {
public:
    explicit DummyLoad(double desired_sample_rate);

    // FIXME: This seems to be in the wrong place in this class definition.
    int loadBurst(short * dummy_burst, int length);

    /** Instantiate the USRP */
    // FIXME: Should this one match the others (the static call?)
    bool make(bool skip_rx = false);

    bool start() override;
    bool stop() override;

    /**
      Read samples from the USRP.
      @param buf preallocated buf to contain read result
      @param len number of samples desired
      @param overrun Set if read buffer has been overrun, e.g. data not being read fast enough
      @param timestamp The timestamp of the first samples to be read
      @param underrun Set if USRP does not have data to transmit, e.g. data not being sent fast enough
      @param RSSI The received signal strength of the read result
      @return The number of samples actually read
    */
    // FIXME: Do this with overloads, not default parameters.  "Clang-Tidy: Default arguments on virtual or override methods are prohibited."
    int readSamples(short * buf, int len, bool * overrun, TIMESTAMP timestamp = 0xffffffff, bool * underrun = nullptr, unsigned * rssi = nullptr) override;

    /**
          Write samples to the USRP.
          @param buf Contains the data to be written.
          @param len number of samples to write.
          @param underrun Set if USRP does not have data to transmit, e.g. data not being sent fast enough
          @param timestamp The timestamp of the first sample of the data buffer.
          @param isControl Set if data is a control packet, e.g. a ping command
          @return The number of samples actually written
    */
    // FIXME: Do this with overloads, not default parameters.  "Clang-Tidy: Default arguments on virtual or override methods are prohibited."
    int writeSamples(short * buf, int len, bool * underrun, TIMESTAMP timestamp = 0xffffffff, bool isControl = false) override;

    /** Update the alignment between the read and write timestamps */
    bool updateAlignment(TIMESTAMP timestamp) override;

    /** Set the transmitter frequency */
    bool setTxFreq(double wFreq) override;

    /** Set the receiver frequency */
    bool setRxFreq(double wFreq) override;

    /** Returns the starting write Timestamp*/
    TIMESTAMP initialWriteTimestamp() override { return 20000; }

    /** Returns the starting read Timestamp*/
    TIMESTAMP initialReadTimestamp() override { return 20000; }

    /** returns the full-scale transmit amplitude **/
    double fullScaleInputValue() override { return 13500.0; }

    /** returns the full-scale receive amplitude **/
    double fullScaleOutputValue() override { return 9450.0; }

    /** Return internal status values */
    inline double getTxFreq() override { return 0; }
    inline double getRxFreq() override { return 0; }
    inline double getSampleRate() override { return m_sample_rate; }

    // FIXME: Should these be uint64_t?  Double seems wrong for counting samples.
    inline double numberRead() override { return m_samples_read; }
    inline double numberWritten() override { return m_samples_written; }

private:
    void updateTime();

    double m_sample_rate; // the desired sampling rate
    std::uint64_t m_samples_read; // number of samples read from USRP
    std::uint64_t m_samples_written; // number of samples sent to USRP

    Mutex m_underrun_lock;

    // FIXME: Should move these to std::chrono or something similar.
    struct timeval m_start_time;
    struct timeval m_current_time;
    TIMESTAMP m_current_timestamp;

    short * m_dummy_burst;
    int m_dummy_burst_size;
    int m_dummy_burst_cursor;
    bool m_underrun;
};

#endif //OBTS_TRANSCEIVER52M_DUMMYLOAD_H
