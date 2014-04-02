/*
 system.cpp

 Copyright (c) 2014 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory 
 or http://opensource.org/licenses/mit-license.php for information.
*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include "system.h"
#include "constants.h"
#include "mathfunctions.h"
#include "timer.h"
#include "memory.h"
#include "error.h"
#include "constraint.h"
#include "fcs.h"
#include "symmetry.h"
#include "fitting.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>

using namespace ALM_NS;

System::System(ALM *alm): Pointers(alm) {}

System::~System() {}

void System::init(){

    int i, j;

    std::cout << " SYSTEM" << std::endl;
    std::cout << " ======" << std::endl << std::endl;

    recips(lavec, rlavec);

    std::cout.setf(std::ios::scientific);

    std::cout << "  Lattice Vector" << std::endl;
    std::cout << "   " << lavec[0][0] << " " << lavec[1][0] << " " << lavec[2][0] << " : a1" << std::endl;
    std::cout << "   " << lavec[0][1] << " " << lavec[1][1] << " " << lavec[2][1] << " : a2" << std::endl;
    std::cout << "   " << lavec[0][2] << " " << lavec[1][2] << " " << lavec[2][2] << " : a3" << std::endl;
    std::cout << std::endl;

    double vec_tmp[3][3];
    for (i = 0; i < 3; ++i){
        for (j = 0; j < 3; ++j){
            vec_tmp[i][j] = lavec[j][i];
        }
    }

    cell_volume = volume(vec_tmp[0], vec_tmp[1], vec_tmp[2]);
    std::cout << "  Cell volume = " << cell_volume << " (a.u)^3" << std::endl << std::endl;

    std::cout << "  Reciprocal Lattice Vector" << std::endl;
    std::cout << "   " << rlavec[0][0] << " " << rlavec[0][1] << " " << rlavec[0][2] << " : b1" << std::endl;
    std::cout << "   " << rlavec[1][0] << " " << rlavec[1][1] << " " << rlavec[1][2] << " : b2" << std::endl;
    std::cout << "   " << rlavec[2][0] << " " << rlavec[2][1] << " " << rlavec[2][2] << " : b3" << std::endl;
    std::cout << std::endl;

    std::cout << "  Atomic species:" << std::endl;
    for (i = 0; i < nkd; ++i) {
        std::cout << std::setw(6) << i + 1 << std::setw(5) << kdname[i] << std::endl;
    }
    std::cout << std::endl;

    std::cout << "  Atomic positions in fractional basis and atomic species" << std::endl;
    for (i = 0; i < nat; ++i) {
        std::cout << std::setw(6) << i + 1;
        std::cout << std::setw(15) << xcoord[i][0];
        std::cout << std::setw(15) << xcoord[i][1];
        std::cout << std::setw(15) << xcoord[i][2];
        std::cout << std::setw(5) << kd[i] << std::endl;
    }
    std::cout << std::endl << std::endl;
    std::cout.unsetf(std::ios::scientific);

    // Generate Cartesian coordinate

    memory->allocate(x_cartesian, nat, 3);

    for (i = 0; i < nat; ++i){
        for (j = 0; j < 3; ++j){
            x_cartesian[i][j] = xcoord[i][j];
        }
    }
    frac2cart(x_cartesian);
    setup_atomic_class(kd);

    timer->print_elapsed();
    std::cout << " --------------------------------------------------------------" << std::endl;
    std::cout << std::endl;
}

void System::recips(double aa[3][3], double bb[3][3])
{
    /*
    Calculate Reciprocal Lattice Vectors

    Here, BB is just the inverse matrix of AA (multiplied by factor 2 Pi)

    BB = 2 Pi AA^{-1},
    = t(b1, b2, b3)

    (b11 b12 b13)
    = (b21 b22 b23)
    (b31 b32 b33),

    b1 = t(b11, b12, b13) etc.
    */

    double det;
    det = aa[0][0] * aa[1][1] * aa[2][2] 
    + aa[1][0] * aa[2][1] * aa[0][2] 
    + aa[2][0] * aa[0][1] * aa[1][2]
    - aa[0][0] * aa[2][1] * aa[1][2] 
    - aa[2][0] * aa[1][1] * aa[0][2]
    - aa[1][0] * aa[0][1] * aa[2][2];

    if(det < eps12) {
        error->exit("recips", "Lattice Vector is singular");
    }

    double factor = 2.0 * pi / det;

    bb[0][0] = (aa[1][1] * aa[2][2] - aa[1][2] * aa[2][1]) * factor;
    bb[0][1] = (aa[0][2] * aa[2][1] - aa[0][1] * aa[2][2]) * factor;
    bb[0][2] = (aa[0][1] * aa[1][2] - aa[0][2] * aa[1][1]) * factor;

    bb[1][0] = (aa[1][2] * aa[2][0] - aa[1][0] * aa[2][2]) * factor;
    bb[1][1] = (aa[0][0] * aa[2][2] - aa[0][2] * aa[2][0]) * factor;
    bb[1][2] = (aa[0][2] * aa[1][0] - aa[0][0] * aa[1][2]) * factor;

    bb[2][0] = (aa[1][0] * aa[2][1] - aa[1][1] * aa[2][0]) * factor;
    bb[2][1] = (aa[0][1] * aa[2][0] - aa[0][0] * aa[2][1]) * factor;
    bb[2][2] = (aa[0][0] * aa[1][1] - aa[0][1] * aa[1][0]) * factor;
}

