#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include "bencode.h"

using namespace std;

typedef struct {
  string path;
  string sha1;
  long int length;
} torrent_file;

typedef struct {
  string tracker_url;
  string title;
  int piece_length;
  vector<torrent_file> files;
} torrent;

torrent_file parse_file(be_node *file_node) {
  string key;
  torrent_file new_file;
  for (int i=0; file_node->val.d[i].val; i++) {
    key = file_node->val.d[i].key;
    if (key == "path")
      new_file.path = file_node->val.d[i].val->val.l[0]->val.s;
    else if (key == "sha1")
      new_file.sha1 = file_node->val.d[i].val->val.s;
    else if (key == "length")
      new_file.length = file_node->val.d[i].val->val.i;
  }
  return new_file;
}

void parse_torrent(be_node *node, torrent &new_torrent) {
  string key;
  be_node *temp_node;
  for (int i=0; node->val.d[i].val; i++) {
    key = node->val.d[i].key;
    if (key == "announce")
      new_torrent.tracker_url = node->val.d[i].val->val.s;
    else if (key == "title")
      new_torrent.title = node->val.d[i].val->val.s;
    else if (key == "info")
      parse_torrent(node->val.d[i].val, new_torrent);
    else if (key == "piece length")
      new_torrent.piece_length = node->val.d[i].val->val.i;
    else if (key == "files") {
      temp_node = node->val.d[i].val;
      for (int k = 0; temp_node->val.l[k]; k++)
        new_torrent.files.push_back(parse_file(temp_node->val.l[k]));
    }
  }
}

void print_file(torrent_file tor_file) {
  cout << "\tPath: " << tor_file.path << endl;
  cout << "\tLength: " << tor_file.length << endl;
  cout << "\tsha1: " << tor_file.sha1 << endl << endl;
}

void print_torrent(torrent tor) {
  cout << "Torrent name: " << tor.title << endl;
  cout << "Tracker: " << tor.tracker_url << endl;
  cout << "Piece length: " << tor.piece_length << endl;
  cout << "Files:" << endl;
  for (torrent_file file : tor.files)
    print_file(file);
}

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    cout << "Usage: pass a torrent file as a parameter" << endl;
    return 1;
  }

  ifstream myfile(argv[1]);
  if (myfile.is_open()) {
    // parsing torrent file as a string
    string line;
    string torrent_str;
    while (getline(myfile, line))
      torrent_str += line + '\n';
    myfile.close();

    // decoding
    be_node *n;
    long long len = torrent_str.length();
    n = be_decoden(torrent_str.c_str(), len);
    if (n) {
      // be_dump(n); // BE_DEBUG must be greater than 0
      torrent mytorrent;
      parse_torrent(n, mytorrent);
      be_free(n);
      print_torrent(mytorrent);
		}
    else
      cout << "Parsing of " << argv[1] << " failed!" << endl;
  }
  else
    cout << "Unable to open the file." << endl;

  return 0;
}
