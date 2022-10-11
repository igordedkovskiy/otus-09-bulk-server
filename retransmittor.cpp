#include <iostream>
#include "retransmittor.hpp"
#include "CmdCollector.hpp"

using namespace async_server;

Retransmittor::Retransmittor(size_type bulk_size):
    m_bulk_size{bulk_size},
    m_static_bulks_handler{async::connect_with(bulk_size)}
{}

Retransmittor::~Retransmittor()
{
    async::disconnect(m_static_bulks_handler);
    for(const auto& [set, h]:m_endpoints_handlers.right)
        async::disconnect(h);
}

void Retransmittor::on_read(rc_data&& data)
{
    const auto addr{std::move(data.m_endpoint)};
    auto el{m_endpoints_handlers.left.find(addr)};
    async::handler_t h{nullptr};
    if(el == m_endpoints_handlers.left.end())
    {
        h = async::connect_with(m_bulk_size);
        m_endpoints_handlers.insert({std::forward<decltype(addr)>(addr), h});
    }
    else
        h = el->second;
    try
    {
        auto where = m_data_preprocessor.run(h, data.m_data.c_str(), data.m_data.size()-1);
        std::size_t start{0};
        for(auto [end, type]:where)
        {
            if(type == DataPreprocessor::BlockType::STATIC)
                async::receive(m_static_bulks_handler, data.m_data.c_str() + start, end - start);
            else
                async::receive(h, data.m_data.c_str() + start, end - start);
            start = end;
        }
    }
    catch(CmdCollector::ParseErr& e)
    {
        m_data_preprocessor.reset(h);
        throw e;
    }
}

void Retransmittor::on_socket_close(size_type address)
{
    auto el{m_endpoints_handlers.left.find(address)};
    if(el != m_endpoints_handlers.left.end())
    {
        const auto h{el->second};
        async::disconnect(h);
        m_data_preprocessor.remove(h);
        el = m_endpoints_handlers.left.erase(el);
    }
}

Retransmittor::DataPreprocessor::where_to_cut_t Retransmittor::DataPreprocessor::run(handler_t h, const char *data, size_type data_size, bool update_state)
{
    auto el{m_map.find(h)};
    if(el == std::end(m_map))
    {
        auto e{m_map.insert(std::make_pair(h, helper{}))};
        el = e.first;
    }

    auto properties{el->second};
    auto& [braces_cntr, curent_type]{properties};

    where_to_cut_t result;

    for(size_type cntr{0}; cntr < data_size; ++cntr)
    {
        if(data[cntr] == '{')
        {
            if(braces_cntr == 0 && cntr > 1)
                result.emplace_back(std::make_pair(cntr, BlockType::STATIC));

            ++braces_cntr;
            curent_type = BlockType::DYNAMIC;
        }
        else if(data[cntr] == '}')
        {
            if(braces_cntr == 0)
                throw CmdCollector::ParseErr::incorrect_format;
            if(--braces_cntr == 0)
            {
                curent_type = BlockType::STATIC;
                result.emplace_back(std::make_pair(cntr + 1, BlockType::DYNAMIC));
            }
        }
    }

    if(update_state)
        el->second = properties;
    if(result.empty())
        result.emplace_back(std::make_pair(data_size, curent_type));
    return result;
}

void Retransmittor::DataPreprocessor::remove(handler_t h)
{
    auto el{m_map.find(h)};
    if(el != std::end(m_map))
        el = m_map.erase(el);
}

void Retransmittor::DataPreprocessor::reset(handler_t h)
{
    auto el{m_map.find(h)};
    if(el != std::end(m_map))
        el->second = helper{};

}
