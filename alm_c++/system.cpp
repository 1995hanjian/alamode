#include <iostream>
#include <iomanip>
#include <fstream>
#include "system.h"
#include "constants.h"
#include "timer.h"
#include "memory.h"
#include "error.h"
#include "constraint.h"
#include "fcs.h"
#include "symmetry.h"
#include "fitting.h"

using namespace ALM_NS;

System::System(ALM *alm): Pointers(alm) {}

System::~System() {}

void System::init(){

    int i, j;

    recips(lavec, rlavec);

    std::cout.setf(std::ios::scientific);

    std::cout << "Lattice Vector" << std::endl;
    for (i = 0; i < 3; ++i){
        for (j = 0; j < 3; ++j){
            std::cout <<  " " << lavec[i][j] ;
        }
        std::cout << std::endl;
    }

    std::cout << std::endl << "Reciprocal Lattice Vector" << std::endl;    
    for (i = 0; i < 3; ++i){
        for (j = 0; j < 3; ++j){
            std::cout <<  " " << rlavec[i][j] ;
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Atomic positions in fractional coordinate and atomic species" << std::endl;
    for (i = 0; i < nat; ++i) {
        std::cout << std::setw(5) << i + 1;
        std::cout << " " << xcoord[i][0];
        std::cout << " " << xcoord[i][1];
        std::cout << " " << xcoord[i][2];
        std::cout << " " << kd[i] << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Number of input data: " << ndata << std::endl;
    std::cout << "nstart = " << nstart << ", nend = " << nend << ", nskip = " << nskip << std::endl;

    std::cout.unsetf(std::ios::scientific);

    // generate cartesian coordinate

    memory->allocate(x_cartesian, nat, 3);

    for (i = 0; i < nat; ++i){
        for (j = 0; j < 3; ++j){
            x_cartesian[i][j] = xcoord[i][j];
        }
    }
    frac2cart(x_cartesian);

    timer->print_elapsed();

}

void System::recips(double vec[3][3], double inverse[3][3])
{
    double det;
    det = vec[0][0] * vec[1][1] * vec[2][2] 
    + vec[1][0] * vec[2][1] * vec[0][2] 
    + vec[2][0] * vec[0][1] * vec[1][2]
    - vec[0][0] * vec[2][1] * vec[1][2] 
    - vec[2][0] * vec[1][1] * vec[0][2]
    - vec[1][0] * vec[0][1] * vec[2][2];

    if(det < eps12) {
        error->exit("recips", "Lattice Vector is singular");
    }

    double factor = 2.0 * pi / det;

    inverse[0][0] = (vec[1][1] * vec[2][2] - vec[1][2] * vec[2][1]) * factor;
    inverse[0][1] = (vec[0][2] * vec[2][1] - vec[0][1] * vec[2][2]) * factor;
    inverse[0][2] = (vec[0][1] * vec[1][2] - vec[0][2] * vec[1][1]) * factor;

    inverse[1][0] = (vec[1][2] * vec[2][0] - vec[1][0] * vec[2][2]) * factor;
    inverse[1][1] = (vec[0][0] * vec[2][2] - vec[0][2] * vec[2][0]) * factor;
    inverse[1][2] = (vec[0][2] * vec[1][0] - vec[0][0] * vec[1][2]) * factor;

    inverse[2][0] = (vec[1][0] * vec[2][1] - vec[1][1] * vec[2][0]) * factor;
    inverse[2][1] = (vec[0][1] * vec[2][0] - vec[0][0] * vec[2][1]) * factor;
    inverse[2][2] = (vec[0][0] * vec[1][1] - vec[0][1] * vec[1][0]) * factor;
}

void System::frac2cart(double **xf)
{
    int i, j;

    double **x_tmp;
    memory->allocate(x_tmp, nat, 3);

    for (i = 0; i < nat; ++i){
        for (j = 0; j < 3; ++j){
            x_tmp[i][j] = lavec[j][0] * xf[i][0] + lavec[j][1] * xf[i][1] + lavec[j][2] * xf[i][2];
        }
    }
    for (i = 0; i < nat; ++i){
        for (j = 0; j < 3; ++j){
            xf[i][j] = x_tmp[i][j];   
        }
    }
    memory->deallocate(x_tmp);

}

void System::rotvec(double y[3], double x[3], double A[3][3]){

    int i;
    double tmp[3];

    for (i = 0; i < 3; ++i) tmp[i] = x[i];

    for (i = 0; i < 3; ++i){
        y[i] = A[i][0] * tmp[0] + A[i][1] * tmp[1] + A[i][2] * tmp[2];
    }
}

void System::load_reference_system()
{
    int i;
    std::ifstream ifs_fc2;

    ifs_fc2.open(constraint->fc2_file.c_str(), std::ios::in);
    if(!ifs_fc2) error->exit("calc_constraint_matrix", "cannot open file fc2_file");

    bool is_found_system = false;
    bool is_found_fc2 = false;

    int nparam_harmonic_ref;
    int nparam_harmonic = fcs->ndup[0].size();

    std::string str_tmp;

    while(!ifs_fc2.eof() && !is_found_system)
    {
        std::getline(ifs_fc2, str_tmp);
        if(str_tmp == "##SYSTEM INFO"){

            is_found_system = true;

            std::getline(ifs_fc2, str_tmp);
            for (i = 0; i < 3; ++i){
                ifs_fc2 >> lavec_s[i][0] >> lavec_s[i][1] >> lavec_s[i][2];
            }
            ifs_fc2.ignore();
            std::getline(ifs_fc2, str_tmp);
            ifs_fc2 >> nkd_s;
            ifs_fc2.ignore();
            for (i = 0; i < nkd_s; ++i){
                std::getline(ifs_fc2, str_tmp);
            }
            std::getline(ifs_fc2, str_tmp);
            ifs_fc2.ignore();

            ifs_fc2 >> nat_s >> symmetry->natmin_s >> symmetry->ntran_s;

            if (symmetry->natmin_s != symmetry->natmin) {
                error->exit("load_reference_system", "The number of atoms in the primitive cell is not consistent");
            }

            if (nat_s != nat) {
                std::cout << "The number of atoms in the reference system differs from input." << std::endl;
                std::cout << "Trying to map the related force constants (^o^)" << std::endl << std::endl;
            }

            memory->allocate(xcoord_s, nat_s, 3);
            memory->allocate(kd_s, nat_s);
            memory->allocate(symmetry->map_p2s_s, symmetry->natmin_s, symmetry->ntran_s);
            memory->allocate(symmetry->map_s2p_s, nat_s);

            unsigned int ikd, itran, icell;
            std::getline(ifs_fc2, str_tmp);
            std::getline(ifs_fc2, str_tmp);
            for (i = 0; i < nat_s; ++i){
                ifs_fc2 >> str_tmp >> ikd >> xcoord_s[i][0] >> xcoord_s[i][1] >> xcoord_s[i][2] >> itran >> icell;
                kd_s[i] = ikd;
                symmetry->map_p2s_s[icell - 1][itran - 1] = i;
                symmetry->map_s2p_s[i].atom_num = icell - 1;
                symmetry->map_s2p_s[i].tran_num = itran - 1;
            }
        }
    }
    if(!is_found_system) error->exit("load_reference_system", "SYSTEM INFO flag not found in the fc2_file");

    // Generate Mapping Information (big supercell -> small supercell)

    double *xtmp;
    double *xdiff;
    int **intpair_tmp;

    memory->allocate(xtmp, 3);
    memory->allocate(xdiff, 3);

    memory->allocate(map_ref, nat_s);

    int iat, jat;
    int icrd;
    bool map_found;
    double dist;

    for (iat = 0; iat < nat_s; ++iat){
        map_found = false;

        rotvec(xtmp, xcoord_s[iat], lavec_s);
        rotvec(xtmp, xtmp, rlavec);

        for (icrd = 0; icrd < 3; ++icrd) xtmp[icrd] /= 2.0 * pi;

        for (jat = 0; jat < nat; ++jat){
            for (icrd = 0; icrd < 3; ++icrd){
                xdiff[icrd] = xtmp[icrd] - xcoord[jat][icrd];
                xdiff[icrd] = std::fmod(xdiff[icrd], 1.0);
            }
            dist = xdiff[0] * xdiff[0] + xdiff[1] * xdiff[1] + xdiff[2] * xdiff[2];

            if (dist < eps12 && kd_s[iat] == kd[jat]) {
                map_ref[iat] = jat;
                map_found = true;
                break;
            }
        }
        if(!map_found) error->exit("load_reference_system", "Could not find an equivalent atom for atom ", iat + 1);
    }

    memory->deallocate(xtmp);
    memory->deallocate(xdiff);

    ifs_fc2.clear();
    ifs_fc2.seekg(0, std::ios_base::beg);

    while(!ifs_fc2.eof() && !is_found_fc2)
    {
        std::getline(ifs_fc2, str_tmp);
        if(str_tmp == "##HARMONIC FORCE CONSTANTS")
        {
            ifs_fc2 >> nparam_harmonic_ref;
            if(nparam_harmonic_ref < nparam_harmonic) {
                error->exit("load_reference_system", "Reference file doesn't contain necessary fc2. (too few)");
            } else if (nparam_harmonic_ref > nparam_harmonic){
                std::cout << "Reference file contains extra force constants." << std::endl;
                std::cout << "They will be mapped to related force constants" << std::endl << std::endl;
            }

            is_found_fc2 = true;

            memory->allocate(fitting->fc2_ref, nparam_harmonic_ref);
            memory->allocate(intpair_tmp, nparam_harmonic_ref, 2);

            for (i = 0; i < nparam_harmonic_ref; ++i){
                ifs_fc2 >> fitting->fc2_ref[i] >> intpair_tmp[i][0] >> intpair_tmp[i][1];           
            }

            for (i = 0; i < nparam_harmonic; ++i){
                constraint->const_mat[i][i] = 1.0;
                 constraint->const_rhs[i] = fitting->fc2_ref[i];
            }
            memory->deallocate(intpair_tmp);
        }
    }

    if(!is_found_fc2) error->exit("load_reference_system", "HARMONIC FORCE CONSTANTS flag not found in the fc2_file");
    ifs_fc2.close();
}