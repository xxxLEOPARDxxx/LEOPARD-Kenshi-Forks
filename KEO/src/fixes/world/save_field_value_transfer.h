#pragma once
#include <string>
namespace save_field_value_transfer {
// Only for the pinned parser's owned mutable temporary, after map insertion.
bool Try(std::string* destination,std::string* source);
}
