#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include <deque>
#include <list>
#include <map>

#include <chrono>
#include <utility>

#include <mutex>  // For std::unique_lock
#include <shared_mutex>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "ledger.hpp"




// ISO date to decimal encoded date
unsigned int parse_date(std::string_view s) {
   if (s.size() != 10) {
      return 0;
   };
   std::string::size_type sz;  // alias of size_t
                               //  s.data is unsafe -
   int date = std::stoi(s.data(), &sz);
   if (sz != 4) {
      return 0;
   }
   s.remove_prefix(5);
   date = date * 100 + std::stoi(s.data(), &sz);
   if (sz != 2) {
      return 0;
   }
   s.remove_prefix(3);
   date = date * 100 + std::stoi(s.data(), &sz);
   if (sz != 2) {
      return 0;
   }

   return date;
}

// Handle both decimal points and decimal comma
// regardless of locale setting
// Todo: This could be replaced with a fixed point class
// but for now double is good.
double parse_number(std::string_view s) {
   bool signbit = false;
   if (s[0] == '-') {
      signbit = true;
      s.remove_prefix(1);
   }
   if (s.size() == 0) {
      return 0;
   };
   double decimals = 0;
   std::string::size_type sz;  // alias of size_t
                               //  s.data is unsafe -
   int integrer_part = std::stoi(s.data(), &sz);
   if (sz >= s.length()) {
      return (signbit ? -(double)integrer_part : (double)integrer_part);
   }
   if (s[sz] == ',' || s[sz] == '.') {
      s.remove_prefix(sz + 1);
      if (s.length()) {
         std::string::size_type sz;
         int d = std::stoi(s.data(), &sz);
         decimals = (double)d / std::pow(10.0, sz);
      }
   }
   decimals = integrer_part + decimals;
   return (signbit ? -(double)decimals : (double)decimals);
   ;
}

