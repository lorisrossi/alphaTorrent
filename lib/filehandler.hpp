/**
 * @file filehandler.hpp
 *
 * Functions for handling filesystem operations (read/write)
 */
#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <boost/dynamic_bitset.hpp>
#include "torrentparser.hpp"

/**
 * @brief Namespace with functions for handling file i/o
 * 
 */
namespace fileio {

  /**
   * @brief Struct corresponding to a pwp "request" message
   * 
   */
  typedef struct {
    size_t index;
    size_t begin;
    size_t length;
  } RequestMsg;

  void check_files(torr::Torrent &torrent);
  int compare_bitfields(boost::dynamic_bitset<> peer_bitfield, boost::dynamic_bitset<> own_bitfield);
  RequestMsg create_request(torr::Torrent &torrent, int piece_index);
  void save_block(char *blockdata, size_t index, size_t begin, size_t length, torr::Torrent &torrent);

}

#endif
