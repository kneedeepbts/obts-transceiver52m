#ifndef OBTS_TRANSCEIVER52M_RADIOINTERFACE_H
#define OBTS_TRANSCEIVER52M_RADIOINTERFACE_H


#include "sigProcLib.h"
#include "GSMCommon.h"
#include "LinkedLists.h"
#include "radioDevice.h"
#include "radioVector.h"
#include "radioClock.h"


/** class to interface the transceiver with the USRP */
class RadioInterface {
public:
    /** start the interface */
    void start();

    /** intialization */
    virtual bool init(int type);

    virtual void close();

    /** constructor */
    // FIXME: Should GSM::Time be used here?  Or move to std::chrono?
    explicit RadioInterface(RadioDevice * radio = nullptr, int recv_offset = 3, int sps = 4, GSM::Time start_time = GSM::Time(0));

    /** destructor */
    virtual ~RadioInterface();

    /** check for underrun, resets underrun value */
    bool isUnderrun();

    /** attach an existing USRP to this interface */
    void attach(RadioDevice * wRadio, int wRadioOversampling);

    /** return the receive FIFO */
    VectorFIFO * receiveFIFO() { return &m_receive_fifo; }

    /** return the basestation clock */
    RadioClock * getClock(void) { return &m_clock; };

    /** set transmit frequency */
    bool tuneTx(double freq);

    /** set receive frequency */
    bool tuneRx(double freq);

    /** set receive gain */
    double setRxGain(double dB);

    /** get receive gain */
    double getRxGain(void);

    /** drive transmission of GSM bursts */
    void driveTransmitRadio(signalVector &radioBurst, bool zeroBurst);

    /** drive reception of GSM bursts */
    void driveReceiveRadio();

    void setPowerAttenuation(double atten);

    /** returns the full-scale transmit amplitude **/
    double fullScaleInputValue();

    /** returns the full-scale receive amplitude **/
    double fullScaleOutputValue();

    /** set thread priority on current thread */
    void setPriority() { m_radio->setPriority(); }

    /** get transport window type of attached device */
    enum RadioDevice::TxWindowType getWindowType() { return m_radio->getWindowType(); }

protected:
    Thread m_thread; // thread that synchronizes transmit and receive sections
    VectorFIFO m_receive_fifo; // FIFO that holds receive  bursts
    RadioDevice * m_radio; // the USRP object

    int m_sps_tx = 1;
    int m_sps_rx = 1;
    signalVector * m_send_buffer = nullptr;
    signalVector * m_recv_buffer = nullptr;
    unsigned m_send_cursor = 0;
    unsigned m_recv_cursor = 0;

    short * m_convert_send_buffer = nullptr;
    short * m_convert_recv_buffer = nullptr;

    bool m_underrun = false; // indicates writes to USRP are too slow
    bool m_overrun = false; // indicates reads from USRP are too slow
    TIMESTAMP m_write_timestamp = -1; // sample timestamp of next packet written to USRP
    TIMESTAMP m_read_timestamp = -1; // sample timestamp of next packet read from USRP

    RadioClock m_clock; // the basestation clock!

    int m_receive_offset; // offset b/w transmit and receive GSM timestamps, in timeslots
    bool m_radio_on = false; // indicates radio is on
    double m_power_scaling = 1.0;

    bool m_load_test = false;
    int m_num_arfcns = 0;
    signalVector * m_final_vec = nullptr;
    signalVector * m_final_vec9 = nullptr;

private:

    /** format samples to USRP */
    int radioifyVector(signalVector &wVector,
                       float * floatVector,
                       bool zero);

    /** format samples from USRP */
    int unRadioifyVector(float * floatVector, signalVector &wVector);

    /** push GSM bursts into the transmit buffer */
    virtual void pushBuffer(void);

    /** pull GSM bursts from the receive buffer */
    virtual void pullBuffer(void);

#if USRP1
    protected:

      /** drive synchronization of Tx/Rx of USRP */
      void alignRadio();

      friend void *AlignRadioServiceLoopAdapter(RadioInterface*);
#endif
};

#if USRP1
/** synchronization thread loop */
void *AlignRadioServiceLoopAdapter(RadioInterface*);
#endif

class RadioInterfaceResamp : public RadioInterface {
public:
    explicit RadioInterfaceResamp(RadioDevice * radio = nullptr, int recv_offset = 3, int sps = 4, GSM::Time start_time = GSM::Time(0));

    ~RadioInterfaceResamp() override;

    bool init(int type);

    void close();

private:
    signalVector * m_inner_send_buffer = nullptr;
    signalVector * m_outer_send_buffer = nullptr;
    signalVector * m_inner_recv_buffer = nullptr;
    signalVector * m_outer_recv_buffer = nullptr;

    void pushBuffer();

    void pullBuffer();
};

#endif //OBTS_TRANSCEIVER52M_RADIOINTERFACE_H
