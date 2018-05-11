#include "peer.h"
#include "pwp.hpp"
#include "torrentparser.hpp"
#include <boost/dynamic_bitset.hpp>
#include <fstream>
using namespace std;

boost::asio::io_service _io_service;    //Link to OS I/O services

int active_peer =0;
boost::mutex mtx_peer_num;

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



int create_socket(pwp::peer_connection& peer_conn_p){
    using namespace boost::asio;
    using namespace boost::asio::ip;
  
    //Start the request
    try{ 
        tcp::resolver resolver(_io_service); //Domain resolver

        if(is_inv_address(peer_conn_p.peer_t.addr)){  //Check if it's invalid IP
            //LOG(ERROR) << "Invalid Address";
            return -3;
        }
                                 
        tcp::resolver::query query(peer_conn_p.peer_t.addr.to_string(), std::to_string(peer_conn_p.peer_t.port));   //Specify IP and Port
        
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        //tcp::socket socket(_io_service);
        peer_conn_p.socket = std::make_shared<tcp::socket>(_io_service);    //Allocate the socket

        boost::system::error_code error;

        LOG(INFO) << "Testing : " << peer_conn_p.peer_t.addr.to_string() << "\t " << std::to_string(peer_conn_p.peer_t.port);
        
        //Try connection
        peer_conn_p.socket->connect(*endpoint_iterator);

        if (error == boost::asio::error::eof){
            LOG(ERROR) << "Connection Closed";
            return -1; // Connection closed cleanly by peer.
        }else if (error){
            throw boost::system::system_error(error); // Some other error.
        }
    }catch (std::exception& e){
        LOG(ERROR) << peer_conn_p.peer_t.addr << ' ' << e.what() << std::endl;
        return -2;
    }
    return 0;
}



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


void pwp_protocol_manager(pwp::peer peer_t, const std::vector<uint8_t> &handshake, const char *info_hash, Torrent &torrent){
        
    std::vector<uint8_t> response = std::vector<uint8_t>(512);

    pwp::peer_state peer_s = {
        false, false
    };
    pwp::client_state client_s = {
        false, false
    };
    pwp::peer_connection peer_conn = {
        peer_t,             //Peer Data
        client_s ,    //Client State
        peer_s,  //Peer State
        boost::dynamic_bitset<>(),
        nullptr,            //Socket pointer
    };

    int error_code = create_socket(peer_conn);

    if(error_code < 0){
        LOG(ERROR) << peer_t.addr.to_string() << " Exit thread";
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

    result = verify_handshake(response, len, peer_conn.peer_t, info_hash);

    if(result < 0){
        LOG(ERROR) << "[X] Handshake verification failed!! \t Code : " << result;
        return;
    }

    cout << peer_t.addr.to_string() << " handshake done succesfully" << endl;

    add_active_peer();  //The current peer is valid

    //If there was no error (result >= 0) thet it's value is the length of the received handshake
    // len = result;

    if(pwp_msg::send_msg(peer_conn, pwp_msg::interested_msg) < 0)
        LOG(ERROR) << "Error senging interested_msg";

    if(pwp_msg::send_msg(peer_conn, pwp_msg::unchoke_msg) < 0)
        LOG(ERROR) << "Error sending unchoke mesg";
    
    LOG(INFO) << "Keep-Alive enabled";
    pwp_msg::enable_keep_alive_message(peer_conn);

    // request length = 2303
    const vector<uint8_t> test= {0,0,0,13,6,0,0,0,0,0,0,0,0,0,0,8,255};

    boost::asio::ip::tcp::socket *sk = (peer_conn.socket.get());

    for (int i=0; i < 10; ++i){
        try{
            //Receive 5 bytes and parse it
            cout << peer_conn.peer_t.addr << " reading something, byte available : " << peer_conn.socket->available() << "\n";
            boost::asio::async_read(*(peer_conn.socket), boost::asio::buffer(response, sizeof(uint8_t)*5), 
                boost::asio::transfer_exactly(5),
                boost::bind(&pwp_msg::read_msg_handler, boost::ref(response), 
                            boost::ref(peer_conn), 
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred
                )
            );

            // Try to send
            // pwp_msg::sender(peer_conn, torrent);

            if(_io_service.stopped()){
                DLOG(INFO) << endl << "IO-Service stopped, resetting";
                _io_service.reset();
                _io_service.run();
            }

            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));  //Sleep for 10 seconds

        }catch(std::exception& e){
            LOG(ERROR) << peer_t.addr << ' ' << e.what() << std::endl;
            rm_active_peer();
            return;
        }


    }

}


/**
 *  Create the handshake for a file
 *  
 *  @param info_hash : the info_hash of the file
 *  @param handshake : the destination array for the handshake
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
 *  Contact the peer with handshake and dowload the response
 * 
 *  @param t_peer    : the peer to contact
 *  @param handshake : the handshake to send
 *  @param response  : a pointer to an array used to store the response
 * 
 */

int send_handshake(pwp::peer_connection& peerc_t, const std::vector<uint8_t> handshake, std::vector<uint8_t> &response){
    using namespace boost::asio;
    using namespace boost::asio::ip;
  
    //Start the request

    size_t len;

    try{ 
        const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

        if(peerc_t.peer_t.addr == inv_address){  //Check if it's invalid IP
            //LOG(ERROR) << "Invalid Address";
            return -3;
        }

        LOG(INFO) << "Initiating the handshake";
        
        boost::system::error_code error;

        //Sending initial request
        LOG(INFO) << "Sending handshake";
        len = peerc_t.socket->send(buffer(handshake));

        LOG(INFO) << "Waiting for response...";
        len = peerc_t.socket->read_some(boost::asio::buffer(response), error);

        if (error == boost::asio::error::eof){
            LOG(ERROR) << "Connection Closed";
            return -1; // Connection closed cleanly by peer.
        }else if (error){
            throw boost::system::system_error(error); // Some other error.
        }

    }catch (std::exception& e){
        LOG(ERROR) << e.what() << std::endl;
        return -2;
    }
    return len;
}


void get_peer_id(string *id){

  /**
   *  Trovare un nuovo modo per generare il peer. Idee
   *  1) Usare il mac address della macchina (Iterare attraverso i file )
   *
   *
   */

  *id = string("-qB3370-aGANEG8-9a3r");   //Soluzione Temporanea

  assert(strnlen(id->c_str(), 20) == 20);

}



/**
 *  Verify if an handshake (received) is correct
 * 
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



uint32_t make_int(pwp::bInt bint){
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



std::vector<uint8_t> from_int_to_bint(int integer){

    uint32_t i = integer;

    uint8_t out[4];
    *(uint32_t*)&out = integer;
    
    std::vector<uint8_t> ret(4);
    ret[0] = out[0];
    ret[1] = out[1];
    ret[2] = out[2];
    ret[3] = out[3];
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





