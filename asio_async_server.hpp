//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <cstdlib>
#include <boost/asio.hpp>

#include "async.h"

namespace async_server
{

using boost::asio::ip::tcp;

using size_type = async::size_type;
using handler_t = async::handler_t;

using endpoint_t = boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>;
struct rc_data
{
    rc_data() = default;
    rc_data(const char* data, std::size_t size, const endpoint_t& epoint);
    rc_data(const char* data, std::size_t size, size_type epoint);

    const std::string m_data;
    const size_type m_endpoint{0};
};

class Retransmittor;

class session: public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket, Retransmittor& r);
    ~session();

    void start();

private:
    void do_read();
//    void do_write(std::size_t length);
    void close();

    tcp::socket m_socket;
    static constexpr std::size_t data_max_length{1024};
    char m_data[data_max_length];
    char m_wdata[data_max_length];
    const size_type m_socket_addr_hash{0};
    Retransmittor& m_retransmittor;
};

class server
{
public:
    server(boost::asio::io_context& io_context, short port, Retransmittor& r);

private:
    void do_accept();

    tcp::acceptor m_acceptor;
    Retransmittor& m_retransmittor;
};

}
