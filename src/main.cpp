#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <list>
#include <string_view>
#include <cmath>
#include <deque>
#include <thread>

#include <sys/socket.h>
#include <chrono>

#include <boost/asio.hpp>

class transaction{
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
	double courtage;
	uint32_t date;
};
// ISO date to decimal encoded date
unsigned int parse_date(std::string_view s){
	if (s.size() != 10) {return 0;};
	std::string::size_type sz;   // alias of size_t
								//  s.data is unsafe -
	int date = std::stoi( s.data() ,&sz );
	if (sz != 4 ){
		return  0;
	}
	s.remove_prefix(5);
	date =date*100 + std::stoi( s.data() ,&sz );
	if (sz != 2 ){
		return  0;
	}
	s.remove_prefix(3);
	date =date*100 + std::stoi( s.data() ,&sz );
	if (sz != 2 ){
		return  0;
	}

	return date;
}


// Handle both decimal points and decimal comma
// regardless of locale setting
// Todo: This could be replaced with a fixed point class
// but for now double is good.
double parse_number(std::string_view s){
	bool signbit = false;
	if( s[0] == '-' ) {
		signbit = true;
		s.remove_prefix(1);
	}
	if (s.size() == 0) {return 0;};
	double decimals = 0;
	std::string::size_type sz;   // alias of size_t
								//  s.data is unsafe -
	int integrer_part = std::stoi( s.data() ,&sz );
	if (sz >=s.length() ){
		return (signbit? -(double) integrer_part : (double) integrer_part);
	}
	if ( s[sz] == ',' || s[sz] == '.'){
		s.remove_prefix(sz+1);
		if ( s.length() ){
			std::string::size_type sz;
			int d = std::stoi( s.data() , &sz );
			decimals = (double) d / std::pow(10.0,sz);
		}
	}
	decimals = integrer_part + decimals;
	return (signbit? -(double) decimals : (double) decimals);;
}

#include <mutex>  // For std::unique_lock
#include <shared_mutex>
class Ledger{
		/// Transactions can be added to ledger but never removed. Iterators will be valid
		std::deque<transaction> ledger;
	 	std::map<std::string,std::list<decltype(ledger.end())>> isin_index;
		std::map<unsigned int,decltype(ledger.end())> date_index;
		mutable std::shared_mutex mutex;

public:
	/// Import from csv stream
	void import_csv(std::istream &infile) {
		std::unique_lock lock(mutex);
        std::string line;
		auto t1 = std::chrono::high_resolution_clock::now();
		bool pushing_data = ledger.empty();
		std::list<transaction> que;
        while (getline(infile,line)) {
            const char *index = nullptr;
            index = line.c_str();
			auto index0 = index;
			std::vector<int> field_index;
			while (*index){
				if (*index++ == ';'){
					field_index.push_back(index-index0);
				}
			}

			// Check if expected numer of fields is found
			if (field_index.size() == 9 ){
				// This is the expected fields
				// 	{ 
				//   0"Datum" : "2016-05-26",
				//   0-1"Konto" : "ISK",
				//   1-2"Typ av transaktion" : "Split",
				//   2-3"Värdepapper/beskrivning" : "FING B",
				//   3-4"Antal" : -67,
				//   4-5"Kurs" : 297.91,
				//   5-6"Belopp" : "-",
				//   6-7"Courtage" : "-",
				//   7-8"Valuta" : "SEK",
				//   9"ISIN" : "SE0000422107"
				//  },
				auto isin = line.substr(field_index[8],12) ;

				transaction t;
				t.date =   parse_date(line.substr(0,field_index[0]-1)) ;
				t.amount = parse_number( line.substr(field_index[5], field_index[6] - field_index[5] -1 )) ;
				t.isin = isin;
				t.curenecy = line.substr(field_index[7], 3);
				t.sec_name = line.substr(field_index[2], field_index[3]-field_index[2]-1);
				t.courtage = parse_number(line.substr(field_index[6], field_index[7]-field_index[6]-1) );


				if (pushing_data) {
					auto iter = ledger.insert(ledger.end(), t) ;

					/// create an index with isin
					/// TOTAL execution time - two iterations
					/// =====================================
					/// With create of index: Reading data took 0.00309825
					/// Without               Reading data took 0.00277055
					///                                         0.0003277
					/// Using index:
					/// Gathering data took 1.0169e-05
					/// fetching data took 0.000367927

					/// Conclusion:: the cost of creating the index is payed back the first time it is used !

					/// Todo: check if data is present and
					isin_index[isin].push_front( iter);

					// Save iterator first date in index.
					// do nothing if already exist
					date_index.emplace(t.date,iter);
				} else {
					// Todo overload equal operator..
					if (t.isin == ledger[0].isin && t.date == ledger[0].date && t.amount == ledger[0].amount  ) {
						for ( auto item = que.rbegin(); item != que.rend() ; ++item ){
							ledger.push_front(*item);
							// isin iterator = ledger.begin();
						}
					}
					que.push_front(t);
				}
			}
        }
		auto t2 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
		std::cout<<"Reading data took "<< time_span.count()<<std::endl<<std::endl <<std::endl <<std::endl;
		return ;
	}
	transaction sum (const std::vector<std::string>& isin ){
		std::shared_lock lock(mutex);
		double courtage = 0;
		double amount = 0;
		transaction t;
		auto t1 = std::chrono::high_resolution_clock::now();

		for (auto s : isin ){
			for(auto j : isin_index[s] ){
				amount += j->amount;
				courtage += j->courtage;
			}
		};
		auto t2 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
		std::cout<<"Gathering data took "<< time_span.count()<<std::endl;
		std::cout << "Transgalactic Sum: "<< amount <<std::endl ;

		t.amount = amount;
		t.courtage = courtage;
		return t;
	}


