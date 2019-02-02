#include <iostream>
#include <string>
#include "json.hpp"
#include <fstream>
using json = nlohmann::json;

int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;


	std::ifstream infile( "/home/simson/Documents/Avanza/transaktioner_20150210_20190126.json" );

	if (infile.is_open()) {
		try
		{
			// parsing input with a syntax error
			double total_value = 0;
			auto js = json::parse(infile);
			if (js.is_array()) {
				for (auto i : (json::array_t) (js) ){
					if (i.is_array() ){}
					else if ( i.is_object() ){
						auto j = (json::object_t) i;
						//						std::cout << "ver: "<< j["ver"] << " time "<< j["time"]<< " hash "<<j["hash"] <<std::endl;
						// Find ISIN field:
						auto isin_iter = i.find("ISIN");
						if ( isin_iter != i.end() ){
							if( std::strcmp("SE0000422107",((std::string) *isin_iter).c_str()) == 0 || 0 == std::strcmp("SE0008374250", ((std::string) *isin_iter).c_str()) ) {
								auto b = i.find("Belopp") ;
								if ( b != i.end() ){
									if ( b->is_number() ) {
										total_value += (double) *b;
										std::cout<<":" << *b << ":"<<std::endl ;
									}else{
										std::cout << i <<" Something\n";
									}
								}
							}
						}
					} else if (i.is_primitive()){

					}
				}
			}
			std::cout<< "Total: "<< total_value<<std::endl;
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
