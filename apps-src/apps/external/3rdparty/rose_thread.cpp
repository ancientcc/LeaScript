

#include "rose_thread.hpp"

static frose_create_thread s_create_thread = nullptr;
void rose_set_create_thread(frose_create_thread fcreate)
{
	s_create_thread = fcreate;
}

trose_thread* create_rose_thread(const std::function<void (bool& exit)>& DoWork, 
		const std::function<void ()>& OnWorkStart, const std::function<void ()>& DoWorkDone, 
		const std::function<void ()>& OnTriggerExit, const std::string& name)
{
	return s_create_thread(DoWork, OnWorkStart, DoWorkDone, OnTriggerExit, name);
}

static frose_create_event s_create_event = nullptr;
void rose_set_create_event(frose_create_event fcreate)
{
	s_create_event = fcreate;
}

trose_event* rose_create_event(bool manual_reset, bool initially_signaled)
{
	return s_create_event(manual_reset, initially_signaled);
}


namespace threading {

mutex::mutex() : m_(SDL_CreateMutex())
{}

mutex::~mutex()
{
	SDL_DestroyMutex(m_);
}

lock::lock(mutex& m) : m_(m)
{
	SDL_mutexP(m_.m_);
}

lock::~lock()
{
	SDL_mutexV(m_.m_);
}

condition::condition() : cond_(SDL_CreateCond())
{}

condition::~condition()
{
	SDL_DestroyCond(cond_);
}

bool condition::wait(const mutex& m)
{
	return SDL_CondWait(cond_,m.m_) == 0;
}

condition::WAIT_TIMEOUT_RESULT condition::wait_timeout(const mutex& m, unsigned int timeout)
{
	const int res = SDL_CondWaitTimeout(cond_,m.m_,timeout);
	switch(res) {
		case 0: return WAIT_OK;
		case SDL_MUTEX_TIMEDOUT: return WAIT_TIMED_OUT;
		default:
			// SDL_CondWaitTimeout: $SDL_GetError()
			return WAIT_ERROR;
	}
}

bool condition::notify_one()
{
	if(SDL_CondSignal(cond_) < 0) {
		// SDL_CondSignal: $SDL_GetError()
		return false;
	}

	return true;
}

bool condition::notify_all()
{
	if(SDL_CondBroadcast(cond_) < 0) {
		// SDL_CondBroadcast: $SDL_GetError()
		return false;
	}
	return true;
}

semaphore::semaphore(Uint32 initial_value) : sem_(SDL_CreateSemaphore(initial_value))
{}

semaphore::~semaphore()
{
	SDL_DestroySemaphore(sem_);
}

int semaphore::wait(Uint32 timeout)
{
	return SDL_SemWaitTimeout(sem_, timeout);
}

void semaphore::post()
{
	SDL_SemPost(sem_);
}

}