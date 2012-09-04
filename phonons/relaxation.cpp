#include <fstream>
#include <iomanip>
#include <algorithm>
#include "relaxation.h"
#include "memory.h"
#include "kpoint.h"
#include "system.h"
#include "dynamical.h"
#include "fcs_phonon.h"
#include "../alm_c++/constants.h"
#include "phonon_thermodynamics.h"
#include "phonon_velocity.h"
#include "parsephon.h"
#include "error.h"
#include <vector>
#include "conductivity.h"
#include "write_phonons.h"
#include <omp.h>

using namespace PHON_NS;

Relaxation::Relaxation(PHON *phon): Pointers(phon) {
    im = std::complex<double>(0.0, 1.0);
}

Relaxation::~Relaxation(){};

void Relaxation::setup_relaxation()
{
    unsigned int nk = kpoint->nk;
    unsigned int nband = dynamical->neval;

    memory->allocate(V, 1);
    memory->allocate(self_E, nk*nband);

    unsigned int i, j, k;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j){
            mat_convert[i][j] = 0.0;
            for (k = 0; k < 3; ++k){
                mat_convert[i][j] += system->rlavec_p[i][k] * system->lavec_s[k][j]; 
            }
        }
    }

    memory->allocate(vec_s, system->ntran, 3);
    memory->allocate(mass_p, system->natmin);

    for (i = 0; i < system->ntran; ++i){
        for (j = 0; j < 3; ++j){
            vec_s[i][j] = system->xr_s[system->map_p2s[0][i]][j];
        }
    }

    for (i = 0; i < system->natmin; ++i){
        mass_p[i] = system->mass[system->map_p2s[i][0]];
    }

    double domega_min;

    domega_min = 10000.0;

    domega_min = std::min(domega_min, writes->in_kayser(dynamical->eval_phonon[1][0]));
    domega_min = std::min(domega_min, writes->in_kayser(dynamical->eval_phonon[kpoint->nkz][0]));
    domega_min = std::min(domega_min, writes->in_kayser(dynamical->eval_phonon[kpoint->nkz*kpoint->nky][0]));

    std::cout << std::endl;
    std::cout << "Estimated minimum energy difference (cm^-1) = " << domega_min << std::endl;
    std::cout << "Given epsilon (cm^-1) = " << epsilon << std::endl << std::endl;

    modify_eigenvectors();

/*
    unsigned int tmp[3];
    tmp[0] = 3; tmp[1] = 3; tmp[2] = 4;
    std::cout << V3(tmp[0], tmp[1], tmp[2]) << std::endl;

    std::cout << V3new(tmp) << std::endl;
*/

    epsilon *= time_ry / Hz_to_kayser;
}

void Relaxation::finish_relaxation()
{
    V[0].clear();
    memory->deallocate(V);
    memory->deallocate(self_E);

    memory->deallocate(vec_s);
    memory->deallocate(mass_p);
}

void Relaxation::calc_ReciprocalV()
{
    // Calculate V_{ks, k's', k''s''}^(3) for Self-energy calculation

    unsigned int nband = dynamical->neval;
    unsigned int nk = kpoint->nk;
    unsigned int i;
    unsigned int k1, k2, k3;
    unsigned int b1, b2, b3;

    std::vector<StructKS> kslist;
    StructKS ks_tmp;
    unsigned int ks_arr[3];

    unsigned int nkp;

    int ik;

    double xk_tmp[3], xk_norm;
    std::complex<double> prod;

    std::vector<FcsClass>::iterator it;

    std::cout << std::endl;
    std::cout << "Calculating force constants in reciprocal space .." << std::endl;

    nkp = 0;

    kslist.clear();

    for (k1 = 0; k1 < nk; ++k1){
        for (k2 = 0; k2 < nk; ++k2){
            for (k3 = 0; k3 < nk; ++k3){

                xk_tmp[0] = kpoint->xk[k1][0] + kpoint->xk[k2][0] + kpoint->xk[k3][0];
                xk_tmp[1] = kpoint->xk[k1][1] + kpoint->xk[k2][1] + kpoint->xk[k3][1];
                xk_tmp[2] = kpoint->xk[k1][2] + kpoint->xk[k2][2] + kpoint->xk[k3][2];

                for (i = 0; i < 3; ++i){
                    xk_tmp[i] = std::fmod(xk_tmp[i], 1.0);
                }
                xk_norm = std::pow(xk_tmp[0], 2) + std::pow(xk_tmp[1], 2) + std::pow(xk_tmp[2], 2);

                // If the momentum-conservation is not satisfied, we skip the loop.

                if (std::sqrt(xk_norm) > eps15) continue;

                for (b1 = 0; b1 < nband; ++b1){
                    for (b2 = 0; b2 < nband; ++b2){
                        for (b3 = 0; b3 < nband; ++b3){

                            ks_tmp.ks1 = nband * k1 + b1;
                            ks_tmp.ks2 = nband * k2 + b2;
                            ks_tmp.ks3 = nband * k3 + b3;

                            if (ks_tmp.ks1 > ks_tmp.ks2 || ks_tmp.ks2 > ks_tmp.ks3) continue;

                            kslist.push_back(ks_tmp);
                        }
                    }
                }
            }
        }
    }

    nkp = kslist.size();

#pragma omp parallel for private(ks_arr, prod) schedule(static)
    for (ik = 0; ik < nkp; ++ik){

        ks_arr[0] = kslist[ik].ks1;
        ks_arr[1] = kslist[ik].ks2;
        ks_arr[2] = kslist[ik].ks3;
        prod = V3(ks_arr[0], ks_arr[1], ks_arr[2]);

        // Add to list
        if (std::abs(prod) > eps15) {
#pragma omp critical
            V[0].push_back(ReciprocalVs(prod, ks_arr, 3));
        }
    }

    kslist.clear();
    std::cout << "Done !" << std::endl;
    std::cout << "Number of nonzero V's: " << V[0].size() << std::endl;
}

