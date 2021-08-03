//
//  sio_client.h
//
//  Created by Melo Yao on 3/25/15.
//

#include "sio_client.h"
#include "internal/sio_client_impl.h"

using namespace websocketpp;
using std::stringstream;

namespace sio
{    
    client::ptr client::create(const std::string& uri)
    {
#if SIO_TLS
        if (client_impl::is_tls(uri))
            return client::ptr(new client_instance<client_config_tls>(uri));
        else
#endif
            return client::ptr(new client_instance<client_config>(uri));
    }

    client::~client()
    {
    }

    client::client()
    {

    }

    void client::run_loop()
    {
        return client_impl::run_loop();
    }
}
