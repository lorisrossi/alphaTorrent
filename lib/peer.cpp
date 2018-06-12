#include <iostream>
#include <exception>

#include <boost/dynamic_bitset.hpp>
#include <fstream>
#include <glog/logging.h>   //Logging Library
#include <cmath>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "peer.h"
#include "pwp.hpp"
#include "rang.hpp"


using namespace std;
using namespace torr;

boost::asio::io_service _io_service;    /*!< Global IO-Service */

int active_peer =0;
boost::mutex mtx_peer_num;              /*!< Mutex for the alive peers number */



//DEBUG FUNCTION
void write_to_file(pwp::PeerList peer_list){
    
    ofstream myfile;
    myfile.open ("peer_list.txt");

    vector<pwp::peer>::iterator it = peer_list->begin();

    for(;it != peer_list->end(); ++it){
        myfile << it->addr.to_string()  << endl;
    
    }
    myfile.close();
}



void handle_receive(
    const boost::system::error_code& ec, 
    boost::asio::ip::tcp::resolver::iterator i,
    boost::system::error_code* out_ec)
{
    *out_ec = ec;
    std::cout << std::endl << "Response Received\t EC : " << out_ec->value()  << std::endl;
}



namespace pwp{


    /**
     * @brief Find invalid peers and remove it
     * 
     * @param peer_list 
     */

    void remove_invalid_peer(pwp::PeerList peer_list){

        vector<pwp::peer>::iterator it;
        const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

        it=peer_list->begin();

        for(; it != peer_list->end(); ++it){

            if(it->addr == inv_address){
                peer_list->erase(it);
                LOG(WARNING) << "Peer eliminated"; 
            }
        }
    }


    /**
     *  @brief Create the socket and connect to the peer
     * 
     * 
     *  Error Code | Meaning 
     *  -----------|------------
     *  -1         | Connection Closed
     *  -2         | Generic Error (see the output)
     *  -3         | Invalid Address
     * 
     * 
     *  @param peer_connection
     *
     */

    int create_socket(pwp::peer_connection& peer_conn_p){
        using namespace boost::asio;
        using namespace boost::asio::ip;

        //Start the request

        try{ 
            tcp::resolver resolver(_io_service); //Domain resolver

            if(is_inv_address(peer_conn_p.peer_.addr)){  //Check if it's invalid IP
                DLOG(ERROR) << "Invalid Address";
                return -3;
            }
                                    
            tcp::resolver::query query(peer_conn_p.peer_.addr.to_string(), std::to_string(peer_conn_p.peer_.port));   //Specify IP and Port
            
            tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
            peer_conn_p.socket = std::make_shared<tcp::socket>(_io_service);    //Allocate the socket


            LOG(INFO) << "Testing : " << peer_conn_p.peer_.addr.to_string() << "\t " << std::to_string(peer_conn_p.peer_.port);
            
            //Try connection

            size_t len;
            boost::system::error_code error;

            peer_conn_p.socket->connect(*endpoint_iterator, error);


            DLOG(INFO) << "Successfully connect";

            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                return -1; // Connection closed cleanly by peer.
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }
        }catch (std::exception& e){
            LOG(ERROR) << peer_conn_p.peer_.addr << ' ' << e.what();
            return -2;
        }