std::complex<double> Relaxation::V3(const unsigned int ks1, const unsigned int ks2, const unsigned int ks3)
{
    unsigned int i;
    unsigned int k1, k2, k3;
    unsigned int b1, b2, b3;
    unsigned int nband = dynamical->neval;

    double mass_prod, phase, omega_prod;
    double vec1[3], vec2[3];
    double omega[3];

    std::vector<FcsClass>::iterator it;
    std::complex<double> ret, tmp;

    ret = std::complex<double>(0.0, 0.0);

    k1 = ks1 / nband;
    k2 = ks2 / nband;
    k3 = ks3 / nband;

    b1 = ks1 % nband;
    b2 = ks2 % nband;
    b3 = ks3 % nband;

    omega[0] = dynamical->eval_phonon[k1][b1];
    omega[1] = dynamical->eval_phonon[k2][b2];
    omega[2] = dynamical->eval_phonon[k3][b3];
    omega_prod = omega[0] * omega[1] * omega[2];

    for (it = fcs_phonon->force_constant[1].begin(); it != fcs_phonon->force_constant[1].end(); ++it){
        FcsClass fcs = *it;

        for (i = 0; i < 3; ++i){
            vec1[i] = vec_s[fcs.elems[1].cell][i] - vec_s[fcs.elems[0].cell][i];
            vec2[i] = vec_s[fcs.elems[2].cell][i] - vec_s[fcs.elems[0].cell][i];
            vec1[i] = dynamical->fold(vec1[i]);
            vec2[i] = dynamical->fold(vec2[i]);
        }

        system->rotvec(vec1, vec1, mat_convert);
        system->rotvec(vec2, vec2, mat_convert);

        phase = vec1[0] * kpoint->xk[k2][0] + vec1[1] * kpoint->xk[k2][1] + vec1[2] * kpoint->xk[k2][2]
        + vec2[0] * kpoint->xk[k3][0] + vec2[1] * kpoint->xk[k3][1] + vec2[2] * kpoint->xk[k3][2];

        mass_prod = mass_p[fcs.elems[0].atom] * mass_p[fcs.elems[1].atom] * mass_p[fcs.elems[2].atom];

        ret += (*it).fcs_val * std::exp(im * phase) / std::sqrt(mass_prod)
            * dynamical->evec_phonon[k1][b1][3 * fcs.elems[0].atom + fcs.elems[0].xyz]
        * dynamical->evec_phonon[k2][b2][3 * fcs.elems[1].atom + fcs.elems[1].xyz]
        * dynamical->evec_phonon[k3][b3][3 * fcs.elems[2].atom + fcs.elems[2].xyz];
    }

    return ret/std::sqrt(omega_prod);
}

