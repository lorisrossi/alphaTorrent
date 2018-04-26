#ifndef TORRENTPARSER_H
#define TORRENTPARSER_H

#include <string>
#include <vector>
#include <assert.h>
#include "bencode.h"


#include <openssl/sha.h>

using namespace std;

// Struct to store information of a file inside a torrent
typedef struct {
  string path;
  long int length;
} TorrentFile;

// Struct to store all the information of a torrent
typedef struct {
  string tracker_url;
  string name;
  int piece_length;
  string pieces;
  vector<TorrentFile> files;
} Torrent;

void parse_torrent(be_node *node, Torrent &new_torrent);
void print_torrent(Torrent torrent);
unsigned char *get_info_node_hash(string *file, string *pieces_string);
#endif
