/**
* Basic asynchronous calls using redisx.
*/

#include <iostream>
#include "../src/redisx.hpp"

using namespace std;

static const string REDIS_HOST = "localhost";
static const int REDIS_PORT = 6379;

unsigned long time_ms() {
  return chrono::system_clock::now().time_since_epoch()
      /chrono::milliseconds(1);
}

int main(int argc, char* argv[]) {

  redisx::Redis r = {REDIS_HOST, REDIS_PORT};
  r.run();

  r.command<const string&>("GET blah", [](const string& cmd, const string& value) {
    cout << "[COMMAND] " << cmd << ": " << value << endl;
  });

  r.command<const char*>("GET blah", [](const string& cmd, const char* value) {
    cout << "[COMMAND] " << cmd << ": " << value << endl;
  });

//  r.command<const redisReply*>("LPUSH yahoo 1 2 3 4 f w", [](const string& cmd, const redisReply* reply) {
//    cout << "[COMMAND] " << cmd << ": " << reply->integer << endl;
//  });


  r.get("blahqwefwqefef", [](const string& cmd, const char* value) {
    cout << "[GET] blah: " << value << endl;
  });
//
//  r.set("name", "lolfewef");
//
//  r.command("SET blah wefoijewfojiwef");
//
//  r.del("name");
//  r.del("wefoipjweojiqw", [](const string& cmd, long long int num_deleted) {
//    cout << "num deleted: " << num_deleted << endl;
//  });

//  r.command_loop("LPUSH count 1", 0, 1000);

  unsigned long t0 = time_ms();
  unsigned long t1 = t0;

  int len = 10000000;
  int count = 0;

  for(int i = 1; i <= len; i++) {
    r.command<int>("lpush test 1", [&t0, &t1, &count, len](const string& cmd, int reply) {

      count++;
      if(count == len) {
        cout << cmd << ": " << reply << endl;

        unsigned long t2 = time_ms();
        cout << "Time to queue async commands: " << t1 - t0 << "ms" << endl;
        cout << "Time to receive all: " << t2 - t1 << "ms" <<  endl;
        cout << "Total time: " << t2 - t0 << "ms" <<  endl;
      }
    });
  }
  t1 = time_ms();

  //r.start();
  while(true) {}

  return 0;
};
