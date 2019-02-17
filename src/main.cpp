
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include <deque>
#include <list>
#include <map>
#include <vector>

#include <chrono>
#include <cmath>
#include <utility>

#include <mutex>  // For std::unique_lock
#include <shared_mutex>
#include <thread>

#include <sys/socket.h>
#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "ledger.hpp"
#include "networkme.hpp"

int main(int argc, char *argv[]) {
   (void)argc;
   (void)argv;

   Ledger avanza;
   std::thread t1(network_me, &avanza);
   {
      std::ifstream infile("/home/simson/Documents/Avanza/transaktioner_20150210_20190126.csv");

      if (infile.is_open()) {
         std::string line;
         // read and ignore first line
         if (!getline(infile, line)) {
            std::cout << "Error reading headers" << std::endl;
            exit(-1);
         }
         avanza.import_csv(infile);
         infile.close();
      }
   }
   avanza.data_integrity_self_check();
   {
      std::ifstream infile("/home/simson/Documents/Avanza/transaktioner_20150210_20190201.csv");

      if (infile.is_open()) {
         std::string line;
         // read and ignore first line
         if (!getline(infile, line)) {
            std::cout << "Error reading headers" << std::endl;
            exit(-1);
         }
         avanza.import_csv(infile);
         infile.close();
      }
   }
   // avanza.find_something();
   avanza.data_integrity_self_check();
   avanza.sum("SE0010546390;SE0010546408;SE0010820613;SE0009382856;SE0000143521");

   avanza.sum("LU0050427557");

   avanza.sum("");
   t1.join();
}
