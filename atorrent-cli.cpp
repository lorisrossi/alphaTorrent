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


void worker(){
  while( true )
	{
    try
    {
      boost::system::error_code ec;
      _io_service.run( ec );
      if( ec )
      {
        std::cout << "[" << boost::this_thread::get_id()
        << "] Error: " << ec << std::endl;
      }
      break;
    }
    catch( std::exception & ex )
    {
      std::cout << "[" << boost::this_thread::get_id()
      << "] Exception: " << ex.what() << std::endl;
    }
	}
}


int main(int argc, char* argv[]) {

  if (argc <= 1) {
    cout << "Usage: pass a .torrent file as a parameter" << endl;
    return 1;
  }




  google::InitGoogleLogging(argv[0]); //Initialize GLog with passed argument

  curl_global_init(CURL_GLOBAL_DEFAULT);

  ifstream myfile(argv[1]);
  if (!myfile.is_open()){
    cout << "Unable to open the file." << endl;
    return -1;  //Exit the progra,m
  }


  // parsing .torrent file as a string
  string line;
  string torrent_str;
  while (getline(myfile, line))
  torrent_str += line + '\n';
  myfile.close();

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
  parse_torrent(node, mytorrent);
  be_free(node);
  print_torrent(mytorrent);

  tracker::TParameter param;

  //Populate the torret parameter
  param.info_hash_raw = get_info_node_hash(&torrent_str, &mytorrent.pieces);  //Extract info_hash
  param.left = mytorrent.piece_length * mytorrent.pieces.size();
  get_peer_id(&param.peer_id);


  pwp::PeerList peer_list = make_shared<vector<pwp::peer>>(10); //Pre-Allocate the peers list

  do{

    peer_list->clear();
    peer_list->resize(10);
    
    //Start contacting the tracker
    int error_code = start_tracker_request(&param, mytorrent.trackers, peer_list);  
    if(error_code < 0){
      return -3;  //Error while encoding param, exit 
    }

    //remove_invalid_peer(peer_list);

    std::vector<uint8_t> handshake = std::vector<uint8_t>();

    DLOG(INFO) << "Building handshake...";
    build_handshake(param.info_hash_raw, handshake);

    vector<pwp::peer>::iterator it = peer_list->begin();
    boost::thread_group t_group;
    //Start the PWP protocol with the peers
    int i=0;

    _io_service.run();
    t_group.add_thread(new boost::thread(worker));
    for(;it != peer_list->end(); ++it){

        cout<< "Starting executing the protocol with " << it->addr.to_string() << ":" << it->port << "... " << i << endl;
        t_group.add_thread(new boost::thread( pwp_protocol_manager, *it, handshake, param.info_hash_raw));
        i++;
    }

    _io_service.run();
    boost::this_thread::sleep_for(boost::chrono::seconds(10));
  }while(active_peer < PEER_TREESHOLD);

  //t_group.join_all();
  //Once finished manage all peer
  //manage_peer_connection(peer_list, param.info_hash_raw);




  if(param.info_hash_raw != nullptr)
    free(param.info_hash_raw);

      
  

  curl_global_cleanup();
  return 0;
}
