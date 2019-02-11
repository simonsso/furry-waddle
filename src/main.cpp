#include <iostream>
#include <string>
#include <fstream>
#include <string_view>

#include <map>
#include <vector>
#include <list>
#include <deque>

#include <chrono>
#include <utility>
#include <cmath>

#include <thread>
#include <mutex>  // For std::unique_lock
#include <shared_mutex>

#include <sys/socket.h>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


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
	bool operator== (transaction &t){
		return (t.isin ==   isin &&
		        t.date ==   date &&
				t.amount == amount &&
				t.curenecy == curenecy &&
				t.sec_name == sec_name &&
				t.courtage== courtage );
	}
};



class transaction_set{
public:
	double amount;
	double courtage;
	int num_trans;
	std::string to_json(){
		boost::property_tree::ptree pt;
		pt.put ("isin.amount", amount);
		pt.put ("isin.courtage", courtage);
		pt.put ("isin.num", num_trans);

		std::ostringstream buf;
		boost::property_tree::write_json(buf, pt, false);
		return buf.str();
	};
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
				// Example csv format. Expect first line to have been shaved off by caller:
				// Datum;Konto;Typ av transaktion;Värdepapper/beskrivning;Antal;Kurs;Belopp;Courtage;Valuta;ISIN
				// 2019-02-01;Depå;Insättning;Insättning;-;-;1000,00;-;SEK;-
				// 2019-01-29;ISK;Köp;Handelsbanken B;20;105,00;-2105;5,00;SEK;SE0007100607

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
					// Todo overload equal operator for transaction ...
					// also last transaction could be identical if identical transaction have been done
					// multiple times the same day.
					if (t == ledger[0] ) {
						for ( auto item = que.rbegin(); item != que.rend() ; ++item ){
							ledger.push_front(*item);
							auto iter = ledger.begin();
							// we are now rebuilding the data in reverse order;
							isin_index[t.isin].push_back( iter);
							// overwrite index to save the first date - which will be called last
							date_index[t.date] = iter;

						}
					}
					que.push_back(t);
				}
			}
        }
		auto t2 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
		std::cout<<"Reading data took "<< time_span.count()<<std::endl<<std::endl <<std::endl <<std::endl;
		return ;
	}
	transaction_set sum (const std::vector<std::string>& isin ){
		std::shared_lock lock(mutex);
		transaction_set t;
		t.courtage = 0;
		t.amount = 0;
		t.num_trans = 0;

		auto t1 = std::chrono::high_resolution_clock::now();

		for (auto s : isin ){
			for(auto j : isin_index[s] ){
				t.amount += j->amount;
				t.courtage += j->courtage;
				++t.num_trans;
			}
		};
		auto t2 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
		std::cout<<"Gathering data took "<< time_span.count()<<std::endl;
		std::cout << "Transgalactic Sum: "<< t.amount << " transcations "<<t.num_trans<<std::endl ;

		return t;
	}

	/// Verify all asumptionions on data was correct
	bool data_integrity_self_check(){
		std::unique_lock lock(mutex);
		bool status = true;
		{
			auto t1 = std::chrono::high_resolution_clock::now();
			bool was_sorted = std::is_sorted(ledger.begin(),ledger.end(),[](transaction &t1, transaction &t2){ return t1.date > t2.date;});
			auto t2 = std::chrono::high_resolution_clock::now();

			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
			std::cout << "Ledger is:"<< ( was_sorted?"sorted":"unsorted" )<< "  " <<time_span.count()<<std::endl;
			status = status & was_sorted;
		}
		for(auto i: isin_index){
			auto t1 = std::chrono::high_resolution_clock::now();
			// i.second is a list of all tranactions with one security in same order
			// as found on ledger - which was sorted.
			int count = 0;
			bool was_sorted = std::is_sorted(i.second.begin(),i.second.end(),[&count](decltype(ledger.end()) &t1, decltype(ledger.end()) &t2){ ++count; return t1->date < t2->date;}  );

			auto t2 = std::chrono::high_resolution_clock::now();
			status = status & was_sorted;
			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
			std::cout << i.first<<" is "<< ( was_sorted?"sorted":"unsorted" ) <<" x "<< count << "  " <<time_span.count()<<std::endl;
		}

		return status;

	}


	transaction_set sum_string (const std::string & isin ){
		transaction_set t;
		t.courtage = 0;
		t.amount = 0;
		t.num_trans = 0;

		std::shared_lock lock(mutex);
		auto t1 = std::chrono::high_resolution_clock::now();

		int limit = isin.size();
		for(int offset = 0; offset+12 <=limit ; offset +=12 ){
			auto s = isin.substr(offset, 12 );
			for(auto j : isin_index[s] ){
				t.amount += j->amount;
				t.courtage += j->courtage;
				++t.num_trans;
			}
			if( offset+13 < limit && isin[offset+12] == ';' ){
				++offset;
			}
		}

		auto t2 = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
		std::cout<<"Gathering data took "<< time_span.count()<<std::endl;
		std::cout << "Transgalactic Sum: "<< t.amount <<std::endl ;

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

		for(auto i: isin_index){
			double sum = 0;
			auto t1 = std::chrono::high_resolution_clock::now();
			// i.second is a list of all tranactions with one security in same order
			// as found on ledger - which was sorted.
			for(auto j: i.second){
				sum += j->amount;
			}
			auto t2 = std::chrono::high_resolution_clock::now();

			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
			std::cout << i.first<<" Sum: "<<sum <<"    Gathering data took "<< time_span.count()<<std::endl;
		}
		{
			// Inside Asia --> Inside Australia: SE0004751337 SE0004113926
			double sum = 0;
			for(auto s : {"SE0004751337","SE0004113926"}) {
				for(auto j : isin_index[s] ){
					sum += j->amount;
				}
			};
			std::cout << "Insite Asia/Australia Sum: "<<sum <<std::endl ;
		}
		// fpc: SE0000422107 SE0008374250
		double sum = 0;
		for(auto s : {"SE0000422107","SE0008374250"}){
			for(auto j : isin_index[s] ){
				sum += j->amount;
			}
		};
		std::cout << "FPC Sum: "<<sum <<std::endl ;
		// adv: SE0009888803 SE0012116267 SE0006219473

		sum = 0;
		for(auto s : {"SE0009888803","SE0012116267","SE0006219473"}){
			for(auto j : isin_index[s] ){
				sum += j->amount;
			}
		};
		std::cout << "Advenica Sum: "<<sum <<std::endl ;
    }
};

