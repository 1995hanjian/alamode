#include "mpi_common.h"
#include "conductivity.h"
#include "memory.h"
#include "kpoint.h"
#include "dynamical.h"
#include "relaxation.h"
#include "phonon_velocity.h"
#include "phonon_thermodynamics.h"
#include "integration.h"
#include <fstream>
#include <iomanip>
#include "parsephon.h"
#include "error.h"
#include "write_phonons.h"
#include "../alm_c++/constants.h"
#include <iostream>
#include "system.h"

using namespace PHON_NS;

Conductivity::Conductivity(PHON *phon): Pointers(phon) {}
Conductivity::~Conductivity(){};

void Conductivity::setup_kl()
{
    nk = kpoint->nk;
    ns = dynamical->neval;

    memory->allocate(vel, nk, ns, 3);
    memory->allocate(tau, nk, ns);

    unsigned int i, j, k;

    for (i = 0; i < nk; ++i){
        phonon_velocity->phonon_vel_k(kpoint->xk[i], vel[i]);

        // Generate phonon velocity in cartesian coordinate
        for (j = 0; j < ns; ++j){
            system->rotvec(vel[i][j], vel[i][j], system->lavec_p, 'T');
            for (k = 0; k < 3; ++k) vel[i][j][k] /= 2.0 * pi;
        }
    }

    std::cout.setf(std::ios::fixed);

    std::cout << " Tmin = " << std::setw(10) << system->Tmin; 
    std::cout << " Tmax = " << std::setw(10) << system->Tmax; 
    std::cout << " dT   = " << std::setw(10) << system->dT; 
    std::cout << std::endl;

    std::cout.unsetf(std::ios::fixed);
}

void Conductivity::finish_kl()
{
    memory->deallocate(vel);
    memory->deallocate(tau);
}

void Conductivity::calc_kl()
{
    unsigned int iT, i, j;
    double T;

    double Tmax = system->Tmax;
    double Tmin = system->Tmin;
    double dT = system->dT;

    std::string file_kl;
    std::ofstream ofs_kl;

    unsigned int NT= static_cast<unsigned int>((Tmax - Tmin) / dT);

    if (mympi->my_rank == 0) {
        file_kl = input->job_title + ".kl";
        ofs_kl.open(file_kl.c_str(), std::ios::out);
        if(!ofs_kl) error->exit("calc_kl", "cannot open file_kl");

        ofs_kl << "# Temperature [K], Thermal Conductivity (xx, xy, xz, yx, yy, yz, zx, zy, zz) [W/mK]" << std::endl;
    }
    
    relaxation->calc_ReciprocalV();

    for (iT = 0; iT <= NT; ++iT){
        T = Tmin + dT * static_cast<double>(iT);
        relaxation->calc_selfenergy_at_T(T);
        calc_kl_at_T(T);

        if (mympi->my_rank == 0) {
            ofs_kl << std::setw(5) << T;
        for (i = 0; i < 3; ++i){
            for (j = 0; j < 3; ++j){
                ofs_kl << std::setw(15) << kl[i][j];
            }
        }
        ofs_kl << std::endl;
        }
    }

    if (mympi->my_rank == 0) ofs_kl.close();
}

void Conductivity::calc_kl_at_T(const double T)
{
    unsigned int i, j;
    unsigned int is, ik;
    unsigned int jk, kk;
    double omega;
    unsigned int knum;
    double tau_tmp;

    std::complex<double> tmp1;
    unsigned int ktmp, stmp;

    unsigned int ikIBZ = 0, nsame = 0;
    
    kk = 0;
 
    /*
    for (ik = 0; ik < kpoint->nk_equiv.size(); ++ik){
        for (jk = 0; jk < kpoint->nk_equiv[ik]; ++jk){
     
            knum = kpoint->kpIBZ[kk].knum;
            if (jk == 0) std::cout << "#";
        std::cout << "#K = " << std::setw(4) << ik + 1;
        std::cout << " knum = " << std::setw(4) << knum + 1;
        std::cout << " xk = " << std::setw(15 ) << kpoint->xk[knum][0]  << std::setw(15) << kpoint->xk[knum][1]  << std::setw(15) << kpoint->xk[knum][2];
        std::cout << ": ";
        for (is = 0; is < ns; ++is){
            omega = dynamical->eval_phonon[knum][is];
            std::cout << std::setw(13) << phonon_thermodynamics->Cv(omega, T);
            std::cout << std::setw(15) << vel[knum][is][0] * vel[knum][is][0];
            std::cout << std::setw(15) << 1.0 / (2.0 *relaxation->selfenergy(T, omega, knum, is).imag());
        }
        std::cout << " : " << std::endl;
        ++kk;
    }
    }
    */
    

    for (i = 0; i < 3; ++i){
        for (j = 0; j < 3; ++j){
            kl[i][j] = 0.0;
        }
    }

    double vv_tmp;

    jk = 0;

    for (ik = 0; ik < kpoint->nk_equiv.size(); ++ik){

        knum = kpoint->kpIBZ[jk].knum;

        for (is = 0; is < ns; ++is){

            omega = dynamical->eval_phonon[knum][is];
            tau[knum][is] = 1.0 / (2.0 * relaxation->self_E[ns * knum + is].imag());
            // tau[knum][is] = 1.0 / (2.0 * relaxation->selfenergy(T, omega, knum, is).imag());
            //          tau[ik][is] = 1.0 / (2.0 * relaxation->self_tetra(T, omega, ik, is));

            for (i = 0; i < 3; ++i){
                for (j = 0; j < 3; ++j){

                    vv_tmp = 0.0;

                    for (kk = 0; kk < kpoint->nk_equiv[ik]; ++kk){
                        ktmp = kpoint->kpIBZ[jk + kk].knum;
                        vv_tmp += vel[ktmp][is][i] * vel[ktmp][is][j];
                    }
                    vv_tmp /= static_cast<double>(kpoint->nk_equiv[ik]);
                                  
                    kl[i][j] += kpoint->weight_k[ik] * phonon_thermodynamics->Cv(omega, T) * vv_tmp * tau[knum][is];
                }
            }
        }

        jk += kpoint->nk_equiv[ik];
    }

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            kl[i][j] /= Bohr_in_Angstrom * 1.0e-10 * time_ry * system->volume_p;
        }
    }
}
