#pragma once
#include <ntddk.h>

class FastMutex {
public:
	void Init();

	void Lock();
	void Unlock();

private:
	FAST_MUTEX _mutex;
};

template<typename TLock>
struct Locker {
	explicit Locker(TLock& lock) : _lock(lock) {
		lock.Lock();
	}
	~Locker() {
		_lock.Unlock();
	}
private:
	TLock& _lock;
};
