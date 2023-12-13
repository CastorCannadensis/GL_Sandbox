#include "Debug.h"

std::ofstream Debug::file("appdebug.txt");

void Debug::terminate() {
	file.close();
}