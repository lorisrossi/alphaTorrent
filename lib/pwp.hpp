#ifndef PWP_H
#define PWP_H

#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "peer.h"

#define KEEP_ALIVE_TIME 5

extern boost::asio::io_service _io_service;  
extern int active_peer;
extern boost::mutex mtx_peer_num;



bool is_inv_address(const boost::asio::ip::address& addr);

namespace pwp_msg{

    enum msg_id{
        chocked = 0x00,
        unchocked = 0x01,
        bitfield = 0x05,
        piece = 0x07, 
    };



    const std::vector<uint8_t> choke_msg = {0,0,0,1,0};
    const std::vector<uint8_t> unchoke_msg = {0,0,0,1,1};
    const std::vector<uint8_t> interested_msg = {0,0,0,1,2};
    const std::vector<uint8_t> non_interested_msg = {0,0,0,1,3};

    //void send_keep_alive(pwp::peer_connection peerc_t);
    void enable_keep_alive_message(pwp::peer_connection& peerc_t);
    int send_msg(pwp::peer_connection& peerc_t, std::vector<uint8_t> msg);

    std::vector<uint8_t> craft_have_msg(int piece_index);

    void read_msg_handler(std::vector<uint8_t>& response, pwp::peer_connection& peer_c, const boost::system::error_code& error, size_t bytes_read);

}

#endif