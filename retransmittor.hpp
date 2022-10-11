#pragma once

#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <boost/asio.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/pool/pool_alloc.hpp>

#include "async.h"
#include "asio_async_server.hpp"

namespace async_server
{

class Retransmittor
{
    class DataPreprocessor
    {
    public:
        enum class BlockType { STATIC, DYNAMIC };
        using where_to_cut_t = std::vector<std::pair<size_type, BlockType>>;

        where_to_cut_t run(handler_t h, const char* data, size_type data_size, bool update_state = true);

        void remove(handler_t h);
        void reset(handler_t h);

    private:
        struct helper
        {
            size_type m_braces_cntr{0};
            BlockType m_type{BlockType::STATIC};
        };

        std::unordered_map<handler_t, helper> m_map;
    };

public:
    Retransmittor() = default;
    Retransmittor(size_type bulk_size);
    ~Retransmittor();

    void on_read(rc_data&& data);
    void on_socket_close(size_type address);
//    void on_write(); ???

private:
    size_type m_bulk_size{3};
    handler_t m_static_bulks_handler{nullptr};
    DataPreprocessor m_data_preprocessor;

    /// \note The allocator is specified because gcc-11 fails to compile the code
    ///       with default allocator and c++20.
    ///       In gcc-12 the problem is fixed.
    boost::bimap<boost::bimaps::unordered_set_of<size_type>
                ,async::handler_t
                ,boost::fast_pool_allocator<std::pair<size_type, async::handler_t>>
                //,boost::container::allocator<std::pair<std::string, async::handler_t>>
                > m_endpoints_handlers;
};

}
