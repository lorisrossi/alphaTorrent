// Reference: https://wiki.theory.org/index.php/BitTorrentSpecification#Metainfo_File_Structure
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring> //For bzero and memset
#include <openssl/sha.h>

#include "torrentparser.hpp"

using namespace std;

namespace torr {
  
  /**
   * @brief Parse a be_node dictionary corresponding to a file of a torrent.
   * This function is called only when parsing a torrent with multiple files.
   *
   * @param  file_node  be_node dictionary corresponding to the file
   * @return            TorrentFile struct with path and length
   */
  TorrentFile parse_file_dict(const be_node *file_node) {
    string key;
    TorrentFile new_file;
    for (int i=0; file_node->val.d[i].val; ++i) {
      key = file_node->val.d[i].key;
      if (key == "path") {
        be_node *list_node = file_node->val.d[i].val;
        for (int j=0; list_node->val.l[j]; ++j)
          new_file.path.push_back(list_node->val.l[j]->val.s);
      }
      else if (key == "length")
        new_file.length = file_node->val.d[i].val->val.i;
    }
    return new_file;
  }

  /**
   * @brief Parse a be_node dictionary corresponding to the "info" key of the torrent.
   * 
   * Parsed keys: name, piece length, pieces, length, files.
   * Save all the information in a Torrent struct passed as a parameter.
   * In the case of a single file torrent, this function parse file info without
   * calling "parse_file_dict".
   *
   * @param info_node   be_node dictionary corresponding to "info" key
   * @param new_torrent Torrent struct where to save the parsed info
   */
  void parse_info_dict(const be_node *info_node, Torrent &new_torrent) {
    string key;
    for (int i=0; info_node->val.d[i].val; ++i) {
      key = info_node->val.d[i].key;
      if (key == "name")
        new_torrent.name = info_node->val.d[i].val->val.s;
      else if (key == "piece length")
        new_torrent.piece_length = info_node->val.d[i].val->val.i;
      else if (key == "pieces") {
        long long len = be_str_len(info_node->val.d[i].val);
        new_torrent.pieces.assign(info_node->val.d[i].val->val.s, len);
        // pieces length must be a multiple of 20, sequence of 20-byte SHA1 hash values
        assert(new_torrent.pieces.size() % 20 == 0); // TODO: handle error
        new_torrent.num_pieces = new_torrent.pieces.size() / 20;
        new_torrent.bitfield.resize(new_torrent.num_pieces);
      }
      else if (key == "length") { // single file torrent
        new_torrent.is_single = true;
        TorrentFile single_file;
        single_file.length = info_node->val.d[i].val->val.i;
        new_torrent.files.push_back(single_file);
      }
      else if (key == "files") {
        be_node *temp_node = info_node->val.d[i].val;
        for (int j=0; temp_node->val.l[j]; ++j)
          new_torrent.files.push_back(parse_file_dict(temp_node->val.l[j]));
      }
    }
    if (new_torrent.is_single)
      new_torrent.files[0].path.push_back(new_torrent.name);
  }

  /**
   * @brief Parse a be_node dictionary corresponding to the decoded torrent file.
   * Parsed keys: announce, announce-list, info.
   * The first element of "announce-list" is equal to "announce", but sometimes
   * there is no "announce-list" key, so we must parse "announce" everytime.
   * Save all the information in a Torrent struct passed as a parameter.
   *
   * @param node         be_node dictionary corresponding to the torrent file
   * @param new_torrent  Torrent struct where to save the parsed info
   */
  void parse_torrent_node(const be_node *node, Torrent &new_torrent) {
    string key;
    for (int i=0; node->val.d[i].val; ++i) {
      key = node->val.d[i].key;
      if (key == "announce") {
        new_torrent.trackers.push_back(node->val.d[i].val->val.s);
      }
      else if (key == "announce-list") {
        // val of "announce-list" is a list of lists of strings
        be_node *list_node = node->val.d[i].val;
        // start from j=1 because the first tracker in "announce-list"
        // is equal to the tracker in "announce"
        for (int j=1; list_node->val.l[j]; ++j) {
          be_node *str_list_node = list_node->val.l[j];
          for (int k=0; str_list_node ->val.l[k]; ++k) {
            new_torrent.trackers.push_back(str_list_node->val.l[k]->val.s);
          }
        }
      }
      else if (key == "info") {
        parse_info_dict(node->val.d[i].val, new_torrent);
      }
    }
  }


