#pragma once
#include <map>
#include <string>
#include <functional>
enum CloseReason
{
	WSE_USER_CLOSE,
	WSE_SERVER_CLOSE,// 服务器关闭
	WSE_DNS_LOOKUP, // 域名解析失败
	WSE_UPGRADE, // UPGRADE失败
};

enum WSState { WS_CLOSED, WS_OPENING, WS_OPEN };

class IWSEvent {
public:
	virtual void onOpen() = 0;
	virtual void onClose(int reason) = 0;
	virtual void onMessage(const char* buff, int size, bool bin) = 0;
	virtual void onHttpResp(int code, const std::map<std::string, std::string>& resp)= 0;
};

class IWSClient {
public:
	static IWSClient* Create(const char* url);
	virtual ~IWSClient() {}

	virtual void SetEvent(IWSEvent* ev) = 0;
	virtual bool Open(const char* url, const std::map<std::string, std::string>& headers = {}) = 0;

	virtual int GetState() = 0;
	virtual bool Close(const char* reason = "") = 0;
	virtual bool Write(const void* buff, int size, bool bin) = 0;
};

class ITimerMgr {
public:
	static ITimerMgr* Instance();
	typedef std::function<void()> TimerCB;
	virtual uint32_t createTimer(int delay, TimerCB cb) = 0;
	virtual void cancelTimer(uint32_t timer) = 0;
};