class transactionless {
  public:
   bool operator()(transaction *a, transaction *b) const { return a->date < b->date; }
   bool operator()(transaction *a, uint32_t b) const { return a->date < b; }
   bool operator()(uint32_t a, transaction *b) const { return a < b->date; }
};
   void Ledger::import_csv(std::istream &infile) {
      std::unique_lock lock(mutex);
    std::string line;
      auto t1 = std::chrono::high_resolution_clock::now();
      bool pushing_data = ledger.empty();
      std::list<transaction> que;
      while (getline(infile, line)) {
         const char *index = nullptr;
         index = line.c_str();
         auto index0 = index;
         std::vector<int> field_index;
         while (*index) {
            if (*index++ == ';') {
               field_index.push_back(index - index0);
            }
         }

         // Check if expected numer of fields is found
         if (field_index.size() == 9) {
            // Example csv format. Expect first line to have been shaved off by caller:
            // Datum;Konto;Typ av transaktion;Värdepapper/beskrivning;Antal;Kurs;Belopp;Courtage;Valuta;ISIN
            // 2019-02-01;Depå;Insättning;Insättning;-;-;1000,00;-;SEK;-
            // 2019-01-29;ISK;Köp;Handelsbanken B;20;105,00;-2105;5,00;SEK;SE0007100607

            auto isin = line.substr(field_index[8], 12);

            transaction t;
            t.date = parse_date(line.substr(0, field_index[0] - 1));
            t.amount = parse_number(line.substr(field_index[5], field_index[6] - field_index[5] - 1));
            t.isin = isin;
            t.curenecy = line.substr(field_index[7], 3);
            t.sec_name = line.substr(field_index[2], field_index[3] - field_index[2] - 1);
            t.brokerage = parse_number(line.substr(field_index[6], field_index[7] - field_index[6] - 1));

            if (pushing_data) {
               auto iter = ledger.insert(ledger.end(), t);

               /// create an index with isin
               /// TOTAL execution time - two iterations
               /// =====================================
               /// With create of index: Reading data took 0.00309825
               /// Without               Reading data took 0.00277055
               ///                                         0.0003277
               /// Using index:
               /// Gathering data took 1.0169e-05
               /// fetching data without index took 0.000367927

               /// Conclusion:: the cost of creating the index is payed back the first time it is used !

               isin_index[isin].push_front(&*iter);

               // Save iterator first date in index.
               // do nothing if already exist
               date_index.emplace(t.date, iter);
            } else {
               // Knowm limitation: last transaction could be identical if identical transaction have been done
               // multiple times the same day.
               if (t == ledger[0]) {
                  for (auto item = que.rbegin(); item != que.rend(); ++item) {
                     ledger.push_front(*item);
                     auto iter = ledger.begin();
                     // we are now rebuilding the data in reverse order;
                     isin_index[item->isin].push_back(&*iter);

                     // overwrite index to save the first date - which will be called last
                     date_index[t.date] = iter;
                  }
                  break;
               }
               que.push_back(t);
            }
         }
      }
      auto t2 = std::chrono::high_resolution_clock::now();

      std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << "Reading data took " << time_span.count() << std::endl << std::endl << std::endl << std::endl;
      return;
   }

   /// Verify all asumptionions on data was correct
   bool Ledger::data_integrity_self_check() {
      std::unique_lock lock(mutex);
      bool status = true;
      {
         auto t1 = std::chrono::high_resolution_clock::now();
         bool was_sorted = std::is_sorted(ledger.begin(), ledger.end(), [](transaction &t1, transaction &t2) { return t1.date > t2.date; });
         auto t2 = std::chrono::high_resolution_clock::now();

         std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
         std::cout << "Ledger is:" << (was_sorted ? "sorted" : "unsorted") << "  " << time_span.count() << std::endl;
         status = status & was_sorted;
      }
      for (auto i : isin_index) {
         auto t1 = std::chrono::high_resolution_clock::now();
         // i.second is a list of all tranactions with one security in same order
         // as found on ledger - which was sorted.
         int count = 0;
         bool was_sorted = std::is_sorted(i.second.begin(), i.second.end(), [&count](transaction *&t1, transaction *&t2) {
            ++count;
            return t1->date < t2->date;
         });

         auto t2 = std::chrono::high_resolution_clock::now();
         status = status & was_sorted;
         std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
         std::cout << i.first << " is " << (was_sorted ? "sorted" : "unsorted") << " x " << count << "  " << time_span.count() << std::endl;
      }
      // Regenerate date index
      decltype(date_index) new_index;
      {
         auto t1 = std::chrono::high_resolution_clock::now();
         for (auto item = ledger.begin(); item != ledger.end(); item++) {
            new_index.emplace((*item).date, item);
         }
         auto t2 = std::chrono::high_resolution_clock::now();
         std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
         std::cout << "Recreate date index " << time_span.count() << std::endl;
      }
      {
         auto t1 = std::chrono::high_resolution_clock::now();
         // check
         auto pair = std::mismatch(new_index.begin(), new_index.end(), date_index.begin());
         status = status && (pair.first == new_index.end() && pair.second == date_index.end());
         auto t2 = std::chrono::high_resolution_clock::now();
         std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
         std::cout << "Verify date index " << time_span.count() << std::endl;
      }
      return status;
   }

   transaction_set Ledger::sum(const std::string &isin, uint32_t startdate , uint32_t stopdate ) {
      transaction_set t;
      t.brokerage = 0;
      t.amount = 0;
      t.num_trans = 0;

      std::shared_lock lock(mutex);
      auto t1 = std::chrono::high_resolution_clock::now();

      int limit = isin.size();
      for (int offset = 0; offset + 12 <= limit; offset += 12) {
         auto s = isin.substr(offset, 12);
         for (auto j : isin_index[s]) {
            if (j->date >= stopdate) {
               break;
            }
            if (j->date >= startdate) {
               t.amount += j->amount;
               t.brokerage += j->brokerage;
               ++t.num_trans;
            }
         }
         if (offset + 13 < limit && isin[offset + 12] == ';') {
            ++offset;
         }
      }

      auto t2 = std::chrono::high_resolution_clock::now();

      std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << "impl1 Gathering data took " << time_span.count() << std::endl;
      std::cout << "Transgalactic Sum: " << t.amount << std::endl;

      return t;
   }
   //
   double Ledger::april(int startdate, int stopdate) {
      std::shared_lock lock(mutex);
      auto t1 = std::chrono::high_resolution_clock::now();

      decltype(date_index.begin()) b = date_index.lower_bound(startdate);
      decltype(date_index.begin()) e = date_index.upper_bound(stopdate);

      double sum = 0;
      if (b != date_index.end()) {
         for (decltype(ledger.begin()) i = b->second; i != e->second; --i) {
            sum += i->brokerage;
         }
      }
      auto t2 = std::chrono::high_resolution_clock::now();
      std::cout << sum << " brokerage payed in April" << std::endl;
      std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << "brokerage calc took " << time_span.count() << std::endl;
      return sum;
   }

   /// Orignal seach code moved into this function.
   void Ledger::find_something() {
      std::shared_lock lock(mutex);
      //  Calculate total sum for all

      for (auto i : isin_index) {
         double sum = 0;
         auto t1 = std::chrono::high_resolution_clock::now();
         // i.second is a list of all tranactions with one security in same order
         // as found on ledger - which was sorted.
         for (auto j : i.second) {
            sum += j->amount;
         }
         auto t2 = std::chrono::high_resolution_clock::now();

         std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
         std::cout << i.first << " Sum: " << sum << "    Gathering data took " << time_span.count() << std::endl;
      }
      {
         // Inside Asia --> Inside Australia: SE0004751337 SE0004113926
         double sum = 0;
         for (auto s : {"SE0004751337", "SE0004113926"}) {
            for (auto j : isin_index[s]) {
               sum += j->amount;
            }
         };
         std::cout << "Insite Asia/Australia Sum: " << sum << std::endl;
      }
      // fpc: SE0000422107 SE0008374250
      double sum = 0;
      for (auto s : {"SE0000422107", "SE0008374250"}) {
         for (auto j : isin_index[s]) {
            sum += j->amount;
         }
      };
      std::cout << "FPC Sum: " << sum << std::endl;
      // adv: SE0009888803 SE0012116267 SE0006219473

      sum = 0;
      for (auto s : {"SE0009888803", "SE0012116267", "SE0006219473"}) {
         for (auto j : isin_index[s]) {
            sum += j->amount;
         }
      };
      std::cout << "Advenica Sum: " << sum << std::endl;
   }