std::complex<double> Relaxation::V3new(const unsigned int ks[3])
{
    std::complex<double> ret(0.0, 0.0);

    unsigned int i;

    unsigned int kn[3];
    unsigned int sn[3];
    unsigned int ns = dynamical->neval;

    double omega[3];
    double mass_prod;
    double phase;
    double vec1[3], vec2[3];

    std::vector<FcsClass>::iterator it;
    std::complex<double> ctmp;

    for (i = 0; i < 3; ++i){
        kn[i] = ks[i] / ns;
        sn[i] = ks[i] % ns;
        omega[i] = dynamical->eval_phonon[kn[i]][sn[i]];
//        std::cout << "k = " << kn [i] << " s = " << sn[i] << " omega = " << omega[i] << std::endl;
//        std::cout << "(" << kpoint->xk[kn[i]][0] << " , " << kpoint->xk[kn[i]][1] << " , " << kpoint->xk[kn[i]][2] << " )" << std::endl;
    }

    for (it = fcs_phonon->force_constant[1].begin(); it != fcs_phonon->force_constant[1].end(); ++it){
        FcsClass fcs = *it;

        for (i = 0; i < 3; ++i){
            vec1[i] = vec_s[fcs.elems[1].cell][i] - vec_s[fcs.elems[0].cell][i];
            vec2[i] = vec_s[fcs.elems[2].cell][i] - vec_s[fcs.elems[0].cell][i];
            vec1[i] = dynamical->fold(vec1[i]);
            vec2[i] = dynamical->fold(vec2[i]);
        }

        system->rotvec(vec1, vec1, mat_convert);
        system->rotvec(vec2, vec2, mat_convert);

        phase = 0.0;
        ctmp = std::complex<double>(1.0, 0.0);
        mass_prod = 1.0;
/*
        std::cout << "#FCS3 = " << fcs.fcs_val << std::endl;
        std::cout << "(m1, a1, mu1) = ( " << fcs.elems[0].cell << " , " << fcs.elems[0].atom << " , " << fcs.elems[0].xyz << " )" << std::endl;
        std::cout << "(m2, a2, mu2) = ( " << fcs.elems[1].cell << " , " << fcs.elems[1].atom << " , " << fcs.elems[1].xyz << " )" << std::endl;
        std::cout << "(m3, a3, mu3) = ( " << fcs.elems[2].cell << " , " << fcs.elems[2].atom << " , " << fcs.elems[2].xyz << " )" << std::endl;
        std::cout << "e1 = " << dynamical->evec_phonon[kn[0]][sn[0]][3 * fcs.elems[0].atom + fcs.elems[0].xyz] << std::endl;
        std::cout << "e2 = " << dynamical->evec_phonon[kn[1]][sn[1]][3 * fcs.elems[1].atom + fcs.elems[1].xyz] << std::endl;
        std::cout << "e3 = " << dynamical->evec_phonon[kn[2]][sn[2]][3 * fcs.elems[2].atom + fcs.elems[2].xyz] << std::endl;
*/

        for (i = 0; i < 3; ++i){
            phase += vec1[i] * kpoint->xk[kn[1]][i] + vec2[i] * kpoint->xk[kn[2]][i];
            mass_prod *= mass_p[fcs.elems[i].atom];
            ctmp *= dynamical->evec_phonon[kn[i]][sn[i]][3 * fcs.elems[i].atom + fcs.elems[i].xyz];
        }

        ret += fcs.fcs_val * std::exp(im * phase) * ctmp / std::sqrt(mass_prod);
    }

    ret /= std::sqrt(omega[0] * omega[1] * omega[2]);

    return ret;
}