        return 0;
    }




    /**
     * @brief Manager of the entire PWP protocol
     * 
     * This is the manager of the entire peer comunication.
     * After creating the socket the handshake is sended.
     * The received handshake is then checked and if it's correct the routine starts.
     * 
     * 1. Send Interested Message
     * 2. Send Unchocked Message
     * 3. Start Keep-Alive Routine
     * 4. Wait (asynchronously) for messages
     * 5. Loop and send appropriate messages
     * 
     * \bug No timeout control is performed so if there is a dead peer that does not respond the software
     * blocks until the default SO timeout is reacher (tipically 2 minutes).
     * 
     * @param peer_         The peer on which execute the protocol
     * @param handshake     The initial handshake to send
     * @param info_hash     The torrent file info_hash
     * @param torrent       The struct containing all torrent information
     */

    void pwp_protocol_manager(pwp::peer peer_, const std::vector<uint8_t> &handshake, const char *info_hash, Torrent &torrent){
            
        std::vector<uint8_t> response = std::vector<uint8_t>(64);

        pwp::peer_connection peer_conn = {
            peer_,             //Peer Data
            pwp::client_state(),//Client State
            pwp::peer_state(),  //Peer State
            boost::dynamic_bitset<>(),
            nullptr,                    //Socket pointer
        };

        int error_code = create_socket(peer_conn);

        if(error_code < 0){
            LOG(ERROR) << peer_.addr.to_string() << " Exit thread";
            return;
        }

        assert(peer_conn.socket != nullptr);

        int result = send_handshake(peer_conn, handshake, response);

        if(result < 0){
            LOG(ERROR) << "Exit thread ";   
            return;
        }
        
        //If there was no error (result >= 0) thet it's value is the length of the received handshake
        size_t len = result;        

        result = verify_handshake(response, len, peer_conn.peer_, info_hash);

        if(result < 0){
            LOG(ERROR) << rang::fg::red << "[X] Handshake verification failed!! \t Code : " << result << rang::fg::reset;
            return;
        }

        cout << peer_.addr.to_string() << rang::fg::green <<  " handshake done succesfully" << rang::fg::reset << endl;

        add_active_peer();  //The current peer is valid

        result = pwp_msg::get_bitfield(peer_conn, torrent);
        if(result < 0){
            LOG(ERROR) << "Bitfield not sended, exit\n";
            //rm_active_peer();
            //return;
        }

        if(pwp_msg::send_msg(peer_conn, pwp_msg::interested_msg) < 0)
            LOG(ERROR) << "Error sending interested_msg";

        if(pwp_msg::send_msg(peer_conn, pwp_msg::unchoke_msg) < 0)
            LOG(ERROR) << "Error sending unchoke msg";
        
        LOG(INFO) << "Keep-Alive enabled";
        pwp_msg::enable_keep_alive_message(peer_conn);



        boost::asio::deadline_timer timer_(_io_service);
        boost::system::error_code ec = boost::asio::error::would_block;

        try{
            // Receive 4 bytes
            bool dead_peer = false;

            // cout << endl << peer_.addr.to_string() << " | Starting timeout count...";
            

            // cout << peer_conn.peer_.addr << " reading something, byte available : " << peer_conn.socket->available() << "\n";
            boost::asio::async_read(*(peer_conn.socket), boost::asio::buffer(response, sizeof(uint8_t)*4), 
                boost::asio::transfer_exactly(4),
                boost::bind(&pwp_msg::read_msg_handler, boost::ref(response), 
                            boost::ref(peer_conn), boost::ref(torrent), boost::ref(dead_peer), boost::ref(timer_),
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred
                )
            );

            int old_begin = -1; // used to not send the same request multiple times

            while(!dead_peer) {

                if(_io_service.stopped()){
                    cout << endl << "IO-Service stopped, resetting" << endl;
                    _io_service.reset();
                    _io_service.run();
                }

                if(pwp_msg::sender(peer_conn, torrent, old_begin) < 0) {
                    dead_peer = true;
                }
                
                boost::this_thread::sleep_for(boost::chrono::milliseconds(500));  // Sleep for 0.5 seconds
            }

        }catch(std::exception& e){
            LOG(ERROR) << peer_.addr << ' ' << e.what() << std::endl;
            rm_active_peer();
            //peer_conn.socket->close();
            return;
        }

        cout << endl << rang::fg::red << "DEAD-PEER - Exiting" << rang::fg::reset << endl;
        //peer_conn.socket->close();
        while(peer_conn.socket->is_open()){
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));            
        }
        rm_active_peer();
        return;
    }


    /**
     *  @brief Create the handshake for a file
     *  
     *  @param info_hash the info_hash of the file
     *  @param handshake the destination array for the handshake
     * 
     */

    void build_handshake(char *info_hash, std::vector<uint8_t> &handshake){
        int hindex = 0;

        const string pstr = "BitTorrent protocol";
        const size_t pstrlen = pstr.length();

        handshake.resize(200);  //TODO Improve this

        //Constructing the handshake request

        //DLOG(INFO) << "Building the handshake";
        assert(pstrlen == 19);

        handshake[hindex] = pstrlen; //First Raw Byte is the 
        hindex++;
        //DLOG(INFO) << "<pstrlen> written";

        for(uint i=0;i<pstrlen;++i){
            handshake[hindex] = pstr[i];    //Copying pstr into buff
            ++hindex;
        }
        //DLOG(INFO) << "<pstr> written";

        for(int i=0; i<8;++i){
            handshake[hindex] = 0;  //Eight reseved byte
            ++hindex;
        }
        //DLOG(INFO) << "<reserved> written";

        for(int i=0; i<20;++i){
            handshake[hindex] = info_hash[i];   //Info hash raw byte
            ++hindex;
        }   
        //DLOG(INFO) << "<info_hash> written";

        string peer_id;
        get_peer_id(&peer_id);

        for(uint i=0; i<peer_id.length();++i){
            handshake[hindex] = peer_id.at(i);
            ++hindex;
        }
        //DLOG(INFO) << "<peer_id> written";

        handshake.shrink_to_fit();  //Resize the handshake
    }

    /**
     *  Contact the peer with handshake and download the response
     * 
     *  Error code | Meaning
     *  -----------|------------
     *  -1         | Unimplemented.
     *  -2         | Socket Error.
     *  -3         | Invalid IP Address.
     *  
     * 
     *  @param t_peer      the peer to contact
     *  @param handshake   the handshake to send
     *  @param response    a pointer to an array used to store the response
     * 
     *  @return 0 on success, < 0 on failure
     * 
     */

    int send_handshake(pwp::peer_connection& peerc_t, const std::vector<uint8_t> handshake, std::vector<uint8_t> &response){
        using namespace boost::asio;
        using namespace boost::asio::ip;
    
        //Start the request

        size_t len;

        try{ 

            if(is_inv_address(peerc_t.peer_.addr)){
                LOG(ERROR) << "Invalid Address";
                return -3;
            }

            //Sending initial request
            LOG(INFO) << "Sending handshake";
            len = peerc_t.socket->send(buffer(handshake));

            LOG(INFO) << "Waiting for response...";
            boost::system::error_code error;
            len = peerc_t.socket->read_some(boost::asio::buffer(response), error);

            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                return -1; // Connection closed cleanly by peer.
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }

        }catch (std::exception& e){
            LOG(ERROR) << "Handshake ERROR : " << e.what();
            return -2;
        }
        return len;
    }





    /**
     *  @brief Put into "id" the string that conrespond to the client ID
     * 
     *  Find a new way to generate the ID:
     *  1. Using MAC address.
     *  2. Initially random and then writtend o a file.
     *
     *  @param id   A pointer to the string that will contains the Peer-ID
     * 
     */

    void get_peer_id(string *id){

        *id = string("-qB3370-aGANEG8-9a3r");   //Temporary solution

        assert(strnlen(id->c_str(), 20) == 20);

    }



    /**
     *  Verify if an handshake (received) is correct
     * 
     *  Error Code | Meaning
     *  -----------|----------
     *  -1         | First byte is not 19
     *  -2         | Invalid protocol string
     *  -3         | Info hash does not match (with the one sended)
     *  -4         | Peer ID does not match (Bypassed)
     * 
     *  \warning The comparison of the *peer-ID* often fail so it's skipped (no return -4)
     *  \bug     peer-ID is checked but nothing is done in case of failing 
     *  @param handshake : the handshake to check 
     *  @param t_peer    : the peer who sended the handshake
     *  @param info_hash : the info_hash of the file
     * 
     */

    int verify_handshake(const vector<uint8_t> handshake, size_t len, const pwp::peer t_peer, const char *info_hash){
        uint hindex = 0;

        string pstr = string("BitTorrent protocol");
        const size_t pstrlen = pstr.length();

        assert(pstrlen == 19);


        if((uint8_t)handshake[0] != pstrlen){   //First Raw Byte is the length
            LOG(ERROR) << "First Byte should be pstrlen (19), instead is " << std::to_string(handshake[hindex]);
            return -1;
        } 

        ++hindex;

        //Check for "BitTorrent protocol"
        for(int i=0;i<(int)pstrlen && hindex<len;++i){
            if(handshake[hindex] != pstr.at(i)){   //Checking pstr
                LOG(ERROR) << "\"BitTorrent protocol\" string not found";
                return -2;
            }    
            ++hindex;
        }

        //Check for reserved byte
        for(int i=0; i<8 && hindex<len;++i){
            if(handshake[hindex] != 0){ //Check for unimplemented function
                LOG(WARNING) << "Found unimplemented function on peer's response";
            }  
            ++hindex;
        }

        //Check for info_hash
        for(int i=0; i<20 && hindex<len;++i){
            if(handshake[hindex] != (uint8_t)info_hash[i]){   //Info hash raw byte
                LOG(ERROR) << "info_hash does not match";
                return -3;
            }
            ++hindex;
        }   
        //DLOG(INFO) << "<info_hash> written";


        /**
         * 
         * If the initiator of the connection receives a handshake in which the peer_id does not match the 
         * expected peerid, then the initiator is expected to drop the connection. Note that the initiator 
         * presumably received the peer information from the tracker, which includes the peer_id that was 
         * registered by the peer. The peer_id from the tracker and in the handshake are expected to match. 
         * 
         */


        string peer_id = t_peer.peer_id;
    
        for(int i=0; i<(int)peer_id.length() && hindex<len;++i){
            if(handshake[hindex] != peer_id.at(i)){
                LOG(ERROR) << peer_id << " Peer ID does not match";
                //return -4;
            }
            ++hindex;
        }    

        return 0;   //All tests passed
    }

}//End namespace




