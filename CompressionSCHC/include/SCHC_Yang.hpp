#ifndef SCHC_YANG_HPP
#define SCHC_YANG_HPP


#include <libyang/libyang.h>
#include <stdexcept>
#include <string>

void validate_json_schc(const std::string& yang_dir,
                               const std::string& json_path) ;

#endif