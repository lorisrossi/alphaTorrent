/**
 * @file pwp.hpp
 *
 * PWP protocol function 
 *
 * PWP protocol messagge specification and implementation
 * 
 */


#ifndef PWP_H
#define PWP_H

#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "torrentparser.hpp"
#include "peer.h"

#define KEEP_ALIVE_TIME 75  /*!< Default chocked message */

extern boost::asio::io_service _io_service;  
extern int active_peer;
extern boost::mutex mtx_peer_num;


bool is_inv_address(const boost::asio::ip::address& addr);

namespace pwp_msg{

    /*! PWP Message ID */
    enum msg_id{
        chocked = 0x00, /*!< Chocked state*/
        unchocked = 0x01, /*!< Unchocked state */
        interested = 0x02,
        not_interested = 0x03,
        have = 0x04,
        bitfield = 0x05, /*!< Bitfield sended/received */
        request = 0x06,
        piece = 0x07, 
        cancel = 0x08,
        port = 0x09
    };

    const std::vector<uint8_t> choke_msg = {0,0,0,1,0};         /*!< Default chocked message */
    const std::vector<uint8_t> unchoke_msg = {0,0,0,1,1};       /*!< Default unchocked message */
    const std::vector<uint8_t> interested_msg = {0,0,0,1,2};    /*!< Default interested message */
    const std::vector<uint8_t> non_interested_msg = {0,0,0,1,3};/*!< Default non interested message */

    void enable_keep_alive_message(pwp::peer_connection& peerc_t);
    int send_msg(pwp::peer_connection& peerc_t, std::vector<uint8_t> msg);

    int get_bitfield(pwp::peer_connection &peer_c, Torrent &torrent);
    void read_msg_handler(std::vector<uint8_t>& response, pwp::peer_connection& peer_c, Torrent &torrent, bool& dead_peer,  boost::asio::deadline_timer &timer_, const boost::system::error_code& error, size_t bytes_read);
    int sender(pwp::peer_connection &peer_conn, Torrent &torrent, int &old_begin);
}

#endif