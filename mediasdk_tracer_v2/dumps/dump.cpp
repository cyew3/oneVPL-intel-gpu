#include "dump.h"

std::string StringToHexString(std::string data)
{
    std::ostringstream result;
    result << std::setw(2) << std::setfill('0') << std::hex << std::uppercase;
    std::copy(data.begin(), data.end(), std::ostream_iterator<unsigned int>(result, " "));
    return result.str();
}
