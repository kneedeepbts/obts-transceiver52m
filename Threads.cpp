
#include <cerrno>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"

#include "Threads.h"
#include "Timeval.h"
//#include "Logger.h"


// FIXME: Moving from utils.cpp to help break dependencies
#include <execinfo.h>
static std::string backtrace_failed = "backtrace failed";
std::string rn_backtrace2()
{
    void *buffer[30];
    int nptrs = backtrace(buffer,30);
    if (nptrs <= 0) { return backtrace_failed; }
    char **strings = backtrace_symbols(buffer,nptrs);
    if (strings == NULL) { return backtrace_failed; }
    std::string result;
    try {
        result = "backtrace:";
        for (int j = 0; j < nptrs; j++) {
            result = result + " " + strings[j];
        }
    } catch(...) {
        return backtrace_failed;
    }
    free(strings);
    return result;
}

std::string format(const char *fmt, ...) {
    va_list ap;
    char buf[200];
    va_start(ap, fmt);
    int n = vsnprintf(buf, 199, fmt, ap);
    va_end(ap);
    std::string result;
    if (n <= 199) {
        result = std::string(buf);
    } else {
        if (n > 5000) {
            SPDLOG_ERROR("oversized string in format");
            n = 5000;
        }
        // We could use vasprintf but we already computed the length...
        // We are not using alloca because it might overflow the small stacks used for our threads.
        char * buffer = (char *) malloc(n + 2);    // add 1 extra superstitiously.
        va_start(ap, fmt);
        vsnprintf(buffer, n + 1, fmt, ap);
        va_end(ap);
        //if (n >= (2000-4)) { strcpy(&buf[(2000-4)],"..."); }
        result = std::string(buffer);
        free(buffer);
    }
    return result;
}






using namespace std;

//int gMutexLogLevel = LOG_INFO;	// The mutexes cannot call gConfig or gGetLoggingLevel so we have to get the log level indirectly.

#if !defined(gettid)
# define gettid() syscall(SYS_gettid)
#endif // !defined(gettid)

//#define LOCKLOG(level,fmt,...) \
//	if (gMutexLogLevel >= LOG_##level) syslog(LOG_##level,"%lu %s %s:%u:%s:lockid=%p " fmt,gettid(),Utils::timestr().c_str(),__FILE__,__LINE__,__FUNCTION__,this,##__VA_ARGS__);
//	//if (gMutexLogLevel >= LOG_##level) syslog(LOG_##level,"%lu %s %s:%u:%s:lockid=%p " fmt,(unsigned long)pthread_self(),Utils::timestr().c_str(),__FILE__,__LINE__,__FUNCTION__,this,##__VA_ARGS__);
//	//printf("%u %s %s:%u:%s:lockid=%u " fmt "\n",(unsigned)pthread_self(),Utils::timestr().c_str(),__FILE__,__LINE__,__FUNCTION__,(unsigned)this,##__VA_ARGS__);




Mutex gStreamLock;		///< Global lock to control access to cout and cerr.

void lockCout()
{
	gStreamLock.lock();
	Timeval entryTime;
	cout << entryTime << " " << pthread_self() << ": ";
}


void unlockCout()
{
	cout << dec << endl << flush;
	gStreamLock.unlock();
}


void lockCerr()
{
	gStreamLock.lock();
	Timeval entryTime;
	cerr << entryTime << " " << pthread_self() << ": ";
}

void unlockCerr()
{
	cerr << dec << endl << flush;
	gStreamLock.unlock();
}







Mutex::Mutex() : mLockCnt(0) //, mLockerFile(0), mLockerLine(0)
{
	memset(mLockerFile,0,sizeof(mLockerFile));
	// Must use getLoggingLevel, not gGetLoggingLevel, to avoid infinite recursion.
	bool res;
	res = pthread_mutexattr_init(&mAttribs);
	assert(!res);
	res = pthread_mutexattr_settype(&mAttribs,PTHREAD_MUTEX_RECURSIVE);
	assert(!res);
	res = pthread_mutex_init(&mMutex,&mAttribs);
	assert(!res);
}


Mutex::~Mutex()
{
	pthread_mutex_destroy(&mMutex);
	bool res = pthread_mutexattr_destroy(&mAttribs);
	assert(!res);
}

