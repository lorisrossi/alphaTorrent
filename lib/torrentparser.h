#include <string>
#include <vector>
#include "bencode.h"

using namespace std;

typedef struct {
  string path;
  long int length;
} TorrentFile;

typedef struct {
  string tracker_url;
  string name;
  int piece_length;
  string pieces;
  vector<TorrentFile> files;
} Torrent;

void parse_torrent(be_node *node, Torrent &new_torrent);
void print_torrent(Torrent torrent);
