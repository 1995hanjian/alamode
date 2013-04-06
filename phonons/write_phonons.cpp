#include "write_phonons.h"
#include "system.h"
#include "dynamical.h"
#include "gruneisen.h"
#include "kpoint.h"
#include "parsephon.h"
#include "error.h"
#include "phonon_dos.h"
#include "phonon_velocity.h"
#include "../alm_c++/constants.h"
#include "memory.h"
#include <iomanip>
#include <fstream>
#include "relaxation.h"
#include "phonon_thermodynamics.h"

using namespace PHON_NS;

Writes::Writes(PHON *phon): Pointers(phon){
    Ry_to_kayser = Hz_to_kayser / time_ry;
};

Writes::~Writes(){};

void Writes::write_phonon_info()
{
    if (nbands < 0 || nbands > 3 * system->natmin) {
        std::cout << "nbands < 0 or nbands > 3 * natmin" << std::endl;
        std::cout << "All modes will be printed." << std::endl;    
        nbands =  3 * system->natmin;
    }

    if(kpoint->kpoint_mode == 1){
        write_phonon_bands();
        write_phonon_vel();
    }

    if(dos->flag_dos) {
        write_phonon_dos();
        write_thermodynamics();
        write_phonon_vel_all();
    }

    if(writeanime) {
        write_mode_anime();
    }

    if(dynamical->eigenvectors) {
        write_eigenvectors();
    }
}

void Writes::write_phonon_bands()
{
    std::ofstream ofs_bands;

    file_bands = input->job_title + ".bands";
    ofs_bands.open(file_bands.c_str(), std::ios::out);
    if(!ofs_bands) error->exit("write_phonon_bands", "cannot open file_bands");

    unsigned int i, j;

    unsigned int nk = kpoint->nk;

    double *kaxis = kpoint->kaxis;
    double **eval = dynamical->eval_phonon;

    ofs_bands << "# k-axis, Eigenvalues [cm^-1]" << std::endl;

    for (i = 0; i < nk; ++i){
        ofs_bands << std::setw(8) << std::fixed << kaxis[i];
        for (j = 0; j < nbands; ++j){
            ofs_bands << std::setw(15) << std::scientific << in_kayser(eval[i][j]);
        }
        ofs_bands << std::endl;
    }

    ofs_bands.close();
}

void Writes::write_phonon_vel()
{
    std::ofstream ofs_vel;

    file_vel = input->job_title + ".phvel";
    ofs_vel.open(file_vel.c_str(), std::ios::out);
    if(!ofs_vel) error->exit("write_phonon_vel", "cannot open file_vel");

    unsigned int i, j;
    unsigned int nk = kpoint->nk;

    double *kaxis = kpoint->kaxis;
    double Ry_to_SI_vel = Bohr_in_Angstrom*1.0e-10/time_ry;

    ofs_vel << "# k-axis, |Velocity| [m / sec]" << std::endl;
    ofs_vel.setf(std::ios::fixed);

    for (i = 0; i < nk; ++i){
        ofs_vel << std::setw(8) << kaxis[i];
        for (j = 0; j < nbands; ++j){
            ofs_vel << std::setw(15) << std::abs(phonon_velocity->phvel[i][j]*Ry_to_SI_vel);
        }
        ofs_vel << std::endl;
    }

    ofs_vel.close();
}

