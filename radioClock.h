#ifndef OBTS_TRANSCEIVER52M_RADIOCLOCK_H
#define OBTS_TRANSCEIVER52M_RADIOCLOCK_H

#include "gsmtime.h"
#include "Threads.h" // FIXME: For Mutex and Signal.  Should make these into separate files.

class RadioClock {
public:
	void set(const GsmTime& time);
	void incTN();
	GsmTime get();
	void wait();

private:
	GsmTime m_clock;
	Mutex mLock;
	Signal updateSignal;
};

#endif //OBTS_TRANSCEIVER52M_RADIOCLOCK_H
