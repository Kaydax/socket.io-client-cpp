#ifndef SIO_CLIENT_IMPL_H
#define SIO_CLIENT_IMPL_H

#include <cstdint>
#include <memory>
#include <map>
#include <mutex>
#include <thread>
#include "../sio_client.h"
#include "sio_packet.h"
#include "IWSClient.h"
namespace sio
{
    class socket_impl;

    class client_impl : public client, 
                        public IWSEvent, public ITimerMgr {
    public:
        enum con_state
        {
            con_opening,
            con_opened,
            con_closing,
            con_closed
        };
        
        client_impl(const std::string& url);
        ~client_impl();

        //set listeners and event bindings.
#define SYNTHESIS_SETTER(__TYPE__,__FIELD__) \
    void set_##__FIELD__(__TYPE__ const& l) \
        { m_##__FIELD__ = l;}
        
        SYNTHESIS_SETTER(client::con_listener,open_listener)
        
        SYNTHESIS_SETTER(client::con_listener,fail_listener)

        SYNTHESIS_SETTER(client::reconnect_listener,reconnect_listener)

        SYNTHESIS_SETTER(client::con_listener,reconnecting_listener)
        
        SYNTHESIS_SETTER(client::close_listener,close_listener)
        
        SYNTHESIS_SETTER(client::socket_listener,socket_open_listener)
        
        SYNTHESIS_SETTER(client::socket_listener,socket_close_listener)
        
        SYNTHESIS_SETTER(client::http_listener, http_listener)

#undef SYNTHESIS_SETTER
        
        void clear_con_listeners()
        {
            m_open_listener = nullptr;
            m_close_listener = nullptr;
            m_fail_listener = nullptr;
            m_reconnect_listener = nullptr;
            m_reconnecting_listener = nullptr;
        }
        
        void clear_socket_listeners()
        {
            m_socket_open_listener = nullptr;
            m_socket_close_listener = nullptr;
        }
        
        // Client Functions - such as send, etc.
        void connect(const std::map<std::string, std::string>& queryString,
                     const std::map<std::string, std::string>& httpExtraHeaders);
        
        sio::socket::ptr const& socket(const std::string& nsp);
        // Closes the connection
        void close();
        
        bool opened() const { return m_con_state == con_opened; }
        
        std::string const& get_sessionid() const { return m_sid; }

        void set_reconnect_attempts(int attempts) {m_reconn_attempts = attempts;}

        void set_reconnect_delay(unsigned millis) {m_reconn_delay = millis;if(m_reconn_delay_max<millis) m_reconn_delay_max = millis;}
        void set_reconnect_delay_max(unsigned millis) {m_reconn_delay_max = millis;if(m_reconn_delay>millis) m_reconn_delay = millis;}

		virtual void set_logs_level(LogLevel level) {}
		void log(const char* fmt, ...);
		static bool is_tls(const std::string& uri);

		virtual void onOpen() override;
		virtual void onClose(int reason) override;
		virtual void onMessage(const char* buff, int size, bool bin) override;
		virtual void onHttpResp(int code, const std::map<std::string, std::string>& resp) override;

		uint32_t createTimer(int delay, TimerCB cb) override;
		void cancelTimer(uint32_t timer) override;

	protected:
        void send(packet& p);
        
        void remove_socket(std::string const& nsp);
        
        void on_socket_closed(std::string const& nsp);
        void on_socket_opened(std::string const& nsp);

        void connect_impl(const std::string& uri, const std::string& query);

        void schedule_ping();
        void ping();
        void timeout_pong();

        void on_ping();
        void on_pong();

        void timeout_reconnect();

        unsigned next_delay() const;

        socket::ptr get_socket_locked(std::string const& nsp);
        
        void sockets_invoke_void(void (sio::socket::*fn)(void));
        
        void on_decode(packet const& pack);
        void on_encode(bool isBinary,shared_ptr<const string> const& payload);
        
        //socket.io callbacks
        void on_handshake(message::ptr const& message);

        void reset_states();
        void clear_timers();

        // Percent encode query string
        std::string encode_query_string(const std::string &query);

        // Socket.IO server settings
        std::string m_sid;
        std::string m_base_url;
        std::string m_query_string;
        std::map<std::string, std::string> m_http_headers;

        unsigned int m_ping_interval;
        unsigned int m_ping_timeout;
        
        packet_manager m_packet_mgr;

        uint32_t m_ping_timeout_timer = 0;
        uint32_t m_ping_timer = 0;
        uint32_t m_reconn_timer = 0;

        con_state m_con_state;

        con_listener m_open_listener;
        con_listener m_fail_listener;
        con_listener m_reconnecting_listener;
        reconnect_listener m_reconnect_listener;
        close_listener m_close_listener;
        http_listener m_http_listener;

        socket_listener m_socket_open_listener;
        socket_listener m_socket_close_listener;
        
        std::map<const std::string, socket::ptr> m_sockets;
        
        std::mutex m_socket_mutex;

        unsigned m_reconn_delay;

        unsigned m_reconn_delay_max;

        unsigned m_reconn_attempts;

        unsigned m_reconn_made;
        IWSClient* m_con = nullptr;
        friend class sio::client;
        friend class sio::socket_impl;
    };

}
#endif // SIO_CLIENT_IMPL_H

