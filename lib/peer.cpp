#include "peer.h"
#include "pwp.hpp"

#include <fstream>
using namespace std;

boost::asio::io_service _io_service;    //Link to OS I/O services

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

        const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

        if(peer_conn_p.peer_t.addr == inv_address){  //Check if it's invalid IP
            //LOG(ERROR) << "Invalid Address";
            return -3;
        }
                                 
        tcp::resolver::query query(peer_conn_p.peer_t.addr.to_string(), std::to_string(peer_conn_p.peer_t.port));   //Specify IP and Port
        
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        //tcp::socket socket(_io_service);
        peer_conn_p.socket = std::make_shared<tcp::socket>(_io_service);

        boost::system::error_code error;

        LOG(INFO) << "Testing : " << peer_conn_p.peer_t.addr.to_string() << "\t " << std::to_string(peer_conn_p.peer_t.port);
        peer_conn_p.socket->connect(*endpoint_iterator);

        peer_conn_p.socket->is_open();

        if (error == boost::asio::error::eof){
            LOG(ERROR) << "Connection Closed";
            return -1; // Connection closed cleanly by peer.
        }else if (error){
            throw boost::system::system_error(error); // Some other error.
        }

        //std::cout.write(&response[0], len);
        
    }catch (std::exception& e){
        LOG(ERROR) << e.what() << std::endl;
        return -2;
    }
    return 0;
}



std::string string_to_hex(const std::vector<uint8_t>& input, size_t len)
{
    static const char* const lut = "0123456789ABCDEF";

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


void pwp_protocol_manager(pwp::peer& peer_t, const std::array<uint8_t, 256> &handshake, const char *info_hash){
    std::array<uint8_t, 256> response;
    response.fill(0);

    pwp::peer_connection peer_conn;
    peer_conn.peer_t = peer_t;
    peer_conn.socket = nullptr;

    int error_code = create_socket(peer_conn);

    if(error_code < 0){
        return;
    }

    int result = send_handshake(peer_conn, handshake, response);
    _io_service.run();

    if(result < 0){
        return;
    }

    size_t len = result;    //To improve this for readibility    

    result = verify_handshake(response, len, peer_conn.peer_t, info_hash);

    if(result < 0){
        LOG(ERROR) << "[X] Handshake verification failed!! \t Code : " << result;
        return;
    }

    cout << endl << endl << "Success Handshaking :-)!!!!" << endl << endl;

    result = get_bitfield(peer_conn, response);
    if(result < 0)
        return;
    len = result;

    LOG(INFO) << "Keep-Alive enabled";
    pwp_msg::enable_keep_alive_message(peer_conn);

    const vector<uint8_t> test= {0,0,1,3,6,0,0,0,0,0,0,0,0,0,0,0,0};

    pwp_msg::send_msg(peer_conn, test);

    try{
        len = peer_conn.socket->read_some(boost::asio::buffer(response));
        std::vector<uint8_t> v(std::begin(response), std::end(response));

        cout << string_to_hex(v, len);

    }catch(std::exception& e){
        LOG(ERROR) << e.what() << std::endl;
        return;
    }
    while(1){
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }
}



int get_bitfield(pwp::peer_connection& peerc_t, std::array<uint8_t, 256> &response){

    if(!peerc_t.socket->is_open()){
        LOG(ERROR) << "get-bitfield : Socket is closed!!!";
        return -1;
    }
    boost::system::error_code error;
    size_t len;

    try{

        len = peerc_t.socket->read_some(boost::asio::buffer(response), error);

    }catch(std::exception& e){
        LOG(ERROR) << e.what() << std::endl;
        return -2;
    }

    return len;
}





/**
 *  Create the handshake for a file
 *  
 *  @param info_hash : the info_hash of the file
 *  @param handshake : the destination array for the handshake
 * 
 */

void build_handshake(char *info_hash, std::array<uint8_t, 256> &handshake){
    int hindex = 0;

    const string pstr = "BitTorrent protocol";
    const size_t pstrlen = pstr.length();

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
}

/**
 *  Contact the peer with handshake and dowload the response
 * 
 *  @param t_peer    : the peer to contact
 *  @param handshake : the handshake to send
 *  @param response  : a pointer to an array used to store the response
 * 
 */

int send_handshake(pwp::peer_connection& peerc_t, const std::array<uint8_t, 256> handshake, std::array<uint8_t, 256> &response){
    using namespace boost::asio;
    using namespace boost::asio::ip;
  
    //Start the request

    size_t len;

    try{ 
        tcp::resolver resolver(_io_service); //Domain resolver

        const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

        if(peerc_t.peer_t.addr == inv_address){  //Check if it's invalid IP
            //LOG(ERROR) << "Invalid Address";
            return -3;
        }
                                 
        tcp::resolver::query query(peerc_t.peer_t.addr.to_string(), std::to_string(peerc_t.peer_t.port));   //Specify IP and Port
        
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        //tcp::socket socket(_io_service);
        peerc_t.socket = std::make_shared<tcp::socket>(_io_service);

        LOG(INFO) << "Testing : " << peerc_t.peer_t.addr.to_string() << "\t " << std::to_string(peerc_t.peer_t.port);
        boost::asio::connect(*(peerc_t.socket), endpoint_iterator);

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

int verify_handshake(const array<uint8_t, 256> handshake, size_t len, const pwp::peer t_peer, const char *info_hash){
    int hindex = 0;

    string pstr = string("BitTorrent protocol");
    const size_t pstrlen = pstr.length();

    assert(pstrlen == 19);


    if((uint8_t)handshake[0] != pstrlen){   //First Raw Byte is the length
        //LOG(ERROR) << "First Byte should be pstrlen (19), instead is " << std::to_string(handshake[hindex]);
        return -1;
    } 

    /*
    //This print the handshake
    cout << endl << "[0]" << to_string(handshake[0]);
	for (int i = 1; i < 64; i++)
	{
		cout << "[" << i << "] " << (char)(handshake[i])<<endl;
	}
    */

    ++hindex;

    for(int i=0;i<(int)pstrlen && hindex<len;++i){
        if(handshake[hindex] != pstr.at(i)){   //Checking pstr
            cout << handshake[hindex];
            //LOG(ERROR) << "\"BitTorrent protocol\" string not found";
            return -2;
        }    
        ++hindex;
    }

    for(int i=0; i<8 && hindex<len;++i){
        if(handshake[hindex] != 0){ //Check for unimplemented function
            LOG(WARNING) << "Found unimplemented function on peer's response";
        }  
        ++hindex;
    }

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
            LOG(ERROR) << "Peer ID does not match";
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