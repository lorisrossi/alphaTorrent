// Reference: https://wiki.theory.org/index.php/BitTorrentSpecification#Metainfo_File_Structure

#include <iostream>
#include <iomanip>
#include "torrentparser.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DICT_NOT_FOUND -5;
#define DEFAULT_BUFF_LEN 1024

using namespace std;

/**
 * Parse a be_node dictionary corresponding to a file of a torrent.
 * This function is called only when parsing a torrent with multiple files.
 *
 * @param  file_node  be_node dictionary corresponding to the file
 * @return            TorrentFile struct with path and length
 */
TorrentFile parse_file_dict(be_node *file_node) {
  string key;
  TorrentFile new_file;
  for (int i=0; file_node->val.d[i].val; i++) {
    key = file_node->val.d[i].key;
    if (key == "path")
      new_file.path = file_node->val.d[i].val->val.l[0]->val.s;
    else if (key == "length")
      new_file.length = file_node->val.d[i].val->val.i;
  }
  return new_file;
}

/**
 * Parse a be_node dictionary corresponding to the "info" key of the torrent.
 * Parsed keys: name, piece length, pieces, length, files.
 * Save all the information in a Torrent struct passed as a parameter.
 * In the case of a single file torrent, this function parse file info without
 * calling "parse_file_dict".
 *
 * @param info_node   be_node dictionary corresponding to "info" key
 * @param new_torrent Torrent struct where to save the parsed info
 */
void parse_info_dict(be_node *info_node, Torrent &new_torrent) {
  string key;
  for (int i=0; info_node->val.d[i].val; i++) {
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
    }
    else if (key == "length") { // single file torrent
      TorrentFile single_file;
      single_file.length = info_node->val.d[i].val->val.i;
      new_torrent.files.push_back(single_file);
    }
    else if (key == "files") {
      be_node *temp_node = info_node->val.d[i].val;
      for (int j=0; temp_node->val.l[j]; j++)
        new_torrent.files.push_back(parse_file_dict(temp_node->val.l[j]));
    }
  }
  if (new_torrent.files.size() == 1 && new_torrent.files[0].path == "") // single file torrent
    new_torrent.files[0].path = new_torrent.name;
}

/**
 * Parse a be_node dictionary corresponding to the decoded torrent file.
 * Parsed keys: announce, info.
 * Save all the information in a Torrent struct passed as a parameter.
 *
 * @param node         be_node dictionary corresponding to the torrent file
 * @param new_torrent  Torrent struct where to save the parsed info
 */
void parse_torrent(be_node *node, Torrent &new_torrent) {
  string key;
  for (int i=0; node->val.d[i].val; i++) {
    key = node->val.d[i].key;
    if (key == "announce")
      new_torrent.tracker_url = node->val.d[i].val->val.s;
    else if (key == "info")
      parse_info_dict(node->val.d[i].val, new_torrent);
  }
}


/**
 *  This function extract the bencoded dictionary from the metainfo file and hash (with SHA1)
 *  the content
 *
 *  @param *file :  a string that cointaining the entire file
 *
 *  @return the uchar array of the hash
 */
char *get_info_node_hash(string *file, string *pieces_string){
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
 * Print path and length of a file in a torrent.
 *
 * @param torrent_file  File parsed with parse_file_dict
 */
void print_file(TorrentFile torrent_file) {
  cout << "\tPath: " << torrent_file.path << endl;
  cout << "\tLength: " << fixed << setprecision(2)
    << torrent_file.length / (float)(1024*1024) << " MB" << endl << endl;
}

/**
 * Print all the parsed information of a torrent
 *
 * @param torrent  Torrent parsed with parse_torrent
 */
void print_torrent(Torrent torrent) {
  string separator = "--------------------------";
  cout << separator << endl;
  cout << "Torrent name: " << torrent.name << endl;
  cout << "Tracker: " << torrent.tracker_url << endl;
  cout << "Piece length: " << torrent.piece_length / 1024 << " KB" << endl;
  cout << "Pieces: " << torrent.pieces.size() << endl;
  cout << "Files:" << endl;
  for (TorrentFile file : torrent.files)
    print_file(file);
  cout << separator << endl;
}
