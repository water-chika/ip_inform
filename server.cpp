#include <boost/asio.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/system/system_error.hpp>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>

using boost::asio::ip::tcp;

class IPInformServer {
public:
  IPInformServer(boost::asio::io_context &io_context,
                 uint16_t port)
      : m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
          try_accept();
        }
  void try_accept() {
    m_acceptor.async_accept([this](auto &&error, tcp::socket socket) {
      if (!error) {
        std::cout << "accept one socket" << std::endl;
        auto current_time = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        std::cout << "current system time : "
                  << std::put_time(localtime(&current_time), "%c %Z")
                  << std::endl;
        receive_ip_info(std::make_shared<tcp::socket>(std::move(socket)));
      } else {
        std::cout << "acceptor async accept error : " << error << std::endl;
      }
      try_accept();
    });
  }
  static void receive_ip_info(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
    auto buf = std::make_shared<std::array<char, 1024*4>>();
    receive_ip_info(std::move(socket), buf);
  }
  static void receive_ip_info(std::shared_ptr<boost::asio::ip::tcp::socket> socket, std::shared_ptr<std::array<char, 1024*4>> buf) {
    boost::asio::async_read(*socket, boost::asio::buffer(*buf), [buf=buf, socket=socket](auto&& error, auto&& size) {
      std::cout.write(buf->data(), size);
      if (!error) {
        receive_ip_info(socket, buf);
      }
      else {
        std::cout << std::endl;
      }
    });
  }

private:
  tcp::acceptor m_acceptor;
};

int main(int argc, char **argv) {
  try {
    uint16_t port=0;
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
     ("help", "Produce help messages")
     ("port", po::value<decltype(port)>(&port)->default_value(8888), "Server port");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 1;
    }

    boost::asio::io_context io_context;
    IPInformServer server(io_context, port);
    io_context.run();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
