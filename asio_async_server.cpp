#include <iostream>
#include <memory>

#include "CmdCollector.hpp"
#include "asio_async_server.hpp"
#include "retransmittor.hpp"

using namespace async_server;

session::session(tcp::socket socket, Retransmittor& r):
    m_socket(std::move(socket)),
    m_socket_addr_hash{std::hash<std::string>{}(
                           m_socket.remote_endpoint().address().to_string() +
                           std::to_string(m_socket.remote_endpoint().port()))},
    m_retransmittor{r}
{
//    std::cout << "create session" << std::endl;
//    std::cout << m_socket.remote_endpoint().address() << ':'
//              << m_socket.remote_endpoint().port() << std::endl;
//    std::cout << m_socket_addr_hash << std::endl;
}

session::~session()
{
//    std::cout << __PRETTY_FUNCTION__ << std::endl;
    close();
}

void session::start()
{
    do_read();
}

void session::do_read()
{
    auto process = [this, self = shared_from_this()](boost::system::error_code ec, std::size_t length)
    {
        if(!ec)
        {
            try
            {
                m_retransmittor.on_read({m_data, length, m_socket_addr_hash});
            }
            catch(CmdCollector::ParseErr& e)
            {
                auto process = [this, self](boost::system::error_code ec, std::size_t /*length*/)
                {
                    if(!ec)
                        do_read();
                };
                boost::asio::async_write(m_socket,
                                         boost::asio::buffer("Incorrect format\n\0",
                                                             sizeof("Incorrect format\n\0")),
                                         process);
            }
            do_read();
        }
        else
        {
            if(ec == boost::asio::error::eof)
            {
//                std::cout << __PRETTY_FUNCTION__ << std::endl;
                m_retransmittor.on_socket_close(m_socket_addr_hash);
                close();
            }
            //else if(ec == boost::asio::error::connection_reset)
        }
    };
    m_socket.async_read_some(boost::asio::buffer(m_data, data_max_length), process);
}

//void session::do_write(std::size_t length)
//{
//    auto self(shared_from_this());
//    auto process = [this, self](boost::system::error_code ec, std::size_t /*length*/)
//    {
//        if(!ec)
//            do_read();
//    };
//    boost::asio::async_write(m_socket, boost::asio::buffer(m_data, length), process);
//}

void session::close()
{
    if(m_socket.is_open())
    {
//        std::cout << "close session" << std::endl;
//        std::cout << m_socket.remote_endpoint().address() << ':'
//                  << m_socket.remote_endpoint().port() << std::endl;
//        std::cout << m_socket_addr_hash << std::endl;

        m_socket.shutdown(boost::asio::socket_base::shutdown_both);
        m_socket.close();
    }
}

server::server(boost::asio::io_context &io_context, short port, Retransmittor& r):
    m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
    m_retransmittor{r}
{
    do_accept();
}

void server::do_accept()
{
    auto process = [this](boost::system::error_code ec, tcp::socket socket)
    {
        if(!ec)
            std::make_shared<session>(std::move(socket), m_retransmittor)->start();
        do_accept();
    };
    m_acceptor.async_accept(process);
}

rc_data::rc_data(const char *data, std::size_t size, const endpoint_t &epoint):
    m_data{data, size},
    m_endpoint{std::hash<std::string>{}(epoint.address().to_string() + std::to_string(epoint.port()))}
{}

rc_data::rc_data(const char *data, std::size_t size, size_type epoint):
    m_data{data, size},
    m_endpoint{epoint}
{}
