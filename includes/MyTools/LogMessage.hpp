#ifndef LOGMESSAGE_HPP
#define LOGMESSAGE_HPP

#include <strsafe.h>
#include <cstdarg>
#include <string>
#include <iostream>

void LogMessage(int level, const std::string& message, ...);

#endif