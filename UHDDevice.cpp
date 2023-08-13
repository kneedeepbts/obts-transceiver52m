/*
 * Device support for Ettus Research UHD driver
 */

#include <cstdint>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"

#include "uhddevice.h"


static double get_dev_offset(enum uhd_dev_type type, int sps) {
    double result = 0.0;

    if (type == USRP1) {
        SPDLOG_ERROR("Invalid device type");
        return result;
    }

    switch (sps) {
        case 1:
            result = uhd_offsets[2 * type + 0].offset;
            break;
        case 4:
            result = uhd_offsets[2 * type + 1].offset;
            break;
        default:
            SPDLOG_ERROR("Unsupported samples-per-symbols: {}", sps);
    }

    return result;
}

/*
 * Select sample rate based on device type and requested samples-per-symbol.
 * The base rate is either GSM symbol rate, 270.833 kHz, or the minimum
 * usable channel spacing of 400 kHz.
 */
static double select_rate(uhd_dev_type type, int sps) {
    double result = -9999.99; // FIXME: Is this the best error value, or should it be -1?

    if ((sps != 4) && (sps != 1)) {
        SPDLOG_ERROR("Unsupported samples-per-symbols (SPS): {}", sps);
        return result;
    }

    switch (type) {
        case USRP2:
        case X3XX:
            result = USRP2_BASE_RT * sps;
            break;
        case B100:
            result = B100_BASE_RT * sps;
            break;
        case B2XX:
        case UMTRX:
            result = GSMRATE * sps;
            break;
        default:
            SPDLOG_ERROR("Unknown device type: {}", type);
            break;
    }

    return result;
}






// FIXME: Where is this used?  It looks like it could be updated to the modern thread library.
void * async_event_loop(uhd_device *dev) {
    while (1) {
        dev->recv_async_msg();
        pthread_testcancel();
    }

    return nullptr;
}


uhd_device::uhd_device(int sps, bool skip_rx) {
    this->samples_per_symbol = sps;
    this->skip_rx = skip_rx;
}

uhd_device::~uhd_device() {
    // FIXME: This should be stopped before the destructor.  How can this be handled better?
    stop();

    delete rx_smpl_buf;
}

void uhd_device::init_gains() {
    uhd::gain_range_t range;

    range = usrp_dev->get_tx_gain_range();
    tx_gain_min = range.start();
    tx_gain_max = range.stop();

    range = usrp_dev->get_rx_gain_range();
    rx_gain_min = range.start();
    rx_gain_max = range.stop();

    usrp_dev->set_tx_gain((tx_gain_min + tx_gain_max) / 2);
    usrp_dev->set_rx_gain((rx_gain_min + rx_gain_max) / 2);

    tx_gain = usrp_dev->get_tx_gain();
    rx_gain = usrp_dev->get_rx_gain();
}

void uhd_device::set_ref_clk(ReferenceType ref) {
    const char * refstr;

    switch (ref) {
        case REF_INTERNAL:
            refstr = "internal";
            break;
        case REF_EXTERNAL:
            refstr = "external";
            break;
        case REF_GPS:
            refstr = "gpsdo";
            break;
        default:
            SPDLOG_ERROR("Invalid reference type");
            return;
    }

    usrp_dev->set_clock_source(refstr);
}

int uhd_device::set_master_clk(double clk_rate) {
    double actual = 1.0;
    double offset = 1.0;
    double limit = 1.0;

    try {
        usrp_dev->set_master_clock_rate(clk_rate);
    } catch (const std::exception &ex) {
        SPDLOG_ERROR("UHD clock rate setting failed: {}", clk_rate);
        SPDLOG_ERROR("{}", ex.what());
        return -1;
    }

    actual = usrp_dev->get_master_clock_rate();
    offset = fabs(clk_rate - actual);

    if (offset > limit) {
        SPDLOG_ERROR("Failed to set master clock rate");
        SPDLOG_ERROR("Requested clock rate: {}", clk_rate);
        SPDLOG_ERROR("Actual clock rate: {}", actual);
        return -1;
    }

    return 0;
}

