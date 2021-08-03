//
//  sio_client_impl.cpp
//  SioChatDemo
//
//  Created by Melo Yao on 4/3/15.
//  Copyright (c) 2015 Melo Yao. All rights reserved.
//

#include "sio_client_impl.h"
#include <functional>
#include <sstream>
#include <chrono>
#include <mutex>
#include <cmath>
#include <stdarg.h>

using std::placeholders::_1;
using std::placeholders::_2;

#if SIO_TLS
// If using Asio's SSL support, you will also need to add this #include.
// Source: http://think-async.com/Asio/asio-1.10.6/doc/asio/using.html
// #include <asio/ssl/impl/src.hpp>
#endif

using std::chrono::milliseconds;
using namespace std;

namespace sio
{
    std::string client_impl::encode_query_string(const std::string &query) {
        ostringstream ss;
        ss << std::hex;
        // Percent-encode (RFC3986) non-alphanumeric characters.
        for (const char c : query) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                ss << c;
            }
            else {
                ss << '%' << std::uppercase << std::setw(2) << int((unsigned char)c) << std::nouppercase;
            }
        }
        ss << std::dec;
        return ss.str();
    }

#ifdef WIN32
#define strcasecmp _stricmp
#endif
    bool client_impl::is_tls(const string& uri)
    {
        websocketpp::uri uo(uri);
        if (!strcasecmp(uo.get_scheme().c_str(), "http") || !strcasecmp(uo.get_scheme().c_str(), "ws"))
        {
            return false;
        }
#if SIO_TLS
        else if (!strcasecmp(uo.get_scheme().c_str(), "https") || !strcasecmp(uo.get_scheme().c_str(), "wss"))
        {
            return true;
        }
#endif
        else
        {
            throw std::runtime_error("unsupported URI scheme");
        }
    }

    /*************************public:*************************/
    client_impl::client_impl(const std::string& url) :
        m_ping_interval(0),
        m_ping_timeout(0),
        m_con_state(con_closed),
        m_reconn_delay(5000),
        m_reconn_delay_max(25000),
        m_reconn_attempts(0xFFFFFFFF),
        m_reconn_made(0)
    {
        m_base_url = url;
        
        m_packet_mgr.set_decode_callback(std::bind(&client_impl::on_decode,this,_1));
        m_packet_mgr.set_encode_callback(std::bind(&client_impl::on_encode,this,_1,_2));
    }
    
    client_impl::~client_impl()
    {
        this->sockets_invoke_void(&sio::socket::on_close);
    }

    template <typename config>
    sio::client_instance<config>::~client_instance()
    {
        close();
    }

    template <typename config>
    sio::client_instance<config>::client_instance(const std::string& uri) : client_impl(uri)
    {
    #ifndef DEBUG
        set_logs_level(client::log_default);
    #endif
        // Initialize the Asio transport policy
        m_client.init_asio(&get_io_service());

        // Bind the clients we are using
        m_client.set_http_handler([this](connection_hdl con) {
            lib::error_code ec;
            typename client_type::connection_ptr conn_ptr = m_client.get_con_from_hdl(con, ec);
            if (conn_ptr && m_http_listener) {
                auto resp = conn_ptr->get_response();
                //std::cout << conn_ptr->get_response_header("token");
                std::map<std::string, std::string> headers(resp.get_headers().begin(), resp.get_headers().end());
                MapStr mheader; mheader.map() = headers;
                m_http_listener((int)resp.get_status_code(), mheader, resp.get_body());
            }
        });
        m_client.set_open_handler(std::bind(&client_instance<config>::on_open, this, _1));
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
            }
            this->on_close(con, code == close::status::normal);
        });
        m_client.set_fail_handler(std::bind(&client_instance<config>::on_fail, this, _1));
        m_client.set_message_handler([this](connection_hdl conn, typename client_type::message_ptr msg) {this->on_message(conn, msg->get_payload()); });
        template_init();
    }

    template<>
    void client_instance<client_config>::template_init()
    {
    }

