//
//  sio_client.h
//
//  Created by Melo Yao on 3/25/15.
//

#ifndef SIO_CLIENT_H
#define SIO_CLIENT_H
#include <string>
#include <functional>
#include "sio_message.h"
#include "sio_socket.h"

namespace sio
{
    class client_impl;

    class client {
    public:
        enum close_reason
        {
            close_reason_normal,
            close_reason_drop
        };

        typedef std::function<void(void)> con_listener;

        typedef std::function<void(close_reason const& reason)> close_listener;

        typedef std::function<void(unsigned, unsigned)> reconnect_listener;

        typedef std::function<void(std::string const& nsp)> socket_listener;
        typedef std::shared_ptr<client> ptr;
        static ptr create(const std::string& uri);
        virtual ~client();

        //set listeners and event bindings.
        virtual void set_open_listener(con_listener const& l) = 0;

        virtual void set_fail_listener(con_listener const& l) = 0;

        virtual void set_reconnecting_listener(con_listener const& l) = 0;

        virtual void set_reconnect_listener(reconnect_listener const& l) = 0;

        virtual void set_close_listener(close_listener const& l) = 0;

        virtual void set_socket_open_listener(socket_listener const& l) = 0;

        virtual void set_socket_close_listener(socket_listener const& l) = 0;

        virtual void clear_con_listeners() = 0;

        virtual void clear_socket_listeners() = 0;

        // Client Functions - such as send, etc.
        virtual void connect(const std::map<std::string, std::string>& query = {},
            const std::map<std::string, std::string>& http_extra_headers = {}) = 0;

        virtual void set_reconnect_attempts(int attempts) = 0;

        virtual void set_reconnect_delay(unsigned millis) = 0;

        virtual void set_reconnect_delay_max(unsigned millis) = 0;

        enum LogLevel
        {
            log_default,
            log_quiet,
            log_verbose
        };
        virtual void set_logs_level(LogLevel level) = 0;

        virtual sio::socket::ptr const& socket(const std::string& nsp = "") = 0;

        // Closes the connection
        virtual void close() = 0;

        virtual void sync_close() = 0;

        virtual bool opened() const = 0;

        virtual std::string const& get_sessionid() const = 0;

    protected:
        client();
    private:
        //disable copy constructor and assign operator.
        client(client const&) {}
        void operator=(client const&) {}
    };

}


#endif // __SIO_CLIENT__H__
