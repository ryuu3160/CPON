#include <iostream>
#include <string>
#include "../src/cpon.hpp"

int main()
{
	cpon myCpon;

	//auto &obj1 = myCpon.CreateObject("Person");
	//auto &block1 = obj1.CreateDataBlock();
	//block1.SetValue("Name", std::string("Alice"));
	//block1.SetValue("Age", 30);
	//auto array1 = block1.CreateArray("Scores", std::vector<int>{85, 90, 95});
	//array1->push_back(100); // í«â¡Ç≈ÉXÉRÉAÇí«â¡
	//auto &block2 = obj1.CreateDataBlock();
	//block2.SetValue("Name", std::string("Bob"));
	//block2.SetValue("Age", 25);
	//block2.CreateArray("Scores", std::vector<int>{80, 88, 92});


	//myCpon.WriteToFile("output.cpon");


	myCpon.LoadFromFile("output.cpon");

	int ObjCount = myCpon.GetObjectCount();
	std::cout << "Object Count: " << ObjCount << std::endl;
	std::cout << "First Object Name: " << myCpon[0].GetObjectName() << std::endl;
	std::cout << "First Object Data Count: " << myCpon[0].GetDataCount() << std::endl;
	std::cout << "First Block Name: " << myCpon[0][0].GetValue<std::string>("Name") << std::endl;
	std::cout << "First Block Age: " << myCpon[0][0].GetValue<int>("Age") << std::endl;
	std::vector<int> Scores = myCpon[0][0].GetArray<int>("Scores");
	std::cout << "First Block Scores: ";
	for (const auto &score : Scores)
	{
		std::cout << score << " ";
	}
	std::cout << std::endl;

	return 0;
}