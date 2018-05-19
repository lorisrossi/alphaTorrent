/**
 * @file peer.h
 *
 * Peer's data and PWP protocol interface
 *
 * This define the basic PWP protocol structure along with peer interface
 * 
 */

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
#include <boost/chrono.hpp>
#include <boost/dynamic_bitset.hpp>

#include <glog/logging.h>   //Logging Library


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <boost/endian/conversion.hpp>
#include <cmath>

#include <boost/lambda/lambda.hpp>

#include "torrentparser.hpp"

#define DEFAULT_BUFF_SIZE 128


extern boost::asio::io_service _io_service;
extern int active_peer;
extern boost::mutex mtx_peer_num;



inline void add_active_peer();
inline void rm_active_peer();

/**
 * Namespace used to define all peer's related data and PWP protocol
 * 
 */
namespace pwp{

    using namespace boost::asio;

    

    typedef struct{
        bool am_choking = true;
        bool am_interested = true;
    }client_state ;

    typedef struct {
        bool peer_choking = true;  
        bool peer_interested = false;
    }peer_state;

    struct peer{
        ip::address addr;       /*!< Peer's IP address (both v4 and v6) */
        uint port;
        std::string peer_id;    /*!< 20 bytes peer id string */

        bool operator==(peer const & rhs) const {
            if(this->addr == rhs.addr && this->port == rhs.port)
                return true;
            return false;
        }

        bool operator>(peer const & rhs) const {
            if(this->peer_id > rhs.peer_id)
                return true;
            return false;
        }
    };


    struct peer_connection{
        struct peer peer_t;
        client_state cstate;
        peer_state pstate;
        boost::dynamic_bitset<> bitfield;
        std::shared_ptr<boost::asio::ip::tcp::socket> socket;
    };






    typedef std::shared_ptr<std::vector<pwp::peer>> PeerList;
    typedef std::shared_ptr<std::vector<pwp::peer_connection>> PeerConnected;
}


struct bInt{
    uint8_t i1;
    uint8_t i2;
    uint8_t i3;
    uint8_t i4;
};

typedef struct bInt bInt;


void manage_peer_connection(pwp::PeerList peer_list, char *info_hash);
void get_peer_id(std::string *id);
void build_handshake(char *info_hash, std::vector<uint8_t> &handshake);
int send_handshake(pwp::peer_connection& peerc_t, const std::vector<uint8_t> handshake, std::vector<uint8_t> &response);
void handshake_request_manager(const std::array<char, 256> &handshake, const pwp::peer t_peer, const char *info_hash, pwp::PeerConnected valid_peer);
int verify_handshake(const std::vector<uint8_t> handshake, size_t len,  const pwp::peer t_peer, const char *info_hash);

void remove_invalid_peer(pwp::PeerList peer_list);
void pwp_protocol_manager(pwp::peer peer_t, const std::vector<uint8_t> &handshake, const char *info_hash, Torrent &torrent);
uint32_t make_int(bInt bint);
uint32_t make_int(std::vector<uint8_t> v);
std::vector<uint8_t> from_int_to_bint(uint integer);
std::vector<uint8_t> from_int64_to_bint(uint64_t integer);

std::string string_to_hex(const std::vector<uint8_t>& input);


static void handle_receive(const boost::system::error_code& ec, std::size_t length, boost::system::error_code* out_ec, std::size_t* out_length);
void check_deadline_udp(boost::asio::ip::udp::socket &socket, boost::asio::deadline_timer &timer_);
void check_deadline_tcp(std::shared_ptr<boost::asio::ip::tcp::socket> socket, boost::asio::deadline_timer &timer_);

#endif