void Relaxation::calc_selfenergy()
{
    double Tmin = system->Tmin;
    double Tmax = system->Tmax;
    double dT = system->dT;

    unsigned int NT= static_cast<unsigned int>((Tmax - Tmin) / dT);

    unsigned int i;

    unsigned int nband = dynamical->neval;
    unsigned int nk = kpoint->nk;

    unsigned int nks = nband*nk;

    unsigned int iks;
    double T;

    std::string file_selfenergy;
    std::ofstream ofs_selfenergy;

    file_selfenergy = input->job_title + ".selfE";
    ofs_selfenergy.open(file_selfenergy.c_str(), std::ios::out);
    if(!ofs_selfenergy) error->exit("write_selfenergy", "cannot open file_selfenergy");

    ofs_selfenergy << "#Temperature, k-point, branch, Energy [cm^-1], Re[Sigma] [cm^-1], Im[Sigma]^[cm^-1], Tau [ps]" << std::endl;
    ofs_selfenergy.setf(std::ios::scientific);

    std::cout << selfenergy(0.0, dynamical->eval_phonon[0][3], 0, 3) << std::endl << std::endl;
//    std::cout << selfenergy(0.0, dynamical->eval_phonon[0][4], 0, 4) << std::endl << std::endl;
//    std::cout << selfenergy(0.0, dynamical->eval_phonon[0][5], 0, 5) << std::endl << std::endl;
    error->exit("hogehoge", "funnga---");

    for (i = 0; i <= NT; ++i){
        T = Tmin + dT * static_cast<double>(i);
        calc_selfenergy_at_T(T);
        for (iks = 0; iks < nks; ++iks){
            ofs_selfenergy << std::setw(5) << T;
            ofs_selfenergy << std::setw(5) << iks / nband;
            ofs_selfenergy << std::setw(5) << iks % nband;
            ofs_selfenergy << std::setw(15) << dynamical->eval_phonon[iks/nband][iks%nband]/time_ry*Hz_to_kayser;
            ofs_selfenergy << std::setw(15) << self_E[iks].real()/time_ry*Hz_to_kayser; 
            ofs_selfenergy << std::setw(15) << self_E[iks].imag()/time_ry*Hz_to_kayser;
            self_E[iks] = selfenergy(T, dynamical->eval_phonon[iks/nband][iks%nband], iks/nband, iks%nband);
            ofs_selfenergy << std::setw(15) << self_E[iks].real()/time_ry*Hz_to_kayser;
            ofs_selfenergy << std::setw(15) << self_E[iks].imag()/time_ry*Hz_to_kayser;
            ofs_selfenergy << std::setw(15) << 1.0e+12/(2.0 * self_E[iks].imag())*time_ry;
            ofs_selfenergy << std::endl;
        }
        ofs_selfenergy << std::endl;
    }
    ofs_selfenergy.close();

    std::string file_test;
    std::ofstream ofs_test;

    double omega_tmp;
    std::complex<double> ctmp;
    unsigned int j;

    file_test = input->job_title + ".test";
    ofs_test.open(file_test.c_str(), std::ios::out);
    if(!ofs_test) error->exit("write_selfenergy", "cannot open file_test");

    for (i = 1; i < 600; i+=5){
        omega_tmp = static_cast<double>(i);
        ofs_test << std::setw(15) << omega_tmp;
        omega_tmp *= time_ry / Hz_to_kayser;
        for (j = 0; j < 6; ++j){
            ctmp = selfenergy(Tmin, omega_tmp, 0, j);
            ofs_test << std::setw(15) << ctmp.real();
            ofs_test << std::setw(15) << ctmp.imag();
        }
        ofs_test << std::endl;
    }

    ofs_test.close();

}

