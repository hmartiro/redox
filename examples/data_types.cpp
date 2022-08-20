/**
* Test special data type templates for multi-element replies using Redox.
*/

#include <iostream>
#include <set>
#include <unordered_set>
#include <vector>
#include "redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

int main(int argc, char* argv[]) {

  redox::Redox rdx; // Initialize Redox (default host/port)
  if(!rdx.connect()) return 1; // Start the event loop

  rdx.del("mylist");

  rdx.commandSync(rdx.strToVec("LPUSH mylist 1 2 3 4 5 6 7 8 9 10"));

  rdx.command<vector<string>>({"LRANGE", "mylist", "0", "4"},
    [](Command<vector<string>>& c){
      if(!c.ok()) return;
      cout << "Last 5 elements as a vector: ";
      for (const string& s : c.reply()) cout << s << " ";
      cout << endl;
    }
  );

  rdx.command<unordered_set<string>>(rdx.strToVec("LRANGE mylist 0 4"),
    [](Command<unordered_set<string>>& c){
      if(!c.ok()) return;
      cout << "Last 5 elements as a hash: ";
      for (const string& s : c.reply()) cout << s << " ";
      cout << endl;
    }
  );

  rdx.command<set<string>>(rdx.strToVec("LRANGE mylist 0 4"),
    [&rdx](Command<set<string>>& c) {
      if(c.ok()) {
        cout << "Last 5 elements as a set: ";
        for (const string& s : c.reply()) cout << s << " ";
        cout << endl;
      }
    }
  );

  rdx.del("myset");
  rdx.commandSync(rdx.strToVec("SADD myset 2 3 5 7 11 13 17 19"));
  rdx.command<vector<int>>(rdx.strToVec("SMISMEMBER myset 1 2 3 4 5"),
    [&rdx](Command<vector<int>>& c) {
      if(c.ok()) {
        cout << "Are {1, 2, 3, 4, 5} in the set, respectively: ";
        for (int i : c.reply()) cout << i << " ";
        cout << endl;
      }
      rdx.stop();
    }
  );

  rdx.wait();
  return 0;
}
