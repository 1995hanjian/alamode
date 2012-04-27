#pragma once

#include "pointers.h"
#include <vector>
#include <set>
#include <string>

namespace ALM_NS {

    class Fitting: protected Pointers {
    public:
        Fitting(class ALM *);
        ~Fitting();

        void fitmain();
       // int rank(const int, const int, double **);
        int rank(int, int, double *);
        // int getRankEigen(const int, const int,const  int);
     //   int constraint_mode;
      //  std::string fc2_file;

        double *params;

    private:

        int inprim_index(const int);
        void wrtfcs(const double *);
        void fit_without_constraints(int, int, int);
        void fit_with_constraints(int, int, int, int);
        void fit_consecutively(int, int, const int, const int,
            const int, const int, const int, const int);
        void calc_matrix_elements(const int, const int, const int, 
            const int, const int, const int, const int);
        double gamma(const int, const int *);
        int factorial(const int);       
      
        double **amat;
        double *fsum;       
    };

    extern "C" {
        
        void dgelss_(int *m, int *n, int *nrhs, double *a, int *lda,	
        double *b, int *ldb, double *s, double *rcond, int *rank,
        double *work, int *lwork, int *info);
       
        void dgglse_(int *m, int *n, int *p, double *a, int *lda,
        double *b, int *ldb, double *c, double *d, double *x,
        double *work, int *lwork, int *info);

        void dgesdd_(const char *jobz, int *m, int *n, double *a, int *lda,
        double *s, double *u, int *ldu, double *vt, int *ldvt, double *work,
        int *lwork, int *iwork, int *info);

        void dgeqrf_(int *m, int *n, double *a, double *tau,
            double *work, int *lwork, int *info);
    }

}