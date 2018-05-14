#ifndef TRACKER_UDP_H
#define TRACKER_UDP_H

#include <boost/asio.hpp>
#include <boost/random.hpp>
#include <boost/algorithm/string.hpp>   //Replace all '/' in the url
#include <boost/asio/deadline_timer.hpp>
#include <string>
#include <stdlib.h>
#include <stdint.h>

#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>


namespace t_udp{

    typedef struct{ 
        int64_t protocol_id = 0x41727101980; //Magic Constant
        int32_t action = 0; //Connect value
        int32_t transaction_id;
    }connect_request;

    typedef struct{ 
        int32_t action = 0; //Connect value
        int32_t transaction_id;
        int64_t connection_id;
    }connect_response;

/**
 * IPv4 announce request:

        Offset  Size    Name    Value
        0       64-bit integer  connection_id
        8       32-bit integer  action          1 // announce
        12      32-bit integer  transaction_id
        16      20-byte string  info_hash
        36      20-byte string  peer_id
        56      64-bit integer  downloaded
        64      64-bit integer  left
        72      64-bit integer  uploaded
        80      32-bit integer  event           0 // 0: none; 1: completed; 2: started; 3: stopped
        84      32-bit integer  IP address      0 // default
        88      32-bit integer  key
        92      32-bit integer  num_want        -1 // default
        96      16-bit integer  port
        98

 * 
 */

    typedef struct{
        int64_t connection_id;
        int32_t action;
        int32_t transaction_id;
        char info_hash[20]; //uchar or char?
        char peer_id[20];
        int64_t downloaded;
        int64_t left;
        int64_t uploaded;
        int32_t event;
        int32_t ip_address;
        int32_t key;
        int32_t num_want;
        int16_t port;
    }announce_request;

/**
 * IPv4 announce response:

    Offset      Size            Name            Value
    0           32-bit integer  action          1 // announce
    4           32-bit integer  transaction_id
    8           32-bit integer  interval
    12          32-bit integer  leechers
    16          32-bit integer  seeders
    20 + 6 * n  32-bit integer  IP address
    24 + 6 * n  16-bit integer  TCP port
    20 + 6 * N

 */



    typedef struct{
        int32_t action; //Must Be 1
        int32_t transaction_id;
        int32_t interval;
        int32_t leechers;
        int32_t seeders;

        //WRONG, this is a dictionary
        int32_t ip_address;
        int16_t port;
    }announce_response;


    enum action_type{
        none = 0,
        announce = 1,
        scrape = 2,
        error = 3
    };



    bool is_udp_tracker(const std::string& tracker_url);
    void get_tracker_domain(std::string tracker_url, std::string& udp_tracker, uint& port);
    void udp_manager(const std::string tracker_url, tracker::TParameter param, pwp::PeerList peer_list);
    void verify_connect_resp(std::vector<uint8_t>& resp, uint32_t& trans_id, uint64_t& conn_id, std::vector<uint8_t>& conn_id_v);
    void get_announce_req(std::vector<uint8_t>& req, const tracker::TParameter& param, std::vector<uint8_t>& conn_id_v);
    void parse_announce_resp(std::vector<uint8_t>& resp, pwp::PeerList peer_list);
    void process_error(std::vector<uint8_t>& resp);
    void parse_announce_resp_peers(std::vector<uint8_t>& resp, pwp::PeerList peer_list);
}

#endif