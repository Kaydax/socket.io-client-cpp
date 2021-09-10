#include "IoService.h"
using std::chrono::milliseconds;
ITimerMgr* ITimerMgr::Instance()
{
	return IoService::Instance();
}

IoService* IoService::Instance()
{
	static IoService ios;
	return &ios;
}

IoService::IoService()
{
	io_service.reset(new asio::io_service());
	// 防止loop一启动就退出
	createTimer(1000, [] {});
	m_network_thread = std::thread(std::bind(&IoService::run_loop, this));//uri lifecycle?
}

IoService::~IoService()
{
	destory();
}

void IoService::destory()
{
	// cancel all timer
	for (auto it : timerMap)
	{
		if (it.second) {
			it.second->cancel();
			delete it.second;
		}
	}
	timerMap.clear();

	if (m_network_thread.joinable())
		m_network_thread.join();
}

void IoService::run_loop()
{
	if (io_service) {
		io_service->run();
		io_service->reset();
		//log("run loop end");
	}
}

asio::steady_timer* IoService::popTimer(uint32_t timer)
{
	asio::steady_timer* ret = nullptr;
	std::unique_lock<std::mutex> l(timerLock);
	auto it = timerMap.find(timer);
	if (it != timerMap.end()) {
		ret = it->second;
		timerMap.erase(it);
	}
	return ret;
}

uint32_t IoService::createTimer(int delay, std::function<void()> cb)
{
	auto timer = new asio::steady_timer(*io_service);
	std::error_code timeout_ec;
	timer->expires_from_now(milliseconds(delay), timeout_ec);
	std::unique_lock<std::mutex> l(timerLock);
	int ret = ++this->curTimer;
	this->timerMap[ret] = timer;
	timer->async_wait([cb, ret, this](const asio::error_code &ec) {
		if (!ec)
			cb();
		auto timer = this->popTimer(ret);
		if (timer)
			delete timer;
	});
	return ret;
}

void IoService::cancelTimer(uint32_t id)
{
	auto timer = popTimer(id);
	if (timer) {
		timer->cancel();
		delete timer;
	}
}