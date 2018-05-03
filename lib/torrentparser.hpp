#ifndef TORRENTPARSER_H
#define TORRENTPARSER_H

#include <string>
#include <vector>
#include <assert.h>
#include <openssl/sha.h>

#include "bencode.h"

// Struct to store information of a file inside a torrent
// The element of path with highest index is the file.
// The other elements (if any) are subfolders
typedef struct {
  std::vector<std::string> path;
  long int length;
} TorrentFile;

// Struct to store all the information of a torrent
typedef struct {
  std::vector<std::string> trackers;
  std::string name;
  int piece_length;
  std::string pieces;
  std::vector<TorrentFile> files;
  bool is_single = false; // true if single file torrent
} Torrent;

void parse_torrent(const be_node *node, Torrent &new_torrent);
void print_torrent(const Torrent &torrent);
char *get_info_node_hash(const std::string *file, const std::string *pieces_string);

#endif
