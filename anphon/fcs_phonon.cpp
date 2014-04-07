/*
fcs_phonon.cpp

Copyright (c) 2014 Terumasa Tadano

This file is distributed under the terms of the MIT license.
Please see the file 'LICENCE.txt' in the root directory 
or http://opensource.org/licenses/mit-license.php for information.
*/

#include "mpi_common.h"
#include "dynamical.h"
#include "kpoint.h"
#include "fcs_phonon.h"
#include "system.h"
#include "memory.h"
#include "error.h"
#include "phonons.h"
#include "relaxation.h"
#include "constants.h"
#include "gruneisen.h"
#include "xml_parser.h"
#include <string>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>

using namespace PHON_NS;

Fcs_phonon::Fcs_phonon(PHON *phon): Pointers(phon) {}

Fcs_phonon::~Fcs_phonon(){}

void Fcs_phonon::setup(std::string mode)
{
    unsigned int i, j, icrd, jcrd;
    unsigned int nat = system->nat;
    unsigned int natmin = system->natmin;

    if (mympi->my_rank == 0) {
        std::cout << " Force constant" << std::endl;
        std::cout << " ==============" << std::endl << std::endl;
    }

    MPI_Bcast(&relaxation->quartic_mode, 1, MPI_LOGICAL, 0, MPI_COMM_WORLD);
    MPI_Bcast(&gruneisen->print_gruneisen, 1, MPI_LOGICAL, 0, MPI_COMM_WORLD);

    if (mode == "PHONONS") {
        require_cubic = false;
        require_quartic = false;
        maxorder = 1;

        if (gruneisen->print_gruneisen) {
            require_cubic = true;
            maxorder = 2;
        }
        if (gruneisen->print_newfcs) {
            require_cubic = true;
            maxorder = 2;

            if (relaxation->quartic_mode) {
                require_quartic = true;
                maxorder = 3;
            }
        }

    } else if (mode == "RTA") {
        require_cubic = true;

        if (relaxation->quartic_mode) {
            maxorder = 3;
            require_quartic = true;
        } else {
            maxorder = 2;
            require_quartic = false;
        }
    }

    if (mympi->my_rank == 0) load_fc2_xml();
    is_fc2_ext = true;

    MPI_Bcast(&is_fc2_ext, 1, MPI_LOGICAL, 0, MPI_COMM_WORLD);
    MPI_Bcast_fc2_ext();

    memory->allocate(force_constant, maxorder);

    if (mympi->my_rank == 0) {
        double *maxdev;

        load_fcs_xml();

        for (i = 0; i < maxorder; ++i){
            std::cout << "  Number of non-zero IFCs for " << i + 2 << " order: ";
            std::cout << force_constant[i].size() << std::endl;
        }
        std::cout << std::endl;

        memory->allocate(maxdev, maxorder);
        examine_translational_invariance(maxorder, system->nat, system->natmin, 
            maxdev, force_constant);

        std::cout << "  Maximum deviation from the translational invariance: " << std::endl;
        for (i = 0; i < maxorder; ++i) {
            std::cout << "   Order " << i + 2 << " : " << std::setw(12) 
                << std::scientific << maxdev[i] << std::endl;
        }
        std::cout << std::endl;
        memory->deallocate(maxdev);
    }

    MPI_Bcast_fc_class(maxorder);
}

void Fcs_phonon::load_fc2_xml()
{
    using namespace boost::property_tree;

    unsigned int atm1, atm2, xyz1, xyz2, cell_s;

    ptree pt;
    std::stringstream ss1, ss2;
    FcsClassExtent fcext_tmp;

    read_xml(file_fcs, pt);

    fc2_ext.clear();

    BOOST_FOREACH (const ptree::value_type& child_, pt.get_child("ForceConstants.HARMONIC")) {
        const ptree& child = child_.second;
        const std::string str_p1 = child.get<std::string>("<xmlattr>.pair1");
        const std::string str_p2 = child.get<std::string>("<xmlattr>.pair2");

        ss1.str("");
        ss2.str("");
        ss1.clear();
        ss2.clear();

        ss1 << str_p1;
        ss2 << str_p2;

        ss1 >> atm1 >> xyz1;
        ss2 >> atm2 >> xyz2 >> cell_s;

        fcext_tmp.atm1 = atm1 - 1;
        fcext_tmp.xyz1 = xyz1 - 1;
        fcext_tmp.atm2 = atm2 - 1;
        fcext_tmp.xyz2 = xyz2 - 1;
        fcext_tmp.cell_s = cell_s - 1;
        fcext_tmp.fcs_val = boost::lexical_cast<double>(child.data());

        fc2_ext.push_back(fcext_tmp);
    }
}