void Writes::write_phonon_vel_all()
{
    std::ofstream ofs_vel;

    file_vel = input->job_title + ".phvel_all";
    ofs_vel.open(file_vel.c_str(), std::ios::out);
    if(!ofs_vel) error->exit("write_phonon_vel_all", "cannot open file_vel_all");

    unsigned int i, j, k;
    unsigned int nk = kpoint->nk;
    unsigned int ns = dynamical->neval;

    double **eval = dynamical->eval_phonon;

    double Ry_to_SI_vel = Bohr_in_Angstrom*1.0e-10/time_ry;
    double **vel;

    memory->allocate(vel, ns, 3);

    ofs_vel << "# Frequency [cm^-1], |Velocity| [m / sec]" << std::endl;
    ofs_vel.setf(std::ios::fixed);

    for (i = 0; i < nk; ++i){

        ofs_vel << "# ik = " << std::setw(8);
        for (j = 0; j < 3; ++j){
            ofs_vel << std::setw(15) << kpoint->xk[i][j];
        }
        ofs_vel << std::endl;

        phonon_velocity->phonon_vel_k(kpoint->xk[i], vel);

        for (j = 0; j < ns; ++j){
            system->rotvec(vel[j], vel[j], system->lavec_p, 'T');
            for (k = 0; k < 3; ++k) vel[j][k] /= 2.0 * pi;
        }

        for (j = 0; j < ns; ++j){
            ofs_vel << std::setw(5) << i;
            ofs_vel << std::setw(5) << j;
            ofs_vel << std::setw(15) << in_kayser(eval[i][j]);
            ofs_vel << std::setw(15) << std::sqrt(std::pow(vel[j][0], 2) + std::pow(vel[j][1], 2) + std::pow(vel[j][2], 2))*Ry_to_SI_vel;
            ofs_vel << std::endl;
        }
        ofs_vel << std::endl;
    }

    ofs_vel.close();

    memory->deallocate(vel);
}

void Writes::write_phonon_dos()
{
    int i, iat;
    std::ofstream ofs_dos;

    file_bands = input->job_title + ".dos";
    ofs_dos.open(file_bands.c_str(), std::ios::out);
    if(!ofs_dos) error->exit("write_phonon_dos", "cannot open file_dos");

    ofs_dos << "# Energy [cm^-1], TOTAL-DOS";
    if (dynamical->eigenvectors){
        ofs_dos << ", Atom Projected-DOS";   
    }
    ofs_dos << std::endl;
    ofs_dos.setf(std::ios::scientific);

    for (i = 0; i < dos->n_energy; ++i){
        ofs_dos << std::setw(15) << dos->energy_dos[i] << std::setw(15) << dos->dos_phonon[i];
        if(dynamical->eigenvectors) {
            for (iat = 0; iat < system->natmin; ++iat){
                ofs_dos << std::setw(15) << dos->pdos_phonon[iat][i];
            }
        }
        ofs_dos << std::endl;
    } 
    ofs_dos.close();

    std::cout << std::endl << "Total DOS ";
    if(dynamical->eigenvectors) {
        std::cout << "and atom projected-DOS ";
    }
    std::cout << "are printed in the file: " << file_bands << std::endl << std::endl;
}

