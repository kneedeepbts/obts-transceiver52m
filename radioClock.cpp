#include "radioClock.h"

void RadioClock::set(const GsmTime& time)
{
	mLock.lock();
	m_clock = time;
	updateSignal.signal();
	mLock.unlock();
}

void RadioClock::incTN()
{
	mLock.lock();
	m_clock.incTN();
	updateSignal.signal();
	mLock.unlock();
}

GsmTime RadioClock::get()
{
	mLock.lock();
	GsmTime retVal = m_clock;
	mLock.unlock();
	return retVal;
}

void RadioClock::wait()
{
	mLock.lock();
	updateSignal.wait(mLock,1);
	mLock.unlock();
}
