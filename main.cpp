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
#include <unistd.h>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using nlohmann::json;
typedef server::message_ptr message_ptr;

#define BAUDRATE 921600
#define COM_PORT "/dev/ttyUSB0"
#define MAX_STACK_SIZE 500

void push_command(serial::Serial& my_serial, uint32_t address, uint32_t value)
{
	my_serial.write((uint8_t*)&address, sizeof address);
	my_serial.write((uint8_t*)&value, sizeof value);
} 

uint32_t read_command(serial::Serial& my_serial, uint32_t address, uint32_t value = 0)
{
	my_serial.write((uint8_t*)&address, sizeof address);
	my_serial.write((uint8_t*)&value, sizeof value);

	uint32_t a = 0;
	my_serial.read((uint8_t*)&a, sizeof a);

	return a;
}


std::string write(serial::Serial & my_serial, uint32_t address, uint32_t value)
{
	std::stringstream ss;

	push_command(my_serial, address, value);
	ss << "Writing [" << std::hex << address << "] -> 0x" << std::hex << value << std::endl;

	return ss.str();
}

void execute(serial::Serial & my_serial)
{
	push_command(my_serial, 1000, 0);

	printf("Writing execute.\n");
}

std::string read(serial::Serial & my_serial)
{
	std::stringstream ss;  

	ss.setf(std::ios::fixed, std::ios::floatfield);
	ss.setf(std::ios::showpoint);
	uint32_t value = 0;

	value = read_command(my_serial, 1001);
	ss << "Register A: " << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
	ss << " (" << std::dec << value << "u) " << "(" << std::dec << (int32_t)value << ") " << "(" << *(float*)((char*)&value) << "f)" << std::endl;

	value = read_command(my_serial, 1002);
	ss << "Register B: " << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
	ss << " (" << std::dec << value << "u) " << "(" << std::dec << (int32_t)value << ") " << "(" << *(float*)((char*)&value) << "f)" << std::endl;

	value = read_command(my_serial, 1003);
	ss << "Register R: " << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
	ss << " (" << std::dec << value << "u) " << "(" << std::dec << (int32_t)value << ") " << "(" << *(float*)((char*)&value) << "f)" << std::endl;

	ss << std::endl << "Flags:" << std::endl;

	ss << "Zero: " << std::dec << read_command(my_serial, 1004) << std::endl;
	ss << "Greater Than: " << std::dec << read_command(my_serial, 1005) << std::endl;
	ss << "Less Than: " << std::dec << read_command(my_serial, 1006) << std::endl;
	ss << "Out of Time: " << std::dec << read_command(my_serial, 1007) << std::endl;

	return ss.str();
}

void on_message(server* s, websocketpp::connection_hdl hdl, message_ptr msg) 
{
	try
	{
		std::stringstream ss;
		ss.setf(std::ios::fixed, std::ios::floatfield);
		ss.setf(std::ios::showpoint);

		ss << "Uploading.\n";

		std::hash<std::string> hasher;

		auto payload = msg->get_payload();
		auto message = json::parse(payload);
		auto hash = hasher(payload);

		serial::Serial my_serial(COM_PORT, BAUDRATE, serial::Timeout::simpleTimeout(30000));
		std::vector<uint32_t> stack = message["stack"];
		 
		if (stack.size() > MAX_STACK_SIZE)
		{
			ss << "Out of bounds write.\n";

			s->send(hdl, ss.str(), msg->get_opcode());
			s->poll();

			return;
		}

		int index = 0; 
		for (auto const& item : stack) 
		{
			ss << write(my_serial, index++, item);
		}

		ss << "Executing.\n";
		
		execute(my_serial);
		 
		while (read_command(my_serial, 1008))
		{
			usleep(100000 >> 2);
		}

		ss << std::endl << "Registers:" << std::endl;
		ss << read(my_serial) << std::endl;

		ss << "Memory Regions:" << std::endl;
		std::vector<json> readRegions = message["readRegions"];

		for (int i = 0; i < readRegions.size(); i++)
		{
			uint32_t address = readRegions[i]["address"];
			std::string name = readRegions[i]["name"];

			if (address > MAX_STACK_SIZE)
			{
				ss << "Out of bounds read.\n";
				continue;
			}

			ss << "[" << name << " -> 0x" << std::hex << std::setfill('0') << address << "] ";
			auto value = read_command(my_serial, 1009, address);

			ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
			ss << " (" << std::dec << value << "u) " << "(" << std::dec << (int32_t)value << ") " << "(" << *(float*)((char*)&value) << "f)" << std::endl;
		}

		if (readRegions.size() == 0) 
		{
			std::vector<uint32_t> regions = message["regions"];

			for (auto const& item : regions)
			{
				if (item > MAX_STACK_SIZE)
				{
					ss << "Out of bounds read.\n";
					continue;
				}

				ss << "[a" << std::dec << item - stack.size() << " -> 0x" << std::hex << std::setfill('0') << item << "] ";
				auto value = read_command(my_serial, 1009, item);

				ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
				ss << " (" << std::dec << value << "u) " << "(" << std::dec << (int32_t)value << ") " << "(" << *(float*)((char*)&value) << "f)" << std::endl;
			}
		}

		ss << "Finished.\n";

		s->send(hdl, ss.str(), msg->get_opcode());
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