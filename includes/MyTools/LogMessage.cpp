#include "LogMessage.hpp"

void LogMessage(int level, const std::string& message, ...) {
    va_list args;
    va_start(args, message.c_str());

    char buffer[1024];
    
    StringCchVPrintfA(buffer, 1024, message.c_str(), args);
    va_end(args);

    switch (level) {
        case 0:
        #ifdef _DEBUG
        std::cout << "DEBUG: " << buffer << std::endl;
        #endif
        break;
        case 1:
        std::cout << "INFO: " << buffer << std::endl;
        break;
        case 2:
        std::cout << "\033[0;93mWARNING: " << buffer << "\033[0m" << std::endl;
        break;
        case 3:
        std::cout << "\033[0;91mERROR: " << buffer << "\033[0m" << std::endl;
        break;
    }
}

void LogMessage(int level, const std::wstring& message, ...) {
    va_list args;
    va_start(args, message.c_str());

    wchar_t buffer[1024];
    
    StringCchVPrintfW(buffer, 1024, message.c_str(), args);
    va_end(args);

    switch (level) {
        case 0:
        #ifdef _DEBUG
        std::wcout << L"DEBUG: " << buffer << std::endl;
        #endif
        break;
        case 1:
        std::wcout << L"INFO: " << buffer << std::endl;
        break;
        case 2:
        std::wcout << L"\033[0;93mWARNING: " << buffer << L"\033[0m" << std::endl;
        break;
        case 3:
        std::wcout << L"\033[0;91mERROR: " << buffer << L"\033[0m" << std::endl;
        break;
    }
}