void System::frac2cart(double **xf)
{
    // x_cartesian = A x_fractional

    int i, j;

    double *x_tmp;
    memory->allocate(x_tmp, 3);

    for (i = 0; i < nat; ++i){

        rotvec(x_tmp, xf[i], lavec);

        for (j = 0; j < 3; ++j){
            xf[i][j] = x_tmp[j];
        }
    }
    memory->deallocate(x_tmp);
}


void System::load_reference_system_xml()
{
    using namespace boost::property_tree;
    ptree pt;

    int nat_ref, natmin_ref, ntran_ref;
    int **intpair_ref;
    double *fc2_ref;

    read_xml(constraint->fc2_file, pt);

    if (boost::optional<std::string> str_entry = pt.get_optional<std::string>("Structure.NumberOfAtoms")) {
        nat_ref = boost::lexical_cast<int>(str_entry.get());
    } else {
        error->exit("load_reference_system_xml", "<NumberOfAtoms> not found.");
    }

    if (boost::optional<std::string> str_entry = pt.get_optional<std::string>("Symmetry.NumberOfTranslations")) {
        ntran_ref = boost::lexical_cast<int>(str_entry.get());
    } else {
        error->exit("load_reference_system_xml", "<NumberOfTranslations> not found.");
    }

    natmin_ref = nat_ref / ntran_ref;
    if (natmin_ref != symmetry->natmin) {
        error->exit("load_reference_system_xml", "The number of atoms in the primitive cell is not consistent.");
    }

    int nfc2_ref;
    if (boost::optional<std::string> str_entry = pt.get_optional<std::string>("ForceConstants.HarmonicUnique.NFC2")) {
        nfc2_ref = boost::lexical_cast<int>(str_entry.get());
    } else {
        error->exit("load_reference_system_xml", "<NFC2> not found.");
    }

    if (nfc2_ref != fcs->ndup[0].size()) {
        error->exit("load_reference_system_xml", "The number of harmonic force constants is not the same.");
    }

    memory->allocate(intpair_ref, nfc2_ref, 2);
    memory->allocate(fc2_ref, nfc2_ref);

    int counter = 0;

    BOOST_FOREACH (const ptree::value_type& child, pt.get_child("ForceConstants.HarmonicUnique")) {
        if (child.first == "FC2") {
            const ptree& child2 = child.second;
            const std::string str_intpair = child2.get<std::string>("<xmlattr>.pairs");
            const std::string str_multiplicity = child2.get<std::string>("<xmlattr>.multiplicity");

            std::istringstream is(str_intpair);
            is >> intpair_ref[counter][0] >> intpair_ref[counter][1];
            fc2_ref[counter] = boost::lexical_cast<double>(child2.data());
            ++counter;
        }
    }

    int i;
    std::set<FcProperty> list_found;
    std::set<FcProperty>::iterator iter_found;
    int *ind;
    memory->allocate(ind, 2);

    list_found.clear();

    for (std::vector<FcProperty>::iterator p = fcs->fc_set[0].begin(); p != fcs->fc_set[0].end(); ++p){
        FcProperty list_tmp = *p; // Using copy constructor
        for (i = 0; i < 2; ++i){
            ind[i] = list_tmp.elems[i];
        }
        list_found.insert(FcProperty(2, list_tmp.coef, ind, list_tmp.mother));
    }

    for (i = 0; i < nfc2_ref; ++i){
        constraint->const_mat[i][i] = 1.0;
    }

    for (i = 0; i < nfc2_ref; ++i){
        iter_found = list_found.find(FcProperty(2, 1.0, intpair_ref[i], 1));
        if(iter_found == list_found.end()) {
            error->exit("load_reference_system", "Cannot find equivalent force constant, number: ", i + 1);
        }
        FcProperty arrtmp = *iter_found;
        constraint->const_rhs[arrtmp.mother] = fc2_ref[i];
    }

    memory->deallocate(intpair_ref);
    memory->deallocate(ind);
    memory->deallocate(fc2_ref);
    list_found.clear();
}

