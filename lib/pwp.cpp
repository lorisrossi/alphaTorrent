#include "pwp.hpp"
#include "peer.h"
#include "filehandler.hpp"


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

        if(_io_service.stopped()){
            DLOG(INFO) << "IO-Service stopped, resetting";
            _io_service.reset();
            _io_service.run();
        }else{
            _io_service.poll();
        }
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

    std::vector<uint8_t> make_request_msg(RequestMsg request) {
        std::vector<uint8_t> msg = {0,0,0,13,6}; // From the protocol

        std::vector<uint8_t> index = from_int_to_bint(request.index);
        std::vector<uint8_t> begin = from_int_to_bint(request.begin);
        std::vector<uint8_t> length = from_int_to_bint(request.length);

        msg.insert(msg.end(), index.begin(), index.end());
        msg.insert(msg.end(), begin.begin(), begin.end());
        msg.insert(msg.end(), length.begin(), length.end());

        return msg;
    }




    void read_msg_handler(std::vector<uint8_t>& response, pwp::peer_connection& peer_c, const boost::system::error_code& error, size_t bytes_read){
        using namespace std;
        using namespace boost::asio;
        //cout << peer_c.peer_t.addr << " hit read_msg_handler, bytes read " << bytes_read << "\n";

        uint32_t msg_len = uint8_t(response[0]) << 24 | uint8_t(response[1]) << 16 | uint8_t(response[2]) << 8 | uint8_t(response[3]);
        uint8_t msg_id = response[4];

        switch(msg_id){

            case pwp_msg::chocked:
                peer_c.pstate.peer_choking = true;
                cout << peer_c.peer_t.addr.to_string() << " CHOKED, stop reading packets" << endl;
                break;

            case pwp_msg::unchocked:
                peer_c.pstate.peer_choking = false;
                cout << peer_c.peer_t.addr.to_string() << " UNCHOKED, stop reading packets" << endl;
                break;

            case pwp_msg::bitfield: {
                boost::dynamic_bitset<> new_bitfield;
                uint32_t bit_len = msg_len - 1;
                
                cout << peer_c.peer_t.addr.to_string() << " bitfield, length: " << bit_len << endl;

                vector<uint8_t> bit_vector = vector<uint8_t>(bit_len);

                try{
                    size_t read_len = boost::asio::read(*(peer_c.socket), buffer(bit_vector),transfer_exactly(bit_len));
                    cout << endl << read_len << " bytes of bitfield readed" << endl;
                }catch(std::exception& e){
                    cout << endl << e.what() << endl;
                    return;
                }

                // loop each byte extracted
                for (uint32_t i = 0; i < bit_len; ++i) {
                    boost::dynamic_bitset<> temp(8, uint8_t(bit_vector[i]));
                    // insert each bit into the bitfield
                    for (int k = 7; k >= 0; --k) {
                        new_bitfield.push_back(temp[k]);
                    }
                }
                peer_c.bitfield = new_bitfield;
                }
                break;

            case pwp_msg::piece:
                uint32_t piece_len = msg_len - 9;
                cout << peer_c.peer_t.addr << " PIECE received, length: " << piece_len << endl;
                response.resize(piece_len);
                peer_c.socket->receive(boost::asio::buffer(response,sizeof(uint8_t)*piece_len));

                cout << peer_c.peer_t.addr << endl << string_to_hex(response) << endl;

                break;

        }

        boost::asio::async_read(*(peer_c.socket), boost::asio::buffer(response, sizeof(uint8_t)*5), 
            boost::asio::transfer_exactly(5),
            boost::bind(&pwp_msg::read_msg_handler, boost::ref(response), 
                        boost::ref(peer_c), 
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred
            )
        );

        if(_io_service.stopped()){
            DLOG(INFO) << endl << "IO-Service stopped, resetting";
            _io_service.reset();
            _io_service.run();
        }

    }

    void sender(pwp::peer_connection &peer_conn, Torrent &torrent) {
        if (!peer_conn.pstate.peer_choking && peer_conn.cstate.am_interested) {
            int piece_index = compare_bitfields(peer_conn.bitfield, torrent.bitfield);
            if (piece_index != -1) {
                RequestMsg request = create_request(torrent, piece_index);
                std::vector<uint8_t> msg = make_request_msg(request);
                
                send_msg(peer_conn, msg);
            }
            else {
                peer_conn.cstate.am_interested = false;
            }
        }
    }
}