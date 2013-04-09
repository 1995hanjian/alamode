#include "mpi_common.h"
#include <iostream>
#include "phonons.h"
#include "timer.h"
#include "parsephon.h"
#include "memory.h"
#include "error.h"
#include "gruneisen.h"
#include "system.h"
#include "symmetry_core.h"
#include "kpoint.h"
#include "fcs_phonon.h"
#include "dynamical.h"
#include "phonon_velocity.h"
#include "phonon_thermodynamics.h"
#include "write_phonons.h"
#include "phonon_dos.h"
#include "integration.h"
#include "relaxation.h"
#include "conductivity.h"
#include <omp.h>

using namespace PHON_NS;

PHON::PHON(int narg, char **arg, MPI_Comm comm)
{
    mympi = new MyMPI(this, comm);

	restart_flag = false;

    if (mympi->my_rank == 0) {
		std::cout << "Phonons program version 1.0 (MPI)" << std::endl;
        std::cout << std::endl << "Job started at " << timer->DataAndTime() <<  std::endl << std::endl;

		std::cout << "The number of MPI threads: " << mympi->nprocs << std::endl;
		std::cout << "The number of OpenMP threads: " << omp_get_num_threads() << std::endl;
		std::cout << std::endl;
    }

    input = new Input(this, narg, arg);
    create_pointers();

    if (mympi->my_rank == 0) {
        input->parce_input();
    }

    mympi->MPI_Bcast_string(input->job_title, 0, MPI_COMM_WORLD);
    mympi->MPI_Bcast_string(mode, 0, MPI_COMM_WORLD);

    if (mode == "phonons") {

        system->setup();
        symmetry->setup_symmetry();
        kpoint->kpoint_setups();
       
        fcs_phonon->setup(mode);
        dynamical->setup_dynamical(mode);
        dos->setup();
        
        dynamical->diagonalize_dynamical_all();

        // Calculate the group velocity of phonons along given direction in
        // the reciprocal space.

        if (kpoint->kpoint_mode == 1)  {
            phonon_velocity->calc_phonon_vel_band();
        }

        if (dos->flag_dos) {
            integration->setup_integration();
            dos->calc_dos();
        }

        if (mympi->my_rank == 0) {
            writes->write_phonon_info();
        }

        memory->deallocate(dynamical->evec_phonon);
        memory->deallocate(dynamical->eval_phonon);

        if (kpoint->kpoint_mode == 1) {
            memory->deallocate(phonon_velocity->phvel);
        }

        if (dos->flag_dos) {
            integration->finish_integration();
        }

    } else if (mode == "boltzmann") {

        system->setup();
        symmetry->setup_symmetry();
        kpoint->kpoint_setups();
		dos->setup();
        fcs_phonon->setup(mode);
        dynamical->setup_dynamical(mode);
        dynamical->diagonalize_dynamical_all();

        integration->setup_integration();
		relaxation->setup_relaxation();

        //     dos->calc_tdos();

       // relaxation->calc_selfenergy();

	//	relaxation->v3_test();

        conductivity->setup_kl();
        conductivity->calc_kl();
    
        integration->finish_integration();
        relaxation->finish_relaxation();
        conductivity->finish_kl();

    } else if (mode == "gruneisen") {
        system->setup();

        kpoint->kpoint_setups();
		dos->setup();
        fcs_phonon->setup(mode);
        dynamical->setup_dynamical(mode);
        dynamical->diagonalize_dynamical_all();
        gruneisen->setup();
        gruneisen->calc_gruneisen();
     //   gruneisen->calc_gruneisen2();
        writes->write_gruneisen();
        gruneisen->finish_gruneisen();

    } else {
        error->exit("phonons", "invalid mode");
    }

    destroy_pointers();

    if (mympi->my_rank == 0) {
        std::cout << std::endl << "Job finished at " << timer->DataAndTime() << std::endl;
	    std::cout << "Bye! :)" << std::endl;
    }

}

PHON::~PHON(){
    delete input;
    delete mympi;
}

void PHON::create_pointers()
{
    memory = new Memory(this);
    timer = new Timer(this);
    error = new Error(this);
    system = new System(this);
    symmetry = new Symmetry(this);
    kpoint = new Kpoint(this);
    fcs_phonon = new Fcs_phonon(this);
    dynamical = new Dynamical(this);
    integration = new Integration(this);
    phonon_velocity = new Phonon_velocity(this);
    phonon_thermodynamics = new Phonon_thermodynamics(this);
    relaxation = new Relaxation(this);
    conductivity = new Conductivity(this);
    writes = new Writes(this);
    dos = new Dos(this);
    gruneisen = new Gruneisen(this);
}

void PHON::destroy_pointers()
{
    delete memory;
    delete timer;
    delete error;
    delete system;
    delete symmetry;
    delete kpoint;
    delete fcs_phonon;
    delete dynamical;
    delete integration;
    delete phonon_velocity;
    delete phonon_thermodynamics;
    delete relaxation;
    delete conductivity;
    delete writes;
    delete dos;
    delete gruneisen;
}
