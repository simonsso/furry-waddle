
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include <deque>
#include <list>
#include <map>
#include <vector>
#include <cstdint>

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


int network_me(Ledger *bank) {
   // Encapuslate stread to saftly tranfer it to thread
   class network_connection {
     public:
      boost::asio::ip::tcp::iostream stream;
   };

   try {
      boost::asio::io_service io_service;

      boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v6(), 9000);
      boost::asio::ip::tcp::acceptor acceptor(io_service, endpoint);

      while (1) {
         network_connection *n = new network_connection();
         boost::system::error_code ec;
         acceptor.accept(*(*n).stream.rdbuf(), &ec);
         if (ec) {
            std::cerr << "Inner error " << ec << " " << ec.message() << std::endl;
            delete n;
         } else {
            // Give n to thread in separeate object to use and delete
            std::thread t(
                [&bank](network_connection *n) -> void {
                   // check if strem is open
                   while (n->stream) {
                      std::string command;
                      n->stream >> command;

                      std::string_view request = command;
                      // if stream was closed while waiting for imput command will be empty
                      // break out of loop and free resources
                      if (command == "") {
                         // empty lines will not endup here they are filterd out in stream >> operation
                         if (n->stream.error()) {
                            // std::cout<< "Empty request:"<< n->stream.error().message() <<std::endl;
                            break;
                         }
                      }
                      // TODO This is a sytem shutdown command,
                      // Change this to do an organized shutdown
                      if (request.substr(0, 4) == "EXIT") {
                         n->stream << "Good Night ByeBye" << std::endl;
                         n->stream.close();
                         delete n;
                         exit(0);
                      }
                      // A single dot to end this connection
                      if (request[0] == '.') {
                         n->stream << "ByeBye" << std::endl;
                         break;
                      }

                      // Command = Interval date date ISIN[[;]ISIN]*
                      if (command == "sum") {
                         uint32_t startdate = 0;
                         uint32_t stopdate = 30000000;  // now + some time
                         std::string subcommand;
                         n->stream >> subcommand;
                         if (subcommand == "from") {
                            n->stream >> startdate;
                            n->stream >> subcommand;
                         }
                         if (subcommand == "to") {
                            n->stream >> stopdate;
                            n->stream >> subcommand;
                         }
                         if (subcommand.size() >= 12) {
                            auto ans = bank->sum(subcommand, startdate, stopdate);
                            n->stream << ans.to_json() << std::endl;
                         } else {
                            n->stream << "Request size too short " << subcommand.size() << std::endl;
                         }
                      } else
                          // Command - Sum ISIN[[;]ISIN]*
                          if (command.size() >= 12) {
                             // For now end of time is defined as 2020-01-01
                               auto ans = bank->sum(command , 0 , 20200101 );
                         n->stream << ans.to_json() << std::endl;
                      } else {
                         n->stream << "Request size too short " << command.size() << std::endl;
                      }
                   }
                   std::cout << "Closing connection      " << &n->stream << " : " << std::endl;
                   n->stream.close();
                   delete n;
                   return;
                },
                n);
            t.detach();
         }
      }
   } catch (std::exception &e) {
      std::cerr << "Outer error " << e.what() << std::endl;
   }
   return 0;
}

///
#define EPOC_BEGIN 0
#define EPOC_END 29990101

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
   avanza.sum("SE0010546390;SE0010546408;SE0010820613;SE0009382856;SE0000143521" ,EPOC_BEGIN, EPOC_END);

   avanza.sum("LU0050427557",EPOC_BEGIN, EPOC_END );

   avanza.sum("",EPOC_BEGIN, EPOC_END);

   avanza.april(20140101,20190101);
   t1.join();
}
