#pragma once

#include "pointers.h"
#include <string>
#include <vector>

namespace ALM_NS {
    class System: protected Pointers {
    public:
        System(class ALM *);
        ~System();
        void init();
        void recips(double [3][3], double [3][3]);
        void frac2cart(double **);
        void load_reference_system();

        int nat, nkd;
        int ndata, nstart, nend, nskip;
        int *kd;
        double lavec[3][3], rlavec[3][3];
        double **xcoord; // fractional coordinate
        double **x_cartesian;
        std::string *kdname;

        int nat_s, nkd_s;
        double lavec_s[3][3];
        int *kd_s;
        double **xcoord_s;

        unsigned int nclassatom;
        std::vector<unsigned int> *atomlist_class;

        int *map_ref;
        double cell_volume;

    private:
        unsigned int coordinate_index(const char);
        double volume(double [3], double [3], double[3]);
        void setup_atomic_class(int *);
    };
}
