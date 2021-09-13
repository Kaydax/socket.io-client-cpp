#pragma once

#ifdef _WIN32
#define _WEBSOCKETPP_CPP11_THREAD_
//#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_NO_CPP11_FUNCTIONAL_
#define INTIALIZER(__TYPE__)
#else
#define _WEBSOCKETPP_CPP11_STL_ 1
#define INTIALIZER(__TYPE__) (__TYPE__)
#endif
#include <websocketpp/client.hpp>
#if _DEBUG || DEBUG
#if SIO_TLS
#include <websocketpp/config/debug_asio.hpp>
typedef websocketpp::config::debug_asio_tls client_config_tls;
#endif //SIO_TLS
#include <websocketpp/config/debug_asio_no_tls.hpp>
typedef websocketpp::config::debug_asio client_config;
#else
#if SIO_TLS
#include <websocketpp/config/asio_client.hpp>
typedef websocketpp::config::asio_tls_client client_config_tls;
#endif //SIO_TLS
#include <websocketpp/config/asio_no_tls_client.hpp>
typedef websocketpp::config::asio_client client_config;
#endif //DEBUG

#if SIO_TLS
#include <asio/ssl/context.hpp>
#endif

#include "IoService.h"

using namespace websocketpp;
template <typename config>
class WsInstance : public IWSClient {
	asio::io_service* io_service;
	typedef websocketpp::client<config> client_type;
	client_type m_client;
	// Connection pointer for client functions.
	connection_hdl m_con;
	// callback event
	IWSEvent* event_ = nullptr;
	bool opened_ = false;
public:
	WsInstance();
	~WsInstance() {
		Close("Destroy");
	}

	void log(const char* fmt, ...);

	void template_init();

	virtual void SetEvent(IWSEvent* ev)
	{
		event_ = ev;
	}

	virtual int GetState();

	virtual bool Open(const char* url, const std::map<std::string, std::string>& header);
	virtual bool Close(const char* reason);
	virtual bool Write(const void* buff, int size, bool bin);

};


