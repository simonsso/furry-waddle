#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <list>
#include <string_view>
#include <cmath>
struct transaction{
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
	std::string date;
	std::string isim;
	double amount;
	std::string curenecy;

};
// ISO date to decimal encoded date
int parse_date(std::string_view s){
	if (s.size() == 0) {return 0;};
	std::string::size_type sz;   // alias of size_t
								//  s.data is unsafe - 
	int date = std::stoi( s.data() ,&sz );
	


	return date;
}
// Handle both decimal points and decimal comma
// regardless of locale setting
double parse_substr(std::string_view s){
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
		if ( int num_digits = s.length() ){
			int d = std::stoi( s.data() );	
			decimals = (double) d / std::pow(10.0,num_digits);
		}
	}
	decimals = integrer_part + decimals;
	return (signbit? -(double) decimals : (double) decimals);;
}
int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;


    // std::string filename;
    // if (argc == 2){
    //     filename = argv[1];
    // }else{
    //     std::cout<<"Usage: " << argv[0] << "filename" <<std::endl;
    //     exit (-1);
    // }  std::ifstream file(filename);
	std::ifstream infile( "/home/simson/Documents/Avanza/transaktioner_20150210_20190126.csv" );
  
    if (infile.is_open()) {
        std::string line;
		//read and ignore first line
		if(	! getline(infile,line) ){
			std::cout << "Error reading headers"<< std::endl;
			exit (-1);
		}
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
				std::cout<< "Hello: "
				<< line.substr(0,field_index[0])  // date
				<< line.substr(field_index[8],12)              // isin
				<< parse_substr( line.substr(field_index[5], field_index[6] - field_index[5] -1 ))  // amount
				<<std::endl;
			}
        }
        infile.close();
    }
}
