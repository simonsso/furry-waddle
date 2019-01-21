#include <iostream>
#include <iostream>
#include <string>
#include <curl/curl.h>


static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

/// Create the example with dog and yellow dog.
// Stupid example but clear to show concept
class Dog{
	/// add some mebers
	int a;		//32 bit
	int64_t b;	
	int32_t d;	//32
	bool e;	bool c;
	std::string *name;


public:
	Dog(){
		name = new std::string("");
		std::cout << "dog construc"<<std::endl;
	}
	Dog(Dog& x){
		(void) x;
		std::cout << "dog copt construc"<<std::endl;
	}

	virtual ~Dog(){
		std::cout << "dog destruct his name was "<<name<<std::endl;
		delete name;
	};
	virtual void bark(){
		std::cout << "dog barking"<<std::endl;
	}
};


class YellowDog: public Dog{
	public:
	YellowDog(){
		std::cout << "YellowDog construc"<<std::endl;
	}
	YellowDog(YellowDog&& x){
		(void)x;
		std::cout << "YellowDog MOVE construc"<<std::endl;
	}
	YellowDog(Dog&& x){
		(void)x;
		std::cout << "YellowDog MOVE2 construc"<<std::endl;
	}
	virtual ~YellowDog(){
		std::cout << "YellowDog destruct"<<std::endl;
	};
	virtual void bark() override{
		std::cout << "Yellowdog barking"<<std::endl;
	}	

};


int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	CURL *curl;
	CURLcode res;
	std::string readBuffer;

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "https://blockchain.info/unconfirmed-transactions?format=json");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		std::cout << readBuffer << std::endl;
	}

	std::cout << "Hello Easy C++ project!" << std::endl;
}