bool Mutex::trylock(const char *file, unsigned line)
{
	if (pthread_mutex_trylock(&mMutex)==0) {
		if (mLockCnt >= 0 && mLockCnt < maxLocks) {
			mLockerFile[mLockCnt] = file; mLockerLine[mLockCnt] = line;		// Now our thread has it locked from here.
		}
		mLockCnt++;
		return true;
	} else {
		return false;
	}
}

// Returns true if the lock was acquired within the timeout, or false if it timed out.
bool Mutex::timedlock(int msecs) // Wait this long in milli-seconds.
{
	Timeval future(msecs);
	struct timespec timeout = future.timespec();
	return ETIMEDOUT != pthread_mutex_timedlock(&mMutex, &timeout);
}

// There is a chance here that the mLockerFile&mLockerLine
// could change while we are printing it if multiple other threads are contending for the lock
// and swapping the lock around while we are in here.
string Mutex::mutext() const
{
	string result;
	result.reserve(100);
	//result += format("lockid=%u lockcnt=%d",(unsigned)this,mLockCnt);
	result += format("lockcnt=%d",mLockCnt);
	for (int i = 0; i < mLockCnt && i < maxLocks; i++) {
		if (mLockerFile[i]) {
			result += format(" %s:%u",mLockerFile[i],mLockerLine[i]);
		} else {
			result += " ?";
		}
	}
	return result;
}

// Pat removed 10-1-2014.
//void Mutex::lock() {
//	if (lockerFile()) LOCKLOG(DEBUG,"lock unchecked");
//	_lock();
//	mLockCnt++;
//}

// WARNING:  The LOG facility calls lock, so to avoid infinite recursion do not call LOG if file == NULL,
// and the file argument should never be used from the Logger facility.
void Mutex::lock(const char *file, unsigned line)
{
	// (pat 10-25-13) Deadlock reporting is now the default behavior so we can detect and report deadlocks at customer sites.
	if (file) {
		//LOCKLOG(DEBUG,"start at %s %u",file,line);
        SPDLOG_DEBUG("start at {} {}", file, line);
		// If we wait more than a second, print an error message.
		if (!timedlock(1000)) {
			string backtrace = rn_backtrace2();
			//LOCKLOG(ERR, "Blocked more than one second at %s %u by %s %s",file,line,mutext().c_str(),backtrace.c_str());
            SPDLOG_ERROR("Blocked more than one second at {} {} by {} {}", file, line, mutext().c_str(), backtrace.c_str());
			//printf("WARNING: %s Blocked more than one second at %s %u by %s %s\n",timestr(4).c_str(),file,line,mutext().c_str(),backtrace.c_str());
			_lock();					// If timedlock failed we are probably now entering deadlock.
		}
	} else {
		//LOCKLOG(DEBUG,"unchecked lock");
		_lock();
	}
	if (mLockCnt >= 0 && mLockCnt < maxLocks) {
		mLockerFile[mLockCnt] = file; mLockerLine[mLockCnt] = line;		// Now our thread has it locked from here.
	}
	mLockCnt++;
	if (file) {
        //LOCKLOG(DEBUG,"lock by %s",mutext().c_str());
        SPDLOG_DEBUG("lock by {}", mutext().c_str());
    }
	//else { LOCKLOG(DEBUG,"lock no file"); }
}

void Mutex::unlock()
{
	if (lockerFile()) {
        //LOCKLOG(DEBUG,"unlock at %s",mutext().c_str());
        SPDLOG_DEBUG("unlock at {}", mutext().c_str());
    }
	//else { LOCKLOG(DEBUG,"unlock unchecked"); }
	mLockCnt--;
	pthread_mutex_unlock(&mMutex);
}

RWLock::RWLock()
{
	bool res;
	res = pthread_rwlockattr_init(&mAttribs);
	assert(!res);
	res = pthread_rwlock_init(&mRWLock,&mAttribs);
	assert(!res);
}


RWLock::~RWLock()
{
	pthread_rwlock_destroy(&mRWLock);
	bool res = pthread_rwlockattr_destroy(&mAttribs);
	assert(!res);
}

