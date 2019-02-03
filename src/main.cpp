#include <iostream>
#include <string>
#include "json.hpp"
#include <fstream>
#include <map>
using json = nlohmann::json;


int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;


	std::ifstream infile( "/home/simson/Documents/Avanza/transaktioner_20150210_20190126.json" );

	if (infile.is_open()) {
		try
		{
			// parsing input with a syntax error
			auto js = json::parse(infile);
			std::map<std::string, double> earning;
			std::map<std::string,std::string> securities_name;
			if (js.is_array()) {
				for (auto i : (json::array_t) (js) ){
					if (i.is_array() ){}
					else if ( i.is_object() ){
						auto j = (json::object_t) i;
						//						std::cout << "ver: "<< j["ver"] << " time "<< j["time"]<< " hash "<<j["hash"] <<std::endl;
						// Find ISIN field:

						auto isin_iter = i.find("ISIN");
						if ( isin_iter != i.end() ){
							auto isin = ((std::string) *isin_iter).c_str();
							auto b = i.find("Belopp") ;
							if ( b != i.end() ){
								if ( b->is_number() ) {
									earning[isin] += (double) *b;
								}
								// This can be used to track assigned warrants
								// else{
								// 	std::cout << i <<" Something\n";
								// }
							}
							if (securities_name[isin] == "" ){
								auto b = i.find("Värdepapper/beskrivning") ;
								if ( b != i.end() ){
									if ( b->is_string() ) {
										securities_name[isin] += ((std::string) *b).c_str();;
									}
								}
							}
						}
					} else if (i.is_primitive()){

					}
				}
			}
			for (auto const [isin,sum] : earning){
				std::cout<< isin << " : "<< securities_name[isin] <<" :" << sum <<std::endl;
			}
		}
		catch (json::parse_error& e)
		{
			// output exception information
			std::cout << "message: " << e.what() << '\n'
					<< "exception id: " << e.id << '\n'
					<< "byte position of error: " << e.byte << std::endl;
		}
	}
}
