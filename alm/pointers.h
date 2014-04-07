/*
 pointers.h

 Copyright (c) 2014 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory 
 or http://opensource.org/licenses/mit-license.php for information.
*/

#pragma once

#include "alamode.h"

namespace ALM_NS {

    class Pointers {
    public:
        Pointers(ALM *ptr) :
            alm(ptr),
            memory(ptr->memory),
            input(ptr->input),
            system(ptr->system),
            interaction(ptr->interaction),
            symmetry(ptr->symmetry),
            fitting(ptr->fitting),
            constraint(ptr->constraint),
            files(ptr->files),
            error(ptr->error),
            displace(ptr->displace),
            fcs(ptr->fcs),
            writes(ptr->writes),
            timer(ptr->timer) {}
        virtual ~Pointers(){}
    protected:
        ALM *alm;
        Memory *&memory;
        Input *&input;
        System *&system;
        Interaction *&interaction;
        Fcs *&fcs;
        Symmetry *&symmetry;
        Fitting *&fitting;
        Constraint *&constraint;
        Files *&files;
        Displace *&displace;
        Writes *&writes;
        Error *&error;
        Timer *&timer;
    };
}
