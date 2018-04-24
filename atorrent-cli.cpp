#include <iostream>
#include <string>
#include <fstream>
#include "bencode.h"

using namespace std;

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    cout << "Usage: pass a file as a parameter" << endl;
    return 1;
  }
  string line;
  string total;
  ifstream myfile(argv[1]);
  if (myfile.is_open()) {
    while (getline(myfile, line))
      total += line + '\n';
    be_node *n;
    long long len = total.length();
    n = be_decoden(total.c_str(), len);
    if (n) {
			be_dump(n);
			be_free(n);
		} else
			cout << "\tparsing failed!\n";
    myfile.close();
  }
  else cout << "Unable to open the file." << endl;

  return 0;
}
