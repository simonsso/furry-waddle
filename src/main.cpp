#include <iostream>


/// Create the example with dog and yellow dog.
// Stupid example but clear to show concept
class Dog{
	/// add some mebers
	int a;		//32 bit
	int64_t b;	
	int32_t d;	//32
	bool e;	bool c;


public:
	Dog(){
		std::cout << "dog construc"<<std::endl;
	}
	// Dog(Dog&& x){
	// 	(void) x;
	// 	std::cout << "dog move construc"<<std::endl;
	// }
	virtual ~Dog(){
		std::cout << "dog destruct"<<std::endl;
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
	Dog *mydog;
	{
		
		std::cout << ": Create dogs:" << std::endl;
		YellowDog buster;
		Dog fido;
		std::cout << ": bark the dogs" << std::endl;
		buster.bark();
		fido.bark();
		std::cout << ": Give a dog pointer" << std::endl;
		mydog = &buster;
		mydog->bark();
		mydog = &fido;
		mydog->bark();
		std::cout << ": Give a dog pointer" << std::endl;
		fido = buster;
		fido.bark();
		mydog->bark();

	}
	std::cout << ": Create a new yellow dog and store it in a dog ref" << std::endl;
	mydog = new YellowDog();
	mydog->bark();
	delete mydog;

	std::cout << "Hello Easy C++ project!" << std::endl;
}