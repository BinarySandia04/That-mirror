#pragma once

#include <iostream>

#include "termcolor.hpp"

namespace Debug
{
    template <typename T>
    void Log(T t){
        std::cout << termcolor::grey << t << termcolor::reset;
    }

    template <typename T>
    void LogError(T t){
        std::cout << termcolor::red << t << termcolor::reset;
    }

    template <typename T>
    void Print(T t){
        std::cout << termcolor::green << t << termcolor::reset;
    }
};