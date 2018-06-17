#include <iomanip>
#include <glog/logging.h>   //Logging Library
#include <boost/date_time/posix_time/posix_time.hpp>

#include "pwp.hpp"
#include "peer.h"
#include "filehandler.hpp"
#include "torrentparser.hpp"
#include "rang.hpp"


using namespace torr;
using namespace fileio;

/**
 *  Checks if the address is equal to "0.0.0.0"
 *  
 *  @param addr :   the address to verify
 *  @return true if it's invalid, false otherwise
 * 
 */

bool is_inv_address(const boost::asio::ip::address& addr){
    const boost::asio::ip::address inv_address = boost::asio::ip::address::from_string("0.0.0.0");  //from_string deprecated function

    if(addr == inv_address){  //Check if it's invalid IP
        return true;
    }        
    return false;
} 


/**
 *  This namespace contains all the function that are related to the messages of the PWP Protocol
 */
namespace pwp_msg{

    /**
     *  Handler that is executed every KEEP_ALIVE_TIME and send a keep-alive message.
     * 
     *  If the peer_c's socket is closed, then the periodic routine is stopped by calling.\n
     *      `timer.cancel()`
     *  So the handler is calcelled from [_io_service](@ref _io_service) 
     *
     * 
     *  @param peer_c   The peer connection structure.
     *  @param timer    The deadline_timer that call the handler. 
     */