void Fcs_phonon::load_fcs_xml()
{
    using namespace boost::property_tree;
    ptree pt;
    unsigned int order;
    std::string str_tag;
    unsigned int i;
    unsigned int atmn, xyz;

    double fcs_val;

    Triplet tri_tmp;
    std::vector<unsigned int> ivec;
    std::vector<Triplet> tri_vec;

    std::stringstream ss;
    std::string str_pairs;
    std::string str_attr;

    std::cout << "  Reading force constants from the info file ... ";

    read_xml(file_fcs, pt);


    for (order = 0; order < maxorder; ++order){

        if (order == 0) {
            str_tag = "ForceConstants.HARMONIC";
        } else {
            str_tag = "ForceConstants.ANHARM" + boost::lexical_cast<std::string>(order + 2);
        }

        boost::optional< ptree& > child_ = pt.get_child_optional(str_tag);

        if (!child_) {
            std::string str_tmp = str_tag + " flag not found in the XML file";
            error->exit("load_fcs_xml", str_tmp.c_str());
        }

        BOOST_FOREACH (const ptree::value_type& child_, pt.get_child(str_tag)) {
            const ptree& child = child_.second;

            fcs_val = boost::lexical_cast<double>(child.data());
            ivec.clear();

            for (i = 0; i < order + 2; ++i) {
                str_attr = "<xmlattr>.pair" + boost::lexical_cast<std::string>(i + 1);
                str_pairs = child.get<std::string>(str_attr);

                ss.str("");
                ss.clear();
                ss << str_pairs;
                ss >> atmn >> xyz;

                if (i == 0) {
                    ivec.push_back(3 * system->map_p2s[atmn - 1][0] + xyz - 1);
                } else {
                    ivec.push_back(3 * (atmn - 1) + xyz - 1);
                }

            }

            if (std::abs(fcs_val) > eps) {
                do {
                    tri_vec.clear();

                    for (i = 0; i < order + 2; ++i){
                        tri_tmp.atom = system->map_s2p[ivec[i] / 3].atom_num;
                        tri_tmp.cell = system->map_s2p[ivec[i] / 3].tran_num;
                        tri_tmp.xyz  = ivec[i] % 3;

                        tri_vec.push_back(tri_tmp);
                    }

                    force_constant[order].push_back(FcsClass(fcs_val, tri_vec));

                } while (std::next_permutation(ivec.begin() + 1, ivec.end()));            
            }
        }
    }

    std::cout << "done !" << std::endl;
}