void Writes::write_mode_anime()
{
    std::ofstream ofs_anime;

    file_anime = input->job_title + ".axsf";
    ofs_anime.open(file_anime.c_str(), std::ios::out);
    if(!ofs_anime) error->exit("write_mode_anime", "cannot open file_anime");

    ofs_anime.setf(std::ios::scientific);

    unsigned int i, j, k;
    unsigned int natmin = system->natmin;
    unsigned int nk = kpoint->nk;

    double force_factor = 100.0;

    double **xmod;
    std::string *kd_tmp;

    memory->allocate(xmod, natmin, 3);
    memory->allocate(kd_tmp, natmin);

    ofs_anime << "ANIMSTEPS " << nbands * nk << std::endl;
    ofs_anime << "CRYSTAL" << std::endl;
    ofs_anime << "PRIMVEC" << std::endl;

    for (i = 0; i < 3; ++i){
        for (j = 0; j < 3; ++j){
            ofs_anime << std::setw(15) << system->lavec_p[j][i]*Bohr_in_Angstrom;
        }
        ofs_anime << std::endl;
    }

    for (i = 0; i < natmin; ++i){
        k = system->map_p2s[i][0];
        for (j = 0; j < 3; ++j){
            xmod[i][j] = system->xc[k][j];
        }
        // system->rotvec(system->lavec_p, xmod[i], xmod[i]);

        for (j = 0; j < 3; ++j){
            xmod[i][j] *= Bohr_in_Angstrom;
        }
        kd_tmp[i] = system->symbol_kd[system->kd[k]];
    }

    unsigned int ik, imode;
    double norm;
    std::complex<double> evec_tmp;
    unsigned int m;
    i = 0;
	

    for (ik = 0; ik < nk; ++ik){
        for (imode = 0; imode < nbands; ++imode){
            ofs_anime << "PRIMCOORD " << std::setw(10) << i + 1 << std::endl;
            ofs_anime << std::setw(10) << natmin << std::setw(10) << 1 << std::endl;
            norm = 0.0;

            for (j = 0; j < 3 * natmin; ++j){
                evec_tmp = dynamical->evec_phonon[ik][imode][j];
                norm += std::pow(evec_tmp.real(), 2) + std::pow(evec_tmp.imag(), 2);
            }

            norm *= force_factor / static_cast<double>(natmin);

            for (j = 0; j < natmin; ++j){

                m = system->map_p2s[j][0];

                ofs_anime << std::setw(10) << kd_tmp[j];

                for (k = 0; k < 3; ++k){
                    ofs_anime << std::setw(15) << xmod[j][k];
                }
                for (k = 0; k < 3; ++k){
                    ofs_anime << std::setw(15) << dynamical->evec_phonon[ik][imode][3 * j + k].real() / (std::sqrt(system->mass[m]) * norm);
                }
                ofs_anime << std::endl;
            }

            ++i;
        }
    }


    memory->deallocate(xmod);
    memory->deallocate(kd_tmp);

    ofs_anime.close();
}

void Writes::write_eigenvectors()
{
    std::ofstream ofs_evec;
    file_evec = input->job_title + ".evec";
    ofs_evec.open(file_evec.c_str(), std::ios::out);
    if(!ofs_evec) error->exit("write_eigenvectors", "cannot open file_evec");

    ofs_evec.setf(std::ios::scientific);
    unsigned int i, j, k;

    ofs_evec << "Lattice vectors of the primitive lattice" << std::endl;

    for (i = 0; i < 3; ++i){
        for (j = 0; j < 3; ++j){
            ofs_evec << std::setw(15) << system->lavec_p[j][i];
        }
        ofs_evec << std::endl;
    }

    ofs_evec << std::endl;

    for (i = 0; i < 3; ++i){
        for (j = 0; j < 3; ++j){
            ofs_evec << std::setw(15) << system->rlavec_p[i][j];
        }
        ofs_evec << std::endl;
    }

    unsigned int nk = kpoint->nk;
    unsigned int neval = dynamical->neval;
    ofs_evec << "Modes and k-points information below" << std::endl;
    //    nbands = neval;
    ofs_evec << std::setw(10) << nbands;
    ofs_evec << std::setw(10) << nk << std::endl;

    for (i = 0; i < nk; ++i){
        ofs_evec << "#" << std::setw(10) << i + 1;
        for (j = 0; j < 3; ++j){
            ofs_evec << std::setw(15) << kpoint->xk[i][j];
        }
        ofs_evec << std::endl;
        for (j = 0; j < nbands; ++j){
            ofs_evec << std::setw(15) << dynamical->eval_phonon[i][j] << std::endl;

            for (k = 0; k < neval; ++k){
                ofs_evec << std::setw(15) << real(dynamical->evec_phonon[i][j][k]);
                ofs_evec << std::setw(15) << imag(dynamical->evec_phonon[i][j][k]) << std::endl;
            }
            ofs_evec << std::endl;
        }
        ofs_evec << std::endl;
    }
    ofs_evec.close();
}

double Writes::in_kayser(const double x)
{
    return x * Ry_to_kayser;
}

