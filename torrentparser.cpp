#include <iostream>
#include "torrentparser.h"

using namespace std;

TorrentFile parse_file(be_node *file_node) {
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
    }
    else if (key == "length") {
      TorrentFile *single_file = new TorrentFile;
      single_file->length = info_node->val.d[i].val->val.i;
      new_torrent.files.push_back(*single_file);
    }
    else if (key == "files") {
      be_node *temp_node = info_node->val.d[i].val;
      for (int k = 0; temp_node->val.l[k]; k++)
        new_torrent.files.push_back(parse_file(temp_node->val.l[k]));
    }
  }
  if (new_torrent.files.size() == 1)
    new_torrent.files[0].path = new_torrent.name;
}

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

void print_file(TorrentFile torrent_file) {
  cout << "\tPath: " << torrent_file.path << endl;
  cout << "\tLength: " << torrent_file.length << endl << endl;
}

void print_torrent(Torrent torrent) {
  cout << "Torrent name: " << torrent.name << endl;
  cout << "Tracker: " << torrent.tracker_url << endl;
  cout << "Piece length: " << torrent.piece_length << endl;
  cout << "Pieces: " << torrent.pieces.size() << endl;
  cout << "Files:" << endl;
  for (TorrentFile file : torrent.files)
    print_file(file);
}
