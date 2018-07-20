#include <iostream>
#include <iomanip>
#include <random>

#include <net/SessionIO.hpp>

int main(int argc, char* argv[])
{
    std::ios_base::sync_with_stdio(false);

    SessionIO obj;

	obj.Run();

	return 0;
}