void Fcs_phonon::MPI_Bcast_fc_class(const unsigned int N)
{
    unsigned int i;
    int j, k;
    int len;
    int nelem;
    double *fcs_tmp;
    unsigned int ***ind;

    Triplet tri_tmp;
    std::vector<Triplet> tri_vec;

    for (i = 0; i < N; ++i) {

        len = force_constant[i].size();
        nelem = i + 2;

        MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);

        memory->allocate(fcs_tmp, len);
        memory->allocate(ind, len, nelem, 3);

        if (mympi->my_rank == 0) {
            for (j = 0; j < len; ++j){
                fcs_tmp[j] = force_constant[i][j].fcs_val;
                for (k = 0; k < nelem; ++k){
                    ind[j][k][0] = force_constant[i][j].elems[k].atom;
                    ind[j][k][1] = force_constant[i][j].elems[k].cell;
                    ind[j][k][2] = force_constant[i][j].elems[k].xyz;
                }
            }
        }

        MPI_Bcast(&fcs_tmp[0], len, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Bcast(&ind[0][0][0], 3*nelem*len, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

        if (mympi->my_rank > 0) {
            force_constant[i].clear();

            for (j = 0; j < len; ++j){

                tri_vec.clear();

                for (k = 0; k < nelem; ++k){
                    tri_tmp.atom = ind[j][k][0];
                    tri_tmp.cell = ind[j][k][1];
                    tri_tmp.xyz  = ind[j][k][2];

                    tri_vec.push_back(tri_tmp);
                }
                force_constant[i].push_back(FcsClass(fcs_tmp[j], tri_vec));
            }
        }

        memory->deallocate(fcs_tmp);
        memory->deallocate(ind);
    }
}

void Fcs_phonon::MPI_Bcast_fc2_ext()
{
    unsigned int i;
    double *fcs_tmp;
    unsigned int **ind;
    unsigned int nfcs;
    FcsClassExtent fcext_tmp;

    nfcs = fc2_ext.size();
    MPI_Bcast(&nfcs, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    memory->allocate(fcs_tmp, nfcs);
    memory->allocate(ind, nfcs, 5);

    if (mympi->my_rank == 0) {
        for (i = 0; i < nfcs; ++i) {
            fcs_tmp[i] = fc2_ext[i].fcs_val;
            ind[i][0] = fc2_ext[i].atm1;
            ind[i][1] = fc2_ext[i].xyz1;
            ind[i][2] = fc2_ext[i].atm2;
            ind[i][3] = fc2_ext[i].xyz2;
            ind[i][4] = fc2_ext[i].cell_s;
        }
    }
    MPI_Bcast(&fcs_tmp[0], nfcs, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&ind[0][0], nfcs*5, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    if (mympi->my_rank != 0) {
        for (i = 0; i < nfcs; ++i) {
            fcext_tmp.atm1 = ind[i][0];
            fcext_tmp.xyz1 = ind[i][1];
            fcext_tmp.atm2 = ind[i][2];
            fcext_tmp.xyz2 = ind[i][3];
            fcext_tmp.cell_s = ind[i][4];
            fcext_tmp.fcs_val = fcs_tmp[i];
            fc2_ext.push_back(fcext_tmp);
        }
    }
    memory->deallocate(fcs_tmp);
    memory->deallocate(ind);
}


void Fcs_phonon::examine_translational_invariance(const int n, const unsigned int nat, const unsigned int natmin, 
                                                  double *ret, std::vector<FcsClass> *fcs)
{
    int i, j, k, l, m;
    int nsize;

    double dev;
    double **sum2;
    double ***sum3;
    double ****sum4;

    for (i = 0; i < n; ++i) ret[i] = 0.0;

    for (i = 0; i < n; ++i) {

        nsize = fcs[i].size();

        if (i == 0) {
            memory->allocate(sum2, 3*natmin, 3);

            for (j = 0; j < 3 * natmin; ++j) {
                for (k = 0; k < 3; ++k) {
                    sum2[j][k] = 0.0;
                }
            }

            for (std::vector<FcsClass>::const_iterator it = fcs[i].begin(); it != fcs[i].end(); ++it) {
                sum2[3 * (*it).elems[0].atom + (*it).elems[0].xyz][(*it).elems[1].xyz] += (*it).fcs_val;
            }

            for (j = 0; j < 3 * natmin; ++j) {
                for (k = 0; k < 3; ++k) {
                    dev = std::abs(sum2[j][k]);
                    if (ret[i] < dev) ret[i] = dev;
                }
            }
            memory->deallocate(sum2);

        } else if (i == 1) {

            memory->allocate(sum3, 3*natmin, 3*nat, 3);

            for (j = 0; j < 3 * natmin; ++j) {
                for (k = 0; k < 3 * nat; ++k) {
                    for (l = 0; l < 3; ++l) {
                        sum3[j][k][l] = 0.0;
                    }
                }
            }

            for (std::vector<FcsClass>::const_iterator it = fcs[i].begin(); it != fcs[i].end(); ++it) {
                j = 3 * (*it).elems[0].atom + (*it).elems[0].xyz;
                k = 3 * (natmin * (*it).elems[1].cell + (*it).elems[1].atom) + (*it).elems[1].xyz;
                l = (*it).elems[2].xyz;
                sum3[j][k][l] += (*it).fcs_val;
            }
            for (j = 0; j < 3 * natmin; ++j) {
                for (k = 0; k < 3 * nat; ++k) {
                    for (l = 0; l < 3; ++l) {
                        dev = std::abs(sum3[j][k][l]);
                        if (ret[i] < dev) ret[i] = dev;
                    }
                }
            }

            memory->deallocate(sum3);

        } else if (i == 2) {

            memory->allocate(sum4, 3*natmin, 3*nat, 3*nat, 3);

            for (j = 0; j < 3 * natmin; ++j) {
                for (k = 0; k < 3 * nat; ++k) {
                    for (l = 0; l < 3 * nat; ++l) {
                        for (m = 0; m < 3; ++m) {
                            sum4[j][k][l][m] = 0.0;
                        }
                    }
                }
            }

            for (std::vector<FcsClass>::const_iterator it = fcs[i].begin(); it != fcs[i].end(); ++it) {
                j = 3 * (*it).elems[0].atom + (*it).elems[0].xyz;
                k = 3 * (natmin * (*it).elems[1].cell + (*it).elems[1].atom) + (*it).elems[1].xyz;
                l = 3 * (natmin * (*it).elems[2].cell + (*it).elems[2].atom) + (*it).elems[2].xyz;
                m = (*it).elems[3].xyz;
                sum4[j][k][l][m] += (*it).fcs_val;
            }

            for (j = 0; j < 3 * natmin; ++j) {
                for (k = 0; k < 3 * nat; ++k) {
                    for (l = 0; l < 3 * nat; ++l) {
                        for (m = 0; m < 3; ++m) {
                            dev = std::abs(sum4[j][k][l][m]);
                            if (ret[i] < dev) ret[i] = dev;

                        }
                    }
                }
            }

            memory->deallocate(sum4);

        }

    }

}