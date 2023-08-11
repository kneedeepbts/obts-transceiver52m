#ifndef OBTS_TRANSCEIVER52M_SIGPROCLIB_H
#define OBTS_TRANSCEIVER52M_SIGPROCLIB_H

#include <complex>

#include "Vector.h"
#include "BitVector.h"
//#include "Complex.h"

//#include "GSMTransfer.h"


//using namespace GSM;

/** Indicated signalVector symmetry */
enum Symmetry {
    NONE = 0,
    ABSSYM = 1
};

/** Convolution type indicator */
enum ConvType {
    START_ONLY,
    NO_DELAY,
    CUSTOM,
    UNDEFINED,
};

enum signalError {
    SIGERR_NONE,
    SIGERR_BOUNDS,
    SIGERR_CLIP,
    SIGERR_UNSUPPORTED,
    SIGERR_INTERNAL,
};

/** the core data structure of the Transceiver */
class signalVector : public Vector<std::complex<float>> {
public:
    /** Constructors */
    signalVector(int dSize = 0, Symmetry wSymmetry = NONE) : Vector<std::complex<float>>(dSize) {
        m_symmetry = wSymmetry;
    };

    signalVector(std::complex<float> * wData, size_t start, size_t span, Symmetry wSymmetry = NONE) : Vector<std::complex<float>>(nullptr, wData + start, wData + start + span) {
        m_symmetry = wSymmetry;
    };

    signalVector(const signalVector &vec1, const signalVector &vec2) : Vector<std::complex<float>>(vec1, vec2) {
        m_symmetry = vec1.m_symmetry;
    };

    signalVector(const signalVector &wVector) : Vector<std::complex<float>>(wVector.size()) {
        wVector.copyTo(*this);
        m_symmetry = wVector.getSymmetry();
    };

    signalVector(size_t size, size_t start) : Vector<std::complex<float>>(size + start) {
        mStart = mData + start;
        m_symmetry = NONE;
    };

    signalVector(const signalVector &wVector, size_t start, size_t tail = 0) : Vector<std::complex<float>>(start + wVector.size() + tail) {
        mStart = mData + start;
        wVector.copyTo(*this);
        memset(mData, 0, start * sizeof(std::complex<float>));
        memset(mStart + wVector.size(), 0, tail * sizeof(std::complex<float>));
        m_symmetry = NONE;
    };

    /** symmetry operators */
    Symmetry getSymmetry() const { return m_symmetry; };
    void setSymmetry(Symmetry sym) { m_symmetry = sym; };

    /** real-valued operators */
    bool isRealOnly() const { return m_realOnly; };
    void isRealOnly(bool real) { m_realOnly = real; };

    /** alignment markers */
    bool isAligned() const { return m_aligned; };
    void setAligned(bool aligned) { this->m_aligned = aligned; };

private:
    Symmetry m_symmetry; // the symmetry of the vector
    bool m_realOnly = false; // true if vector is real-valued, not complex-valued
    bool m_aligned = false;
};

/** Convert a linear number to a dB value */
float dB(float x);

/** Convert a dB value into a linear value */
float dBinv(float x);

/** Compute the energy of a vector */
float vectorNorm2(const signalVector &x);

/** Compute the average power of a vector */
float vectorPower(const signalVector &x);

/** Setup the signal processing library */
bool sigProcLibSetup(int sps);

/** Destroy the signal processing library */
void sigProcLibDestroy();

/** 
 	Convolve two vectors. 
	@param a,b The vectors to be convolved.
	@param c, A preallocated vector to hold the convolution result.
	@param spanType The type/span of the convolution.
	@return The convolution result or NULL on error.
*/
signalVector * convolve(const signalVector * a, const signalVector * b, signalVector * c, ConvType spanType,
                        int start = 0, unsigned len = 0, unsigned step = 1, int offset = 0);

/** 
        Frequency shift a vector.
	@param y The frequency shifted vector.
	@param x The vector to-be-shifted.
	@param freq The digital frequency shift
	@param startPhase The starting phase of the oscillator 
	@param finalPhase The final phase of the oscillator
	@return The frequency shifted vector.
*/
signalVector * frequencyShift(signalVector * y, signalVector * x, float freq = 0.0,
                              float startPhase = 0.0, float * finalPhase = nullptr);

/** 
        Correlate two vectors. 
        @param a,b The vectors to be correlated.
        @param c, A preallocated vector to hold the correlation result.
        @param spanType The type/span of the correlation.
        @return The correlation result.
*/
signalVector * correlate(signalVector * a, signalVector * b, signalVector * c, ConvType spanType,
                         bool bReversedConjugated = false, unsigned startIx = 0, unsigned len = 0);

/** Operate soft slicer on real-valued portion of vector */
bool vectorSlicer(signalVector * x);

/** GMSK modulate a GSM burst of bits */
signalVector * modulateBurst(const BitVector &wBurst, int guardPeriodLength, int sps, bool emptyPulse = false);

/** Sinc function */
float sinc(float x);

/** Delay a vector */
bool delayVector(signalVector &wBurst, float delay);

/** Add two vectors in-place */
bool addVector(signalVector &x, signalVector &y);

/** Multiply two vectors in-place*/
bool multVector(signalVector &x, signalVector &y);

/** Generate a vector of gaussian noise */
signalVector * gaussianNoise(int length, float variance = 1.0, std::complex<float> mean = std::complex<float>(0.0));

