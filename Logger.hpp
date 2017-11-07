#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <string>
#include <iostream>

class Logger{
private:
    static std::string m_ModuleName;
    static bool isSuppressed;

    template <typename ArgT>
    static void m_Out(std::ostream& out, ArgT info_message){
        out << info_message << " ";
    }

    template <class FirstArgT, class ... OtherArgsT>
    static void m_Out(std::ostream& out, FirstArgT info_message, OtherArgsT ... other_args){
        m_Out(out, info_message);
        m_Out(out, other_args...);
    }

public:
    static void SuppressOutput(bool value){
        isSuppressed = value;
    }

    static void ModuleName(std::string module_name){
        m_ModuleName = std::string(module_name);
    }

    template <class FirstArgT, class ... OtherArgsT>
    static void Info(FirstArgT info_message, OtherArgsT ... other_args){
        if (isSuppressed == true)
            return;

        std::cout << "["<< m_ModuleName << " INFO]:";
        m_Out(std::cout, info_message, other_args...);
        std::cout << std::endl;
    }

    template <class FirstArgT, class ... OtherArgsT>
    static void Error(FirstArgT error_message, OtherArgsT ... other_args){
        std::cerr << "\x1B[31m" << "["<< m_ModuleName << " ERROR]:";
        m_Out(std::cerr, error_message, other_args...);
        std::cerr << "\033[0m" << std::endl;
    }

    template <class FirstArgT, class ... OtherArgsT>
    static void Warning(FirstArgT warning_message, OtherArgsT ... other_args){
        std::cout << "\x1B[34m" << "["<< m_ModuleName << " WARNING]:";
        m_Out(std::cout, warning_message, other_args...);
        std::cout << "\033[0m" << std::endl;
    }
};

#endif 
