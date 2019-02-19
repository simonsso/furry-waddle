#include <cstdint>
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
   size_t sz;
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
   size_t sz;

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
}

TransactionSet::TransactionSet() {
   brokerage = 0;
   amount = 0;
   num_trans = 0;
}

std::string TransactionSet::to_json() {
   boost::property_tree::ptree pt;
   pt.put("isin.amount", amount);
   pt.put("isin.brokerage", brokerage);
   pt.put("isin.num", num_trans);

   std::ostringstream buf;
   boost::property_tree::write_json(buf, pt, false);
   return buf.str();
};


class Transactionless {
  public:
   bool operator()(const Transaction *const a, const Transaction *const b) const { return a->date < b->date; }
   bool operator()(const Transaction *const a, uint32_t b) const { return a->date < b; }
   bool operator()(const uint32_t a, const Transaction *const b) const { return a < b->date; }
};

/// Tranasaction Implementation
bool Transaction::operator==(const Transaction &t) const {
   return (t.brokerage == brokerage && t.isin == isin && t.date == date && t.amount == amount && t.curenecy == curenecy &&
           t.sec_name == sec_name);
}
bool Transaction::operator<(const Transaction &t) const { return date < t.date; }



void Ledger::import_csv(std::istream &infile) {
   std::unique_lock lock(mutex);
   std::string line;
   const auto t1 = std::chrono::high_resolution_clock::now();
   bool pushing_data = ledger.empty();
   std::list<Transaction> que;
   while (getline(infile, line)) {
      const char *index = nullptr;
      index = line.c_str();
      const auto index0 = index;
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

         const auto isin = line.substr(field_index[8], 12);

         Transaction t;
         t.date = parse_date(line.substr(0, field_index[0] - 1));
         t.amount = parse_number(line.substr(field_index[5], field_index[6] - field_index[5] - 1));
         t.isin = isin;
         t.curenecy = line.substr(field_index[7], 3);
         t.sec_name = line.substr(field_index[2], field_index[3] - field_index[2] - 1);
         t.brokerage = parse_number(line.substr(field_index[6], field_index[7] - field_index[6] - 1));

         if (pushing_data) {
            const auto iter = ledger.insert(ledger.end(), t);

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
            // Knowm limitation: last Transaction could be identical if identical Transaction have been done
            // multiple times the same day.
            if (t == ledger[0]) {
               for (auto item = que.rbegin(); item != que.rend(); ++item) {
                  ledger.push_front(*item);
                  LedgerDeque::const_iterator iter = ledger.begin();
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
   const auto t2 = std::chrono::high_resolution_clock::now();

   std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
   std::cout << "Reading data took " << time_span.count() << std::endl << std::endl << std::endl << std::endl;
   return;
}

/// Verify all asumptionions on data was correct
bool Ledger::data_integrity_self_check() {
   std::unique_lock lock(mutex);
   bool status = true;
   std::cout << "Ledger length is: " << ledger.size() << std::endl;

   {
      const auto t1 = std::chrono::high_resolution_clock::now();
      bool was_sorted = std::is_sorted(ledger.begin(), ledger.end(), [](Transaction &t1, Transaction &t2) { return t1.date > t2.date; });
      const auto t2 = std::chrono::high_resolution_clock::now();

      std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << "Ledger is:" << (was_sorted ? "sorted" : "unsorted") << "  " << time_span.count() << std::endl;
      status = status && was_sorted;
   }
   for (const auto i : isin_index) {
      const auto t1 = std::chrono::high_resolution_clock::now();
      // i.second is a list of all tranactions with one security in same order
      // as found on ledger - which was sorted.
      int count = 0;
      bool was_sorted = std::is_sorted(i.second.begin(), i.second.end(), [&count](const Transaction *t1, const Transaction *t2) {
         ++count;
         return t1->date < t2->date;
      });

      const auto t2 = std::chrono::high_resolution_clock::now();
      status = status && was_sorted;
      std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << i.first << " is " << (was_sorted ? "sorted" : "unsorted") << " x " << count << "  " << time_span.count() << std::endl;
   }
   // Regenerate date index
   LedgerDateIndex new_index;
   {
      const auto t1 = std::chrono::high_resolution_clock::now();
      for (LedgerDeque::const_iterator item = ledger.cbegin(); item != ledger.cend(); item++) {
         new_index.emplace(item->date, item);
      }
      const auto t2 = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << "Recreate date index " << time_span.count() << std::endl;
   }
   {
      const auto t1 = std::chrono::high_resolution_clock::now();
      // check
      auto pair = std::mismatch(new_index.begin(), new_index.end(), date_index.begin());
      status = status && (pair.first == new_index.end() && pair.second == date_index.end());
      const auto t2 = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
      std::cout << "Verify date index " << time_span.count() << std::endl;
   }
   return status;
}

TransactionSet Ledger::sum(const std::string &isin, uint32_t startdate, uint32_t stopdate) {
   TransactionSet t;

   std::shared_lock lock(mutex);
   const auto t1 = std::chrono::high_resolution_clock::now();
   const std::string::size_type limit = isin.size();

   for (std::string::size_type offset = 0; offset + 12 <= limit; offset += 12) {
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

   const auto t2 = std::chrono::high_resolution_clock::now();

   std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
   std::cout << "impl1 Gathering data took " << time_span.count() << std::endl;
   std::cout << "Transgalactic Sum: " << t.amount << std::endl;

   return t;
}
//
double Ledger::april(int startdate, int stopdate) {
   std::shared_lock lock(mutex);
   const auto t1 = std::chrono::high_resolution_clock::now();
   double sum = 0;

   const auto &begin_dateindex_iter = date_index.lower_bound(startdate);
   if (begin_dateindex_iter != date_index.end()) {
      const auto &dateindex_final_date = date_index.upper_bound(stopdate);

      const auto &ledger_final_date_iter = ( dateindex_final_date != date_index.end()) ? dateindex_final_date->second : ledger.begin()-1 ;
      const auto &ledger_fist_date_iter = begin_dateindex_iter->second;

      // Todo there is need for a unit test of the corner case.
      // dates are stored falling order.
      for (LedgerDeque::const_iterator legeriter = ledger_fist_date_iter ; legeriter != ledger_final_date_iter; --legeriter) {
         sum += legeriter->brokerage;
      }
   }
   const auto t2 = std::chrono::high_resolution_clock::now();
   std::cout << sum << " brokerage payed in April" << std::endl;
   std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
   std::cout << "brokerage calc took " << time_span.count() << std::endl;
   return sum;
}

/// Orignal seach code moved into this function.
void Ledger::find_something() {
   std::shared_lock lock(mutex);
   //  Calculate total sum for all

   for (const auto i : isin_index) {
      double sum = 0;
      const auto t1 = std::chrono::high_resolution_clock::now();
      // i.second is a list of all tranactions with one security in same order
      // as found on ledger - which was sorted.
      for (const auto j : i.second) {
         sum += j->amount;
      }
      const auto t2 = std::chrono::high_resolution_clock::now();

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
   for (const auto s : {"SE0000422107", "SE0008374250"}) {
      for (const auto j : isin_index[s]) {
         sum += j->amount;
      }
   };
   std::cout << "FPC Sum: " << sum << std::endl;
   // adv: SE0009888803 SE0012116267 SE0006219473

   sum = 0;
   for (const auto s : {"SE0009888803", "SE0012116267", "SE0006219473"}) {
      for (const auto j : isin_index[s]) {
         sum += j->amount;
      }
   };
   std::cout << "Advenica Sum: " << sum << std::endl;
}
