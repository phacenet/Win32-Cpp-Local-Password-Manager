#include "Vector.h"
#include <iostream>
#include <iomanip>

int main()
{
	Vector<int> v;

	std::cout << std::boolalpha << v.empty();

	v.print();

	v.push_back(6);

	std::cout << v.empty();

	v.print();



}