/**
 *  Takes vector of bytes and print its HEX rappresentation
 * 
 *  @param input    The bytes to convert
 *  @return         An HEX string
 * 
 * TODO rename this nonsense function name
 * 
 */

std::string string_to_hex(const std::vector<uint8_t>& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.size();
    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}





uint32_t make_int(bInt bint){
    uint32_t dataBoth = 0x0000;

    dataBoth = bint.i1;
    dataBoth = dataBoth << 8;
    dataBoth |= bint.i2;
    dataBoth = dataBoth << 8;
    dataBoth |= bint.i3;       
    dataBoth = dataBoth << 8;
    dataBoth |= bint.i4;   


    return ntohl(dataBoth);;
}



uint32_t make_int(std::vector<uint8_t> v){
    uint32_t dataBoth = 0x0000;

    assert(v.size() == 4);

    dataBoth = v[0];
    dataBoth = dataBoth << 8;
    dataBoth |= v[1];
    dataBoth = dataBoth << 8;
    dataBoth |= v[2];       
    dataBoth = dataBoth << 8;
    dataBoth |= v[3];  

    return dataBoth;
}


std::vector<uint8_t> from_int_to_bint(uint integer){

    uint8_t out[4];
    *(uint32_t*)&out = integer;
    
    std::vector<uint8_t> ret(4);
    ret[0] = out[3];
    ret[1] = out[2];
    ret[2] = out[1];
    ret[3] = out[0];
    return ret;

}

