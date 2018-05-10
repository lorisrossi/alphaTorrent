#include "pwp.hpp"
#include "peer.h"


bool is_inv_address(const boost::asio::ip::address& addr){
    const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

    if(addr == inv_address){  //Check if it's invalid IP
        return true;
    }        
    return false;
} 


/**
 *  This namespace contains all the function that are related to the messages of the PWP Protocol
 * 
 */

namespace pwp_msg{


    void send_keep_alive(const pwp::peer_connection& peerc_t){
        using namespace boost::asio;
        using namespace boost::asio::ip;

        //Start the request
        if(is_inv_address(peerc_t.peer_t.addr)){  //Check if it's invalid IP
            return;
        }

        try{ 

            if(!peerc_t.socket->is_open()){
                std::cout << std::endl << "Keep-Alive routine : Error socket not opened!!!!!";
                return;
            }

            
            boost::system::error_code error;
            size_t len;
            std::array<uint8_t, 4> keep_alive_msg;

            keep_alive_msg.fill(0);

            std::cout << std::endl << "Sending keep-alive";
            len = peerc_t.socket->send(buffer(keep_alive_msg));
        
            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                return;
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }
            
        }catch (std::exception& e){
            LOG(ERROR) << "Keep-Alive request : " << e.what() << std::endl;
        }
    }


    void enable_keep_alive_message(pwp::peer_connection& peerc_t){
        boost::asio::deadline_timer timer(_io_service, boost::posix_time::seconds(KEEP_ALIVE_TIME));

        timer.async_wait(boost::bind(send_keep_alive, peerc_t));

        _io_service.run();
    }

    
    int send_msg(pwp::peer_connection& peerc_t, std::vector<uint8_t> msg){
        using namespace boost::asio;
        using namespace boost::asio::ip;
        
        if(is_inv_address(peerc_t.peer_t.addr)){  //Check if it's invalid IP
            return -1;
        }

        //Start the request
        try{ 

            LOG(INFO) << "Checking connection";
            if(!peerc_t.socket->is_open()){
                std::cout << std::endl << "Error socket not opened!!!!!";
                return -1;
            }

            
            boost::system::error_code error;
            size_t len;

            LOG(INFO) << "Sending message";
            len = peerc_t.socket->send(buffer(msg));

            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                return -1;
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }
            
        }catch (std::exception& e){
            LOG(ERROR) << peerc_t.peer_t.addr.to_string() << " " << e.what() << std::endl;
            return -2;
        }
        return 0;
    }


    std::vector<uint8_t> craft_have_msg(int piece_index){
        std::vector<uint8_t> msg = {0,0,0,5,4}; //From the protocol

        std::vector<uint8_t> piece_coded = from_int_to_bint(piece_index);

        msg.insert( msg.end(), piece_coded.begin(), piece_coded.end());

        return msg;
    }


}