void Writes::write_thermodynamics()
{
    unsigned int i, NT;
    double Tmin = system->Tmin;
    double Tmax = system->Tmax;
    double dT = system->dT;

    double T, TD;
    std::string file_thermo;

    NT = static_cast<unsigned int>((Tmax - Tmin) / dT);

    std::ofstream ofs_thermo;
    file_thermo = input->job_title + ".thermo";
    ofs_thermo.open(file_thermo.c_str(), std::ios::out);
    if(!ofs_thermo) error->exit("write_thermodynamics", "cannot open file_cv");
    ofs_thermo << "# Temperature [K], Internal Energy [Ry], Heat Capacity / kB" << std::endl;

    TD = 1000.0;
    phonon_thermodynamics->Debye_T(Tmax, TD);
    std::cout << "TD = " << TD << std::endl;

    for (i = 0; i <= NT; ++i){
        T = Tmin + dT * static_cast<double>(i);
        //       phonon_thermodynamics->Debye_T(T, TD);

        ofs_thermo << std::setw(15) << T;
        ofs_thermo << std::setw(15) << phonon_thermodynamics->Internal_Energy(T);
        ofs_thermo << std::setw(15) << phonon_thermodynamics->Cv_tot(T) / k_Boltzmann << std::endl;
        //       ofs_cv << std::setw(15) << TD << std::endl;
    }

    ofs_thermo.close();
}

void Writes::write_gruneisen()
{

    if (kpoint->kpoint_mode == 1) {
    if (nbands < 0 || nbands > 3 * system->natmin) {
        std::cout << "WARNING: nbands < 0 or nbands > 3 * natmin" << std::endl;
        std::cout << "All modes will be printed." << std::endl;    
        nbands =  3 * system->natmin;
    }

    std::ofstream ofs_gruneisen;

    file_vel = input->job_title + ".gruneisen";
    ofs_gruneisen.open(file_vel.c_str(), std::ios::out);
    if(!ofs_gruneisen) error->exit("write_gruneisen", "cannot open file_vel");

    unsigned int i, j;
    unsigned int nk = kpoint->nk;

    double *kaxis = kpoint->kaxis;
    
    ofs_gruneisen << "# k-axis, gamma" << std::endl;
    ofs_gruneisen.setf(std::ios::fixed);

    for (i = 0; i < nk; ++i){
        ofs_gruneisen << std::setw(8) << kaxis[i];
        for (j = 0; j < nbands; ++j){
            ofs_gruneisen << std::setw(15) << gruneisen->gruneisen[i][j].real();
        }
        ofs_gruneisen << std::endl;
    }

    ofs_gruneisen.close();

    } else {
    
        std::ofstream ofs_gruall;
        std::string file_gruall;
        file_gruall = input->job_title + ".gru_all";
        ofs_gruall.open(file_gruall.c_str(), std::ios::out);
        if (!ofs_gruall) error->exit("write_gruneisen", "cannot open file_gruall");

        unsigned int i, j, k;
        unsigned int nk = kpoint->nk;
        unsigned int ns = dynamical->neval;

        ofs_gruall << "# knum, snum, omega [cm^-1], gruneisen parameter" << std::endl;

        for (i = 0; i < nk; ++i){
            ofs_gruall << "# knum = " << i;
            for (k = 0; k < 3; ++k) {
            ofs_gruall << std::setw(15)<< kpoint->xk[i][k];
            }
            ofs_gruall << std::endl;

            for (j = 0; j < ns; ++j){
                ofs_gruall << std::setw(5) << i;
                ofs_gruall << std::setw(5) << j;
                ofs_gruall << std::setw(15) << in_kayser(dynamical->eval_phonon[i][j]);
                ofs_gruall << std::setw(15) << gruneisen->gruneisen[i][j].real();
                ofs_gruall << std::endl;
            }
        }

        ofs_gruall.close();
    }
}
