#include "pwp.hpp"
#include "peer.h"


namespace pwp_msg{

    void send_keep_alive(const pwp::peer_connection& peerc_t){
        using namespace boost::asio;
        using namespace boost::asio::ip;

        //Start the request

        try{ 
            const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

            if(peerc_t.peer_t.addr == inv_address){  //Check if it's invalid IP
                LOG(ERROR) << "Invalid Address";
                return;
            }
        

            if(!peerc_t.socket->is_open()){
                std::cout << std::endl << "Keep-Alive routine : Error socket not opened!!!!!";
                return;
            }

            
            boost::system::error_code error;
            size_t len;
            std::array<char, 4> keep_alive_msg;
            keep_alive_msg.fill(0);

            //Sending initial request
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

    
    void send_msg(pwp::peer_connection& peerc_t, std::vector<uint8_t> msg){
        using namespace boost::asio;
        using namespace boost::asio::ip;
    

        //Start the request

        try{ 
            const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

            if(peerc_t.peer_t.addr == inv_address){  //Check if it's invalid IP
                //LOG(ERROR) << "Invalid Address";
                return;
            }

            LOG(INFO) << "Checking connection";

            if(!peerc_t.socket->is_open()){
                std::cout << std::endl << "Error socket not opened!!!!!";
                return;
            }

            
            boost::system::error_code error;
            size_t len;

            //Sending initial request
            LOG(INFO) << "Sending message";
            len = peerc_t.socket->send(buffer(msg));


            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                return;
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }
            
        }catch (std::exception& e){
            LOG(ERROR) << e.what() << std::endl;
        }
    }


    std::vector<uint8_t> craft_have_msg(int piece_index){
        std::vector<uint8_t> msg = {0,0,0,5,4}; //From the protocol

        std::vector<uint8_t> piece_coded = from_int_to_bint(piece_index);

        msg.insert( msg.end(), piece_coded.begin(), piece_coded.end());

        return msg;
    }


}