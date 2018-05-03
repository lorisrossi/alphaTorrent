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
  vector<string> path;
  long int length;
} TorrentFile;

// Struct to store all the information of a torrent
typedef struct {
  vector<string> tracker_urls;
  string name;
  int piece_length;
  string pieces;
  vector<TorrentFile> files;
  bool is_single = false; // true if single file torrent
} Torrent;

void parse_torrent(be_node *node, Torrent &new_torrent);
void print_torrent(Torrent torrent);
char *get_info_node_hash(const string *file, const string *pieces_string);
#endif
