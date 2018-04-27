#include <boost/filesystem.hpp>
#include <iostream>
#include "filehandler.h"

void make_file(string &path, long int size) {
  ofstream new_file(path);
  new_file.close();
  boost::filesystem::resize_file(path, size);
}

void check_files(Torrent torrent) {
  if (torrent.is_single) {
    // check for file
    if (boost::filesystem::exists(torrent.name)) {
      cout << "File exists, check of downloaded pieces (bitfield)" << endl;
    }
    else {
      cout << "New torrent, make new file and allocate memory" << endl;
      make_file(torrent.files[0].path, torrent.files[0].length);
    }
  }
  else {
    // check for folder, then for files
    if (boost::filesystem::create_directory(torrent.name)) {
      cout << "New torrent, folder created" << endl;
    }
    else
      cout << "Folder exists, check of downloaded pieces" << endl;
  }
}
