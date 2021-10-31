#include <iostream>
#include <vector>
#include <pthread.h>
#include <serial/serial.h>

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <json.hpp> 

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using nlohmann::json;
typedef server::message_ptr message_ptr;

void enumerate_ports()
{
        std::vector<serial::PortInfo> devices_found = serial::list_ports();

        std::vector<serial::PortInfo>::iterator iter = devices_found.begin();

        while( iter != devices_found.end() )
        {
                serial::PortInfo device = *iter++;

                printf( "(%s, %s, %s)\n", device.port.c_str(), device.description.c_str(),
     device.hardware_id.c_str() );
        }
}

void on_message(server* s, websocketpp::connection_hdl hdl, message_ptr msg) 
{
	try
	{
		s->send(hdl, "Hi.\n", msg->get_opcode());
		s->poll();
	}
	catch (json::exception const & e)
	{
		std::cout << "json failed because: "
			<< "(" << e.what() << ")" << std::endl;

	}
	catch (websocketpp::exception const & e) 
	{
		std::cout << "websocket failed because: "
			<< "(" << e.what() << ")" << std::endl;
	}
	catch (std::exception const & e)
	{
		std::cout << "something else failed because: "
			<< "(" << e.what() << ")" << std::endl;
	}
	 
}

int main()
{	
	enumerate_ports();
	
	server echo_server;

	try
	{
		echo_server.set_access_channels(websocketpp::log::alevel::none);
		echo_server.clear_access_channels(websocketpp::log::alevel::none);

		echo_server.init_asio();
		echo_server.set_message_handler(bind(&on_message, &echo_server, ::_1, ::_2));
		echo_server.listen(2082);
		echo_server.start_accept();

		echo_server.run();
	}
	catch (websocketpp::exception const& e)
	{
		std::cout << e.what() << std::endl;
	}
	catch (...) {
		std::cout << "other exception" << std::endl;
	}
	
	return 0;
}
	
