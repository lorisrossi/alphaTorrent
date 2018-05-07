#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include "torrentparser.hpp"

typedef struct {
  size_t index;
  size_t begin;
  size_t length;
} RequestMsg;

void check_files(Torrent &torrent);

#endif