void System::load_reference_system()
{
    int i;
    int iat, jat;
    int icrd;

    int nat_s, nkd_s;
    double lavec_s[3][3];
    int *kd_s;
    double **xcoord_s;
    int *map_ref;

    std::ifstream ifs_fc2;

    ifs_fc2.open(constraint->fc2_file.c_str(), std::ios::in);
    if(!ifs_fc2) error->exit("calc_constraint_matrix", "cannot open file fc2_file");

    bool is_found_system = false;

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
                ifs_fc2 >> lavec_s[0][i] >> lavec_s[1][i] >> lavec_s[2][i];
            }
            ifs_fc2.ignore();
            std::getline(ifs_fc2, str_tmp);
            ifs_fc2 >> nkd_s;
            ifs_fc2.ignore();
            std::getline(ifs_fc2, str_tmp);
            std::getline(ifs_fc2, str_tmp);

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

    //
    // Generate Mapping Information (big supercell -> small supercell)
    //

    double *xtmp;
    double *xdiff;
    int **intpair_tmp;

    memory->allocate(xtmp, 3);
    memory->allocate(xdiff, 3);
    memory->allocate(map_ref, nat_s);

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

    double *fc2_ref;

    bool is_found_fc2 = false;

    while(!ifs_fc2.eof() && !is_found_fc2)
    {
        std::getline(ifs_fc2, str_tmp);
        if(str_tmp == "##HARMONIC FORCE CONSTANTS")
        {
            ifs_fc2 >> nparam_harmonic_ref;
            if(nparam_harmonic_ref < nparam_harmonic) {
                error->exit("load_reference_system", "Reference file doesn't contain necessary fc2. (too few)");
            } else if (nparam_harmonic_ref > nparam_harmonic){
                error->exit("load_reference_system","Reference file contains extra force constants." );
            }

            is_found_fc2 = true;

            memory->allocate(fc2_ref, nparam_harmonic);
            memory->allocate(intpair_tmp, nparam_harmonic, 2);

            for (i = 0; i < nparam_harmonic; ++i){
                ifs_fc2 >> fc2_ref[i] >> intpair_tmp[i][0] >> intpair_tmp[i][1];
            }

            std::set<FcProperty> list_found;
            std::set<FcProperty>::iterator iter_found;
            int *ind;
            memory->allocate(ind, 2);

            list_found.clear();
            for(std::vector<FcProperty>::iterator p = fcs->fc_set[0].begin(); p != fcs->fc_set[0].end(); ++p){
                FcProperty list_tmp = *p; // Using copy constructor
                for (i = 0; i < 2; ++i){
                    ind[i] = list_tmp.elems[i];
                }
                list_found.insert(FcProperty(2, list_tmp.coef, ind, list_tmp.mother));
            }

            for (i = 0; i < nparam_harmonic; ++i){
                constraint->const_mat[i][i] = 1.0;
            }

            for (i = 0; i < nparam_harmonic; ++i){

                iter_found = list_found.find(FcProperty(2, 1.0, intpair_tmp[i], 1));
                if(iter_found == list_found.end()) {
                    error->exit("load_reference_system", "Cannot find equivalent force constant, number: ", i + 1);
                }
                FcProperty arrtmp = *iter_found;
                constraint->const_rhs[arrtmp.mother] = fc2_ref[i];
            }

            memory->deallocate(intpair_tmp);
            memory->deallocate(ind);
            memory->deallocate(fc2_ref);
            list_found.clear();
        }
    }

    if(!is_found_fc2) error->exit("load_reference_system", "HARMONIC FORCE CONSTANTS flag not found in the fc2_file");
    ifs_fc2.close();
}

double System::volume(double vec1[3], double vec2[3], double vec3[3])
{
    double vol;

    vol = std::abs(vec1[0]*(vec2[1]*vec3[2] - vec2[2]*vec3[1]) 
        + vec1[1]*(vec2[2]*vec3[0] - vec2[0]*vec3[2]) 
        + vec1[2]*(vec2[0]*vec3[1] - vec2[1]*vec3[0]));

    return vol;
}

void System::setup_atomic_class(int *kd) {

    // This function can be modified when one needs to 
    // compute symmetry operations of spin polarized systems.

    unsigned int i;
    std::set<unsigned int> kd_uniq;
    kd_uniq.clear();

    for (i = 0; i < nat; ++i) {
        kd_uniq.insert(kd[i]);
    }
    nclassatom = kd_uniq.size();

    memory->allocate(atomlist_class, nclassatom);

    for (i = 0; i < nat; ++i) {
        int count = 0;
        for (std::set<unsigned int>::iterator it = kd_uniq.begin(); it != kd_uniq.end(); ++it)  {
            if (kd[i] == (*it)) {
                atomlist_class[count].push_back(i);
            }
            ++count;
        }
    }

    kd_uniq.clear();
}