void ScopedLockMultiple::_init(int wOwner, Mutex& wA, Mutex&wB, Mutex&wC) {
	ownA[0] = wOwner & 1; ownA[1] = wOwner & 2; ownA[2] = wOwner & 4;
	mA[0] = &wA; mA[1] = &wB; mA[2] = &wC;
	_saveState();
}
void ScopedLockMultiple::_lock(int which) {
	if (state[which]) return;
	mA[which]->lock(_file,_line);
	state[which] = true;
}
bool ScopedLockMultiple::_trylock(int which) {
	if (state[which]) return true;
	state[which] = mA[which]->trylock(_file,_line);
	return state[which];
}
void ScopedLockMultiple::_unlock(int which) {
	if (state[which]) mA[which]->unlock();
	state[which] = false;
}
void ScopedLockMultiple::_saveState() {
	// The caller may enter with mutex locked by the calling thread if the owner bit is set.
	for (int i = 0; i <= 2; i++) {
		// Test is deceptive because currently the owner bit is an assertion that owner has the bit locked.
		// If we dont require that, then how would we know whether the lock was held by the current thread, or by some other thread?
		// We would need to add some per-thread storage, and store it in the Mutex during Mutex::lock() or Mutex::trylock().
		state[i] = ownA[i];	// (ownA[i] && mA[i]->lockcnt());
		if (ownA[i]) { if (mA[i]->lockcnt() != 1) printf("mA[%d].lockcnt=%d\n",i,mA[i]->lockcnt()); assert(mA[i]->lockcnt() == 1); }
	}
}
void ScopedLockMultiple::_restoreState() {
	// Leave state of each lock the way we found it.
	for (int i = 0; i <= 2; i++) {
		// This is a little redundant because the _lock and _unlock now test state.
		if (!ownA[i] && state[i]) _unlock(i);
		else if (ownA[i] && ! state[i]) _lock(i);
	}
}
void ScopedLockMultiple::_lockAll() {
	// Do not return until we have locked all three mutexes.
	for (int n=0; true; n++) {
		// Attempt to lock in order 0,1,2.
		_lock(0); 						// Wait on 0, unless it was locked by us on entry.
		if (_trylock(1) && _trylock(2)) return;			// Then try to acquire 1 and 2
		_unlock(1);										// If failure, release all.
		_unlock(0);
		// Attempt to lock in order 1,0,2.
		_lock(1);
		if (_trylock(0) && _trylock(2)) return;
		_unlock(0);
		_unlock(1);
		// Attempt to lock in order 2,1,0.
		_lock(2);
		if (_trylock(0) && _trylock(1)) return;
		_unlock(0);
		_unlock(2);	
		//LOCKLOG(DEBUG,"Multiple lock attempt %d",n);	// Seeing this message is a hint we are having contention issues.
        SPDLOG_DEBUG("Multiple lock attempt {}", n);
	}
}


/** Block for the signal up to the cancellation timeout in msecs. */
// (pat 8-2013) Our code had places (InterthreadQueue) that passed in negative timeouts which create deadlock.
// To prevent that, use signed, not unsigned timeout.
void Signal::wait(Mutex& wMutex, long timeout) const
{
	if (timeout <= 0) { return; }	// (pat) Timeout passed already
	Timeval then(timeout);
	struct timespec waitTime = then.timespec();
	pthread_cond_timedwait(&mSignal,&wMutex.mMutex,&waitTime);
}

struct wrapArgs
{
        void *(*task)(void *);
        void *arg;
};

static void *
thread_main(void *arg)
{
    struct wrapArgs *p = (struct wrapArgs *)arg;
    void *(*task)(void *) = p->task;
    void *param = p->arg;
    delete p;
    return (*task)(param);
}

void Thread::start(void *(*task)(void*), void *arg)
{
	assert(mThread==((pthread_t)0));
	bool res;
	// (pat) Moved initialization to constructor to avoid crash in destructor.
	//res = pthread_attr_init(&mAttrib);
	//assert(!res);
	res = pthread_attr_setstacksize(&mAttrib, mStackSize);
	assert(!res);
        struct wrapArgs *p = new wrapArgs;
        p->task = task;
        p->arg = arg;
	res = pthread_create(&mThread, &mAttrib, &thread_main, p);
	// (pat) Note: the error is returned and is not placed in errno.
	if (res) {
        SPDLOG_ERROR("pthread_create failed, error: {}", strerror(res));
    }
	assert(!res);
}

void Thread::start2(void *(*task)(void*), void *arg, int stacksize)
{
	mStackSize = stacksize;
	start(task,arg);
}



// vim: ts=4 sw=4
