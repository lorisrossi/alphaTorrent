// WARNING: as of now, only single file torrents can be used.
// Multiple file torrents are not supported.

#include <boost/filesystem.hpp>
#include <boost/dynamic_bitset.hpp>
#include <iostream>
#include <string>
#include <algorithm> // min()
#include <openssl/sha.h>

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
    ifstream cur_file(path);
    if (cur_file.is_open()) {

      cur_file.seekg (0, cur_file.end);
      // file_length = torrent.files[0].length
      int file_length = cur_file.tellg();
      cur_file.seekg (0, cur_file.beg);

      // variables used in for cycle
      string piece_hash;
      char file_hash[torrent.piece_length];
      unsigned char digest[20];
      int hashlength = torrent.piece_length;

      for (size_t i=0; i < torrent.num_pieces - 1; ++i) {
        piece_hash = torrent.pieces.substr(i*20, 20);
        cur_file.read(file_hash, hashlength);
        cur_file.seekg((i+1)*hashlength);
        SHA1((unsigned char*)(file_hash), hashlength, digest);
        torrent.bitfield.at(i) = (memcmp(piece_hash.c_str(), digest, 20) == 0) ? '1' : '0';
      }
      // last piece
      hashlength = file_length % torrent.piece_length;
      piece_hash = torrent.pieces.substr((torrent.num_pieces - 1)*20, 20);
      cur_file.read(file_hash, hashlength);
      SHA1((unsigned char*)(file_hash), hashlength, digest);
      torrent.bitfield.at(torrent.num_pieces-1) = (memcmp(piece_hash.c_str(), digest, 20) == 0) ? '1' : '0';

      cur_file.close();
      cout << "Bitfield: " << torrent.bitfield << endl;
    }
  }
  else {
    cout << "Bitfield of multiple files torrent not supported" << endl;
  }
}

void check_files(Torrent &torrent) {
  if (torrent.is_single) {
    // check for file
    if (boost::filesystem::exists(torrent.name)) {
      cout << torrent.name << " is already completed" << endl;
    }
    else if (boost::filesystem::exists(torrent.name + ".part")) {
      cout << "File exists, initialize bitfield" << endl;
      init_bitfield(torrent);
    }
    else {
      cout << "New torrent, make new file and allocate memory" << endl;
      make_file(".", torrent.files.at(0));
      init_bitfield(torrent);
    }
  }
  else {
    cout << "ERROR: multiple files torrent not supported" << endl;
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

// return index of a piece that the peer can give to us
// return -1 if the peer doesn't have any piece we need
int compare_bitfields(string peer_bitfield, string own_bitfield) {
  assert(peer_bitfield.size() == own_bitfield.size());
  boost::dynamic_bitset<unsigned char> own(own_bitfield), peer(peer_bitfield);
  boost::dynamic_bitset<unsigned char> temp = ~own & peer;
  size_t block_index = temp.find_first();
  if (block_index != boost::dynamic_bitset<>::npos) {
    // find_first() start from the right, but bitfield has 0-index on the left,
    // so we need to reverse the value
    return own_bitfield.size() - 1 - block_index;
  }
  else return -1;
}

// check the value of the bitfield at index "piece_index"
bool check_bitfield_piece(Torrent &torrent, size_t piece_index) {
  bool is_complete = false;
  // single file torrent only
  string path = torrent.files[0].path[0] + ".part";
  ifstream source(path);
  if (source.is_open()) {
    int hashlength = torrent.piece_length;
    // check if last piece
    if (piece_index == (torrent.num_pieces - 1)) {
      hashlength = torrent.files[0].length % torrent.piece_length;
    }

    string piece_hash = torrent.pieces.substr(piece_index*20, 20);
    char file_hash[hashlength];
    unsigned char digest[20];
    source.seekg(piece_index * torrent.piece_length);
    source.read(file_hash, hashlength);
    SHA1((unsigned char*)(file_hash), hashlength, digest);

    is_complete = memcmp(piece_hash.c_str(), digest, 20) == 0;
    torrent.bitfield.at(piece_index) = is_complete ? '1' : '0';

    if (is_complete) {
      cout << "New piece completed! Piece index: " << piece_index << endl;
    }
  }
  source.close();
  return is_complete;
}

RequestMsg compose_request_msg(string &path, Torrent &torrent, size_t piece_index) {
  RequestMsg request;
  request.index = piece_index;
  fstream source(path);
  if (source.is_open()) {
    int blocklength = torrent.piece_length;
    // check if last piece
    if (piece_index == (torrent.num_pieces - 1)) {
      blocklength = torrent.files[0].length % torrent.piece_length;
    }

    char piece_str[blocklength];
    source.seekg(request.index * torrent.piece_length);
    source.read(piece_str, blocklength);
    string str(piece_str, blocklength);
    // we are assuming that the file doesn't have any NULL string...
    request.begin = str.find("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 0, 15);
    if (request.begin != string::npos) {
      // TODO: the 2nd value is hardcoded, improve it
      request.length = min(blocklength - request.begin, (size_t)40000);

      cout << "Requesting block, index: " << request.index << ", begin:"
        << request.begin << ", length: " << request.length << endl;
    }
    else {
      // nothing to request, maybe the piece is complete or it contains wrong data
      if (!check_bitfield_piece(torrent, piece_index)) {
        cout << "Data corrupted, deleting all the block" << endl;
        source.seekp(request.index * torrent.piece_length);
        char null_string[torrent.piece_length];
        memset(null_string, '\0', blocklength);
        source.write(null_string, blocklength);
      }
    }
  }
  source.close();
  return request;
}

void get_block_from_request(string &path, Torrent &torrent, RequestMsg request, char *block) {
  ifstream source(path);
  if (source.is_open()) {
    source.seekg(request.index * torrent.piece_length + request.begin);
    source.read(block, request.length);
  }
  else {
    cout << "Error opening " << path << endl;
  }
  source.close();
}

void save_block(char* blockdata, string path, RequestMsg request, Torrent &torrent) {
  fstream dest(path);
  if (dest.is_open()) {
    cout << "Writing block,    index: " << request.index << ", begin:"
      << request.begin << ", length: " << request.length << endl;
    dest.seekp(request.index * torrent.piece_length + request.begin);
    dest.write(blockdata, request.length);
  }
  dest.close();
}

void check_file_complete(Torrent &torrent) {
  size_t found = torrent.bitfield.find_first_of('0');
  if (found == string::npos) {
    cout << torrent.name << " completed!" << endl;
    boost::filesystem::rename(torrent.name + ".part", torrent.name);
  }
}