int uhd_device::set_rates(double tx_rate, double rx_rate) {
    double offset_limit = 1.0;
    double tx_offset = 0.0;
    double rx_offset = 0.0;

    // B2XX is the only device where we set FPGA clocking
    if (dev_type == B2XX) {
        if (set_master_clk(B2XX_CLK_RT) < 0)
            return -1;
    }

    // Set sample rates
    try {
        usrp_dev->set_tx_rate(tx_rate);
        usrp_dev->set_rx_rate(rx_rate);
    } catch (const std::exception &ex) {
        SPDLOG_ERROR("UHD rate setting failed");
        SPDLOG_ERROR(ex.what());
        return -1;
    }
    this->tx_rate = usrp_dev->get_tx_rate();
    this->rx_rate = usrp_dev->get_rx_rate();

    tx_offset = fabs(this->tx_rate - tx_rate);
    rx_offset = fabs(this->rx_rate - rx_rate);
    if ((tx_offset > offset_limit) || (rx_offset > offset_limit)) {
        SPDLOG_ERROR("Actual sample rate differs from desired rate");
        SPDLOG_ERROR("Tx/Rx ({}/{})", this->tx_rate, this->rx_rate);
        return -1;
    }

    return 0;
}

double uhd_device::setTxGain(double db) {
    usrp_dev->set_tx_gain(db);
    tx_gain = usrp_dev->get_tx_gain();

    SPDLOG_INFO("Set TX gain to {} dB", tx_gain);

    return tx_gain;
}

double uhd_device::setRxGain(double db) {
    usrp_dev->set_rx_gain(db);
    rx_gain = usrp_dev->get_rx_gain();

    SPDLOG_INFO("Set RX gain to {} dB", rx_gain);

    return rx_gain;
}

/*
    Parse the UHD device tree and mboard name to find out what device we're
    dealing with. We need the window type so that the transceiver knows how to
    deal with the transport latency. Reject the USRP1 because UHD doesn't
    support timestamped samples with it.
 */
bool uhd_device::parse_dev_type() {
    std::string mboard_str;
    std::string dev_str;
    uhd::property_tree::sptr prop_tree;
    size_t usrp1_str;
    size_t usrp2_str;
    size_t b100_str;
    size_t b200_str;
    size_t b210_str;
    size_t x300_str;
    size_t x310_str;
    size_t umtrx_str;

    prop_tree = usrp_dev->get_device()->get_tree();
    dev_str = prop_tree->access<std::string>("/name").get();
    mboard_str = usrp_dev->get_mboard_name();

    usrp1_str = dev_str.find("USRP1");
    usrp2_str = dev_str.find("USRP2");
    b100_str = mboard_str.find("B100");
    b200_str = mboard_str.find("B200");
    b210_str = mboard_str.find("B210");
    x300_str = mboard_str.find("X300");
    x310_str = mboard_str.find("X310");
    umtrx_str = dev_str.find("UmTRX");

    if (usrp1_str != std::string::npos) {
        SPDLOG_ERROR("USRP1 is not supported using the UHD driver");
        SPDLOG_ERROR("Please compile with GNU Radio libusrp support");
        dev_type = USRP1;
        return false;
    }

    if (b100_str != std::string::npos) {
        tx_window = TX_WINDOW_USRP1;
        dev_type = B100;
    } else if (b200_str != std::string::npos) {
        tx_window = TX_WINDOW_FIXED;
        dev_type = B2XX;
    } else if (b210_str != std::string::npos) {
        tx_window = TX_WINDOW_FIXED;
        dev_type = B2XX;
    } else if (x300_str != std::string::npos) {
        tx_window = TX_WINDOW_FIXED;
        dev_type = X3XX;
    } else if (x310_str != std::string::npos) {
        tx_window = TX_WINDOW_FIXED;
        dev_type = X3XX;
    } else if (usrp2_str != std::string::npos) {
        tx_window = TX_WINDOW_FIXED;
        dev_type = USRP2;
    } else if (umtrx_str != std::string::npos) {
        tx_window = TX_WINDOW_FIXED;
        dev_type = UMTRX;
    } else {
        SPDLOG_ERROR("Unknown UHD device type ", dev_str);
        return false;
    }

    if(tx_window == TX_WINDOW_USRP1) {
        SPDLOG_INFO("Using USRP1 type transmit window for {} {}", dev_str, mboard_str);
    } else if (tx_window == TX_WINDOW_FIXED) {
        SPDLOG_INFO("Using fixed transmit window for {} {}", dev_str, mboard_str);
    }

    return true;
}

