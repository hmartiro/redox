/**
* Test special data type templates for multi-element replies using Redox.
*/

#include <iostream>
#include "../src/redox.hpp"
#include <set>
#include <unordered_set>
#include <vector>

using namespace std;

int main(int argc, char* argv[]) {

  redox::Redox rdx; // Initialize Redox (default host/port)
  if(!rdx.start()) return 1; // Start the event loop

  rdx.del("mylist");

  rdx.command_blocking("LPUSH mylist 1 2 3 4 5 6 7 8 9 10");

  rdx.command<vector<string>>("LRANGE mylist 0 4",
    [](const string& cmd, const vector<string>& reply){
      cout << "Last 5 elements as a vector: ";
      for(const string& s : reply) cout << s << " ";
      cout << endl;
    },
    [](const string& cmd, int status) {
      cerr << "Error with LRANGE: " << status << endl;
    }
  );

  rdx.command<unordered_set<string>>("LRANGE mylist 0 4",
    [](const string& cmd, const unordered_set<string>& reply){
      cout << "Last 5 elements as an unordered set: ";
      for(const string& s : reply) cout << s << " ";
      cout << endl;
    },
    [](const string& cmd, int status) {
      cerr << "Error with LRANGE: " << status << endl;
    }
  );

  rdx.command<set<string>>("LRANGE mylist 0 4",
    [&rdx](const string& cmd, const set<string>& reply){
      cout << "Last 5 elements as a set: ";
      for(const string& s : reply) cout << s << " ";
      cout << endl;
      rdx.stop_signal();
    },
    [&rdx](const string& cmd, int status) {
      cerr << "Error with LRANGE: " << status << endl;
      rdx.stop_signal();
    }
  );

  rdx.block(); // Shut down the event loop
}
