#include <iostream>
#include <iostream>
#include <string>
#include <curl/curl.h>


//
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

class EasyCurlWrp {
	CURL *curl= nullptr;
	std::string readBuffer;
public:
	EasyCurlWrp(){
		curl = curl_easy_init();
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, "http://www.example.com");
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		}
	};
	EasyCurlWrp(char const *url){
		curl = curl_easy_init();
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		}
	};
	~EasyCurlWrp(){
		curl = curl_easy_init();
		if(curl) {
			curl_easy_cleanup(curl);
			curl = nullptr;
		}
	};
	CURLcode perform(){
		if (curl) {
			return (curl_easy_perform(curl));
		}else{
			return CURLE_FAILED_INIT;
		}
	}
	std::string getBuffer(){
		return readBuffer;
	}

};



int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	EasyCurlWrp backlog("https://blockchain.info/unconfirmed-transactions?format=json");
	EasyCurlWrp githubio("http://simonsso.github.io");
	EasyCurlWrp githubioS("https://simonsso.github.io");

	if (CURLcode res = githubioS.perform() ){
			std::cout << "Hello Error: " <<res << std::endl;
	}
	if (CURLcode res = backlog.perform() ){
			std::cout << "Hello Error: " <<res << std::endl;
	}
	if (CURLcode res = githubio.perform() ){
			std::cout << "Hello Error: " <<res << std::endl;
	}

	std::cout << backlog.getBuffer().length()<< std::endl;;
	std::cout << githubio.getBuffer().length()<< std::endl;;
	std::cout << githubioS.getBuffer().length()<< std::endl;;

	std::cout << "Hello Easy C++ project!" << std::endl;
}
