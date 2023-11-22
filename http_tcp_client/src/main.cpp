
//
// Example: HTTP client, asynchronous
//

// Quickly add boost DLLs with: https://www.nuget.org/packages/boost-vc141/

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <random>
#include <string_view>

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <exchange.pb.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std::literals;

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}


std::string RandomString(size_t length){
    const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwyz";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    std::string random_string;

    for(size_t i = 0; i < length; ++i){
        random_string += characters[distribution(generator)];
    }

    return random_string;
}

// Performs an HTTP GET and prints the response
class session : public std::enable_shared_from_this<session>
{
    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;

public:
    // Objects are constructed with a strand to
    // ensure that handlers do not execute concurrently.
    explicit
        session(net::io_context& ioc)
        : resolver_(net::make_strand(ioc))
        , stream_(net::make_strand(ioc))
    {
    }

    // Start the asynchronous operation
    void
        run(
            char const* host,
            char const* port,
            char const* target,
            char const* body,
            int version)
    {
        // Set up an HTTP POST request message
        req_.version(version);
        std::cout << version << std::endl;
        req_.method(http::verb::get);
        req_.target(target);
        req_.set(http::field::content_type, "text/html");

        size_t object_size{128*100};
        char object[object_size];
        google::protobuf::io::ArrayOutputStream object_stream( object, sizeof( object ) );
        Exchange::ClientToServer client_to_server;
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t

        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.add_hashes(RandomString(128)); // uint32_t
        client_to_server.SerializeToZeroCopyStream( &object_stream );

        std::string body_str{object, static_cast<size_t>(object_stream.ByteCount())};
        req_.body() = body_str;
        req_.content_length(body_str.size());
        req_.prepare_payload();

        // Look up the domain name
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &session::on_resolve,
                shared_from_this()));
    }

    void
        on_resolve(
            beast::error_code ec,
            tcp::resolver::results_type results)
    {
        if (ec)
            return fail(ec, "resolve");

        // Set a timeout on the operation
        stream_.expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        stream_.async_connect(
            results,
            beast::bind_front_handler(
                &session::on_connect,
                shared_from_this()));
    }

    void
        on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if (ec)
            return fail(ec, "connect");

        // Set a timeout on the operation
        stream_.expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        http::async_write(stream_, req_,
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }

    void
        on_write(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Receive the HTTP response
        http::async_read(stream_, buffer_, res_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
        on_read(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "read");

        // Write the message to standard out
        Exchange::ServerToClient server_to_client;
        try {
            if(server_to_client.ParseFromArray( res_.body().data(), res_.body().size())){
                std::cout << "Parse server response Ok, blocks count "sv << server_to_client.hash_and_block_size() << std::endl;
            }else{
                std::cout << "Parse server response error"sv << std::endl;
            }
        }
        catch (...) {
            std::cout << "Parse server response error by exception"sv << std::endl;
        }

        // Gracefully close the socket
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes so don't bother reporting it.
        if (ec && ec != beast::errc::not_connected)
            return fail(ec, "shutdown");

        // If we get here then the connection is closed gracefully
    }
};

std::string create_body()
{
    boost::property_tree::ptree tree;
    tree.put("foo", "bar");
    std::basic_stringstream<char> jsonStream;
    boost::property_tree::json_parser::write_json(jsonStream, tree, false);
    return jsonStream.str();
}

int main(int argc, char** argv)
{
    // Check command line arguments.
    if (argc != 4 && argc != 5)
    {
        std::cerr <<
            "Usage: http-client-async <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
            "Example:\n" <<
            "    http-client-async www.example.com 80 /\n" <<
            "    http-client-async www.example.com 80 / 1.0\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const target = argv[3];
    int version = argc == 5 && !std::strcmp("1.0", argv[4]) ? 10 : 11;

    // The io_context is required for all I/O
    net::io_context ioc;

    // Launch the asynchronous operation
    std::make_shared<session>(ioc)->run(host, port, target, create_body().c_str(), version);

    // Run the I/O service. The call will return when
    // the get operation is complete.
    ioc.run();

    return EXIT_SUCCESS;
}