/**
	Given a non-integer index, interpolate a sample.
	@param inSig The signal from which to interpolate.
	@param ix The index.
	@return The interpolated signal value.
*/
std::complex<float> interpolatePoint(const signalVector &inSig, float ix);

/**
	Given a correlator output, locate the correlation peak.
	@param rxBurst The correlator result.
	@param peakIndex Pointer to value to receive interpolated peak index.
	@param avgPower Power to value to receive mean power.
	@return Peak value.
*/
std::complex<float> peakDetect(const signalVector &rxBurst, float * peakIndex, float * avgPwr);

/**
        Apply a scalar to a vector.
        @param x The vector of interest.
        @param scale The scalar.
*/
void scaleVector(signalVector &x, std::complex<float> scale);

/**      
        Add a constant offset to a vecotr.
        @param x The vector of interest.
        @param offset The offset.
*/
void offsetVector(signalVector &x, std::complex<float> offset);

/**
        Generate a modulated GSM midamble, stored within the library.
        @param gsmPulse The GSM pulse used for modulation.
        @param sps The number of samples per GSM symbol.
        @param TSC The training sequence [0..7]
        @return Success.
*/
bool generateMidamble(int sps, int tsc);

/**
        Generate a modulated RACH sequence, stored within the library.
        @param gsmPulse The GSM pulse used for modulation.
        @param sps The number of samples per GSM symbol.
        @return Success.
*/
bool generateRACHSequence(int sps);

/**
        Energy detector, checks to see if received burst energy is above a threshold.
        @param rxBurst The received GSM burst of interest.
        @param windowLength The number of burst samples used to compute burst energy
        @param detectThreshold The detection threshold, a linear value.
        @param avgPwr The average power of the received burst.
        @return True if burst energy is above threshold.
*/
bool energyDetect(signalVector &rxBurst, unsigned windowLength, float detectThreshold, float * avgPwr = nullptr);

/**
        RACH correlator/detector.
        @param rxBurst The received GSM burst of interest.
        @param detectThreshold The threshold that the received burst's post-correlator SNR is compared against to determine validity.
        @param sps The number of samples per GSM symbol.
        @param amplitude The estimated amplitude of received RACH burst.
        @param TOA The estimate time-of-arrival of received RACH burst.
        @return positive if threshold value is reached, negative on error, zero otherwise
*/
int detectRACHBurst(signalVector &rxBurst, float detectThreshold, int sps, std::complex<float> * amplitude, float * TOA);

/**
        Normal burst correlator, detector, channel estimator.
        @param rxBurst The received GSM burst of interest.
 
        @param detectThreshold The threshold that the received burst's post-correlator SNR is compared against to determine validity.
        @param sps The number of samples per GSM symbol.
        @param amplitude The estimated amplitude of received TSC burst.
        @param TOA The estimate time-of-arrival of received TSC burst.
        @param maxTOA The maximum expected time-of-arrival
        @param requestChannel Set to true if channel estimation is desired.
        @param channelResponse The estimated channel.
        @param channelResponseOffset The time offset b/w the first sample of the channel response and the reported TOA.
        @return positive if threshold value is reached, negative on error, zero otherwise
*/
int analyzeTrafficBurst(signalVector &rxBurst, unsigned TSC, float detectThreshold, int sps,
                        std::complex<float> * amplitude, float * TOA, unsigned maxTOA, bool requestChannel = false,
                        signalVector ** channelResponse = nullptr, float * channelResponseOffset = nullptr);

/**
	Decimate a vector.
        @param wVector The vector of interest.
        @param decimationFactor The amount of decimation, i.e. the decimation factor.
        @return The decimated signal vector.
*/
signalVector * decimateVector(signalVector &wVector, int decimationFactor);

/**
        Demodulates a received burst using a soft-slicer.
	@param rxBurst The burst to be demodulated.
        @param gsmPulse The GSM pulse.
        @param sps The number of samples per GSM symbol.
        @param channel The amplitude estimate of the received burst.
        @param TOA The time-of-arrival of the received burst.
        @return The demodulated bit sequence.
*/
SoftVector * demodulateBurst(signalVector &rxBurst, int sps, std::complex<float> channel, float TOA);

/**
	Design the necessary filters for a decision-feedback equalizer.
	@param channelResponse The multipath channel that we're mitigating.
	@param SNRestimate The signal-to-noise estimate of the channel, a linear value
	@param Nf The number of taps in the feedforward filter.
	@param feedForwardFilter The designed feed forward filter.
	@param feedbackFilter The designed feedback filter.
	@return True if DFE can be designed.
*/
bool designDFE(signalVector &channelResponse, float SNRestimate, int Nf, signalVector ** feedForwardFilter, signalVector ** feedbackFilter);

/**
	Equalize/demodulate a received burst via a decision-feedback equalizer.
	@param rxBurst The received burst to be demodulated.
	@param TOA The time-of-arrival of the received burst.
	@param sps The number of samples per GSM symbol.
	@param w The feed forward filter of the DFE.
	@param b The feedback filter of the DFE.
	@return The demodulated bit sequence.
*/
SoftVector * equalizeBurst(signalVector &rxBurst, float TOA, int sps, signalVector &w, signalVector &b);

#endif //OBTS_TRANSCEIVER52M_SIGPROCLIB_H
