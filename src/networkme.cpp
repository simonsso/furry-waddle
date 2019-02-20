// #include <fstream>
#include <iostream>
#include <string>

#include <mutex>  // For std::unique_lock
#include <shared_mutex>
#include <thread>

#include <sys/socket.h>
#include <boost/asio.hpp>

#include "ledger.hpp"

int network_me(const Ledger* const bank) {
   // Encapuslate stread to saftly tranfer it to thread
   class network_connection {
     public:
      boost::asio::ip::tcp::iostream stream;
   };

   try {
      boost::asio::io_service io_service;

      boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v6(), 9000);
      boost::asio::ip::tcp::acceptor acceptor(io_service, endpoint);

      while (true) {
         network_connection* n = new network_connection();
         boost::system::error_code ec;
         acceptor.accept(*n->stream.rdbuf(), &ec);
         if (ec) {
            std::cerr << "Inner error " << ec << " " << ec.message() << std::endl;
            delete n;
         } else {
            // Give n to thread in separeate object to use and delete
            std::thread t(
               [&bank](network_connection* n) -> void {
                  // check if strem is open
                  while (n->stream) {
                     std::string command;
                     n->stream >> command;
                     // if stream was closed while waiting for imput command will be empty
                     // break out of loop and free resources
                     if (command == "") {
                        // empty lines will not endup here they are filterd out in stream >> operation
                        // but a terminated iostream could
                        if (n->stream.error()) {
                           // std::cout<< "Empty request:"<< n->stream.error().message() <<std::endl;
                           break;
                        }
                     }
                     // TODO This is a sytem shutdown command,
                     // Change this to do an organized shutdown
                     if (command == "EXIT") {
                        n->stream << "Good Night ByeBye" << std::endl;
                        n->stream.close();
                        delete n;
                        exit(0);
                     }
                     // A single dot to end this connection
                     if (command == ".") {
                        n->stream << "ByeBye" << std::endl;
                        break;
                     }

                     // handle data request commands

                     // default intervals
                     uint32_t startdate = 0;
                     uint32_t stopdate = 30000000;  // now + some distant time
                     // Command = Interval date date ISIN[[;]ISIN]*
                     if (command == "sum") {
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
                        auto ans = bank->sum(command, startdate, stopdate);
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
   } catch (std::exception& e) {
      std::cerr << "Outer error " << e.what() << std::endl;
   }
   return 0;
}