std::complex<double> Relaxation::selfenergy(const double T, const double omega, const unsigned int knum, const unsigned int snum)
{
    std::complex<double> ret(0.0, 0.0);

    unsigned int nk = kpoint->nk;
    unsigned int ns = dynamical->neval;
    unsigned int nks;

    unsigned int ik, jk;
    unsigned int is, js;

    unsigned int i;
    unsigned int knum_inv;
    double ret_re, ret_im;

    nks = nk * ns;
    knum_inv = kpoint->knum_minus[knum];

#pragma omp parallel private(ik, jk, is, js)
    {

        int iks2;
        unsigned int iks, jks;
        unsigned int arr[3];
        double xk_tmp[3], xk_norm;
        double omega_inner[2];
        double n1, n2;

        std::complex<double> ctmp;

        ret_re = 0.0;
        ret_im = 0.0;
        arr[0] = ns * kpoint->knum_minus[knum] + snum;

#pragma omp for reduction (+:ret_re, ret_im)
        for (iks2 = 0; iks2 < nks*nks; ++iks2){
            iks = iks2 / nks;
            jks = iks2 % nks;
            ik = iks / ns;
            is = iks % ns;
            jk = jks / ns;
            js = jks % ns;

            arr[1] = iks;
            arr[2] = jks;

            xk_tmp[0] = kpoint->xk[knum_inv][0] + kpoint->xk[ik][0] + kpoint->xk[jk][0];
            xk_tmp[1] = kpoint->xk[knum_inv][1] + kpoint->xk[ik][1] + kpoint->xk[jk][1];
            xk_tmp[2] = kpoint->xk[knum_inv][2] + kpoint->xk[ik][2] + kpoint->xk[jk][2];

            for (i = 0; i < 3; ++i)  xk_tmp[i] = std::fmod(xk_tmp[i], 1.0);
            xk_norm = std::pow(xk_tmp[0], 2) + std::pow(xk_tmp[1], 2) + std::pow(xk_tmp[2], 2);
            if (std::sqrt(xk_norm) > eps15) continue; 

            omega_inner[0] = dynamical->eval_phonon[ik][is];
            omega_inner[1] = dynamical->eval_phonon[jk][js];
            n1 = phonon_thermodynamics->fB(omega_inner[0], T) + phonon_thermodynamics->fB(omega_inner[1], T) + 1.0;
            n2 = phonon_thermodynamics->fB(omega_inner[0], T) - phonon_thermodynamics->fB(omega_inner[1], T);
            
//            std::cout << " V3[" << arr[0] << "," << arr[1] << "," << arr[2] << "] = " << V3new(arr).real() << " " << V3new(arr).imag() << std::endl;
//            std::cout << "k1 = (" << kpoint->xk[arr[0]/ns][0] << "," << kpoint->xk[arr[0]/ns][1] << "," << kpoint->xk[arr[0]/ns][2] << ")" << std::endl;
//            std::cout << "k2 = (" << kpoint->xk[arr[1]/ns][0] << "," << kpoint->xk[arr[1]/ns][1] << "," << kpoint->xk[arr[1]/ns][2] << ")" << std::endl;
//            std::cout << "k3 = (" << kpoint->xk[arr[2]/ns][0] << "," << kpoint->xk[arr[2]/ns][1] << "," << kpoint->xk[arr[2]/ns][2] << ")" << std::endl;
//            std::cout << " 1st= " << 1.0 / (omega + omega_inner[0] + omega_inner[1] + im * epsilon) << std::endl;
//            std::cout << " 2nd= " << 1.0 / (omega - omega_inner[0] - omega_inner[1] + im * epsilon) << std::endl;

            ctmp =  std::norm(V3new(arr))
                * ( n1 / (omega + omega_inner[0] + omega_inner[1] + im * epsilon)
                - n1 / (omega - omega_inner[0] - omega_inner[1] + im * epsilon) 
                + n2 / (omega - omega_inner[0] + omega_inner[1] + im * epsilon)
                - n2 / (omega + omega_inner[0] - omega_inner[1] + im * epsilon));

            ret_re += ctmp.real();
            ret_im += ctmp.imag();
        }
    }

    return (ret_re + im * ret_im) * std::pow(0.5, 4) / static_cast<double>(system->ntran);
}

std::complex<double> Relaxation::selfenergy2(const double T, const double omega, const unsigned int knum, const unsigned int snum)
{
    std::complex<double> ret(0.0, 0.0);

    unsigned int nk = kpoint->nk;
    unsigned int ns = dynamical->neval;
    unsigned int nks;

    unsigned int ik, jk;
    unsigned int is, js;

    unsigned int i;
    unsigned int knum_inv;

    nks = nk * ns;
    knum_inv = kpoint->knum_minus[knum];

    unsigned int arr[3];
    double xk_tmp[3], xk_norm;
    double omega_inner[2];
    double n1, n2;

    std::complex<double> ctmp;

    arr[0] = ns * kpoint->knum_minus[knum] + snum;

    for (ik = 0; ik < nk; ++ik){
        for (jk = 0; jk < nk; ++jk){

            xk_tmp[0] = kpoint->xk[knum_inv][0] + kpoint->xk[ik][0] + kpoint->xk[jk][0];
            xk_tmp[1] = kpoint->xk[knum_inv][1] + kpoint->xk[ik][1] + kpoint->xk[jk][1];
            xk_tmp[2] = kpoint->xk[knum_inv][2] + kpoint->xk[ik][2] + kpoint->xk[jk][2];

            for (i = 0; i < 3; ++i){
                xk_tmp[i] = std::fmod(xk_tmp[i], 1.0);
            }
            xk_norm = std::pow(xk_tmp[0], 2) + std::pow(xk_tmp[1], 2) + std::pow(xk_tmp[2], 2);


            if (std::sqrt(xk_norm) > eps15) continue; 

            for (is = 0; is < ns; ++is){
                for (js = 0; js < ns; ++js){

                    arr[1] = ns * ik + is;
                    arr[2] = ns * jk + js;
                    omega_inner[0] = dynamical->eval_phonon[ik][is];
                    omega_inner[1] = dynamical->eval_phonon[jk][js];
                    n1 = phonon_thermodynamics->fB(omega_inner[0], T) + phonon_thermodynamics->fB(omega_inner[1], T) + 1.0;
                    n2 = phonon_thermodynamics->fB(omega_inner[0], T) - phonon_thermodynamics->fB(omega_inner[1], T);

                    ctmp =  std::norm(V3new(arr))
                        * ( n1 / (omega + omega_inner[0] + omega_inner[1] + im * epsilon)
                        - n1 / (omega - omega_inner[0] - omega_inner[1] + im * epsilon) 
                        + n2 / (omega - omega_inner[0] + omega_inner[1] + im * epsilon)
                        - n2 / (omega + omega_inner[0] - omega_inner[1] + im * epsilon));

                    ret += ctmp;
                }
            }
        }
    }

    return ret * std::pow(0.5, 4) / static_cast<double>(system->ntran); 
}

