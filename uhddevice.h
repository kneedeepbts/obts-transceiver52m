#ifndef OBTS_TRANSCEIVER52M_UHDDEVICE_H
#define OBTS_TRANSCEIVER52M_UHDDEVICE_H

#include "radioDevice.h"
#include "Threads.h"
#include "smplbuf.h"

#include <uhd/version.hpp>
#include <uhd/property_tree.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/thread.hpp>


#define B2XX_CLK_RT      26e6
#define B100_BASE_RT     400000
#define USRP2_BASE_RT    390625
#define TX_AMPL          0.3
#define SAMPLE_BUF_SZ    (1 << 20)

enum uhd_dev_type {
    USRP1,
    USRP2,
    B100,
    B2XX,
    X3XX,
    UMTRX,
    NUM_USRP_TYPES,
};

struct uhd_dev_offset {
    enum uhd_dev_type type;
    int sps;
    double offset;
    const std::string desc;
};

/*
 * USRP version dependent device timings
 */
#ifdef USE_UHD_3_9
#define B2XX_TIMING_1SPS	1.7153e-4
#define B2XX_TIMING_4SPS	1.1696e-4
#else
#define B2XX_TIMING_1SPS    9.9692e-5
#define B2XX_TIMING_4SPS    6.9248e-5
#endif

/*
 * Tx / Rx sample offset values. In a perfect world, there is no group delay
 * though analog components, and behaviour through digital filters exactly
 * matches calculated values. In reality, there are unaccounted factors,
 * which are captured in these empirically measured (using a loopback test)
 * timing correction values.
 *
 * Notes:
 *   USRP1 with timestamps is not supported by UHD.
 */
static struct uhd_dev_offset uhd_offsets[NUM_USRP_TYPES * 2] = {
        {USRP1, 1, 0.0,              "USRP1 not supported"},
        {USRP1, 4, 0.0,              "USRP1 not supported"},
        {USRP2, 1, 1.2184e-4,        "N2XX 1 SPS"},
        {USRP2, 4, 8.0230e-5,        "N2XX 4 SPS"},
        {B100,  1, 1.2104e-4,        "B100 1 SPS"},
        {B100,  4, 7.9307e-5,        "B100 4 SPS"},
        {B2XX,  1, B2XX_TIMING_1SPS, "B200 1 SPS"},
        {B2XX,  4, B2XX_TIMING_4SPS, "B200 4 SPS"},
        {X3XX,  1, 1.5360e-4,        "X3XX 1 SPS"},
        {X3XX,  4, 1.1264e-4,        "X3XX 4 SPS"},
        {UMTRX, 1, 9.9692e-5,        "UmTRX 1 SPS"},
        {UMTRX, 4, 7.3846e-5,        "UmTRX 4 SPS"},
};


/*
    uhd_device - UHD implementation of the Device interface. Timestamped samples
                are sent to and received from the device. An intermediate buffer
                on the receive side collects and aligns packets of samples.
                Events and errors such as underruns are reported asynchronously
                by the device and received in a separate thread.
*/
class uhd_device : public RadioDevice {
public:
    uhd_device(int sps, bool skip_rx);
    ~uhd_device() override;

    int open(const std::string &args, ReferenceType ref) override;

    bool start() override;
    bool stop() override;
    void restart(uhd::time_spec_t ts);

    void setPriority() override;

    enum TxWindowType getWindowType() override { return tx_window; }

    int readSamples(short *buf, int len, bool * overrun, TIMESTAMP timestamp, bool * underrun, unsigned * RSSI) override;
    int writeSamples(short *buf, int len, bool * underrun, TIMESTAMP timestamp, bool isControl) override;

    bool updateAlignment(TIMESTAMP timestamp) override;

    bool setTxFreq(double wFreq) override;
    bool setRxFreq(double wFreq) override;

    inline TIMESTAMP initialWriteTimestamp() override { return 0; }
    inline TIMESTAMP initialReadTimestamp() override { return 0; }

    inline double fullScaleInputValue() override { return 32000 * TX_AMPL; }
    inline double fullScaleOutputValue() override { return 32000; }

    double setRxGain(double db) override;
    double getRxGain() override { return rx_gain; }
    double maxRxGain() override { return rx_gain_max; }
    double minRxGain() override { return rx_gain_min; }

    double setTxGain(double db) override;
    double maxTxGain() override { return tx_gain_max; }
    double minTxGain() override { return tx_gain_min; }

    double getTxFreq() override { return tx_freq; }
    double getRxFreq() override { return rx_freq; }

    inline double getSampleRate() override { return tx_rate; }
    inline double numberRead() override { return rx_pkt_cnt; }
    inline double numberWritten() override { return 0; }

    /** Receive and process asynchronous message
        @return true if message received or false on timeout or error
    */
    bool recv_async_msg();

    enum err_code {
        ERROR_TIMING = -1,
        ERROR_UNRECOVERABLE = -2,
        ERROR_UNHANDLED = -3,
    };

private:
    uhd::usrp::multi_usrp::sptr usrp_dev;
    uhd::tx_streamer::sptr tx_stream;
    uhd::rx_streamer::sptr rx_stream;
    enum TxWindowType tx_window;
    enum uhd_dev_type dev_type;

    int samples_per_symbol = 0;
    double tx_rate = 0.0;
    double rx_rate = 0.0;

    double tx_gain = 0.0;
    double tx_gain_min = 0.0;
    double tx_gain_max = 0.0;
    double rx_gain = 0.0;
    double rx_gain_min = 0.0;
    double rx_gain_max = 0.0;

    double tx_freq = 0.0;
    double rx_freq = 0.0;
    size_t tx_spp = 0;
    size_t rx_spp = 0;

    bool started = false;
    bool aligned = false;
    bool skip_rx = false;

    size_t rx_pkt_cnt = 0;
    size_t drop_cnt = 0;
    uhd::time_spec_t prev_ts{0, 0};

    TIMESTAMP ts_offset = 0;
    smpl_buf * rx_smpl_buf = nullptr;

    void init_gains();

    void set_ref_clk(ReferenceType ref);

    int set_master_clk(double rate);

    int set_rates(double tx_rate, double rx_rate);

    bool parse_dev_type();

    bool flush_recv(size_t num_pkts);

    int check_rx_md_err(uhd::rx_metadata_t &md, ssize_t num_smpls);

    std::string str_code(uhd::rx_metadata_t metadata);

    std::string str_code(uhd::async_metadata_t metadata);

    Thread async_event_thrd;
};

#endif //OBTS_TRANSCEIVER52M_UHDDEVICE_H
