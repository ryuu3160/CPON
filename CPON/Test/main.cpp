#include <iostream>
#include <string>
#include "../src/cpon.hpp"

int main()
{
	cpon myCpon;

	auto &obj1 = myCpon.CreateObject("Person");
	auto block1 = obj1.CreateDataBlock();
	
	auto array1 = block1->CreateArray("Scores", std::vector<int>{85, 90, 95});
	auto obj2 = block1->CreateObject("Address");
	array1->push_back(100); // ’Ç‰Á‚ÅƒXƒRƒA‚ð’Ç‰Á
	auto block2 = obj1.CreateDataBlock();
	block2->SetValue("Name", std::string("Bob"));
	block2->SetValue("Age", 25);
	block2->CreateArray("Scores", std::vector<int>{80, 88, 92});
	
	auto obj2b = obj2->CreateDataBlock();
	obj2b->SetValue("City", std::string("New York"));
	block1->SetValue("Name", std::string("Alice"));
	block1->SetValue("Age", 30);

	myCpon.WriteToFile("output.cpon");


	myCpon.LoadFromFile("output.cpon");

	/*int ObjCount = myCpon.GetObjectCount();
	std::string ObjName = myCpon[0].GetObjectName();
	std::cout << "Object Count: " << ObjCount << std::endl;
	std::cout << "First Object Name: " << ObjName << std::endl;
	std::cout << "First Object Data Count: " << myCpon[ObjName].GetDataCount() << std::endl;
	std::cout << "First Block Name: " << myCpon[ObjName][1].GetValue<std::string>("Name") << std::endl;
	std::cout << "First Block Age: " << myCpon[ObjName][1].GetValue<int>("Age") << std::endl;
	std::vector<int> &Scores = myCpon[ObjName][1].GetArray<int>("Scores");
	std::cout << "First Block Scores: ";
	for (const auto &score : Scores)
	{
		std::cout << score << " ";
	}
	std::cout << std::endl;

	myCpon[ObjName][0].SetValue("Age", 31);
	Scores.push_back(99);*/

	myCpon.WriteToFile("output_modified.cpon");

	return 0;
}