#pragma once

#include <string>
#include "pointers.h"

#if defined(WIN32) || defined(_WIN32)
#include <Windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

namespace ALM_NS {
    class Timer : protected Pointers {
    public:
        Timer(class ALM *);
        ~Timer();

        void reset();
        double elapsed();
        void print_elapsed();
        std::string DataAndTime();

    private:
#if defined(WIN32) || defined(_WIN32)
        LARGE_INTEGER time_ref;
        LARGE_INTEGER frequency;
#else
        timeval time_ref;
#endif
    };
}
