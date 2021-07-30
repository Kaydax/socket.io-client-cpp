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
        if (client_base::is_tls(uri))
            return shared_ptr<client>(new client_impl<client_type_tls>(uri));
        else
#endif
            return shared_ptr<client>(new client_impl<client_type_no_tls>(uri));
    }

    client::~client()
    {
    }

    client::client()
    {

    }

}