std::vector<uint8_t> from_int64_to_bint(uint64_t integer){

    uint8_t out[8];
    *(uint64_t*)&out = integer;
    
    std::vector<uint8_t> ret(8);
    ret[0] = out[0];
    ret[1] = out[1];
    ret[2] = out[2];
    ret[3] = out[3];
    ret[4] = out[4];
    ret[5] = out[5];
    ret[6] = out[6];
    ret[7] = out[7];
    return ret;

}


inline void add_active_peer(){
    mtx_peer_num.lock();
    active_peer++;
    mtx_peer_num.unlock();
}

inline void rm_active_peer(){
    mtx_peer_num.lock();
    active_peer--;
    assert(active_peer >= 0);
    mtx_peer_num.unlock();
}





void check_deadline_udp(boost::asio::ip::udp::socket &socket, boost::asio::deadline_timer &timer_){

    if (timer_.expires_at() <= boost::asio::deadline_timer::traits_type::now()){
        // The deadline has passed. The socket is closed so that any outstanding
        // asynchronous operations are cancelled. This allows the blocked
        // connect(), read_line() or write_line() functions to return.
        boost::system::error_code ec;
        //socket.close(ignored_ec);
        std::cout << std::endl << "Expired ... Closing socket " << std::endl;
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        if(ec){
            std::cout << std::endl << "Error while closing the socket";
        }
        socket.cancel();
        socket.close();
        // There is no longer an active deadline. The expiry is set to positive
        // infinity so that the actor takes no action until a new deadline is set.
        timer_.expires_at(boost::posix_time::pos_infin);
    }

    // Put the actor back to sleep.
    timer_.async_wait(boost::bind(&check_deadline_udp, std::ref(socket), std::ref(timer_)));

}

void check_deadline_tcp(std::shared_ptr<boost::asio::ip::tcp::socket> socket, boost::asio::deadline_timer &timer_){

    if (timer_.expires_at() <= boost::asio::deadline_timer::traits_type::now()){
        // The deadline has passed. The socket is closed so that any outstanding
        // asynchronous operations are cancelled. This allows the blocked
        // connect(), read_line() or write_line() functions to return.
        boost::system::error_code ec;
        //socket.close(ignored_ec);
        std::cout << std::endl << "Expired ... Closing socket " << std::endl;
        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        if(ec){
            std::cout << std::endl << "Error while closing the socket";
        }
        socket->cancel();
        socket->close();
        // There is no longer an active deadline. The expiry is set to positive
        // infinity so that the actor takes no action until a new deadline is set.
        timer_.expires_at(boost::posix_time::pos_infin);
    }

    // Put the actor back to sleep.
    timer_.async_wait(boost::bind(&check_deadline_tcp, socket, std::ref(timer_)));

}


