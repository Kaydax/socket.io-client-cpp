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
			return std::make_shared<client_impl>(uri);
			//return shared_ptr<sio::client>(new client_impl(uri));
		}

		client::~client()
    {
    }

		client::client()
		{

		}

}
