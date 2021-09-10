#pragma once
#include <map>
#include <thread>
#include <mutex>
#include <asio/steady_timer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_service.hpp>
#include "IWSClient.h"

class IoService : public ITimerMgr
{
public:
	static IoService* Instance();
	asio::io_service* get_io_service() { return io_service.get(); }
	void destory();

	uint32_t createTimer(int delay, std::function<void()> cb);
	void cancelTimer(uint32_t timer);
protected:
	IoService();
	~IoService();

	asio::steady_timer* popTimer(uint32_t timer);
	std::mutex timerLock;
	std::map<uint32_t, asio::steady_timer*> timerMap;
	uint32_t curTimer = 0;

	void run_loop();
	std::thread m_network_thread;
	std::unique_ptr<asio::io_service> io_service;
};

