#ifndef PWP_H
#define PWP_H

#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "peer.h"

#define KEEP_ALIVE_TIME 2

extern boost::asio::io_service _io_service;  

namespace pwp_msg{


    const std::vector<uint8_t> choke_msg = {0,0,0,1,0};
    const std::vector<uint8_t> unchoke_msg = {0,0,0,1,1};
    const std::vector<uint8_t> interested_msg = {0,0,0,1,2};
    const std::vector<uint8_t> non_interested_msg = {0,0,0,1,3};

    //void send_keep_alive(pwp::peer_connection peerc_t);
    void enable_keep_alive_message(pwp::peer_connection peerc_t);
    void send_msg(pwp::peer_connection& peerc_t, std::vector<uint8_t> msg);



    std::vector<uint8_t> craft_have_msg(int piece_index);
}

#endif