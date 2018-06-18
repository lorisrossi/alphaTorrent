#include <iostream>
#include <string>
#include <string.h>
#include <glog/logging.h>   //Logging Library
#include <boost/asio/signal_set.hpp>  //For interrupt signal

#include "torrentparser.hpp"
#include "tracker.h"
#include "filehandler.hpp"
#include "peer.h"
#include "pwp.hpp"  //For is_inv_address
#include "rang.hpp"

using namespace std;
using namespace torr;
using namespace fileio;
using namespace rang;

#define PEER_TREESHOLD 3

tracker::TParameter set_parameter(const string& torrent_str, const Torrent& torr){
    tracker::TParameter param;

    param.info_hash_raw = get_info_node_hash(&torrent_str, &torr.pieces);  //Extract info_hash
    param.left = torr.piece_length * torr.pieces.size();
    pwp::get_peer_id(&param.peer_id);
    
    return param;
}

/**
 * @brief Handler of the SIGINT signal
 * 
 * This handler clear the memory and stops the threads safely
 * 
 * @param t_group The group of active thread
 */

void innerrupt_handler(boost::thread_group &t_group){

  cout << "Handled interrupt signal " << endl;
  cout << "Interrupting thread...";
  t_group.interrupt_all();

  curl_global_cleanup();
  cout << fg::red << "Exiting" << fg::reset << endl;
  exit(1);


}



int main(int argc, char* argv[]) {

  using namespace rang;
  using namespace std;

  if (argc <= 1) {
    cout << "Usage: pass a .torrent file as a parameter" << endl;
    return 1;
  }

  google::InitGoogleLogging(argv[0]); //Initialize GLog with passed argument
  curl_global_init(CURL_GLOBAL_DEFAULT);


  pwp::PeerList peer_list = make_shared<vector<pwp::peer>>(); //Pre-Allocate the peers list
  boost::thread_group t_group;  //Thread Group for managing the peers


  boost::asio::signal_set signals(_io_service, SIGINT );
  signals.async_wait( boost::bind(&innerrupt_handler, boost::ref(t_group)) );


  Torrent mytorrent;
  string torrent_str;
  // parse .torrent file
  int success = parse_torrent(mytorrent, torrent_str, argv[1]);
  if(success < 0) {
    return success;
  }

  check_files(mytorrent);

  //Populate the torrent parameter
  tracker::TParameter param = set_parameter(torrent_str, mytorrent);

  while(true){

    while(active_peer < PEER_TREESHOLD){

        peer_list->clear();
        
        //Start contacting the tracker

        cout << fg::gray << bg::magenta << "Start Tracker Request" << style::reset << bg::reset << endl;;
        int error_code = start_tracker_request(&param, mytorrent.trackers, peer_list);  
        if(error_code < 0){
          cout << fg::red << "Error while encoding parameters, exit" << fg::reset << endl;
          return -3; 
        }

        pwp::remove_invalid_peer(peer_list);

        DLOG(INFO) << "Building handshake..." << endl;
        std::vector<uint8_t> handshake = std::vector<uint8_t>();
        pwp::build_handshake(param.info_hash_raw, handshake);


        //Start the PWP protocol with the peers

        vector<pwp::peer>::iterator it = peer_list->begin();

        for(;it != peer_list->end(); ++it){
            if(!is_inv_address(it->addr)){
              cout<< "Starting executing the protocol with " << it->addr.to_string() << ":" << it->port << "... " << endl;
              t_group.add_thread(new boost::thread( &pwp::pwp_protocol_manager, *it, handshake, param.info_hash_raw, mytorrent));
            }
        }

        if(_io_service.stopped()){
            cout << endl << "-------------------------STOPPED-----------------------" << endl;
            _io_service.reset();
            signals.async_wait( boost::bind(&innerrupt_handler, boost::ref(t_group)) );
            _io_service.run();
        }

        boost::this_thread::sleep_for(boost::chrono::seconds(10));  //Sleep for 10 seconds
    };

    cout << endl << "Having enought active peers (" << active_peer << "), recheck after 10 seconds... " << endl;
    boost::this_thread::sleep_for(boost::chrono::seconds(10));
  }

  t_group.join_all();



  if(param.info_hash_raw != nullptr)
    free(param.info_hash_raw);

      
  curl_global_cleanup();
  return 0;
}
