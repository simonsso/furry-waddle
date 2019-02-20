#ifndef LEDGER_HPP
#define LEDGER_HPP

#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

struct Transaction {
   /// Field names from data set
   // 	{
   //   0"Datum" : "2016-05-26",
   //   1"Konto" : "ISK",
   //   2"Typ av transaktion" : "Split",
   //   3"Värdepapper/beskrivning" : "FING B",
   //   4"Antal" : -67,
   //   5"Kurs" : 297.91,
   //   6"Belopp" : "-",
   //   7"Courtage" : "-",
   //   8"Valuta" : "SEK",
   //   9"ISIN" : "SE0000422107"
   //  },
   std::string isin;
   std::string curenecy;
   std::string sec_name;
   double amount;
   double brokerage;
   uint32_t date;

   // Operations
   bool operator==(const Transaction &t) const;
   bool operator<(const Transaction &t) const;
};

struct TransactionSet {
   double amount;
   double brokerage;
   int num_trans;

   // Operations
   TransactionSet();
   std::string to_json();
};



class Ledger {

   // Transactions can be added to ledger but never removed. Saved Iterators will be valid
   using LedgerDeque = std::deque<Transaction>;
   using LedgerIsinIndex = std::map<std::string, std::list<Transaction const*>>;
   using LedgerDateIndex = std::map<uint32_t, LedgerDeque::const_iterator>;

   LedgerDeque ledger;
   mutable LedgerIsinIndex isin_index;
   LedgerDateIndex date_index;

   mutable std::shared_mutex mutex;

  public:
   /// Import from csv stream
   void import_csv(std::istream &infile);
   /// Verify all asumptionions on data was correct
   bool data_integrity_self_check() const;

   TransactionSet sum(const std::string &isin, uint32_t startdate, uint32_t stopdate) const;

   /// Legacy functions
   double april(int startdate, int stopdate) const;
   /// Orignal seach code moved into this function.
   void find_something() const;
};
#endif