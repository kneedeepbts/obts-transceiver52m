#include "radioVector.h"

radioVector::radioVector(const signalVector &wVector, GsmTime &wTime)
        : signalVector(wVector), mTime(wTime) {
}

GsmTime radioVector::getTime() const {
    return mTime;
}

void radioVector::setTime(const GsmTime& wTime)
{
	mTime = wTime;
}

bool radioVector::operator>(const radioVector &other) const {
    return mTime > other.mTime;
}

noiseVector::noiseVector(size_t n) {
    this->resize(n);
    it = this->begin();
}

float noiseVector::avg() {
    float val = 0.0;

    for (int i = 0; i < size(); i++)
        val += (*this)[i];

    return val / (float) size();
}

bool noiseVector::insert(float val) {
    if (empty())
        return false;

    if (it == this->end())
        it = this->begin();

    *it++ = val;

    return true;
}

unsigned VectorFIFO::size() {
    return mQ.size();
}

void VectorFIFO::put(radioVector * ptr) {
    mQ.put((void *) ptr);
}

radioVector * VectorFIFO::get() {
    return (radioVector *) mQ.get();
}

GsmTime VectorQueue::nextTime() const
{
	//GsmTime retVal = GsmTime();
	mLock.lock();

	while (mQ.empty())
		mWriteSignal.wait(mLock);

	GsmTime retVal = mQ.top()->getTime();
	mLock.unlock();

	return retVal;
}

radioVector * VectorQueue::getStaleBurst(const GsmTime &targTime) {
    mLock.lock();
    if ((mQ.empty())) {
        mLock.unlock();
        return nullptr;
    }

    if (mQ.top()->getTime() < targTime) {
        radioVector * retVal = mQ.top();
        mQ.pop();
        mLock.unlock();
        return retVal;
    }
    mLock.unlock();

    return nullptr;
}

radioVector * VectorQueue::getCurrentBurst(const GsmTime &targTime) {
    mLock.lock();
    if ((mQ.empty())) {
        mLock.unlock();
        return nullptr;
    }

    if (mQ.top()->getTime() == targTime) {
        radioVector * retVal = mQ.top();
        mQ.pop();
        mLock.unlock();
        return retVal;
    }
    mLock.unlock();

    return nullptr;
}