    void send_keep_alive(const pwp::peer_connection& peer_c, boost::asio::deadline_timer& timer){
        using namespace boost::asio;
        using namespace boost::asio::ip;

        //Start the request
        if(is_inv_address(peer_c.peer_.addr)){  //Check if it's invalid IP
            return;
        }

        try{ 

            if(!peer_c.socket->is_open()){
                LOG(ERROR) << "Keep-Alive routine : Error socket not opened, cancelling keep-alive routine";
                timer.cancel();
                return;
            }

            boost::system::error_code error;
            std::array<uint8_t, 4> keep_alive_msg;

            keep_alive_msg.fill(0);

            LOG(INFO) << "Sending keep-alive";
            peer_c.socket->send(buffer(keep_alive_msg),0, error);
        

            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                return;
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }
            
        }catch (std::exception& e){
            LOG(ERROR) << "Keep-Alive request : " << e.what();
        }
    }


    /**
     *  @brief Starts Keep-Alive request loop 
     *  Start the timer that each KEEP_ALIVE_TIME send a Keep-alive request
     * 
     *  @param peer_c   The peer's connection structure
     * 
     */

    void enable_keep_alive_message(pwp::peer_connection& peer_c){
        boost::asio::deadline_timer timer(_io_service, boost::posix_time::seconds(KEEP_ALIVE_TIME));

        timer.async_wait(boost::bind(send_keep_alive, peer_c, std::ref(timer)));

        if(_io_service.stopped()){
            DLOG(INFO) << "IO-Service stopped, resetting";
            _io_service.reset();
            _io_service.run();
        }else{
            _io_service.poll();
        }
    }


    /**
     *  @brief Send `msg` to the peer specified in `peer_c`
     * 
     *  Error code list
     *  
     *  Error Code | Explaination
     *  -----------|-------------
     *  -1         | Invalid Address
     *  -2         | Cannot open socket
     *  -3         | Connection closed
     *  -4         | Exception (see the output)
     *  
     *  @param peer_c   Peer connection structure
     *  @param msg      Message to send
     *  @return 0 on success, <0 otherwise
     */
    
    int send_msg(pwp::peer_connection& peer_c, std::vector<uint8_t> msg){
        using namespace boost::asio;
        using namespace boost::asio::ip;
        
        if(is_inv_address(peer_c.peer_.addr)){  //Check if it's invalid IP
            return -1;
        }

        //Start the request
        try{ 

            LOG(INFO) << "Checking connection";
            if(!peer_c.socket->is_open()){
                std::cout << std::endl << "Error socket not opened!!!!!";
                return -2;
            }

            
            boost::system::error_code error;
            size_t len;

            LOG(INFO) << "Sending message";
            len = peer_c.socket->send(buffer(msg));

            if (error == boost::asio::error::eof){
                LOG(ERROR) << "Connection Closed";
                return -3;
            }else if (error){
                throw boost::system::system_error(error); // Some other error.
            }
            
        }catch (std::exception& e){
            LOG(ERROR) << peer_c.peer_.addr.to_string() << " " << e.what() << std::endl;
            return -4;
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

        std::vector<uint8_t> index = from_int_to_bint((uint)request.index);
        std::vector<uint8_t> begin = from_int_to_bint((uint)request.begin);
        std::vector<uint8_t> length = from_int_to_bint((uint)request.length);

        msg.insert(msg.end(), index.begin(), index.end());
        msg.insert(msg.end(), begin.begin(), begin.end());
        msg.insert(msg.end(), length.begin(), length.end());

        return msg;
    }

    int get_bitfield(pwp::peer_connection &peer_c, Torrent &torrent) {
        using namespace std;
        using namespace boost::asio;

        vector<uint8_t> response(5);
        try{
            boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(5));
        }catch(exception& e){
            LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << endl;
            return -3;
        }

        cout << endl << string_to_hex(response) << endl;

        uint8_t msg_id = response[4];
        if (msg_id != pwp_msg::bitfield) {
/*
            if(response[3] == 1){
                cout << peer_c.peer_.addr << "Received Choke request---------------" << endl;
            }else{*/
                cout << peer_c.peer_.addr << " Error reading bitfield, abort. Message ID : " << to_string(msg_id) << "\n";
                return -1;
            //}


        }

        uint32_t bit_len = (response[0] << 24 | response[1] << 16 | response[2] << 8 | response[3]) - 1;
        uint32_t real_len = torrent.num_pieces/8 + 1;
        if (bit_len > real_len) {
            cout << peer_c.peer_.addr << " BITFIELD TOO LONG: " << bit_len << endl;
            return -2;
        }

        vector<uint8_t> *bit_vector = new vector<uint8_t>(bit_len);

        cout << peer_c.peer_.addr << " BITFIELD, length: " << bit_len << endl;

        try{
            boost::asio::read(*(peer_c.socket), buffer(*bit_vector), transfer_exactly(bit_len));
        }catch(exception& e){
            LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << endl;
            return -3;
        }

        boost::dynamic_bitset<> *new_bitfield = new boost::dynamic_bitset<>();

        // loop each byte extracted
        for (uint32_t i = 0; i < bit_len; ++i) {
            boost::dynamic_bitset<> temp(8, uint8_t(bit_vector->at(i)));
            // insert each bit into the bitfield
            for (int k = 7; k >= 0; --k) {
                new_bitfield->push_back(temp[k]);
            }
        }

        peer_c.bitfield = *new_bitfield;

        delete bit_vector;
        delete new_bitfield;   //Deallocate memory
        
        return 0;
    }

    /**
     * @brief The handler of the message for the PWP protocol
     * 
     * 
     * Initially the message length is readed and next the corresponding byte are readed from 
     * the socket stream. 
     * Then the message ID is parsed and the appropriate action is taken
     * 
     * See [@ref pwp_msg::msg_id] for message types
     * 
     * 
     * @param response  The response received
     * @param peer_c    The peer_connection who received the response
     * @param torrent   The torrent structure
     * @param dead_peer Reference to a flag to set if the peer is death
     * @param timer_    Reference of the timer that call the ???? Really necessary?
     * @param error     Socket error   
     * @param bytes_read 
     */


    void read_msg_handler(std::vector<uint8_t>& response, pwp::peer_connection& peer_c, Torrent &torrent, bool& dead_peer,  boost::asio::deadline_timer &timer_, const boost::system::error_code& error, size_t bytes_read){
        using namespace std;
        using namespace boost::asio;
        using namespace rang;

        uint32_t msg_len = response[0] << 24 | response[1] << 16 | response[2] << 8 | response[3];
        uint8_t msg_id;
        if (msg_len > 0) {
            try{
                boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(1));
            }catch(std::exception& e){
                LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << endl;
                dead_peer = true;
                return;
            }
            msg_id = response[0];
        }
        else {
            msg_id = 255; // random value
        }

        timer_.expires_at(boost::posix_time::pos_infin);


        switch(msg_id){

            case 255:
                // cout << setw(15) << left << peer_c.peer_.addr << " KEEP ALIVE received" << endl;
                break;

            case pwp_msg::chocked:
                peer_c.pstate.peer_choking = true;
                cout << setw(15) << left << peer_c.peer_.addr << bg::magenta << " CHOKED" << bg::reset << ", stop sending requests" << endl;
                break;

            case pwp_msg::unchocked:
                peer_c.pstate.peer_choking = false;
                cout << setw(15) << left << peer_c.peer_.addr << bg::magenta << " UNCHOKED " << bg::reset << "received" << endl;
                break;
            
            case pwp_msg::interested:
                peer_c.pstate.peer_interested = true;
                cout << setw(15) << left << peer_c.peer_.addr << " INTERESTED received" << endl;
                break;

            case pwp_msg::not_interested:
                peer_c.pstate.peer_interested = false;
                cout << setw(15) << left << peer_c.peer_.addr << " NOT INTERESTED received" << endl;
                break;
            
            case pwp_msg::have: {
                try{
                    boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(4));
                }catch(std::exception& e){
                    LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << endl;
                    dead_peer = true;
                    return;
                }
                uint32_t piece = response[0] << 24 | response[1] << 16 | response[2] << 8 | response[3];
                
                cout << setw(15) << left << peer_c.peer_.addr << bg::green << " HAVE " << bg::reset << "piece n°" << piece << endl;
                if (piece < (torrent.num_pieces/8 +1)) {
                    peer_c.bitfield.set(piece);
                }
                break;
            }
            
            case pwp_msg::request:
                // seeding not supported, only clean the buffer
                try{
                    boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(12));
                }catch(std::exception& e){
                    LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << endl;
                    dead_peer = true;
                    return;
                }
                cout << setw(15) << left << peer_c.peer_.addr << " REQUEST received" << endl;
                break;

            case pwp_msg::piece: {
                uint32_t index;
                uint32_t begin;
                int32_t piece_len;

                try{
                    // Get index
                    boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(4));
                    index = response[0] << 24 | response[1] << 16 | response[2] << 8 | response[3];

                    // Get begin
                    boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(4));
                    begin = response[0] << 24 | response[1] << 16 | response[2] << 8 | response[3];

                    // Get blockdata
                    piece_len = msg_len - 9;
                    response.resize(piece_len);
                    boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(piece_len));
                }catch(std::exception& e){
                    LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << std::endl;
                    dead_peer = true;
                    return;
                }
                cout << setw(15) << left << peer_c.peer_.addr << bg::cyan << " PIECE " << bg::reset << "received, index: "
                    << index  << ", begin: " << begin << ", length: " << piece_len << endl;

                char *blockdata = reinterpret_cast<char*>(response.data());
                save_block(blockdata, index, begin, piece_len, torrent);

                break;
            }

            case pwp_msg::cancel:
                // seeding not supported, only clean the buffer
                try{
                    boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(12));
                }catch(std::exception& e){
                    LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << std::endl;
                    dead_peer = true;
                    return;
                }
                cout << setw(15) << left << peer_c.peer_.addr << " CANCEL received" << endl;
                break;

            case pwp_msg::port:
                // message not suported, only clean the buffer
                try{
                    boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(2));
                }catch(std::exception& e){
                    LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << std::endl;
                    dead_peer = true;
                    return;
                }
                cout << setw(15) << left << peer_c.peer_.addr << " PORT received" << endl;
                break;
            
            default:
                // Clean the socket buffer
                size_t buffer_len = peer_c.socket->available();
                cout << peer_c.peer_.addr << " Unknown message... cleaning buffer" << endl;
                response.resize(buffer_len);
                boost::asio::read(*(peer_c.socket), buffer(response), transfer_exactly(buffer_len));
                break;
        }

        boost::this_thread::sleep_for(boost::chrono::milliseconds(200));

        try{

            timer_.expires_from_now(boost::posix_time::seconds(15));

            boost::asio::async_read(*(peer_c.socket), boost::asio::buffer(response, sizeof(uint8_t)*4), 
                boost::asio::transfer_exactly(4),
                boost::bind(&pwp_msg::read_msg_handler, boost::ref(response), 
                            boost::ref(peer_c), boost::ref(torrent), boost::ref(dead_peer), boost::ref(timer_),
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred
                )
            );

            if(_io_service.stopped()){
                DLOG(INFO) << std::endl << "IO-Service stopped, resetting";
                _io_service.reset();
                _io_service.run();
            }
        }catch(std::exception& e){
                    LOG(ERROR) << peer_c.peer_.addr << ' ' << e.what() << std::endl;
                    dead_peer = true;
                    return;
        }

    }

    /**
     * @brief Send appropriate PWP message to download the file
     * 
     * The bitfield is compared and a piece index is returned; then a _request_ type message
     * is created and sended to the peer 
     * 
     * @param peer_conn 
     * @param torrent 
     * @param old_begin 
     * @return int 
     */

    int sender(pwp::peer_connection &peer_conn, Torrent &torrent, int &old_begin) {
        using namespace std;
        
        if (!peer_conn.pstate.peer_choking && peer_conn.cstate.am_interested && peer_conn.bitfield.size() > 0) {
            
            int piece_index = compare_bitfields(peer_conn.bitfield, torrent.bitfield);

            if (piece_index != -1) {
                RequestMsg request = create_request(torrent, piece_index);
                if (request.begin == old_begin) {
                    return 0;
                }
                else {
                    old_begin = request.begin;
                }

                if (request.begin != std::string::npos) {
                    vector<uint8_t> msg = make_request_msg(request);
                    // cout << peer_conn.peer_.addr << " sending REQUEST: " << string_to_hex(msg) << std::endl;
                    return send_msg(peer_conn, msg);                    
                }
            }
            else {
                peer_conn.cstate.am_interested = false;
            }
        }
        return 0;
    }
}