int uhd_device::open(const std::string &args, ReferenceType ref) {
    // Find UHD devices
    uhd::device_addr_t addr(args);
    uhd::device_addrs_t dev_addrs = uhd::device::find(addr);
    if (dev_addrs.size() == 0) {
        SPDLOG_ERROR("No UHD devices found with address '{}'", args);
        return -1;
    }

    // Use the first found device
    SPDLOG_INFO("Using discovered UHD device {}", dev_addrs[0].to_string());
    try {
        usrp_dev = uhd::usrp::multi_usrp::make(dev_addrs[0]);
    } catch (...) {
        SPDLOG_ERROR("UHD make failed, device {}", dev_addrs[0].to_string());
        return -1;
    }

    // Check for a valid device type and set bus type
    if (!parse_dev_type())
        return -1;

    set_ref_clk(ref);

    // Create TX and RX streamers
    uhd::stream_args_t stream_args("sc16");
    tx_stream = usrp_dev->get_tx_stream(stream_args);
    rx_stream = usrp_dev->get_rx_stream(stream_args);

    // Number of samples per over-the-wire packet
    tx_spp = tx_stream->get_max_num_samps();
    rx_spp = rx_stream->get_max_num_samps();

    // Set rates
    double _tx_rate = select_rate(dev_type, samples_per_symbol);
    double _rx_rate = _tx_rate / samples_per_symbol;
    if ((_tx_rate > 0.0) && (set_rates(_tx_rate, _rx_rate) < 0))
        return -1;

    // Create receive buffer
    // FIXME: Move this #define value to a more reasonable spot.
    size_t buf_len = SAMPLE_BUF_SZ / sizeof(uint32_t);
    rx_smpl_buf = new smpl_buf(buf_len, rx_rate);

    // Set receive chain sample offset
    double offset = get_dev_offset(dev_type, samples_per_symbol);
    if (offset == 0.0) {
        SPDLOG_ERROR("Unsupported configuration, no correction applied");
        ts_offset = 0;
    } else {
        ts_offset = (TIMESTAMP) (offset * rx_rate);
    }

    // Initialize and shadow gain values
    init_gains();

    // Print configuration
    // FIXME: Make DEBUG?
    SPDLOG_INFO("{}", usrp_dev->get_pp_string());

    int result = NORMAL;
    switch (dev_type) {
        case B100:
            result = RESAMP_64M;
            break;
        case USRP2:
        case X3XX:
            result = RESAMP_100M;
            break;
        case B2XX:
        case UMTRX:
        default:
            result = NORMAL;
            break;
    }
    return result;
}

bool uhd_device::flush_recv(size_t num_pkts) {
    uhd::rx_metadata_t md;
    size_t num_smpls;
    uint32_t buff[rx_spp];
    float timeout;

    // Use .01 sec instead of the default .1 sec
    timeout = .01;

    for (size_t i = 0; i < num_pkts; i++) {
        num_smpls = rx_stream->recv(buff, rx_spp, md, timeout, true);
        if (!num_smpls) {
            switch (md.error_code) {
                case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
                    return true;
                default:
                    continue;
            }
        }
    }

    return true;
}

void uhd_device::restart(uhd::time_spec_t ts) {
    uhd::stream_cmd_t cmd = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
    rx_stream->issue_stream_cmd(cmd);

    flush_recv(50);

    usrp_dev->set_time_now(ts);
    aligned = false;

    cmd = uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS;
    cmd.stream_now = true;
    rx_stream->issue_stream_cmd(cmd);
}

bool uhd_device::start() {
    SPDLOG_INFO("Starting USRP Device");

    if (started) {
        SPDLOG_ERROR("Device already started");
        return false;
    }

    setPriority();

    // Start asynchronous event (underrun check) loop
    async_event_thrd.start((void *(*)(void *)) async_event_loop, (void *) this);

    // Start streaming
    restart(uhd::time_spec_t(0.0));

    // Display usrp time
    double time_now = usrp_dev->get_time_now().get_real_secs();
    SPDLOG_INFO("The current time is {} seconds", time_now);

    started = true;
    return true;
}

bool uhd_device::stop() {
    SPDLOG_INFO("Stoping the USRP Device");
    uhd::stream_cmd_t stream_cmd = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;

    usrp_dev->issue_stream_cmd(stream_cmd);

    started = false;
    return true;
}

void uhd_device::setPriority() {
    uhd::set_thread_priority_safe();
}

int uhd_device::check_rx_md_err(uhd::rx_metadata_t &md, ssize_t num_smpls) {
    uhd::time_spec_t ts;

    if (!num_smpls) {
        SPDLOG_ERROR("{}", str_code(md)); // FIXME: What is this error for?

        switch (md.error_code) {
            case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
                SPDLOG_ERROR("UHD: Receive timed out");
                return ERROR_UNRECOVERABLE;
            case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
            case uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND:
            case uhd::rx_metadata_t::ERROR_CODE_BROKEN_CHAIN:
            case uhd::rx_metadata_t::ERROR_CODE_BAD_PACKET:
            default:
                return ERROR_UNHANDLED;
        }
    }

    // Missing timestamp
    if (!md.has_time_spec) {
        SPDLOG_ERROR("UHD: Received packet missing timestamp");
        return ERROR_UNRECOVERABLE;
    }

    ts = md.time_spec;

    // Monotonicity check
    if (ts < prev_ts) {
        SPDLOG_ERROR("UHD: Loss of monotonic time");
        SPDLOG_ERROR("Current time: {}, Previous time: {}", ts.get_real_secs(), prev_ts.get_real_secs());
        return ERROR_TIMING;
    } else {
        prev_ts = ts;
    }

    return 0;
}