#if SIO_TLS
    typedef websocketpp::lib::shared_ptr<asio::ssl::context> context_ptr;
    static context_ptr on_tls_init(connection_hdl conn)
    {
        context_ptr ctx = context_ptr(new  asio::ssl::context(asio::ssl::context::tlsv12));
        asio::error_code ec;
        ctx->set_options(asio::ssl::context::default_workarounds |
            asio::ssl::context::single_dh_use, ec);
        if (ec)
        {
            cerr << "Init tls failed,reason:" << ec.message() << endl;
        }

        return ctx;
    }

    template<>
    void client_instance<client_config_tls>::template_init()
    {        
        m_client.set_tls_init_handler(&on_tls_init);
    }
#endif

    template<typename config>
    bool client_instance<config>::ws_connect(const std::string& uri, bool http)
    {
        lib::error_code ec;
        typename client_type::connection_ptr con = m_client.get_connection(uri, ec);
        if (ec) {
            log("Get Connection Error: %s", ec.message().c_str());
            return false;
        }

        for (auto&& header : m_http_headers) {
            con->replace_header(header.first, header.second);
        }
        con->set_http(http);
        m_client.connect(con);
        return true;
    }

    void client_impl::connect(const MapStr& query, const MapStr& headers)
    {
        if(m_reconn_timer)
        {
            m_reconn_timer->cancel();
            m_reconn_timer.reset();
        }
        m_con_state = con_opening;
        m_reconn_made = 0;

        string query_str;
        auto& map = query.map();
        for(auto it=map.begin(); it!=map.end(); ++it){
            query_str.append("&");
            query_str.append(it->first);
            query_str.append("=");
            query_str.append(encode_query_string(it->second));
        }
        m_query_string=move(query_str);

        m_http_headers = headers.map();

        this->reset_states();
        get_io_service().dispatch(std::bind(&client_impl::connect_impl,this,m_base_url,m_query_string));
        // m_network_thread.reset(new thread(std::bind(&client_impl::run_loop,this)));//uri lifecycle?
    }

    socket::ptr const& client_impl::socket(const char* nsp)
    {
        lock_guard<mutex> guard(m_socket_mutex);
        string aux;
        if(!nsp || !nsp[0])
        {
            aux = "/";
        }
        else if(nsp[0] != '/')
        {
            aux.append("/", 1);
            aux.append(nsp);
        }
        else
        {
            aux = nsp;
        }

        auto it = m_sockets.find(aux);
        if(it!= m_sockets.end())
        {
            return it->second;
        }
        else
        {
            socket::ptr p = sio::socket::create(this, aux.c_str());
            m_sockets[aux] = p;
            return p;
        }
    }

    void client_impl::close()
    {
        m_con_state = con_closing;
        this->sockets_invoke_void(&sio::socket::close);
        get_io_service().dispatch(std::bind(&client_impl::close_impl, this,close::status::normal,"End by user"));
    }

    template<typename config>
	void client_instance<config>::set_logs_level(LogLevel level)
	{
		m_client.clear_access_channels(websocketpp::log::alevel::all);
		switch (level)
		{
		default:
			break;
		case client::log_default:
			m_client.set_access_channels(websocketpp::log::alevel::connect | websocketpp::log::alevel::disconnect | websocketpp::log::alevel::app);
			break;
		case client::log_quiet:
			break;
		case client::log_verbose:
			m_client.set_access_channels(websocketpp::log::alevel::all);
			break;
		}
	}

    template<typename config>
    void client_instance<config>::log(const char* fmt, ...)
    {
        char line[1024];
        va_list vl;
        va_start(vl, fmt);
        vsnprintf(line, sizeof(line) - 1, fmt, vl);
        m_client.get_alog().write(websocketpp::log::alevel::app, line);
        va_end(vl);
    }

    /*************************protected:*************************/
    void client_impl::send(packet& p)
    {
        m_packet_mgr.encode(p);
    }

    void client_impl::remove_socket(string const& nsp)
    {
        lock_guard<mutex> guard(m_socket_mutex);
        auto it = m_sockets.find(nsp);
        if(it!= m_sockets.end())
        {
            m_sockets.erase(it);
        }
    }

    void client_impl::on_socket_closed(string const& nsp)
    {
        if(m_socket_close_listener)m_socket_close_listener(nsp.c_str());
    }

    void client_impl::on_socket_opened(string const& nsp)
    {
        if(m_socket_open_listener)m_socket_open_listener(nsp.c_str());
    }

    asio::io_service& client_impl::get_io_service()
    {
        static std::unique_ptr<asio::io_service> io_service;
        if (!io_service) {
            io_service.reset(new lib::asio::io_service());
        }
        return *io_service;
    }

    /*************************private:*************************/
    void client_impl::run_loop()
    {
        if (asio::io_service* ios = &get_io_service()) {
            ios->run();
            ios->reset();
            //log("run loop end");
        }
    }

    void client_impl::connect_impl(const string& uri, const string& queryString)
    {
        do{
            ostringstream ss;
            websocketpp::uri uo(uri);
            if (strcasecmp(uo.get_scheme().substr(0, 2).c_str(), "ws")) {
                ws_connect(uri, true);
                return;
            }

            if (is_tls(m_base_url)) {
                ss << "wss://";
            }
            else {
                ss << "ws://";
            }
            const std::string host(uo.get_host());
            // As per RFC2732, literal IPv6 address should be enclosed in "[" and "]".
            if(host.find(':')!=std::string::npos){
                ss<<"["<<uo.get_host()<<"]";
            } else {
                ss<<uo.get_host();
            }

            // If a resource path was included in the URI, use that, otherwise
            // use the default /socket.io/.
                const std::string path(uo.get_resource() == "/" ? "/socket.io/" : uo.get_resource());
            ss<<":"<<uo.get_port() << path << "?EIO=4&transport=websocket";
            if(m_sid.size()>0){
                ss<<"&sid="<<m_sid;
            }
            ss<<"&t="<<time(NULL)<<queryString;
            ws_connect(ss.str(), false);
            return;
        }
        while(0);
        if(m_fail_listener)
        {
            m_fail_listener();
        }
    }

    template <typename config>
    void client_instance<config>::close_impl(close::status::value const& code,string const& reason)
    {
        log("Close by reason: %d", reason.c_str());
        if(m_reconn_timer)
        {
            m_reconn_timer->cancel();
            m_reconn_timer.reset();
        }
        if (m_con.expired())
        {
            log("Error: No active session");
        }
        else
        {
            lib::error_code ec;
            m_client.close(m_con, code, reason, ec);
        }
    }

    template <typename config>
    void client_instance<config>::send_impl(shared_ptr<const string> const& payload_ptr, frame::opcode::value opcode)
    {
        if(m_con_state == con_opened)
        {
            lib::error_code ec;
            m_client.send(m_con,*payload_ptr,opcode,ec);
            if(ec)
            {
                log("Send failed,reason: %d", ec.message().c_str());
            }
        }
    }

    void client_impl::ping(const asio::error_code& ec)
    {
        if(ec || m_con.expired())
        {
            if (ec != asio::error::operation_aborted)
                log("ping exit,con %s, ec: %s", m_con.expired()?"expired":"", ec.message().c_str());
            return;
        }
        packet p(packet::frame_ping);
        m_packet_mgr.encode(p, [&](bool /*isBin*/,shared_ptr<const string> payload)
        {
            send_impl(payload, frame::opcode::text);
        });
        if(!m_ping_timeout_timer)
        {
            m_ping_timeout_timer.reset(new asio::steady_timer(get_io_service()));
            std::error_code timeout_ec;
            m_ping_timeout_timer->expires_from_now(milliseconds(m_ping_timeout), timeout_ec);
            m_ping_timeout_timer->async_wait(std::bind(&client_impl::timeout_pong, this, std::placeholders::_1));
        }
    }

    void client_impl::timeout_pong(const asio::error_code &ec)
    {
        if(ec)
        {
            return;
        }
        log("Pong timeout");
        get_io_service().dispatch(std::bind(&client_impl::close_impl, this,close::status::policy_violation,"Pong timeout"));
    }

    void client_impl::timeout_reconnect(asio::error_code const& ec)
    {
        if(ec)
        {
            return;
        }
        if(m_con_state == con_closed)
        {
            m_con_state = con_opening;
            m_reconn_made++;
            this->reset_states();
            log("Reconnecting...");
            if(m_reconnecting_listener) m_reconnecting_listener();
            get_io_service().dispatch(std::bind(&client_impl::connect_impl,this,m_base_url,m_query_string));
        }
    }

    unsigned client_impl::next_delay() const
    {
        //no jitter, fixed power root.
        unsigned reconn_made = min<unsigned>(m_reconn_made,32);//protect the pow result to be too big.
        return static_cast<unsigned>(min<double>(m_reconn_delay * pow(1.5,reconn_made),m_reconn_delay_max));
    }

    socket::ptr client_impl::get_socket_locked(string const& nsp)
    {
        lock_guard<mutex> guard(m_socket_mutex);
        auto it = m_sockets.find(nsp);
        if(it != m_sockets.end())
        {
            return it->second;
        }
        else
        {
            return socket::ptr();
        }
    }

    void client_impl::sockets_invoke_void(void (sio::socket::*fn)(void))
    {
        map<const string,socket::ptr> socks;
        {
            lock_guard<mutex> guard(m_socket_mutex);
            socks.insert(m_sockets.begin(),m_sockets.end());
        }
        for (auto it = socks.begin(); it!=socks.end(); ++it) {
            ((*(it->second)).*fn)();
        }
    }

    void client_impl::on_fail(connection_hdl)
    {
        if (m_con_state == con_closing) {
            log("Connection failed while closing.");
            this->close();
            return;
        }

        m_con.reset();
        m_con_state = con_closed;
        this->sockets_invoke_void(&sio::socket::on_disconnect);
        log("Connection failed.");
        if(m_reconn_made<m_reconn_attempts)
        {
            log("Reconnect for attempt: %d", m_reconn_made);
            unsigned delay = this->next_delay();
            if(m_reconnect_listener) m_reconnect_listener(m_reconn_made,delay);
            m_reconn_timer.reset(new asio::steady_timer(get_io_service()));
            asio::error_code ec;
            m_reconn_timer->expires_from_now(milliseconds(delay), ec);
            m_reconn_timer->async_wait(std::bind(&client_impl::timeout_reconnect,this, std::placeholders::_1));
        }
        else
        {
            if(m_fail_listener)m_fail_listener();
        }
    }
    
    void client_impl::on_open(connection_hdl con)
    {
        if (m_con_state == con_closing) {
            log("Connection opened while closing.");
            this->close();
            return;
        }

        log("Connected.");
        m_con_state = con_opened;
        m_con = con;
        m_reconn_made = 0;
        this->sockets_invoke_void(&sio::socket::on_open);
        this->socket("");
        if(m_open_listener)m_open_listener();
    }
    
    void client_impl::on_close(connection_hdl con, bool normal_close)
    {
        log("Client Disconnected.");
        con_state m_con_state_was = m_con_state;
        m_con_state = con_closed;

        m_con.reset();

        this->clear_timers();
        client::close_reason reason;

        // If we initiated the close, no matter what the close status was,
        // we'll consider it a normal close. (When using TLS, we can
        // sometimes get a TLS Short Read error when closing.)
        if(normal_close || m_con_state_was == con_closing)
        {
            this->sockets_invoke_void(&sio::socket::on_disconnect);
            reason = client::close_reason_normal;
        }
        else
        {
            this->sockets_invoke_void(&sio::socket::on_disconnect);
            if(m_reconn_made<m_reconn_attempts)
            {
                log("Reconnect for attempt: %d", m_reconn_made);
                unsigned delay = this->next_delay();
                if(m_reconnect_listener) m_reconnect_listener(m_reconn_made,delay);
                m_reconn_timer.reset(new asio::steady_timer(get_io_service()));
                asio::error_code ec;
                m_reconn_timer->expires_from_now(milliseconds(delay), ec);
                m_reconn_timer->async_wait(std::bind(&client_impl::timeout_reconnect,this, std::placeholders::_1));
                return;
            }
            reason = client::close_reason_drop;
        }
        
        if(m_close_listener)
        {
            m_close_listener(reason);
        }
    }
    
    void client_impl::on_message(connection_hdl, const string& msg)
    {
        if (m_ping_timeout_timer) {
            asio::error_code ec;
            m_ping_timeout_timer->expires_from_now(milliseconds(m_ping_timeout),ec);
            m_ping_timeout_timer->async_wait(std::bind(&client_impl::timeout_pong, this, std::placeholders::_1));
        }
        // Parse the incoming message according to socket.IO rules
        m_packet_mgr.put_payload(msg);
    }
    
    void client_impl::on_handshake(message::ptr const& message)
    {
        if(message && message->get_flag() == message::flag_object)
        {
            const object_message* obj_ptr =static_cast<object_message*>(message.get());
            auto it = obj_ptr->at("sid");
            if (it) {
                m_sid = it->get_string();
            }
            else
            {
                goto failed;
            }

            it = obj_ptr->at("pingInterval");
            if (it&&it->get_flag() == message::flag_integer) {
                m_ping_interval = (unsigned)it->get_int();
            }
            else
            {
                m_ping_interval = 25000;
            }

            it = obj_ptr->at("pingTimeout");
            if (it&&it->get_flag() == message::flag_integer) {
                m_ping_timeout = (unsigned) it->get_int();
            }
            else
            {
                m_ping_timeout = 60000;
            }

            return;
        }
failed:
        //just close it.
        get_io_service().dispatch(std::bind(&client_impl::close_impl, this,close::status::policy_violation,"Handshake error"));
    }

    void client_impl::on_ping()
    {
        packet p(packet::frame_pong);
        m_packet_mgr.encode(p, [&](bool /*isBin*/,shared_ptr<const string> payload)
        {
            send_impl(payload, frame::opcode::text);
        });

        if(m_ping_timeout_timer)
        {
            m_ping_timeout_timer->cancel();
            m_ping_timeout_timer.reset();
        }
    }

    void client_impl::on_decode(packet const& p)
    {
        switch(p.get_frame())
        {
        case packet::frame_message:
        {
            socket::ptr so_ptr = get_socket_locked(p.get_nsp());
            if(so_ptr)so_ptr->on_message_packet(p);
            break;
        }
        case packet::frame_open:
            this->on_handshake(p.get_message());
            break;
        case packet::frame_close:
            //FIXME how to deal?
            this->close_impl(close::status::abnormal_close, "End by server");
            break;
        case packet::frame_ping:
            this->on_ping();
            break;

        default:
            break;
        }
    }
    
    void client_impl::on_encode(bool isBinary,shared_ptr<const string> const& payload)
    {
        log("encoded payload length: %d", payload->length());
        get_io_service().dispatch(std::bind(&client_impl::send_impl,this,payload,isBinary?frame::opcode::binary:frame::opcode::text));
    }
    
    void client_impl::clear_timers()
    {
        log("clear timers");
        asio::error_code ec;
        if(m_ping_timeout_timer)
        {
            m_ping_timeout_timer->cancel(ec);
            m_ping_timeout_timer.reset();
        }
    }
    
    void client_impl::reset_states()
    {
        m_sid.clear();
        m_packet_mgr.reset();
    }

    template class client_instance<client_config>;
#if SIO_TLS
    template class client_instance<client_config_tls>;
#endif
}
