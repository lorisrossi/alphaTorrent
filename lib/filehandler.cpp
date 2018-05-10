// WARNING: as of now, only single file torrents can be used.
// Multiple file torrents are not supported.

#include <boost/filesystem.hpp>
#include <boost/dynamic_bitset.hpp>
#include <iostream>
#include <string>
#include <algorithm> // min()
#include <openssl/sha.h>

#include "filehandler.hpp"

#define MAX_REQUEST_LENGTH 16384 // 2^14

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

/**
 * Initialize bitfield of a torrent, checking the pieces already downloaded.
 * 
 * @param torrent  Torrent struct
 */
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
        torrent.bitfield.set(i, (memcmp(piece_hash.c_str(), digest, 20) == 0));
      }
      // last piece
      hashlength = file_length % torrent.piece_length;
      piece_hash = torrent.pieces.substr((torrent.num_pieces - 1)*20, 20);
      cur_file.read(file_hash, hashlength);
      SHA1((unsigned char*)(file_hash), hashlength, digest);
      torrent.bitfield.set(torrent.num_pieces-1, (memcmp(piece_hash.c_str(), digest, 20) == 0));

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

/**
 * Get the index of a piece the peer can give to us
 * 
 * @param peer_bitfield 
 * @param own_bitfield 
 * @return int          Index of a needed piece, or -1 if no piece available 
 */
int compare_bitfields(boost::dynamic_bitset<> peer_bitfield, boost::dynamic_bitset<> own_bitfield) {
  assert(peer_bitfield.size() == own_bitfield.size());
  boost::dynamic_bitset<> needed_pieces = ~own_bitfield & peer_bitfield;
  size_t block_index = needed_pieces.find_first();
  if (block_index != boost::dynamic_bitset<>::npos) {
    return block_index;
  }
  else return -1;
}

// check the value of the bitfield at index "piece_index"
/**
 * Check the value of the bitfield at index "piece_index", and change it accordingly.
 * 
 * @param torrent      Torrent struct
 * @param piece_index  Index to check
 * @return true        If the piece is completed, otherwise false
 */
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
    torrent.bitfield.set(piece_index, is_complete);

    if (is_complete) {
      cout << "New piece completed! Piece index: " << piece_index << endl;
    }
  }
  source.close();
  return is_complete;
}

/**
 * Create a RequestMsg struct starting from "piece_index".
 * 
 * @param torrent    
 * @param piece_index  
 * @return RequestMsg 
 */
RequestMsg create_request(Torrent &torrent, int piece_index) {
  RequestMsg request;
  request.index = piece_index;
  fstream source(torrent.name + ".part");
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
    request.begin = str.find("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 0, 16);
    if (request.begin != string::npos) {
      request.length = min(blocklength - request.begin, (size_t)MAX_REQUEST_LENGTH);
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
  else {
    cout << "Error: can't create request for " << torrent.name + ".part" << endl;
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

/**
 * Save a blockdata received from a "piece" message.
 * WARNING: only single file torrents are supported. 
 * 
 * @param blockdata  Byte-array of data
 * @param index      Piece index 
 * @param begin      Block index (within the piece)
 * @param length     Length of blockdata
 * @param torrent    Torrent struct
 */
void save_block(char* blockdata, size_t index, size_t begin, size_t length, Torrent &torrent) {
  fstream dest(torrent.name);
  if (dest.is_open()) {
    dest.seekp(index * torrent.piece_length + begin);
    dest.write(blockdata, length);
  }
  dest.close();
}

/**
 * Check if a torrent is 100% downloaded.
 * If that is the case, remove the ".part" suffix from the filename.
 * WARNING: only single file torrents are supported.
 * 
 * @param torrent  Torrent struct
 */
void check_file_is_complete(Torrent &torrent) {
  // If the bitfield is all set, ~bitfield is all 0s,
  // and find_first() doesn't find any set bit
  size_t found = (~torrent.bitfield).find_first();
  if (found == string::npos) {
    cout << torrent.name << " completed!" << endl;
    boost::filesystem::rename(torrent.name + ".part", torrent.name);
  }
}

void save_piece(Torrent &torrent, size_t piece_index)
{
  // save piece in the file.part

  // compare_bitfields("101110", "000110");
  string source_path = "real/Raccolta.Ebook.29.11.2017-iCV.rar";
  string dest_path = "Raccolta.Ebook.29.11.2017-iCV.rar.part";
  
  RequestMsg request = create_request(torrent, piece_index);
  
  if (request.begin != string::npos) {
    char block[request.length];
    get_block_from_request(source_path, torrent, request, block);
    save_block(block, dest_path, request, torrent);
  }
  
  // if(check_bitfield_piece(torrent, request.index)) {
  //   is_(torrent);
  // }
}
