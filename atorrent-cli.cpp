#include <iostream>
#include <fstream>
#include <string>
#include <string.h>

#include "bencode.h"
#include "torrentparser.h"
#include "tracker.h"
#include "filehandler.h"

using namespace std;

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

int main(int argc, char* argv[]) {

  if (argc <= 1) {
    cout << "Usage: pass a .torrent file as a parameter" << endl;
    return 1;
  }
  curl_global_init(CURL_GLOBAL_DEFAULT);

  ifstream myfile(argv[1]);
  if (myfile.is_open()) {
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
    if (node) {
      // be_dump(n); // BE_DEBUG must be greater than 0
      Torrent mytorrent;
      parse_torrent(node, mytorrent);
      be_free(node);
      print_torrent(mytorrent);

      TrackerParameter param;

      param.info_hash_raw = get_info_node_hash(&torrent_str, &mytorrent.pieces);
      param.tracker_urls = mytorrent.tracker_urls;

      param.left = mytorrent.piece_length * mytorrent.pieces.size();
      get_peer_id(&param.peer_id);
      
      start_tracker_request(&param);

      free(param.info_hash_raw);
    }
    else
      cout << "Parsing of " << argv[1] << " failed!" << endl;
  }
  else
    cout << "Unable to open the file." << endl;




  curl_global_cleanup();
  return 0;
}
