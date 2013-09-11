/*
   Copyright (C) 2013 by Pierre Talbot <ptalbot@mopong.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "tests/umcd/send_receive_test.hpp"
#include "umcd/protocol/make_header.hpp"
#include "umcd/client/client.hpp"
#include "umcd/server/transfer_events.hpp"

#include "config.hpp"
#include "serialization/parser.hpp"
#include "wml_exception.hpp"

#include <boost/make_shared.hpp>

#include <exception>
#include <fstream>
#include <iostream>
#include <cassert>

using namespace umcd;

std::size_t send_receive_test::test_num_ = 0;

send_receive_test::send_receive_test(const std::string& test_name
    , const std::string& data_to_send_filename
    , const std::string& expected_schema_filename)
: data_to_send_filename_(data_to_send_filename)
, expected_schema_filename_(expected_schema_filename)
, test_name_(test_name)
, test_no_(++test_num_)
{}

void send_receive_test::async_launch(boost::asio::io_service& io_service)
{
	boost::shared_ptr<client> c = boost::make_shared<client>(boost::ref(io_service));
	c->on_event<try_connecting_with_ip>(boost::bind(&send_receive_test::on_try_to_connect, shared_from_this(), _1));
  c->on_event<connection_success>(boost::bind(&send_receive_test::on_connection_success, shared_from_this(), _1));
  c->on_event<connection_failure>(boost::bind(&send_receive_test::on_connection_failure, shared_from_this(), _1));
  c->async_connect("localhost", "12522");
}

void send_receive_test::on_connection_failure(const boost::system::error_code& error)
{
  error_printer(BOOST_CURRENT_FUNCTION, error.message());
}

void send_receive_test::on_connection_success(const boost::shared_ptr<boost::asio::ip::tcp::socket>& socket)
{
  std::cout << test_no_str() << " Connected to " << socket->remote_endpoint() << std::endl;
  socket_ = socket;
  begin_transfer();
}

void send_receive_test::on_try_to_connect(const std::string& host_ip)
{
  std::cout << test_no_str() << " Trying to connect at " << host_ip << std::endl;
}

void send_receive_test::begin_transfer()
{
	assert(static_cast<bool>(socket_));

	std::ifstream request_file(data_to_send_filename_.c_str());
  config request;
  ::read(request, request_file);

  boost::shared_ptr<header_sender> sender = make_header_sender(socket_, request);
  sender->on_event<transfer_complete>(boost::bind(&send_receive_test::async_receive, shared_from_this()));
  sender->on_event<transfer_error>(boost::bind(&send_receive_test::on_error, shared_from_this(), BOOST_CURRENT_FUNCTION, _1));
  sender->async_send();
}

void send_receive_test::async_receive()
{
	boost::shared_ptr<header_receiver> receiver = make_header_receiver(socket_, response_);
  receiver->on_event<transfer_complete>(boost::bind(&send_receive_test::on_receive_complete, shared_from_this()));
  receiver->on_event<transfer_error>(boost::bind(&send_receive_test::on_error, shared_from_this(), BOOST_CURRENT_FUNCTION, _1));
  receiver->async_receive();
}

void send_receive_test::on_receive_complete()
{
	try
	{
		validator_type validator(expected_schema_filename_);
		validator.validate(response_, "", 0, "");
		success_printer();
		std::cerr << test_no_str() << " Response received:\n" << response_.to_string() << std::endl;
	}
	catch(const twml_exception& e)
	{
		error_printer(BOOST_CURRENT_FUNCTION, e.dev_message);
		std::cerr << test_no_str() << " Response received:\n" << response_.to_string() << std::endl;
	}
	catch(const std::exception& e)
	{
		error_printer(BOOST_CURRENT_FUNCTION, e.what());
		std::cerr << test_no_str() << " Response received:\n" << response_.to_string() << std::endl;
	}
}

void send_receive_test::on_error(const std::string &from_function, const boost::system::error_code& error)
{
	error_printer(from_function, error.message());
}

void send_receive_test::error_printer(const std::string &from_function, const std::string &error_message) const
{
	std::cerr << test_no_str() << " Failure during test \"" << test_name_ << "\" from function " 
		<< from_function << " with error: " << error_message << "." << std::endl;
}

void send_receive_test::success_printer() const
{
	std::cout << test_no_str() << " Success of test \"" << test_name_ << "\"." << std::endl;
}

std::string send_receive_test::test_no_str() const
{
	return "[" + boost::lexical_cast<std::string>(test_no_) + "/" + boost::lexical_cast<std::string>(test_num_) + "]";
}
