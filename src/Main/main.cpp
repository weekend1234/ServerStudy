#include <iostream>
#include <stdlib.h>
#include <thread>
#include "../LogicLib/Main.h"

int main()
{
	NLogicLib::Main main;
	main.Initialize();

	std::thread logicThread([&main]() 
	{
		main.Run();
	});
	
	std::cout << "press any key to exit...";
	getchar();

	main.Stop();

	logicThread.join();

	return 0;
}