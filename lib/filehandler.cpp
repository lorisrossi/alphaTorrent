#include <boost/filesystem.hpp>
#include <iostream>
#include "filehandler.hpp"

using namespace std;

/**
 * Make a new file in the filesystem (create subfolders, if any).
 * The new file has the right size (taken from torrent) but it is empty
 *
 * @param main_folder  String of the main folder where to create the file.
 *                     For current directory use ".".
 * @param tfile        TorrentFile struct with file information
 */
void make_file(const string &main_folder, TorrentFile tfile) {
  string temp_path = main_folder;
  // create subfolders
  for (size_t i=0; i < tfile.path.size() - 1; ++i) {
    temp_path += '/' + tfile.path[i];
    boost::filesystem::create_directory(temp_path);
  }
  temp_path += '/' + tfile.path.back();
  // create file
  ofstream new_file(temp_path);
  new_file.close();
  boost::filesystem::resize_file(temp_path, tfile.length);
}

void check_files(Torrent torrent) {
  if (torrent.is_single) {
    // check for file
    if (boost::filesystem::exists(torrent.name)) {
      cout << "File exists, check of downloaded pieces (bitfield)" << endl;
    }
    else {
      cout << "New torrent, make new file and allocate memory" << endl;
      make_file(".", torrent.files[0]);
    }
  }
  else {
    // check for folder, then for files
    if (boost::filesystem::create_directory(torrent.name)) {
      cout << "New torrent, folder created" << endl;
    }
    else {
      cout << "Folder exists, check of downloaded pieces" << endl;
    }
    for (size_t i=0; i<torrent.files.size(); ++i)
      make_file(torrent.name, torrent.files[i]);
  }
}


void get_bitfield() {
  // check with sha1
}
