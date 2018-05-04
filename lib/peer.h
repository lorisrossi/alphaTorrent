#ifndef PEER_H
#define PEER_H

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <exception>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>

#include <glog/logging.h>   //Logging Library

#define DEFAULT_BUFF_SIZE 128

//Namespace Peer Wire Protocol
namespace pwp{

    using namespace boost::asio;


    enum client_state{
        am_choking,
        am_interested
    };

    enum peer_state{
        peer_choking,
        peer_interested
    };

    struct peer{
        ip::address addr;
        uint port;
        std::string peer_id;
    };


    struct peer_connection{
        struct peer peer_t;
        client_state cstate;
        peer_state pstate;
        //Add connection instance here
    };

    typedef std::shared_ptr<std::vector<pwp::peer>> PeerList;

}



void manage_peer_connection(pwp::PeerList peer_list, char *info_hash);
void get_peer_id(std::string *id);
void build_handshake(char *info_hash, std::array<char, 256> &handshake);
void send_handshake(const pwp::peer t_peer, const std::array<char, 256> handshake, std::array<char, 256> &response);
void handshake_request_manager(const std::array<char, 256> &handshake, const pwp::peer t_peer, const char *info_hash);
int verify_handshake(const std::array<char, 256> handshake, const pwp::peer t_peer, const char *info_hash);
#endif