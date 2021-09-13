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
#include <iomanip>
#include <websocketpp/uri.hpp>

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

    bool client_impl::is_tls(const string& uri)
    {
		std::string scheme;
		auto pos = uri.find("://");
		if (pos != std::string::npos) {
			scheme = uri.substr(0, pos);
			std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
		}
        if (scheme == "http" || scheme =="ws")
        {
            return false;
        }
        else if (scheme == "https" || scheme == "wss")
        {
            return true;
        }
        else
        {
            throw std::runtime_error("unsupported URI scheme");
        }
    }


	uint32_t client_impl::createTimer(int delay, TimerCB cb)
	{
		return ITimerMgr::Instance()->createTimer(delay, cb);
	}

	void client_impl::cancelTimer(uint32_t timer)
	{
		ITimerMgr::Instance()->cancelTimer(timer);
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
        m_packet_mgr.set_decode_callback(std::bind(&client_impl::on_decode,this, std::placeholders::_1));
        m_packet_mgr.set_encode_callback(std::bind(&client_impl::on_encode,this, std::placeholders::_1, std::placeholders::_2));
    }
    
    client_impl::~client_impl()
    {
        this->sockets_invoke_void(&sio::socket::on_close);
        close();
    }

    void client_impl::connect(const map<string, string>& query, const map<string, string>& headers)
    {
        if(m_reconn_timer)
        {
            cancelTimer(m_reconn_timer);
            m_reconn_timer = 0;
        }
		/*
        if(m_network_thread)
        {
            if(m_con_state == con_closing||m_con_state == con_closed)
            {
                //if client is closing, join to wait.
                //if client is closed, still need to join,
                //but in closed case,join will return immediately.
                m_network_thread->join();
                m_network_thread.reset();//defensive
            }
            else
            {
                //if we are connected, do nothing.
                return;
            }
        }*/
        m_con_state = con_opening;
        m_reconn_made = 0;

        string query_str;
        for(map<string,string>::const_iterator it=query.begin();it!=query.end();++it){
            query_str.append("&");
            query_str.append(it->first);
            query_str.append("=");
            query_str.append(encode_query_string(it->second));
        }
        m_query_string=move(query_str);

        m_http_headers = headers;

        this->reset_states();

        connect_impl(m_base_url, m_query_string);
    }

    socket::ptr const& client_impl::socket(string const& nsp)
    {
        lock_guard<mutex> guard(m_socket_mutex);
        string aux;
        if(nsp == "")
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
            pair<const string, socket::ptr> p(aux, sio::socket::create(this,aux));
            return (m_sockets.insert(p).first)->second;
        }
    }

    void client_impl::close()
    {
        m_con_state = con_closing;
        this->sockets_invoke_void(&sio::socket::close);
		if (m_reconn_timer)
		{
			cancelTimer(m_reconn_timer);
			m_reconn_timer = 0;
		}
		if (m_con) {
			m_con->Close("End by user");
			m_con = nullptr;
		}
	}

    void client_impl::log(const char* fmt, ...)
    {
        char line[4096];
        va_list vl;
        va_start(vl, fmt);
        int n = vsnprintf(line, sizeof(line), fmt, vl);
        va_end(vl);
		if (n && line[n - 1] != '\n') {
			line[n++] = '\n';
			line[n++] = 0;
		}
		fputs(line, stdout);
        // m_client.get_alog().write(websocketpp::log::alevel::app, line);
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
        if(m_socket_close_listener)m_socket_close_listener(nsp);
    }

    void client_impl::on_socket_opened(string const& nsp)
    {
        if(m_socket_open_listener)m_socket_open_listener(nsp);
    }

	void client_impl::connect_impl(const string& uri, const string& queryString)
	{
		do {
			ostringstream ss;
			websocketpp::uri uo(uri);
			/*
			if (strcasecmp(scheme.substr(0, 2).c_str(), "ws")) {
				ws_connect(uri, true);
				return;
			}
			*/
			if (is_tls(m_base_url)) {
				ss << "wss://";
			}
			else {
				ss << "ws://";
			}
			const std::string host(uo.get_host());
			// As per RFC2732, literal IPv6 address should be enclosed in "[" and "]".
			if (host.find(':') != std::string::npos) {
				ss << "[" << uo.get_host() << "]";
			}
			else {
				ss << uo.get_host();
			}

			// If a resource path was included in the URI, use that, otherwise
			// use the default /socket.io/.
			const std::string path(uo.get_resource() == "/" ? "/socket.io/" : uo.get_resource());
			ss << ":" << uo.get_port() << path << "?EIO=4&transport=websocket";
			if (m_sid.size() > 0) {
				ss << "&sid=" << m_sid;
			}
			ss << "&t=" << time(NULL) << queryString;

			m_con = IWSClient::Create(uri.c_str());
			m_con->SetEvent(this);
			m_con->Open(ss.str().c_str(), m_http_headers);
			// ws_connect(ss.str(), false);
			return;
		} while (0);
		if (m_fail_listener)
		{
			m_fail_listener();
		}
	}

    void client_impl::ping()
    {
        if(!m_con)
            return;

        packet p(packet::frame_ping);
        m_packet_mgr.encode(p, [&](bool /*isBin*/,shared_ptr<const string> payload)
        {
            log("send ping");
			m_con->Write(payload->data(), payload->length(), false);
        });
        if(!m_ping_timeout_timer)
            m_ping_timeout_timer = createTimer(m_ping_timeout, std::bind(&client_impl::timeout_pong, this));
        schedule_ping();
    }

    void client_impl::timeout_pong()
    {
		log("Pong timeout");
		if (m_con) {
			m_con->Close("Pong timeout");
			m_con = nullptr;
		}
    }

	void client_impl::timeout_reconnect()
	{
		if (m_con_state == con_closed)
		{
			m_con_state = con_opening;
			m_reconn_made++;
			this->reset_states();
			log("Reconnecting...");
			if (m_reconnecting_listener) m_reconnecting_listener();
			this->connect_impl(m_base_url, m_query_string);
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

	/*
	void client_impl::on_fail()
	{
		if (m_con_state == con_closing) {
			log("Connection failed while closing.");
			this->close();
			return;
		}

		m_con_state = con_closed;
		this->sockets_invoke_void(&sio::socket::on_disconnect);
		log("Connection failed.");
		if(m_reconn_made<m_reconn_attempts)
		{
			log("Reconnect for attempt: %d", m_reconn_made);
			unsigned delay = this->next_delay();
			if(m_reconnect_listener) m_reconnect_listener(m_reconn_made, delay);
			m_reconn_timer = createTimer(delay, std::bind(&client_impl::timeout_reconnect,this));
		}
		else
		{
			if(m_fail_listener)m_fail_listener();
		}
	}
	*/
	void client_impl::onHttpResp(int code, const std::map<std::string, std::string>& resp)
	{
	}

    void client_impl::onOpen()
    {
        if (m_con_state == con_closing) {
            log("Connection opened while closing.");
            this->close();
            return;
        }

        log("Connected.");
        m_con_state = con_opened;
        m_reconn_made = 0;
        this->sockets_invoke_void(&sio::socket::on_open);
        this->socket("");
        if(m_open_listener)m_open_listener();
    }
    
    void client_impl::onClose(int normal_close)
    {
        log("Client Disconnected.");
        con_state m_con_state_was = m_con_state;
        m_con_state = con_closed;

        m_con = nullptr;

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
                m_reconn_timer = createTimer(delay, std::bind(&client_impl::timeout_reconnect,this));
                return;
            }
            reason = client::close_reason_drop;
        }
        
        if(m_close_listener)
        {
            m_close_listener(reason);
        }
    }

	void client_impl::onMessage(const char* buff, int size, bool bin)
	{
		/*
		if (m_ping_timer) {
			cancelTimer(m_ping_timer);
			schedule_ping();
		}
		*/
		// Parse the incoming message according to socket.IO rules
		m_packet_mgr.put_payload(std::string(buff, size));
	}

	void client_impl::on_handshake(message::ptr const& message)
    {
        if(message && message->get_flag() == message::flag_object)
        {
            const object_message* obj_ptr =static_cast<object_message*>(message.get());
            const map<string,message::ptr>* values = &(obj_ptr->get_map());
            auto it = values->find("sid");
            if (it!= values->end()) {
                m_sid = static_pointer_cast<string_message>(it->second)->get_string();
            }
            else
            {
                goto failed;
            }

            it = values->find("pingInterval");
            if (it!= values->end()&&it->second->get_flag() == message::flag_integer) {
                m_ping_interval = (unsigned)static_pointer_cast<int_message>(it->second)->get_int();
            }
            else
            {
                m_ping_interval = 25000;
            }

            it = values->find("pingTimeout");
            if (it!=values->end()&&it->second->get_flag() == message::flag_integer) {
                m_ping_timeout = (unsigned) static_pointer_cast<int_message>(it->second)->get_int();
            }
            else
            {
                m_ping_timeout = 60000;
            }

            log("on_handshake sid=%s, pingInterval=%d, pingTimeout=%d", m_sid.c_str(), m_ping_interval, m_ping_timeout);
            schedule_ping();
            return;
        }
failed:
        //just close it.
		if (m_con) {
			m_con->Close("Handshake error");
			m_con = nullptr;
		}
    }

    void client_impl::schedule_ping()
    {
        if (m_ping_interval > 0) {
			if(m_ping_timer) cancelTimer(m_ping_timer);
            m_ping_timer = createTimer(m_ping_interval, std::bind(&client_impl::ping, this));
        }
    }

    void client_impl::on_ping()
    {
        log("recv ping");
        packet p(packet::frame_pong);
        m_packet_mgr.encode(p, [&](bool /*isBin*/,shared_ptr<const string> payload)
        {
			if (m_con)
				m_con->Write(payload->data(), payload->length(), false);
        });
    }

    void client_impl::on_pong()
    {
        log("recv pong");
        if (m_ping_timeout_timer)
        {
            cancelTimer(m_ping_timeout_timer);
            m_ping_timeout_timer = 0;
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
            if (m_con) {
                m_con->Close("End by server");
                m_con = nullptr;
            }
            break;
        case packet::frame_ping:
            this->on_ping();
            break;
        case packet::frame_pong:
            this->on_pong();
            break;
        default:
            break;
        }
    }
    
    void client_impl::on_encode(bool isBinary,shared_ptr<const string> const& payload)
    {
        log("encoded payload length: %d", payload->length());
        if (m_con) m_con->Write(payload->data(), payload->length(), isBinary);
    }
    
    void client_impl::clear_timers()
    {
        log("clear timers");
        if(m_ping_timeout_timer)
        {
            cancelTimer(m_ping_timeout_timer);
            m_ping_timeout_timer = 0;
        }
        if (m_ping_timer)
        {
            cancelTimer(m_ping_timer);
            m_ping_timer = 0;
        }
    }
    
    void client_impl::reset_states()
    {
        m_sid.clear();
        m_packet_mgr.reset();
    }
}