  /**
   *  @brief Extract the bencoded dictionary from the metainfo file and hash
   *  (with SHA1) the content
   *
   *  @param *file :  a string cointaining the entire file
   *
   *  @return the uchar array of the hash
   */
  char *get_info_node_hash(const string *file, const string *pieces_string) {
    string info_key;
    unsigned char digest[SHA_DIGEST_LENGTH];
    char *info_hash;


    // info_hash string begins at the "d" after "info"
    size_t start = file->find("4:infod") + 6;
    info_key = file->substr(start);

    // cut the string until "6:pieces"
    size_t end = info_key.find("6:pieces") + 8;
    info_key = info_key.substr(0, end);

    // add the remaining part <num_of_pieces>:<byte_string_of_pieces>e
    info_key = info_key + to_string(pieces_string->size()) + ':' + *pieces_string + 'e';



    info_hash=(char*)malloc(sizeof(char)* (SHA_DIGEST_LENGTH+1)); //+1 is for the \0 string terminator
    if(!info_hash){
      cerr << "Cannot allocate memory" << endl;
      return 0;
    }

    SHA1((unsigned char*)(info_key.c_str()), info_key.length(), digest);

    bzero(info_hash, sizeof(char)* (SHA_DIGEST_LENGTH+ 1)); //Need to change just the last byte to 0
    memcpy(info_hash, digest, SHA_DIGEST_LENGTH);

    return info_hash;

    // TODO: handle errors caused by file->find()
    // }else{
    //   cerr << "Malformed torrent file : no info dictionaries" << endl;
    //   return 0;
    // }
  }

  /**
   * @brief Print path and length of a file in a torrent.
   *
   * @param torrent_file  File parsed with parse_file_dict
   */
  void print_file(const TorrentFile &torrent_file) {
    cout << "\tPath: ";
    for (size_t i=0; i < torrent_file.path.size(); ++i) {
      cout << torrent_file.path[i];
      if (i != torrent_file.path.size() - 1) {
        cout << '/';
      }
    }
    cout << endl;
    cout << "\tLength: " << fixed << setprecision(2)
      << torrent_file.length / (1024*1024*1.0) << " MB" << endl << endl;
  }

  /**
   * @brief Print all the parsed information of a torrent
   *
   * @param torrent  Torrent parsed with parse_torrent
   */
  void print_torrent(const Torrent &torrent) {
    const size_t TRACKERS_LIMIT = 10;
    const size_t FILES_LIMIT = 10;

    string separator = "--------------------------";
    cout << separator << endl;
    cout << "Torrent name: " << torrent.name << endl;
    cout << "Trackers:" << endl;
    for (size_t i=0; i < torrent.trackers.size() && i < TRACKERS_LIMIT; ++i) {
      cout << '\t' << torrent.trackers[i] << endl;
    }
    if (torrent.trackers.size() > TRACKERS_LIMIT) {
      cout << "\t[..." << torrent.trackers.size() - TRACKERS_LIMIT << " more trackers]" << endl;
    }

    cout << "Piece length: " << torrent.piece_length / 1024 << " KB" << endl;
    cout << "Pieces: " << torrent.num_pieces << endl;
    cout << "Total dimension: " << fixed << setprecision(2)
      << torrent.piece_length * torrent.num_pieces / (1024*1024*1.0)
      << " MB" << endl;

    cout << "Single file: " << (torrent.is_single ? "Yes" : "No") << endl;
    cout << "Files:" << endl;
    for (size_t i=0; i < torrent.files.size() && i < FILES_LIMIT; ++i) {
      print_file(torrent.files[i]);
    }
    if (torrent.files.size() > FILES_LIMIT) {
      cout << "\t[..." << torrent.files.size() - FILES_LIMIT << " more files]" << endl;
    }

    cout << separator << endl;
  }

  /**
   * @brief Main function for parsing a .torrent file.
   * 
   * @param torrent     Torrent struct
   * @param torrent_str String for storing the torrent file
   * @param filename    Path of the .torrent file
   * @return int        Return negative number in case of error, otherwise 0
   */
  int parse_torrent(Torrent &torrent, string &torrent_str, const char *filename) {
    ifstream myfile(filename);
    if (!myfile.is_open()){
      cout << "Error opening " << filename << endl;
      return -1;
    }

    // parse .torrent file as a string
    string line;
    while (getline(myfile, line)) {
      torrent_str += line + '\n';
    }
    myfile.close();

    // decoding
    be_node *node;
    long long len = torrent_str.length();
    node = be_decoden(torrent_str.c_str(), len);
    if (!node) {
      cout << "Parsing of " << filename << " failed!" << endl;
      return -2;
    }

    // Start processing the torrent file
    parse_torrent_node(node, torrent);
    be_free(node);
    print_torrent(torrent);

    return 0;
  }
}
