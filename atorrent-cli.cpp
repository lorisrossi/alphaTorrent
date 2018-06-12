



#include <iostream>
#include <string>
#include <string.h>
#include <glog/logging.h>   //Logging Library

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

#define PEER_TREESHOLD 10

tracker::TParameter set_parameter(const string& torrent_str, const Torrent& torr){
    tracker::TParameter param;

    param.info_hash_raw = get_info_node_hash(&torrent_str, &torr.pieces);  //Extract info_hash
    param.left = torr.piece_length * torr.pieces.size();
    pwp::get_peer_id(&param.peer_id);
    
    return param;
}


int main(int argc, char* argv[]) {

  if (argc <= 1) {
    cout << "Usage: pass a .torrent file as a parameter" << endl;
    return 1;
  }

  google::InitGoogleLogging(argv[0]); //Initialize GLog with passed argument
  curl_global_init(CURL_GLOBAL_DEFAULT);

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

  pwp::PeerList peer_list = make_shared<vector<pwp::peer>>(); //Pre-Allocate the peers list
  boost::thread_group t_group;  //Thread Group for managing the peers

  while(1){

    while(active_peer < PEER_TREESHOLD){

      peer_list->clear();
      peer_list->resize(10);
      
      //Start contacting the tracker

      cout << fg::gray << bg::magenta << "Start Tracker Request\n" << style::reset << bg::reset;
      int error_code = start_tracker_request(&param, mytorrent.trackers, peer_list);  
      if(error_code < 0){
        return -3;  //Error while encoding param, exit 
      }

      pwp::remove_invalid_peer(peer_list);

      cout << "Building handshake...";
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
          cout << endl << endl << "-------------------------STOPPED-----------------------" << endl;
          _io_service.reset();
          _io_service.run();
      }
      boost::this_thread::sleep_for(boost::chrono::seconds(10));  //Sleep for 10 seconds


    };

    cout << "--------------------EXITED : " << to_string(active_peer) << "------------------------" << endl;
    
    
    if(t_group.size() > active_peer){
      cout << endl << "More thread than active peers!!!!!\t" << t_group.size() << endl;

    }
    boost::this_thread::sleep_for(boost::chrono::seconds(5));
  }
  t_group.join_all();



  if(param.info_hash_raw != nullptr)
    free(param.info_hash_raw);

      
  curl_global_cleanup();
  return 0;
}