	transaction sum_string (const std::string & isin ){
		double courtage = 0;
		double amount = 0;
		transaction t;
		std::shared_lock lock(mutex);
		auto t1 = std::chrono::high_resolution_clock::now();
		auto s = isin.substr(0, 12);

		for(auto j : isin_index[s] ){
			amount += j->amount;
			courtage += j->courtage;
		}

		auto t2 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
		std::cout<<"Gathering data took "<< time_span.count()<<std::endl;
		std::cout << "Transgalactic Sum: "<< amount <<std::endl ;

		t.amount = amount;
		t.courtage = courtage;
		return t;
	}
	double april(int startdate,int stopdate) {
		std::shared_lock lock(mutex);
		auto t1 = std::chrono::high_resolution_clock::now();

		decltype(date_index.begin()) b = date_index.lower_bound(startdate);
		decltype(date_index.begin()) e = date_index.upper_bound(stopdate);

		double sum = 0;
		if (b != date_index.end() ){
			for( decltype(ledger.begin()) i = b->second ;i !=e->second ; --i){
				sum += i->courtage;
			}
		}
		auto t2 = std::chrono::high_resolution_clock::now();
		std::cout<< sum <<" Courtage payed in April" <<std::endl;
		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
		std::cout<<"courtage calc took "<< time_span.count()<<std::endl;
	return sum;
	}

	/// Orignal seach code moved into this function.
	void find_something(){
		std::shared_lock lock(mutex);
		//  Calculate total sum for all
		for (auto i: isin_index) {
			double sum = 0;
			auto t1 = std::chrono::high_resolution_clock::now();
			// i.second is a list of all tranactions with one security in same order
			// as found on ledger - which was sorted.
			for(auto j: i.second) {
				sum += j->amount;
			}
			auto t2 = std::chrono::high_resolution_clock::now();

			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
			std::cout << i.first<<" Sum: "<<sum <<"    Gathering data took "<< time_span.count()<<std::endl;
		}		
		{
			// Inside Asia --> Inside Australia: SE0004751337 SE0004113926
			double sum = 0;
			for (auto s : {"SE0004751337","SE0004113926"}){
				for(auto j : isin_index[s] ){
					sum += j->amount;
				}
			};
			std::cout << "Insite Asia/Australia Sum: "<<sum <<std::endl ;
		}
		// fpc: SE0000422107 SE0008374250
		double sum = 0;
		for (auto s : {"SE0000422107","SE0008374250"}){
			for(auto j : isin_index[s] ){
				sum += j->amount;
			}
		};
		std::cout << "FPC Sum: "<<sum <<std::endl ;
		// adv: SE0009888803 SE0012116267 SE0006219473

		sum = 0;
		for (auto s : {"SE0009888803","SE0012116267","SE0006219473"}){
			for(auto j : isin_index[s] ){
				sum += j->amount;
			}
		};
		std::cout << "Advenica Sum: "<<sum <<std::endl ;
    }
};

Ledger avanza;


using boost::asio::ip::tcp;

class network_connection{
public:
	tcp::iostream stream;
	int handle_request(){
	  std::string buf;
	  std::getline(stream, buf );
	  std::string_view request = buf;

	  request.remove_suffix(1);
	  if (request == "EXIT" ) {
		  exit(0);
	  }
	  auto ans = avanza.sum_string(buf);
      stream << ans.amount<< std::endl;
	  return 0;
	};
};

int network_me()
{
  try
  {
    boost::asio::io_service io_service;

    tcp::endpoint endpoint(tcp::v4(), 9000);
    tcp::acceptor acceptor(io_service, endpoint);

    for (;;)
    {
      network_connection n;
      acceptor.accept(*n.stream.rdbuf());
	  n.handle_request();



    }
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}





int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;
	std::thread t1(	network_me );

	std::ifstream infile( "/home/simson/Documents/Avanza/transaktioner_20150210_20190201.csv" );

    if (infile.is_open()) {
        std::string line;
		//read and ignore first line
		if(	! getline(infile,line) ){
			std::cout << "Error reading headers"<< std::endl;
			exit (-1);
		}
		avanza.import_csv(infile);
	    infile.close();
	}
	//avanza.find_something();
	avanza.sum( {"SE0010546390","SE0010546408","SE0010820613","SE0009382856","SE0000143521" } );

	avanza.sum( {"LU0050427557"});

	avanza.sum( {});
	t1.join();

}

