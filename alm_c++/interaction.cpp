#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <set>
#include <boost/lexical_cast.hpp>
#include "interaction.h"
#include "memory.h"
#include "system.h"
#include "error.h"
#include "symmetry.h"
#include "combination.h"
#include "listcomparison.h"
#include "files.h"
#include "timer.h"
#include <cmath>

using namespace ALM_NS;

Interaction::Interaction(ALM *alm) : Pointers(alm) {
    nsize[0] = nsize[1] = nsize[2] = 1;
}

Interaction::~Interaction() {
    memory->deallocate(xcrd);
    memory->deallocate(distlist);
    memory->deallocate(str_order);
    memory->deallocate(ninter);
    memory->deallocate(intpairs);
    memory->deallocate(relvec);
}

void Interaction::init()
{
    int i, j, k;
    int nat = system->nat;
    int nkd = system->nkd;

    memory->allocate(str_order, maxorder);
    set_ordername();

    std::cout << "*** Cutoff Radii Matrix in Bohr Unit. (nkd x nkd matrix) ***" << std::endl;

    for (i = 0; i < maxorder; ++i){
        std::cout << std::setw(9) << str_order[i] << std::endl; 
        for (j = 0; j < nkd; ++j){
            for (k = 0; k < nkd; ++k){
                std::cout << std::setw(9) << rcs[i][j][k];
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    for (i = 0; i < 3; ++i){
        if(!is_periodic[i]) nsize[i] = 0;
    }

    std::cout << std::endl << "Periodicity Flags (0: Non-Periodic, else: Periodic)" << std::endl;
    std::cout 
        << "a axis: " << std::setw(3) << is_periodic[0] << std::endl
        << "b axis: " << std::setw(3) << is_periodic[1] << std::endl
        << "c axis: " << std::setw(3) << is_periodic[2] << std::endl << std::endl;

    nneib = (2 * nsize[0] + 1) * (2 * nsize[1] + 1) * (2 * nsize[2] + 1);
    memory->allocate(xcrd, nneib, nat, 3);
    memory->allocate(distlist, nat, nat);

    calc_distlist(nat, system->xcoord);

    files->ofs_int.open(files->file_int.c_str(), std::ios::out);
    if(!files->ofs_int) error->exit("openfiles", "cannot open int file");

    search_interactions();
    calc_minvec();

    files->ofs_int.close();
    timer->print_elapsed();
}

double Interaction::distance(double *x1, double *x2)
{
    double dist;    
    dist = std::pow(x1[0] - x2[0], 2) + std::pow(x1[1] - x2[1], 2) + std::pow(x1[2] - x2[2], 2);
    dist = std::sqrt(dist);

    return dist;
}

void Interaction::calc_distlist(int nat, double **xf)
{
    int icell = 0;
    int i, j;
    int isize, jsize, ksize;

    for (i = 0; i < nat; ++i){
        for (j = 0; j < 3; ++j){
            xcrd[0][i][j] = xf[i][j];
        }
    }

    for (isize = -nsize[0]; isize <= nsize[0] ; ++isize){
        for (jsize = -nsize[1]; jsize <= nsize[1] ; ++jsize){
            for (ksize = -nsize[2]; ksize <= nsize[2] ; ++ksize){
                if (isize == 0 && jsize == 0 && ksize == 0) continue;

                ++icell;
                for (i = 0; i < nat; ++i){
                    xcrd[icell][i][0] = xf[i][0] + static_cast<double>(isize);
                    xcrd[icell][i][1] = xf[i][1] + static_cast<double>(jsize);
                    xcrd[icell][i][2] = xf[i][2] + static_cast<double>(ksize);
                }
            }
        }
    }

    for (icell = 0; icell < nneib; ++icell) system->frac2cart(xcrd[icell]);

    double dist_tmp;

    for (i = 0; i < nat; ++i){
        for (j = i; j < nat; ++j){
            distlist[i][j] = distance(xcrd[0][i], xcrd[0][j]);
            distlist[j][i] = distlist[i][j];
        }
    }

    for (icell = 1; icell < nneib; ++icell){
        for (i = 0; i < nat; ++i){
            for (j = i; j < nat; ++j){
                dist_tmp = distance(xcrd[0][i], xcrd[icell][j]);
                distlist[i][j] = std::min<double>(dist_tmp, distlist[i][j]);
                distlist[j][i] = distlist[i][j];
            }
        }
    }
    
    unsigned int iat;
    int icount;
    std::cout << std::endl;
    std::cout << "List of distance (in Bohr)" << std::endl;
    for (i = 0; i < symmetry->natmin; ++i){
        icount = 0;
        iat = symmetry->map_p2s[i][0];
        std::cout << std::setw(5) << iat + 1 << " (" << std::setw(3) << system->kdname[system->kd[iat]-1] << "):  ";
        for (j = i; j < nat; ++j){
            if (icount && icount % 6 == 0) {
                std::cout << std::endl;
                std::cout << "              ";
            }
            ++icount;

            std::cout << std::setw(3) << j + 1 << "(" << std::setw(3) << system->kdname[system->kd[j]-1] << ")";
            std::cout << std::setw(8) << distlist[iat][j] << "  ";
        }
        std::cout << std::endl << std::endl;
    }
}

void Interaction::search_interactions()
{
    int icell;
    int i, j;
    int iat, jat;
    int order;

    double dist;

    int natmin = symmetry->natmin;
    int nat = system->nat;

    int ***countint;

    memory->allocate(countint, natmin, nat, maxorder);
    memory->allocate(intpairs, natmin, maxorder, nat);
    memory->allocate(ninter, natmin, maxorder);
    memory->allocate(relvec, natmin, maxorder, nat, 3);

    // initialize arrays
    for (i = 0; i < natmin; ++i){
        for (j = 0; j < nat; ++j){
            for (order = 0; order < maxorder; ++order){
                countint[i][j][order] = 0;
            }
        }
    }

    for (i = 0; i < natmin; ++i){
        for (order = 0; order < maxorder; ++order){
            for (j = 0; j < nat; ++j){
                intpairs[i][order][j] = 0;
            }
        }
    }

    for (i = 0; i < natmin; ++i){
        for (order = 0; order < maxorder; ++order){
            ninter[i][order] = 0;
        }
    }
    ///

    for (icell = 0; icell < nneib; ++icell){
        for (i = 0; i < natmin; ++i){

            iat = symmetry->map_p2s[i][0]; //index of an atom in the primitive cell

            for (jat = 0; jat < nat; ++jat){

                dist = distance(xcrd[0][iat], xcrd[icell][jat]);

                for (order = 0; order < maxorder; ++order){

                    if(dist <= rcs[order][system->kd[iat] - 1][system->kd[jat] - 1]) {

                        if(!countint[i][jat][order]) {
                            intpairs[i][order][ninter[i][order]] = jat;

                            // store relative vectors for molecular dynamics simulation
                            for(j = 0; j < 3; ++j){
                                relvec[i][order][ninter[i][order]][j] = xcrd[icell][jat][j] - xcrd[0][iat][j];
                            }
                            ++ninter[i][order];
                        }
                        ++countint[i][jat][order];
                    }
                }
            }
        }
    }

    if(maxval(natmin, nat, order, countint) > 1) {
        error->warn("search_interactions", "Duplicate interaction exits\nThis will be a critical problem for a large cell MD.");
    }

#ifdef _DEBUG
    for (i = 0; i < natmin; i++){
        iat = symmetry->map_p2s[i][0];
        for (j = 0; j < nat; j++){
            std::cout << std::setw(5) << iat << std::setw(5) << j + 1; 
            for (order = 0; order < maxorder; ++order){
                std::cout << std::setw(3) << countint[i][j][order];
            }
            std::cout << std::endl;
        }
    }
#endif

    std::vector<int> intlist;

    intlist.clear();
    for(order = 0; order < maxorder; ++order){

        std::set<IntList> listset;

        std::cout << std::endl << "***" << str_order[order] << "***" << std::endl;

        for(i = 0; i < natmin; ++i){

            if(ninter[i][order] == 0) {
                std::cout << "No interacting atoms! ... skipped" << std::endl;
                continue; // no interaction atoms
            }

            iat = symmetry->map_p2s[i][0];

            for(j = 0; j < ninter[i][order]; ++j){
                intlist.push_back(intpairs[i][order][j]);
            }
            std::sort(intlist.begin(), intlist.end());

#ifdef _DEBUG
            for(std::vector<int>::iterator it = intlist.begin(); it != intlist.end(); ++it){
                std::cout << std::setw(5) << iat + 1 << std::setw(7) << *it + 1<< std::endl;
            }
#endif
            // write atoms inside the cutoff radius
            int id = 0;
            std::cout << "Atom " << std::setw(5) << iat + 1  
                << "(" << std::setw(3) << system->kdname[system->kd[iat]-1] << ")" << " interacts with atoms ... " << std::endl;
            for(std::vector<int>::iterator it = intlist.begin(); it != intlist.end(); ++it){
                std::cout << std::setw(5) << *it + 1 << "(" << std::setw(3) << system->kdname[system->kd[*it]-1] << ")";
                if(!(++id%15)) std::cout << std::endl;
            }
            std::cout << std::endl;
            std::cout << "Number of total interaction pairs (duplication allowed) = " << ninter[i][order] << std::endl << std::endl;

            int *intarr;        
            memory->allocate(intarr, order + 2);

            if(intlist.size() > 0) {
                if(order == 0){
                    for(unsigned int ielem = 0; ielem < intlist.size(); ++ielem){
                        intarr[0] = iat;
                        intarr[1] = intlist[ielem];
                        insort(order+2, intarr);

                        listset.insert(IntList(order + 2, intarr));
                    }
                } else if (order > 0) {
                    CombinationWithRepetition<int> g(intlist.begin(), intlist.end(), order + 1);
                    do {
                        std::vector<int> data = g.now();
                        intarr[0] = iat;
                        intarr[1] = data[0];
                        for(unsigned int isize = 1; isize < data.size() ; ++isize){
                            intarr[isize + 1] = data[isize];
                        }

                        if(!is_incutoff(order+2, intarr)) continue;
                        insort(order+2, intarr);

                        listset.insert(IntList(order + 2, intarr));

                    } while(g.next());
                }
            }
            intlist.clear();
            memory->deallocate(intarr);
        }
        // Write interaction pairs of atoms to pairs files
        files->ofs_int << listset.size() << std::endl;
        for (std::set<IntList>::iterator p = listset.begin(); p != listset.end(); ++p){
            files->ofs_int << *p;
        }
    }

    memory->deallocate(countint);
}

bool Interaction::is_incutoff(int n, int *atomnumlist)
{
    int i, j;
    double **dist_tmp;
    double tmp;
    int *min_neib;
    int ncheck = n - 1;

    memory->allocate(dist_tmp, nneib, ncheck);
    memory->allocate(min_neib, ncheck);

    // distance from a reference atom[0] in the original cell
    // to atom number atom[j](j > 0) in the neighboring cells.
    for (i = 0; i < nneib; ++i){
        for (j = 0; j < ncheck; ++j){
            dist_tmp[i][j] = distance(xcrd[0][atomnumlist[0]], xcrd[i][atomnumlist[j+1]]);
        }
    }

    // find the indecis of neighboring cells which minimize the distances.
    for (i = 0; i < ncheck; ++i){

        tmp = dist_tmp[0][i];
        min_neib[i] = 0;

        for (j = 1; j < nneib; ++j){
            if(dist_tmp[j][i] < tmp) {
                tmp = dist_tmp[j][i];
                min_neib[i] = j;
            }
        }
    }

    // judge whether or not the given atom list interact with each other
    for (i = 0; i < ncheck; ++i){
        for (j =  i + 1; j < ncheck; ++j){

            tmp = distance(xcrd[min_neib[i]][atomnumlist[i + 1]], xcrd[min_neib[j]][atomnumlist[j + 1]]);
            if(tmp > rcs[ncheck - 1][system->kd[atomnumlist[i + 1]] - 1][system->kd[atomnumlist[j + 1]] - 1]){
                memory->deallocate(dist_tmp);
                memory->deallocate(min_neib);
                return false;
            }
        }
    }

    memory->deallocate(dist_tmp);
    memory->deallocate(min_neib);
    return true;
}

void Interaction::calc_minvec()
{

    int i, j, k;
    int isize, jsize, ksize;
    int nat = system->nat;
    int natmin = symmetry->natmin;
    int iat, jat;
    int nneib, icell;
    int **minloc;
    double **x0 = system->xcoord;
    double ***x_neib;
    double dist;
    double **dist_tmp;
    double x_center[3];

    std::set<InteractionCluster> xset;

    xset.clear();

    memory->allocate(minvec, natmin, nat, 3);
    memory->allocate(x_neib, nat, 3);
    memory->allocate(dist_tmp, natmin, nat);
    memory->allocate(minloc, natmin, nat);

    nneib = (2 * nsize[0] + 1) * (2 * nsize[1] + 1) * (2 * nsize[2] + 1);
    memory->allocate(x_neib, nneib, nat, 3);

    for (i = 0; i < nat; ++i){
        for (j = 0; j < 3; ++j){
            x_neib[0][i][j] = x0[i][j];
        }
    }
    system->frac2cart(x_neib[0]);

    icell = 0;
    for (isize = -nsize[0]; isize <= nsize[0] ; ++isize){
        for (jsize = -nsize[1]; jsize <= nsize[1] ; ++jsize){
            for (ksize = -nsize[2]; ksize <= nsize[2] ; ++ksize){
                if (isize == 0 && jsize == 0 && ksize == 0) continue;

                ++icell;	

                for (i = 0; i < nat; ++i){
                    x_neib[icell][i][0] = x0[i][0] + static_cast<double>(isize);
                    x_neib[icell][i][1] = x0[i][1] + static_cast<double>(jsize);
                    x_neib[icell][i][2] = x0[i][2] + static_cast<double>(ksize);
                }
                system->frac2cart(x_neib[icell]);
            }
        }
    }

    for (i = 0;	i < natmin; ++i){
        iat = symmetry->map_p2s[i][0];
        for (j = 0; j < ninter[i][0]; ++j){
            jat = intpairs[i][0][j];
            dist_tmp[i][jat] = distance(x_neib[0][iat], x_neib[0][jat]);
            minloc[i][jat] = 0;
            for (icell = 1; icell < nneib; ++icell){
                dist = distance(x_neib[0][iat], x_neib[icell][jat]);
                if (dist < dist_tmp[i][jat]){
                    dist_tmp[i][jat] = dist;
                    minloc[i][jat] = icell;
                }

            }
        }
    }

    for (i = 0; i < natmin; ++i){
        for (j = 0; j < ninter[i][0]; ++j){
            jat = intpairs[i][0][j];
            xset.insert(InteractionCluster(x_neib[minloc[i][jat]][jat]));
        }
    }

    for (i = 0; i < 3; ++i) x_center[i] = 0.0;

#ifdef _DEBUG
    std::cout << "Size of the cluster : " << xset.size() << std::endl;
    for (std::set<InteractionCluster>::iterator p = xset.begin(); p != xset.end(); ++p){
        InteractionCluster x_tmp = *p;
        for (i = 0; i < 3; ++i){
            std::cout << std::setw(15) << x_tmp.x[i];
        }
        std::cout << std::endl;
    }
#endif

    for (std::set<InteractionCluster>::iterator p = xset.begin(); p != xset.end(); ++p){
        InteractionCluster x_tmp = *p;
        for (i = 0; i < 3; ++i){
            x_center[i] += x_tmp.x[i];
        }
    }

    for (i = 0; i < 3; ++i) x_center[i] /= static_cast<double>(xset.size()); 

    // It was found that the result does not depend on x_center.

#ifdef _DEBUG
    std::cout << "Coordinate of the center" << std::endl;
    for (i = 0; i < 3; ++i) {
        std::cout << std::setw(15) << x_center[i];
    }
    std::cout << std::endl;
#endif

    for (i = 0; i < natmin; ++i){

        iat = symmetry->map_p2s[i][0];

        for (j = 0; j < ninter[i][0]; ++j){
            jat = intpairs[i][0][j];
            for (k = 0; k < 3; ++k){
                minvec[i][jat][k] = x_neib[minloc[i][jat]][jat][k] - x_center[k];
            }
        }
    }

#ifdef _DEBUG
    std::cout << "Relative Coordinate From Center of the System" << std::endl;

    for (i = 0; i < natmin; ++i){
        iat = symmetry->map_p2s[i][0];
        std::cout << std::setw(5) << iat + 1 << std::endl;
        for (j = 0; j < ninter[i][0]; ++j) {
            jat = intpairs[i][0][j];
            for (k = 0; k < 3; ++k){
                std::cout << std::setw(18) << std::scientific << minvec[i][jat][k];
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    std::cout.unsetf(std::ios::scientific);
#endif

    memory->deallocate(minloc);
    memory->deallocate(x_neib);
    memory->deallocate(dist_tmp);
}

void Interaction::set_ordername(){

    std::string strnum;

    str_order[0] = "HARMONIC";

    for (int i = 1;  i < maxorder; ++i){
        strnum = boost::lexical_cast<std::string>(i+2);
        str_order[i] = "ANHARM" + strnum;
    }

}
