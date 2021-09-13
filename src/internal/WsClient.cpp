#include "WsClient.h"
#include <stdarg.h>
#include <stdio.h>
#if SIO_TLS
// If using Asio's SSL support, you will also need to add this #include.
// Source: http://think-async.com/Asio/asio-1.10.6/doc/asio/using.html
// #include <asio/ssl/impl/src.hpp>
#endif
using namespace std;

template <typename config>
WsInstance<config>::WsInstance()
{
	// Initialize the Asio transport policy
#ifndef DEBUG
	using websocketpp::log::alevel;
	m_client.clear_access_channels(alevel::all);
	m_client.set_access_channels(alevel::connect | alevel::disconnect | alevel::app);
#endif
	this->io_service = IoService::Instance()->get_io_service();
	m_client.init_asio(io_service);

	// Bind the clients we are using
	m_client.set_http_handler([this](connection_hdl con) {
		lib::error_code ec;
		typename client_type::connection_ptr conn_ptr = m_client.get_con_from_hdl(con, ec);
		if (conn_ptr) {
			auto resp = conn_ptr->get_response();
			std::map<std::string, std::string> headers(resp.get_headers().begin(), resp.get_headers().end());
			// m_http_listener((int)resp.get_status_code(), headers, resp.get_body());
			if (event_)
				event_->onHttpResp((int)resp.get_status_code(), headers);
		}
	});
	m_client.set_open_handler([this](connection_hdl con) {
		opened_ = true;
		if (event_)
			event_->onOpen();
	});

	m_client.set_close_handler([this](connection_hdl con) {
		lib::error_code ec;
		close::status::value code = close::status::normal;
		typename client_type::connection_ptr conn_ptr = m_client.get_con_from_hdl(con, ec);
		if (ec) {
			log("OnClose get conn failed: %d", ec.value());
		}
		else
		{
			code = conn_ptr->get_local_close_code();
			log("sio close %s", close::status::get_string(code).c_str());
		}
		m_con.reset();
		opened_ = false;
		if (event_)
			event_->onClose(code == close::status::normal);
	});

	m_client.set_fail_handler([this](connection_hdl con) {
		opened_ = false;
		m_con.reset();
		if (event_)
			event_->onClose(false);
	});

	m_client.set_message_handler([this](connection_hdl conn, typename client_type::message_ptr msg) {
		if (event_) {
			auto data = msg->get_payload();
			event_->onMessage(data.data(), data.length(), false);
		}
	});
	template_init();
}

template<>
void WsInstance<client_config>::template_init()
{
}

#if SIO_TLS
typedef websocketpp::lib::shared_ptr<asio::ssl::context> context_ptr;
static context_ptr on_tls_init(connection_hdl conn)
{
	asio::error_code ec;
	context_ptr ctx = context_ptr(new  asio::ssl::context(asio::ssl::context::tlsv12));
	ctx->set_options(asio::ssl::context::default_workarounds | asio::ssl::context::single_dh_use, ec);
	if (ec)
	{
		cerr << "Init tls failed,reason:" << ec.message() << endl;
	}

	return ctx;
}

template<>
void WsInstance<client_config_tls>::template_init()
{
	m_client.set_tls_init_handler(&on_tls_init);
}
#endif


template <typename config>
int WsInstance<config>::GetState()
{
	if (m_con.expired())
		return WS_CLOSED;
	else
		return opened_ ? WS_OPEN : WS_OPENING;
}

template <typename config>
bool WsInstance<config>::Open(const char* url, const std::map<std::string, std::string>& headers)
{
	std::string uri = url;
	if (!m_con.expired()) {
		log("already opened, skip open %s, please call close first", url);
		return false;
	}
	io_service->dispatch([=]() {
		lib::error_code ec;
		typename client_type::connection_ptr con = m_client.get_connection(uri, ec);
		if (ec) {
			log("get_connection %s Error: %s", uri.c_str(), ec.message().c_str());
			return;// false;
		}
		for (auto&& header : headers) {
			con->replace_header(header.first, header.second);
		}
		// con->set_http(http);
		m_client.connect(con);
		m_con = con;
	});
	return true;
}

template <typename config>
bool WsInstance<config>::Close(const char* r)
{
	if(m_con.expired()){
		log("Error: No active session");
		return true;
	}
	std::string reason = r;
	io_service->dispatch([=]() {
		log("Close reason: %s", reason.c_str());
		if (m_con.expired())
		{
			log("Error: No active session");
		}
		else
		{
			lib::error_code ec;
			m_client.close(m_con, close::status::normal, reason, ec);
		}
	});
	return true;
}

template <typename config>
bool WsInstance<config>::Write(const void* buff, int size, bool bin)
{
	if (m_con.expired())
		return false;
	std::string data((const char*)buff, size);
	frame::opcode::value opcode = bin ? frame::opcode::binary : frame::opcode::text;
	io_service->dispatch([=]() {
		if (m_con.expired())
			return;
		lib::error_code ec;
		m_client.send(m_con, data, opcode, ec);
		if (ec)
		{
			log("send %d failed, reason: %d", size, ec.message().c_str());
		}
	});
	return true;
}

template <typename config>
void WsInstance<config>::log(const char* fmt, ...)
{
	char line[4096];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(line, sizeof(line), fmt, vl);
	m_client.get_alog().write(websocketpp::log::alevel::app, line);
	va_end(vl);
}

#include "sio_client_impl.h"
IWSClient* IWSClient::Create(const char* url)
{
#if SIO_TLS
	if (sio::client_impl::is_tls(url)) {
		return new WsInstance<client_config_tls>();
	}
#endif
	return new WsInstance<client_config>();
}

