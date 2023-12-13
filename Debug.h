#ifndef BEAVER_BUILDER_DEBUG
#define BEAVER_BUILDER_DEBUG

#include <string>
#include <fstream>

class Debug
{
public:
	static void log(std::string s);
	static void logS(std::string s);
	static void terminate();
private:
	static std::ofstream file;
};

inline void Debug::log(std::string s) {
	file << s << std::endl;
}

inline void Debug::logS(std::string s) {
	file << s << " ";
}

#endif