Ledger avanza;

int network_me(){
	// Encapuslate stread to saftly tranfer it to thread
	class network_connection{
	public:
		boost::asio::ip::tcp::iostream stream;
	};

	try{
		boost::asio::io_service io_service;

		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v6(), 9000);
		boost::asio::ip::tcp::acceptor acceptor(io_service, endpoint);
		std::list<network_connection *> connections;
		while(1){
			network_connection *n = new network_connection();
			boost::system::error_code  ec;
			acceptor.accept(*(*n).stream.rdbuf(),&ec);
			if(ec){
				std::cerr <<"Inner error "<< ec <<" "<<ec.message()<< std::endl;
				delete n;
			}else{
				// Give n to thread in separeate object to use and delete
				std::thread t([](network_connection *n) -> void {
					// check if strem is open
					while(n->stream){
						std::string buf;
						n->stream >> buf;

						std::string_view request = buf;
						// if stream was closed while waiting for imput buf will be empty
						// break out of loop and free resources
						if(buf == "" ){
							// empty lines will not endup here they are filterd out in stream >> operation
							if (n->stream.error() ){
								// std::cout<< "Empty request:"<< n->stream.error().message() <<std::endl;
								break;
							}
						}
						// TODO This is a sytem shutdown command,
						// Change this to do an organized shutdown
						if(request.substr(0, 4) == "EXIT"){
							n->stream << "Good Night ByeBye"<<std::endl;
							n->stream.close();
							delete n;
							exit(0);
						}
						// A single dot to end this connection
						if(request[0] == '.'){
							n->stream << "ByeBye"<<std::endl;
							break;
						}

						if(buf.size() >=12){
							auto ans = avanza.sum_string(buf);
							n->stream << ans.to_json()<< std::endl;
						}else{
							n->stream << "Request size too short "<<buf.size() << std::endl;
						}
					}
					std::cout<< "Closing connection      "<< &n->stream  << " : " <<std::endl;
					n->stream.close();
					delete n;
					return;
				} ,n );
				t.detach();
			}
		}
	}
	catch (std::exception& e) {
		std::cerr << "Outer error "<< e.what() << std::endl;
	}
	return 0;
}



int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;
	std::thread t1(	network_me );
	{
		std::ifstream infile( "/home/simson/Documents/Avanza/transaktioner_20150210_20190126.csv" );

		if(infile.is_open()) {
			std::string line;
			//read and ignore first line
			if(!getline(infile,line) ){
				std::cout << "Error reading headers"<< std::endl;
				exit (-1);
			}
			avanza.import_csv(infile);
			infile.close();
		}
	}
	avanza.data_integrity_self_check();
	{
		std::ifstream infile( "/home/simson/Documents/Avanza/transaktioner_20150210_20190201.csv" );

		if(infile.is_open()) {
			std::string line;
			//read and ignore first line
			if(!getline(infile,line) ){
				std::cout << "Error reading headers"<< std::endl;
				exit (-1);
			}
			avanza.import_csv(infile);
			infile.close();
		}
	}
	//avanza.find_something();
	avanza.data_integrity_self_check();
	avanza.sum( {"SE0010546390","SE0010546408","SE0010820613","SE0009382856","SE0000143521" } );

	avanza.sum( {"LU0050427557"});

	avanza.sum( {});
	t1.join();
}

