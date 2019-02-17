#ifndef LEDGER_HPP
#define LEDGER_HPP

#include <deque>
#include <list>
#include <map>
#include <mutex>


class transaction {
  public:
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
   bool operator==(transaction &t) {
      return (t.brokerage == brokerage && t.isin == isin && t.date == date && t.amount == amount && t.curenecy == curenecy &&
              t.sec_name == sec_name);
   }
   bool operator<(transaction &t) {
      //	std::cout << "DBG"<< this->date <<" < " <<t.date << std::endl;
      return this->date < t.date;
   }
};
class transaction_set {
  public:
   double amount;
   double brokerage;
   int num_trans;
   std::string to_json() {
      boost::property_tree::ptree pt;
      pt.put("isin.amount", amount);
      pt.put("isin.brokerage", brokerage);
      pt.put("isin.num", num_trans);

      std::ostringstream buf;
      boost::property_tree::write_json(buf, pt, false);
      return buf.str();
   };
};
class Ledger {
   /// Transactions can be added to ledger but never removed. Iterators will be valid
   std::deque<transaction> ledger;
   std::map<std::string, std::list<transaction *>> isin_index;

   std::map<unsigned int, decltype(ledger.end())> date_index;

   mutable std::shared_mutex mutex;
public:
   /// Import from csv stream
   void import_csv(std::istream &infile) ;
   /// Verify all asumptionions on data was correct
   bool data_integrity_self_check() ;

   transaction_set sum(const std::string &isin, uint32_t startdate = 0, uint32_t stopdate = 30000000);
   //
   double april(int startdate, int stopdate) ;
   /// Orignal seach code moved into this function.
   void find_something();
};
#endif