#include <msgpack.hpp>
#include <string>
#include <iostream>
#include <sstream>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/yield.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>

namespace detail {
    using Packer = msgpack::packer<msgpack::sbuffer>;

    template<class X>
        Packer& pack(Packer& pk, const X& x)
        {
              return pk << x;
        }
    template<class X, class Y, class...Z>
        Packer& pack(Packer& pk, const X &x, const Y &y, const Z &...z)
        {
              return pack(pack(pk, x), y, z...);
        }


    static Packer& pack(Packer& pk)
    {
          return pk;
    }
} // namespace detail

class Client {
    boost::asio::io_service &io_service_;
    boost::asio::ip::tcp::socket socket_;
    
    enum {
        REQUEST  = 0,
        RESPONSE = 1,
        NOTIFY   = 2
    };
    
public:
    Client(boost::asio::io_service &io_service)
        : io_service_(io_service),
          socket_(io_service)
    {

    }

    void start() {
        connect();
    }

private:

    void connect() {
        socket_.async_connect(
                boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 6666),
                boost::bind(&Client::on_connect, this, boost::asio::placeholders::error));
    }

    void on_connect(const boost::system::error_code &error) {
        if(error) {
            std::cout << "connect failed: " << error.message() << std::endl;
        } else {
            std::cout << "connected" << std::endl;
            send(); //debug vim_del_current_line
        }
    }
    
    template<typename...T>
    void send(const T&...t) {
        //msgpack::type::tuple<int, std::string, std::string> src(2, "vim_command", "vsplit");

        msgpack::sbuffer sbuf;
        detail::Packer pk(&sbuf);
        pk.pack_array(4) << (uint64_t)REQUEST
                         << (uint64_t)0 //msgid
                         << std::string("vim_list_runtime_paths");
        
        pk.pack_array(sizeof...(t));
        detail::pack(pk, t...);
        
        msgpack::object_handle oh = msgpack::unpack(sbuf.data(), sbuf.size());

        msgpack::object deserialized = oh.get();

        std::cout << "sbuf = " << deserialized << std::endl;

        boost::asio::spawn(io_service_, [this, &sbuf](boost::asio::yield_context yield_ctx){
            boost::system::error_code ec;
                
            boost::asio::async_write(socket_,
                boost::asio::buffer(std::string(sbuf.data(), sbuf.size())), yield_ctx[ec]);
            
            if(ec) {
                std::cout << "send failed: " << ec.message() << std::endl;
                return;
            } else {
                std::cout << "send correct!" << std::endl;
            }

            msgpack::unpacker unpacker;
            unpacker.reserve_buffer(32*1024ul);
            size_t size = async_read(socket_, boost::asio::buffer(unpacker.buffer(), unpacker.buffer_capacity()),
                boost::asio::transfer_at_least(1), yield_ctx[ec]);
            
            if(ec) {
                std::cout << "read failed: " << ec.message() << std::endl;
                return;
            } else {
                std::cout << "read correct!" << std::endl;
            }

            msgpack::unpacked result;
            unpacker.buffer_consumed(size);
            while(unpacker.next(result)) {
                const msgpack::object &obj = result.get();
                std::cout << "res = " << obj << std::endl;
                result.zone().reset();
            }
        });
    }
};

int main() {
    boost::asio::io_service io_service;
    Client client(io_service);

    client.start();
    //client.connect();
    io_service.run();

    //boost::asio::io_service::strand strand = socket.get_io_service();
    
    msgpack::type::tuple<int, bool, std::string> src(1, true, "example");

    std::stringstream buffer;
    msgpack::pack(buffer, src);

    buffer.seekg(0);

    std::string str(buffer.str()); //binary

    msgpack::object_handle oh = msgpack::unpack(str.data(), str.size());

    msgpack::object deserialized = oh.get();

    std::cout << deserialized << std::endl;
    
    msgpack::type::tuple<int, bool, std::string> dst;
    deserialized.convert(dst);

    sleep(5);

    return 0;
}