void Relaxation::calc_selfenergy_at_T(const double T)
{
    unsigned int i;
    unsigned int nk = kpoint->nk;
    unsigned int nband = dynamical->neval;
    unsigned int nks;
    double v_norm;

    unsigned int *ind, *knum, *snum;
    double *omega;
    double n1, n2;

    double fcell = 1.0 / static_cast<double>(system->ntran);

    std::complex<double> ctmp;

    memory->allocate(ind, 3);
    memory->allocate(knum, 3);
    memory->allocate(snum, 3);
    memory->allocate(omega, 3);

    nks = nk * nband;

    for (i = 0; i < nks; ++i) {
        self_E[i] = std::complex<double>(0.0, 0.0);
    }
    for (std::vector<ReciprocalVs>::iterator it = V[0].begin(); it != V[0].end(); ++it){
        ReciprocalVs obj = *it;
        v_norm = std::norm(obj.v);

        do {
            for (i = 0; i < 3; ++i){
                ind[i] = obj.ks[i];
                knum[i] = ind[i] / nband;
                snum[i] = ind[i] % nband;
                omega[i] = dynamical->eval_phonon[knum[i]][snum[i]];
            }

            n1 = phonon_thermodynamics->fB(omega[1], T) + phonon_thermodynamics->fB(omega[2], T) + 1.0;
            n2 = phonon_thermodynamics->fB(omega[1], T) - phonon_thermodynamics->fB(omega[2], T);

            ctmp =  std::pow(0.5, 4) * fcell * v_norm
                * ( n1 / (omega[0] + omega[1] + omega[2] + im * epsilon)
                - n1 / (omega[0] - omega[1] - omega[2] + im * epsilon) 
                + n2 / (omega[0] - omega[1] + omega[2] + im * epsilon)
                - n2 / (omega[0] + omega[1] - omega[2] + im * epsilon));

            self_E[nband * kpoint->knum_minus[knum[0]] + snum[0]] += ctmp;

        } while (std::next_permutation(obj.ks.begin(), obj.ks.end()));

    }
}

void Relaxation::modify_eigenvectors()
{
    bool *flag_done;
    unsigned int ik, nk;
    unsigned int is, js, ns;
    unsigned int nk_inv;
    std::complex<double> *evec_tmp;
    
    nk = kpoint->nk;
    ns = dynamical->neval;

    std::cout << "**********      NOTICE      **********" << std::endl;
    std::cout << "For the brevity of the calculation, " << std::endl;
    std::cout << "phonon eigenvectors will be modified" << std::endl;
    std::cout << "so that e_{-ks}^{mu} = (e_{ks}^{mu})^{*}. " << std::endl;

    memory->allocate(flag_done, nk);
    memory->allocate(evec_tmp, ns);

    for (ik = 0; ik < nk; ++ik) flag_done[ik] = false;

    for (ik = 0; ik < nk; ++ik){

        if (!flag_done[ik]) {

            nk_inv = kpoint->knum_minus[ik];   

            for (is = 0; is < ns; ++is){
                for (js = 0; js < ns; ++js){
                    evec_tmp[js] = dynamical->evec_phonon[ik][is][js];
                }

                for (js = 0; js < ns; ++js){
                    dynamical->evec_phonon[nk_inv][is][js] = std::conj(evec_tmp[js]);
                }
            }

            flag_done[ik] = true;
            flag_done[nk_inv] = true;
        }
    }

    memory->deallocate(flag_done);
    memory->deallocate(evec_tmp);

    std::cout << "done !" << std::endl;
    std::cout << "**************************************" << std::endl;
}

double Relaxation::delta_lorentz(const double omega)
{
    return epsilon / (omega*omega + epsilon*epsilon) / pi;
}
