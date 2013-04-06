#pragma once

#include "phonons.h"

namespace PHON_NS {

    class Pointers {
    public:
        Pointers(PHON *ptr) :
          phon(ptr),
              memory(ptr->memory),
              error(ptr->error),
              input(ptr->input),
              system(ptr->system),
              symmetry(ptr->symmetry),
              kpoint(ptr->kpoint),
              integration(ptr->integration),
              fcs_phonon(ptr->fcs_phonon),
              dynamical(ptr->dynamical),
              phonon_velocity(ptr->phonon_velocity),
              phonon_thermodynamics(ptr->phonon_thermodynamics),
              relaxation(ptr->relaxation),
              conductivity(ptr->conductivity),
              writes(ptr->writes),
              dos(ptr->dos),
              gruneisen(ptr->gruneisen),
              mympi(ptr->mympi),
              timer(ptr->timer) {}
          virtual ~Pointers(){}
    protected:
        PHON *phon;
        Memory *&memory;
        Error *&error;
        Input *&input;
        System *&system;
        Symmetry *&symmetry;
        Kpoint *&kpoint;
        Integration *&integration;
        Fcs_phonon *&fcs_phonon;
        Dynamical *&dynamical;
        Phonon_velocity *&phonon_velocity;
        Phonon_thermodynamics *&phonon_thermodynamics;
        Relaxation *&relaxation;
        Conductivity *&conductivity;
        Writes *&writes;
        Dos *&dos;
        Gruneisen *&gruneisen;
        MyMPI *&mympi;
        Timer *&timer;
    };
}