int uhd_device::readSamples(short *buf, int len, bool *overrun, TIMESTAMP timestamp, bool *underrun, unsigned *RSSI) {
    ssize_t rc;
    uhd::time_spec_t ts;
    uhd::rx_metadata_t metadata;
    uint32_t pkt_buf[rx_spp];

    // FIXME: Why would RX be skipped?
    if (skip_rx) {
        return 0;
    }

    *overrun = false;
    *underrun = false;

    // Shift read time with respect to transmit clock
    timestamp += ts_offset;

    ts = uhd::time_spec_t::from_ticks(timestamp, rx_rate);
    SPDLOG_DEBUG("Requested timestamp = {}", ts.get_real_secs());

    // Check that timestamp is valid
    rc = rx_smpl_buf->avail_smpls(timestamp);
    if (rc < 0) {
        SPDLOG_ERROR("{}", rx_smpl_buf->str_code(rc)); // FIXME: What are these errors for?
        SPDLOG_ERROR("{}", rx_smpl_buf->str_status()); // FIXME: What are these errors for?
        return 0;
    }

    // Receive samples from the usrp until we have enough
    while (rx_smpl_buf->avail_smpls(timestamp) < len) {
        size_t num_smpls = rx_stream->recv((void *) pkt_buf, rx_spp, metadata, 0.1, true);
        rx_pkt_cnt++;

        // Check for errors
        rc = check_rx_md_err(metadata, num_smpls);
        switch (rc) {
            case ERROR_UNRECOVERABLE:
                SPDLOG_ERROR("UHD: Version {}", uhd::get_version_string());
                SPDLOG_ERROR("UHD: Unrecoverable error, exiting...");
                exit(-1); // FIXME: Why is there such a hard bailout here?
            case ERROR_TIMING:
                restart(prev_ts);
            case ERROR_UNHANDLED:
                continue;
        }


        ts = metadata.time_spec;
        SPDLOG_DEBUG("Received timestamp = {}", ts.get_real_secs());
        rc = rx_smpl_buf->write(pkt_buf, num_smpls, metadata.time_spec);
        SPDLOG_DEBUG("After smpl_buf write");

        // Continue on local overrun, exit on other errors
        if ((rc < 0)) {
            SPDLOG_ERROR("{}", rx_smpl_buf->str_code(rc)); // FIXME: What are these errors for?
            SPDLOG_ERROR("{}", rx_smpl_buf->str_status()); // FIXME: What are these errors for?
            if (rc != smpl_buf::ERROR_OVERFLOW)
                return 0;
        }
    }

    SPDLOG_DEBUG("After while loop");

    // We have enough samples
    rc = rx_smpl_buf->read(buf, len, timestamp);
    SPDLOG_DEBUG("After smpl_buf read");

    if ((rc < 0) || (rc != len)) {
        SPDLOG_ERROR("{}", rx_smpl_buf->str_code(rc)); // FIXME: What are these errors for?
        SPDLOG_ERROR("{}", rx_smpl_buf->str_status()); // FIXME: What are these errors for?
        return 0;
    }

    SPDLOG_DEBUG("return len: {}", len);
    return len;
}

