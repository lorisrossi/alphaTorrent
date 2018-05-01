#include <boost/filesystem.hpp>
#include <iostream>
#include <string>
#include <openssl/sha.h>

#include <chrono>

#include "filehandler.hpp"

using namespace std;

/**
 * Make a new file in the filesystem (create subfolders, if any).
 * The new file has the right size (taken from torrent) but it is empty
 * The pathname has the suffix ".part", it will be removed once the file is complete
 *
 * @param main_folder  String of the main folder where to create the file.
 *                     For current directory use ".".
 * @param tfile        TorrentFile struct with file information
 */
void make_file(const string &main_folder, const TorrentFile &tfile) {
  string temp_path = main_folder;
  // create subfolders
  for (size_t i=0; i < tfile.path.size() - 1; ++i) {
    temp_path += '/' + tfile.path[i];
    boost::filesystem::create_directory(temp_path);
  }
  temp_path += '/' + tfile.path.back() + ".part";
  // create file
  ofstream new_file(temp_path);
  new_file.close();
  boost::filesystem::resize_file(temp_path, tfile.length);
}

void init_bitfield(Torrent &torrent) {
  if (torrent.is_single) {
    string path = torrent.name + ".part";
    ifstream cur_file(path, ifstream::binary);
    if (cur_file.is_open()) {

      cur_file.seekg (0, cur_file.end);
      // file_length = files[0].length
      int file_length = cur_file.tellg();
      cur_file.seekg (0, cur_file.beg);

      size_t num_pieces = torrent.pieces.size() / 20;
      // bitfield size is multiple of 8
      torrent.bitfield.resize(num_pieces + (8 - num_pieces%8));

      // variables used in for cycle
      string piece_hash;
      char file_hash[torrent.piece_length];
      unsigned char digest[20];
      int hashlength = torrent.piece_length;

      for (size_t i=0; i < num_pieces - 1; ++i) {
        piece_hash = torrent.pieces.substr(i*20, 20);
        cur_file.read(file_hash, hashlength);
        cur_file.seekg((i+1)*hashlength);
        SHA1((unsigned char*)(file_hash), hashlength, digest);
        torrent.bitfield.at(i) = (memcmp(piece_hash.c_str(), digest, 20) == 0) ? '1' : '0';
      }
      // last piece
      hashlength = file_length % torrent.piece_length;
      piece_hash = torrent.pieces.substr((num_pieces - 1)*20, 20);
      cur_file.read(file_hash, hashlength);
      SHA1((unsigned char*)(file_hash), hashlength, digest);
      torrent.bitfield.at(num_pieces-1) = (memcmp(piece_hash.c_str(), digest, 20) == 0) ? '1' : '0';

      // add spare bits at the end of bitfield
      if (num_pieces % 8 != 0) {
        for (size_t i = num_pieces; i < torrent.bitfield.size(); i++) {
          torrent.bitfield.at(i) = 0;
        }
      }

      cur_file.close();
      cout << "Bitfield: " << torrent.bitfield << endl;
    }
  }
}

void check_files(Torrent &torrent) {
  if (torrent.is_single) {
    // check for file
    if (boost::filesystem::exists(torrent.name)) {
      cout << "File is completed." << endl;
    }
    else if (boost::filesystem::exists(torrent.name + ".part")) {
      cout << "File exists, initialize bitfield" << endl;
      init_bitfield(torrent);
    }
    else {
      cout << "New torrent, make new file and allocate memory" << endl;
      make_file(".", torrent.files.at(0));
    }
  }
  else {
    cout << "check_files with multiple files torrent not implemented yet" << endl;
  }
  // else {
  //   // check for folder, then for files
  //   if (boost::filesystem::create_directory(torrent.name)) {
  //     cout << "New torrent, folder created" << endl;
  //   }
  //   else {
  //     cout << "Folder exists, check of downloaded pieces" << endl;
  //   }
  //   for (size_t i=0; i<torrent.files.size(); ++i)
  //     make_file(torrent.name, torrent.files[i]);
  // }
}
