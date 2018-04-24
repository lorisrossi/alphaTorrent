#include <iostream>
#include <fstream>
#include <string>

#include "bencode.h"
#include "torrentparser.h"

using namespace std;

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    cout << "Usage: pass a .torrent file as a parameter" << endl;
    return 1;
  }

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
    }
    else
      cout << "Parsing of " << argv[1] << " failed!" << endl;
  }
  else
    cout << "Unable to open the file." << endl;

  return 0;
}