int uhd_device::writeSamples(short *buf, int len, bool *underrun, TIMESTAMP timestamp, bool isControl) {
    SPDLOG_DEBUG("writeSamaples");
    uhd::tx_metadata_t metadata;
    metadata.has_time_spec = true;
    metadata.start_of_burst = false;
    metadata.end_of_burst = false;
    SPDLOG_DEBUG("Just before time_spec");
    metadata.time_spec = uhd::time_spec_t::from_ticks(timestamp, tx_rate);

    *underrun = false;

    // No control packets
    if (isControl) {
        SPDLOG_ERROR("Control packets not supported");
        return 0;
    }

    // Drop a fixed number of packets (magic value)
    SPDLOG_DEBUG("aligned: {}, drop_cnt: {}", aligned, drop_cnt);
    if (!aligned) {
        drop_cnt++;

        if (drop_cnt == 1) {
            SPDLOG_DEBUG("Aligning transmitter: stop burst");
            *underrun = true;
            metadata.end_of_burst = true;
        } else if (drop_cnt < 30) {
            SPDLOG_DEBUG("Aligning transmitter: packet advance");
            return len;
        } else {
            SPDLOG_DEBUG("Aligning transmitter: start burst");
            metadata.start_of_burst = true;
            aligned = true;
            drop_cnt = 0;
        }
    }

    SPDLOG_DEBUG("about to tx_stream->send");
    size_t num_smpls = tx_stream->send(buf, len, metadata);
    SPDLOG_DEBUG("num_smpls: {}", num_smpls);

    if (num_smpls != (unsigned) len) {
        SPDLOG_ERROR("UHD: Device send timed out");
        SPDLOG_ERROR("UHD: Version {}", uhd::get_version_string());
        SPDLOG_ERROR("UHD: Unrecoverable error, exiting...");
        exit(-1); // FIXME: Why is there such a hard bailout here?
    }

    return num_smpls;
}

bool uhd_device::updateAlignment(TIMESTAMP timestamp) {
    return true;
}

bool uhd_device::setTxFreq(double wFreq) {
    uhd::tune_result_t tr = usrp_dev->set_tx_freq(wFreq);
    SPDLOG_INFO("{}", tr.to_pp_string());
    tx_freq = usrp_dev->get_tx_freq();

    return true;
}

bool uhd_device::setRxFreq(double wFreq) {
    uhd::tune_result_t tr = usrp_dev->set_rx_freq(wFreq);
    SPDLOG_INFO("{}", tr.to_pp_string());
    rx_freq = usrp_dev->get_rx_freq();

    return true;
}

bool uhd_device::recv_async_msg() {
    uhd::async_metadata_t md;
    if (!tx_stream->recv_async_msg(md))
        return false;

    // Assume that any error requires resynchronization
    if (md.event_code != uhd::async_metadata_t::EVENT_CODE_BURST_ACK) {
        aligned = false;

        if ((md.event_code != uhd::async_metadata_t::EVENT_CODE_UNDERFLOW) &&
            (md.event_code != uhd::async_metadata_t::EVENT_CODE_TIME_ERROR)) {
            SPDLOG_ERROR("{}", str_code(md)); // FIXME: What are these errors for?
        }
    }

    return true;
}

// FIXME: ostringstream seems heavy for an error code decoder.
std::string uhd_device::str_code(uhd::rx_metadata_t metadata) {
    std::ostringstream ost("UHD: ");

    switch (metadata.error_code) {
        case uhd::rx_metadata_t::ERROR_CODE_NONE:
            ost << "No error";
            break;
        case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
            ost << "No packet received, implementation timed-out";
            break;
        case uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND:
            ost << "A stream command was issued in the past";
            break;
        case uhd::rx_metadata_t::ERROR_CODE_BROKEN_CHAIN:
            ost << "Expected another stream command";
            break;
        case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
            ost << "An internal receive buffer has filled";
            break;
        case uhd::rx_metadata_t::ERROR_CODE_BAD_PACKET:
            ost << "The packet could not be parsed";
            break;
        default:
            ost << "Unknown error " << metadata.error_code;
    }

    if (metadata.has_time_spec)
        ost << " at " << metadata.time_spec.get_real_secs() << " sec.";

    return ost.str();
}

// FIXME: ostringstream seems heavy for an error code decoder.
std::string uhd_device::str_code(uhd::async_metadata_t metadata) {
    std::ostringstream ost("UHD: ");

    switch (metadata.event_code) {
        case uhd::async_metadata_t::EVENT_CODE_BURST_ACK:
            ost << "A packet was successfully transmitted";
            break;
        case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW:
            ost << "An internal send buffer has emptied";
            break;
        case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR:
            ost << "Packet loss between host and device";
            break;
        case uhd::async_metadata_t::EVENT_CODE_TIME_ERROR:
            ost << "Packet time was too late or too early";
            break;
        case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW_IN_PACKET:
            ost << "Underflow occurred inside a packet";
            break;
        case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR_IN_BURST:
            ost << "Packet loss within a burst";
            break;
        default:
            ost << "Unknown error " << metadata.event_code;
    }

    if (metadata.has_time_spec)
        ost << " at " << metadata.time_spec.get_real_secs() << " sec.";

    return ost.str();
}


RadioDevice * RadioDevice::make(int sps, bool skip_rx) {
    return new uhd_device(sps, skip_rx);
}
