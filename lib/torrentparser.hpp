/**
 * @file torrentparser.hpp
 *
 * Functions for parsing a metainfo (.torrent) file and structs to store parsed information
 */
#ifndef TORRENTPARSER_HPP
#define TORRENTPARSER_HPP

#include <string>
#include <vector>
#include <boost/dynamic_bitset.hpp>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <openssl/sha.h>
#include <string.h>

#include "bencode.h"



/**
 * @brief Struct to store information of a file inside a torrent.
 * 
 * The element of path with highest index is the file.
 * The other elements (if any) are subfolders
 */
typedef struct {
  std::vector<std::string> path;
  long int length;
} TorrentFile;

/**
 * @brief Struct to store all the information of a torrent
 * 
 */
typedef struct {
  std::vector<std::string> trackers;
  std::string name;
  int piece_length;
  std::string pieces;
  size_t num_pieces;
  std::vector<TorrentFile> files;
  boost::dynamic_bitset<> bitfield;
  bool is_single = false; // true if single file torrent
} Torrent;

int parse_torrent(Torrent &new_torrent, std::string &torrent_str, const char* filename);
char *get_info_node_hash(const std::string *file, const std::string *pieces_string);

#endif
