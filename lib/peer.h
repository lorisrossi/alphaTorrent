#include <boost/asio.hpp>
#include <string>

namespace peer_namespace{

    using namespace boost::asio;

    struct peer{
        ip::address addr;
        uint port;
        std::string peer_id;
    };


}