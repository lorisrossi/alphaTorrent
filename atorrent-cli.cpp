#include <iostream>
#include <fstream>
#include <string>
#include <string.h>

#include "bencode.h"
#include "torrentparser.hpp"
#include "tracker.h"
#include "filehandler.hpp"
#include "peer.h"

using namespace std;

#define PEER_TREESHOLD 10

void read_file(const char *path, string& dest){
  
    ifstream torrent_file(path);

    // parsing .torrent file as a string
    string line;
    while (getline(torrent_file, line))
    dest += line + '\n';
    torrent_file.close();
}


tracker::TParameter set_parameter(const string& torrent_str, const Torrent& torr){
    tracker::TParameter param;

    param.info_hash_raw = get_info_node_hash(&torrent_str, &torr.pieces);  //Extract info_hash
    param.left = torr.piece_length * torr.pieces.size();
    get_peer_id(&param.peer_id);
    
    return param;
}


int main(int argc, char* argv[]) {

  if (argc <= 1) {
    cout << "Usage: pass a .torrent file as a parameter" << endl;
    return 1;
  }


  google::InitGoogleLogging(argv[0]); //Initialize GLog with passed argument
  curl_global_init(CURL_GLOBAL_DEFAULT);  //Initialize cURL

  string torrent_str;
  try{
    read_file(argv[1], torrent_str);
  }catch(const ifstream::failure& e){
    LOG(ERROR) << "Error : " << e.what();
    return -1;
  }


  // decoding
  be_node *node;
  long long len = torrent_str.length();
  node = be_decoden(torrent_str.c_str(), len);
  if (!node) {
    cout << "Parsing of " << argv[1] << " failed!" << endl;
    return -2;    //Exit the program
  }

  //Start processing the torrent file
  Torrent mytorrent;
  {
    parse_torrent(node, mytorrent);
    be_free(node);
    print_torrent(mytorrent);
  }


  //Populate the torret parameter
  tracker::TParameter param = set_parameter(torrent_str, mytorrent);

  pwp::PeerList peer_list = make_shared<vector<pwp::peer>>(); //Pre-Allocate the peers list
  boost::thread_group t_group;  //Thread Group for managing the peers

  do{

    peer_list->clear();
    peer_list->resize(10);
    
    //Start contacting the tracker
    int error_code = start_tracker_request(&param, mytorrent.trackers, peer_list);  
    if(error_code < 0){
      return -3;  //Error while encoding param, exit 
    }

    remove_invalid_peer(peer_list);

    LOG(INFO) << "Building handshake...";
    std::vector<uint8_t> handshake = std::vector<uint8_t>();
    build_handshake(param.info_hash_raw, handshake);


    //Start the PWP protocol with the peers

    vector<pwp::peer>::iterator it = peer_list->begin();

    for(;it != peer_list->end(); ++it){
        cout<< "Starting executing the protocol with " << it->addr.to_string() << ":" << it->port << "... " << endl;
        t_group.add_thread(new boost::thread( pwp_protocol_manager, *it, handshake, param.info_hash_raw));
    }

    _io_service.run();
    boost::this_thread::sleep_for(boost::chrono::seconds(10));  //Sleep for 10 seconds


  }while(active_peer < PEER_TREESHOLD);

  t_group.join_all();


  if(param.info_hash_raw != nullptr)
    free(param.info_hash_raw);

      
  curl_global_cleanup();
  return 0;
}
