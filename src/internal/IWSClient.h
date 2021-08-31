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
};

class IWSClient {
public:
	static IWSClient* Create();
	virtual ~IWSClient() {}

	virtual void SetEvent(IWSEvent* ev) = 0;
	virtual bool Open(const char* url) = 0;
	virtual int GetState() = 0;
	virtual bool Close() = 0;
	virtual bool Write(const void* buff, int size, bool bin) = 0;
};