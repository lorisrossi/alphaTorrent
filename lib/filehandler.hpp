#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <boost/dynamic_bitset.hpp>
#include "torrentparser.hpp"

typedef struct {
  size_t index;
  size_t begin;
  size_t length;
} RequestMsg;

void check_files(Torrent &torrent);
int compare_bitfields(boost::dynamic_bitset<> peer_bitfield, boost::dynamic_bitset<> own_bitfield);
RequestMsg create_request(Torrent &torrent, int piece_index);

#endif
