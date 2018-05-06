#include "peer.h"


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


/**
 *  Manage all the peer connection and create the appropriate threads
 * 
 *  @param peer_list : the list of the peers fetched from the tracker
 *  @param info_hash : the info_hash of the file to download
 * 
 */
void manage_peer_connection(pwp::PeerList peer_list, char *info_hash){

    if(peer_list == nullptr){
        LOG(FATAL) << "Empty Peer List should be not allowed";
    }

    vector<pwp::peer>::iterator it = peer_list->begin();
    boost::thread_group t_group;

    remove_invalid_peer(peer_list);
    write_to_file(peer_list);

    std::array<char, 256> handshake;

    DLOG(INFO) << "Building handshake...";
    build_handshake(info_hash, handshake);



    for(;it != peer_list->end(); ++it){

        LOG(INFO) << "Starting handshake request to " << it->addr.to_string() << ":" << it->port << "...";
        t_group.add_thread(new boost::thread(handshake_request_manager, handshake, *it, info_hash));
    
    }

    t_group.join_all();
}


/**
 *  Manage the handshake of a single peer
 *      1 - Send the handshake
 *      2 - Verify the handshake
 * 
 *  @param handshake :  the handshake sequence to send
 *  @param t_peer    :  the peer to contact
 *  @param info_hash :  the info_hash of the file
 * 
 */

void handshake_request_manager(const std::array<char, 256> &handshake, const pwp::peer t_peer, const char *info_hash){

    std::array<char, 256> response;

    send_handshake(t_peer, handshake, response);

    int result = verify_handshake(response, t_peer, info_hash);

    if(result < 0){
        LOG(ERROR) << "[X] Handshake verification failed!! \t Code : " << result;
        return;
    }

    LOG(INFO) << "Success Handshaking :-)!!!!";

}


/**
 *  Create the handshake for a file
 *  
 *  @param info_hash : the info_hash of the file
 *  @param handshake : the destination array for the handshake
 * 
 */

void build_handshake(char *info_hash, std::array<char, 256> &handshake){
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

void send_handshake(const pwp::peer t_peer, const std::array<char, 256> handshake, std::array<char, 256> &response){
    using namespace boost::asio;
    using namespace boost::asio::ip;
  

    //Start the request

    try{ 
        tcp::resolver resolver(_io_service); //Domain resolver

        const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

        if(t_peer.addr == inv_address){  //Check if it's invalid IP
            LOG(ERROR) << "Invalid Address";
            return;
        }
                                 
        tcp::resolver::query query(t_peer.addr.to_string(), std::to_string(t_peer.port));   //Specify IP and Port
        
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        tcp::socket socket(_io_service);

        LOG(INFO) << "Testing : " << t_peer.addr.to_string() << "\t " << std::to_string(t_peer.port);
        boost::asio::connect(socket, endpoint_iterator);

        LOG(INFO) << "Initiating the handshake";

        for (;;){
            boost::system::error_code error;
            size_t len;

            //Sending initial request
            LOG(INFO) << "Sending handshake";
            len = socket.send(buffer(handshake));

            LOG(INFO) << "Waiting for response...";
            len = socket.read_some(boost::asio::buffer(response), error);

            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                break; // Connection closed cleanly by peer.
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }

            std::cout.write(&response[0], len);
            
        }
    }catch (std::exception& e){
        LOG(ERROR) << e.what() << std::endl;
    }

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

int verify_handshake(const array<char, 256> handshake, const pwp::peer t_peer, const char *info_hash){
    int hindex = 0;

    const string pstr = "BitTorrent protocol";
    const size_t pstrlen = pstr.length();


    if(handshake[hindex] != pstrlen){   //First Raw Byte is the length
        //FIXME
        /*
        *   Convert first 4 bytest to an int, according to the procotol : 
        * 
        *       Data Types
        *       Unless specified otherwise, all integers in the peer wire protocol are encoded as four byte big-endian values. This includes the length prefix on all messages that come after the handshake.
        *       Message flow
        *       The peer wire protocol consists of an initial handshake. After that, peers communicate via an exchange of length-prefixed messages. The length-prefix is an integer as described above. 
        */


        LOG(ERROR) << "First Byte should be pstrlen (19), instead is " << std::to_string(handshake[hindex]);
        return -1;
    } 
    
    ++hindex;

    for(uint i=0;i<pstrlen;++i){
        if(handshake[hindex] != pstr[i]){   //Checking pstr
            LOG(ERROR) << "\"BitTorrent protocol\" string not found";
            return -2;
        }    
        ++hindex;
    }

    for(int i=0; i<8;++i){
        if(handshake[hindex] != 0){ //Check for unimplemented function
            LOG(WARNING) << "Found unimplemented function on peer's response";
        }  
        ++hindex;
    }

    for(int i=0; i<20;++i){
        if(handshake[hindex] != info_hash[i]){   //Info hash raw byte
            LOG(ERROR) << "info_hash does not match";
            return -3;
        }
        ++hindex;
    }   
    //DLOG(INFO) << "<info_hash> written";

    string peer_id = t_peer.peer_id;
   
    for(uint i=0; i<peer_id.length();++i){
        if(handshake[hindex] != peer_id.at(i)){
            LOG(ERROR) << "Peer ID does not match";
            return -4;
        }
        ++hindex;
    }    

    return 0;   //All tests passed
}