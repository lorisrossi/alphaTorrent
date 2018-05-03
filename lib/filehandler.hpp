#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include "torrentparser.hpp"

typedef struct {
  size_t index;
  size_t begin;
  size_t length;
} RequestMsg;

void check_files(Torrent &torrent);

#endif
