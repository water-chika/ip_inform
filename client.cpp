#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>

std::string get_ip_address_info();

class PipeExec {
public:
  PipeExec(const char *cmd, const char *modes = "r")
      : c_file{popen(cmd, modes)} {
    if (c_file == nullptr) {
      throw std::logic_error("popen fail");
    }
  }
  PipeExec(const PipeExec &) = delete;
  PipeExec &operator=(const PipeExec &) = delete;
  PipeExec(PipeExec &&) = default;
  PipeExec &operator=(PipeExec &&) = default;
  ~PipeExec() noexcept { pclose(c_file); }

  int getc() { return fgetc(c_file); }

private:
  FILE *c_file;
};

namespace ip = boost::asio::ip;
class IPInformer {
  using tcp = boost::asio::ip::tcp;
  using io_context_t = boost::asio::io_context;
  using endpoint_t = tcp::endpoint;
  using seconds_t = std::chrono::seconds;
  using socket_t = tcp::socket;
  using timer_t = boost::asio::steady_timer;
  using socket_ptr_t = std::shared_ptr<socket_t>;

public:
  IPInformer(io_context_t &io_context, endpoint_t endpoint,
             seconds_t sleep_time, seconds_t retry_time)
      : m_io_context{io_context},
      m_endpoint{std::move(endpoint)},
        m_sleep_time{std::move(sleep_time)},
        m_retry_time{std::move(retry_time)} {
    try_connect();
  }
  void try_connect() {
    std::cout << "try connect to server" << std::endl;
    auto socket = std::make_shared<socket_t>(m_io_context);
    socket->async_connect(m_endpoint, [this, socket = socket](auto &&error) {
      if (!error) {
        inform_ip(std::move(socket));
        auto timer = std::make_shared<timer_t>(m_io_context,m_sleep_time);
        timer->async_wait([this, timer = timer](auto &&error) {
          if (!error) {
            try_connect();
          }
        });
      } else {
        auto timer = std::make_shared<timer_t>(m_io_context,m_retry_time);
        timer->async_wait([this, timer = timer](auto &&error) {
          if (!error) {
            try_connect();
          }
        });
      }
    });
  }
  void inform_ip(socket_ptr_t socket) {
    std::string buf{get_ip_address_info()};
    boost::system::error_code error;

    size_t len = socket->write_some(boost::asio::buffer(buf), error);
    std::cout << "will sleep some time" << std::endl;
  }
  void set_retry_time(seconds_t retry_time) {
    m_retry_time = retry_time;
  }
  void set_sleep_time(seconds_t sleep_time) {
    m_sleep_time = sleep_time;
  }

private:
  io_context_t& m_io_context;
  endpoint_t m_endpoint;
  seconds_t m_retry_time;
  seconds_t m_sleep_time;
};

int main(int argc, char *argv[]) {
  try {
    std::string host;
    ip::port_type port;
    int retry_time, sleep_time;
    namespace po = boost::program_options;
    using namespace std::literals;
    po::options_description desc{"Allowed options"};
    desc.add_options()("help", "Produce help")(
        "host", po::value<std::string>(&host), "Server host name")(
        "port", po::value<ip::port_type>(&port),
        "Server port")("retry_time", po::value(&retry_time)->default_value(60),
                       "Retry time, unit is seconds")(
        "sleep_time", po::value(&sleep_time)->default_value(60 * 60),
        "Sleep time, unit is seconds");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << std::endl;
      return 1;
    }

    boost::asio::io_context io_context;
    IPInformer ip_informer{
        io_context,
        boost::asio::ip::tcp::endpoint(ip::make_address(std::move(host)), port),
        std::chrono::seconds(sleep_time), std::chrono::seconds(retry_time)};

    io_context.run();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}

std::string get_ip_address_info() {
  PipeExec pipe_exec{"ip address show"};
  std::stringbuf info;
  int c;
  while ((c = pipe_exec.getc()) != EOF) {
    info.sputc(c);
  }
  return std::move(info).str();
}
