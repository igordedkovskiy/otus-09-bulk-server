#include <vector>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cassert>
#include "gtest/gtest.h"

#include "read_input.hpp"
#include "async.h"
#include "retransmittor.hpp"

namespace
{

constexpr async::size_type bulk_size{3};

auto find_file(std::string masks)
{
    const auto regexp_cmp{std::regex(masks)};
    namespace fs = std::filesystem;
    const auto path{fs::absolute(".")};
//    const auto path{fs::absolute("./bulk")};
    using iterator = fs::directory_iterator;
    std::vector<std::string> fnames;
    for(auto it{iterator(path)}; it != iterator(); ++it)
    {
        const auto path{fs::absolute(it->path())};
        if(!fs::is_directory(path))
        {
            const auto fname{path.filename().string()};
            if(regex_match(fname, regexp_cmp))
                fnames.emplace_back(std::move(fname));
        }
    }
    return fnames;
}

bool check(std::stringstream ref, std::string masks)
{
    const auto fname{find_file(std::move(masks))};
    if(fname.empty())
    {
        std::cout << "File not found!\n";
        return false;
    }
    for(const auto& fn:fname)
    {
        std::fstream file{fn, std::fstream::in};
        std::stringstream out;
        out << file.rdbuf();
        if(out.str() == ref.str())
        {
            std::cout << "File " << fn << " fits" << std::endl;
            std::cout << "File: " << out.str() << std::endl;
            std::cout << "Ref:  " << ref.str() << std::endl;
            std::cout << std::endl;
            return true;
        }
        std::cout << "File " << fn << " doesn't fit" << std::endl;
        std::cout << "File: " << out.str() << std::endl;
        std::cout << "Ref:  " << ref.str() << std::endl;
        std::cout << std::endl;
    }
    return false;
}

}

TEST(TEST_ASYNC, async_sinlge_thread)
{
    const auto h1{async::connect_with(bulk_size)};
    const auto h2{async::connect_with(bulk_size)};

    {
        async::receive(h2, "cmd1\n", 5);
        async::receive(h1, "cmd1\ncmd2\n", 10);
        async::receive(h2, "cmd2\n{\ncmd3\ncmd4\n}\n", 19);
        async::receive(h2, "{\n", 2);
        async::receive(h2, "cmd5\ncmd6\n{\ncmd7\ncmd8\n}\ncmd9\n}\n", 31);
        async::receive(h1, "cmd3\ncmd4\n", 10);
        async::receive(h2, "{\ncmd10\ncmd11\n", 12);
        async::receive(h1, "cmd5\n", 5);

        async::disconnect(h1);
        async::disconnect(h2);
    }

    // context 1
    std::cout << "masks:" << std::endl;
    std::cout << std::string{"(bulk-" + std::to_string((unsigned long long)h1) + "-.*.log)"} << std::endl;
    ASSERT_TRUE(check(std::stringstream{"bulk: cmd1, cmd2, cmd3\n"},
                      "(bulk-" + std::to_string((unsigned long long)h1) + "-.*.log)"));
    std::cout << std::string{"(bulk-" + std::to_string((unsigned long long)h1) + "-.*.log)"} << std::endl;
    ASSERT_TRUE(check(std::stringstream{"bulk: cmd4, cmd5\n"},
                      "(bulk-" + std::to_string((unsigned long long)h1) + "-.*.log)"));

    // context 2
    std::cout << std::string{"(bulk-" + std::to_string((unsigned long long)h2) + "-.*.log)"} << std::endl;
    ASSERT_TRUE(check(std::stringstream{"bulk: cmd1, cmd2\n"},
                      "(bulk-" + std::to_string((unsigned long long)h2) + "-.*.log)"));
    std::cout << std::string{"(bulk-" + std::to_string((unsigned long long)h2) + "-.*.log)"} << std::endl;
    ASSERT_TRUE(check(std::stringstream{"bulk: cmd3, cmd4\n"},
                      "(bulk-" + std::to_string((unsigned long long)h2) + "-.*.log)"));
    std::cout << std::string{"(bulk-" + std::to_string((unsigned long long)h2) + "-.*.log)"} << std::endl;
    ASSERT_TRUE(check(std::stringstream{"bulk: cmd5, cmd6, cmd7, cmd8, cmd9\n"},
                      "(bulk-" + std::to_string((unsigned long long)h2) + "-.*.log)"));
}

TEST(TEST_CMD_HANDLER, retransmittor_server_cmd_handler)
{
    auto run = []()
    {
        const std::size_t bulk_size{3};
        async_server::Retransmittor retransmittor{bulk_size};

        const std::size_t socket_addr_hash_1{1};
        const std::size_t socket_addr_hash_2{2};

        retransmittor.on_read({"If\n", sizeof("If\n"), socket_addr_hash_1});
        retransmittor.on_read({"a\n", sizeof("a\n"), socket_addr_hash_2});
        retransmittor.on_read({"search\n", sizeof("search\n"), socket_addr_hash_1});
        retransmittor.on_read({"for\n", sizeof("for\n"), socket_addr_hash_1});
        retransmittor.on_read({"{\nthe\nname\n", sizeof("{\nthe\nname\n"), socket_addr_hash_1});
        retransmittor.on_read({"{\nget_return_object_on_allocation_failure\nin\n",
                               sizeof("{\nget_return_object_on_allocation_failure\nin\n"), socket_addr_hash_2});
        retransmittor.on_read({"}\n", sizeof("}\n"), socket_addr_hash_2});
        retransmittor.on_read({"}\n", sizeof("}\n"), socket_addr_hash_1});
    };
    run();

    ASSERT_TRUE(check(std::stringstream{"bulk: If, a, search\n"},
                      "(bulk-.*-.*-.*.log)"));
    ASSERT_TRUE(check(std::stringstream{"bulk: for\n"},
                      "(bulk-.*-.*-.*.log)"));
    ASSERT_TRUE(check(std::stringstream{"bulk: the, name\n"},
                      "(bulk-.*-.*-.*.log)"));
    ASSERT_TRUE(check(std::stringstream{"bulk: get_return_object_on_allocation_failure, in\n"},
                      "(bulk-.*-.*-.*.log)"));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
