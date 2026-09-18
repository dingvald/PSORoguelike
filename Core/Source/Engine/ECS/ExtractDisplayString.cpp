#include "Engine/ECS/ExtractDisplayString.h"

#include <cctype>

namespace psr {

std::string ExtractDisplayString(const std::string& entityId)
{
    const std::size_t last_dot = entityId.find_last_of('.');
    std::string result = last_dot == std::string::npos ? entityId : entityId.substr(last_dot + 1);

    for (char& c : result)
        if (c == '_')
            c = ' ';

    bool start_of_word = true;
    for (char& c : result)
    {
        if (start_of_word)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        start_of_word = (c == ' ');
    }

    return result;
}

} // namespace psr
