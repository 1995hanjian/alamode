#include "mpi_common.h"
#include "dynamical.h"
#include "system.h"
#include "memory.h"
#include "kpoint.h"
#include "fcs_phonon.h"
#include <complex>
#include <vector>
#include "../alm_c++/constants.h"
#include "fcs_phonon.h"
#include <iomanip>
#include <fstream>
#include "timer.h"
#include "error.h"

using namespace PHON_NS;

Dynamical::Dynamical(PHON *phon): Pointers(phon){
	eigenvectors = false;
}

Dynamical::~Dynamical(){
	memory->deallocate(xshift_s);
	memory->deallocate(kvec_na);

	if (nonanalytic) {
		memory->deallocate(borncharge);
	}
}

void Dynamical::setup_dynamical(std::string mode)
{
	int i;
	int ix, iy, iz;
	int icell = 0;

	neval = 3 * system->natmin;
	UPLO = 'U';

	MPI_Bcast(&eigenvectors, 1, MPI_LOGICAL, 0, MPI_COMM_WORLD);

	memory->allocate(xshift_s, 27, 3);

	for (i = 0; i < 3; ++i) xshift_s[0][i] = 0.0;

	for (ix = -1; ix <= 1; ++ix) {
		for (iy = -1; iy <= 1; ++iy) {
			for (iz = -1; iz <= 1; ++iz) {
				if (ix == 0 && iy == 0 && iz == 0) continue;

				++icell;

				xshift_s[icell][0] = static_cast<double>(ix);
				xshift_s[icell][1] = static_cast<double>(iy);
				xshift_s[icell][2] = static_cast<double>(iz);
			}
		}
	}

	MPI_Bcast(&nonanalytic, 1, MPI_LOGICAL, 0, MPI_COMM_WORLD);
	memory->allocate(kvec_na, kpoint->nk, 3);

	if (nonanalytic) {
		if (mympi->my_rank == 0) {
			std::cout << std::endl;
			std::cout << " NONANALYTIC = 1 : Non-analytic part of the dynamical matrix will be considered. " << std::endl;
			std::cout << std::endl;
		}
		memory->allocate(borncharge, system->natmin, 3, 3);

		if (mympi->my_rank == 0) load_born();

		MPI_Bcast(&dielec[0][0], 9, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		MPI_Bcast(&borncharge[0][0][0], 9*system->natmin, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		MPI_Bcast(&na_sigma, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

		if (mympi->my_rank == 0) {
			std::cout << std::endl;
			std::cout << "Damping factor for the non-analytic term: " << na_sigma << std::endl;
			std::cout << std::endl;
		}

		setup_na_kvec();
	}
}

void Dynamical::eval_k(double *xk_in, double *kvec_in, double ****fc2_in, double *eval_out, std::complex<double> **evec_out, bool require_evec) {

	// Calculate phonon energy for the specific k-point given in fractional basis

	unsigned int i, j;

	std::complex<double> **dymat_k;
	double **dymat_na_k;

	memory->allocate(dymat_k, neval, neval);

	calc_analytic_k(xk_in, fc2_in, dymat_k);

	if (nonanalytic) {
		memory->allocate(dymat_na_k, neval, neval);
		calc_nonanalytic_k(xk_in, kvec_in, dymat_na_k);

		for (i = 0; i < neval; ++i) {
			for (j = 0; j < neval; ++j) {
				dymat_k[i][j] += dymat_na_k[i][j];
			}
		}
		memory->deallocate(dymat_na_k);
	}

	/*
	// Hermitize the dynamical matrix

	std::complex<double> **dymat_tmp, **dymat_transpose;

	memory->allocate(dymat_tmp, neval, neval);
	memory->allocate(dymat_transpose, neval, neval);

	for (i = 0; i < neval; ++i){
	for (j = 0; j < neval; ++j){
	dymat_tmp[i][j] = dymat_k[i][j];
	dymat_transpose[i][j] = dymat_k[j][i];
	}
	}
	for (i = 0; i < neval; ++i){
	for (j = 0; j < neval; ++j){
	dymat_k[i][j] = 0.5 * (dymat_tmp[i][j] + std::conj(dymat_transpose[i][j]));
	}
	}

	memory->deallocate(dymat_tmp);
	memory->deallocate(dymat_transpose);
	*/

	char JOBZ;
	int INFO, LWORK;
	double *RWORK;
	std::complex<double> *WORK;

	LWORK = (2 * neval - 1) * 10;
	memory->allocate(RWORK, 3*neval - 2);
	memory->allocate(WORK, LWORK);

	std::complex<double> *amat;
	memory->allocate(amat, neval * neval);

	unsigned int k = 0;
	int n = dynamical->neval;
	for(i = 0; i < neval; ++i){
		for (j = 0; j < neval; ++j){
			amat[k++] = dymat_k[i][j];
		}
	}

	memory->deallocate(dymat_k);

	if (require_evec) {
		JOBZ = 'V';
	} else {
		JOBZ = 'N';
	}

	// Perform diagonalization
	zheev_(&JOBZ, &UPLO, &n, amat, &n, eval_out, WORK, &LWORK, RWORK, &INFO);

	if (eigenvectors && require_evec){
		k = 0;
		for(i = 0; i < neval; ++i){
			for (j = 0; j < neval; ++j){
				evec_out[i][j] = amat[k++];
			}
		}
	}

	memory->deallocate(RWORK);
	memory->deallocate(WORK);
	memory->deallocate(amat);
}

void Dynamical::eval_k(double *xk_in, double *kvec_in, std::vector<FcsClassExtent> fc2_ext, double *eval_out, std::complex<double> **evec_out, bool require_evec) {

	// Calculate phonon energy for the specific k-point given in fractional basis

	unsigned int i, j;

	std::complex<double> **dymat_k;
	double **dymat_na_k;

	memory->allocate(dymat_k, neval, neval);

	calc_analytic_k(xk_in, fc2_ext, dymat_k);

	if (nonanalytic) {
		memory->allocate(dymat_na_k, neval, neval);
		calc_nonanalytic_k(xk_in, kvec_in, dymat_na_k);

		for (i = 0; i < neval; ++i) {
			for (j = 0; j < neval; ++j) {
				dymat_k[i][j] += dymat_na_k[i][j];
			}
		}
		memory->deallocate(dymat_na_k);
	}

	char JOBZ;
	int INFO, LWORK;
	double *RWORK;
	std::complex<double> *WORK;

	LWORK = (2 * neval - 1) * 10;
	memory->allocate(RWORK, 3*neval - 2);
	memory->allocate(WORK, LWORK);

	std::complex<double> *amat;
	memory->allocate(amat, neval * neval);

	unsigned int k = 0;
	int n = dynamical->neval;
	for(i = 0; i < neval; ++i){
		for (j = 0; j < neval; ++j){
			amat[k++] = dymat_k[i][j];
		}
	}

	memory->deallocate(dymat_k);

	if (require_evec) {
		JOBZ = 'V';
	} else {
		JOBZ = 'N';
	}

	// Perform diagonalization
	zheev_(&JOBZ, &UPLO, &n, amat, &n, eval_out, WORK, &LWORK, RWORK, &INFO);

	if (eigenvectors && require_evec){
		k = 0;
		for(i = 0; i < neval; ++i){
			for (j = 0; j < neval; ++j){
				evec_out[i][j] = amat[k++];
			}
		}
	}

	memory->deallocate(RWORK);
	memory->deallocate(WORK);
	memory->deallocate(amat);
}

void Dynamical::calc_analytic_k(double *xk_in, double ****fc2_in, std::complex<double> **dymat_out) {

	unsigned int i, j;
	unsigned int icrd, jcrd;
	unsigned int itran;
	unsigned int ntran = system->ntran;
	unsigned int natmin = system->natmin;
	unsigned int atm_p1, atm_p2, atm_s2;

	double phase;
	double vec[3];
	std::complex<double> ctmp[3][3];
	std::complex<double> im(0.0, 1.0);
	std::complex<double> exp_phase;


	for (i = 0; i < natmin; ++i){

		atm_p1 = system->map_p2s[i][0];

		for (j = 0; j < natmin; ++j){

			atm_p2 = system->map_p2s[j][0];

			for(icrd = 0; icrd < 3; ++icrd){
				for(jcrd = 0; jcrd < 3; ++jcrd){
					ctmp[icrd][jcrd] = std::complex<double>(0.0, 0.0);
				}
			}

			for(itran = 0; itran < ntran; ++itran){
				atm_s2 =system->map_p2s[j][itran];

				for(icrd = 0; icrd < 3; ++icrd){
					if (system->cell_dimension[icrd] == 1) {
						vec[icrd] = system->xr_s[atm_p1][icrd] - system->xr_s[atm_s2][icrd];
						if (std::abs(vec[icrd]) < 0.5) {
							vec[icrd] = 0.0;
						} else {
							if (system->xr_s[atm_p1][icrd] < 0.5) {
								vec[icrd] = 1.0;
							} else {
								vec[icrd] = -1.0;
							}
						}
					} else if (system->cell_dimension[icrd] == 2) {
						vec[icrd] = system->xr_s[atm_p2][icrd] - system->xr_s[atm_s2][icrd];
						vec[icrd] = fold(vec[icrd]);
						if (std::abs(system->xr_s[atm_p1][icrd] - system->xr_s[atm_s2][icrd]) > 0.5) vec[icrd] *= -1.0;
					} else {
						//                     vec[icrd] = system->xr_s[atm_p2][icrd] - system->xr_s[atm_s2][icrd];
						vec[icrd] = system->xr_s[atm_p1][icrd] - system->xr_s[atm_s2][icrd];
						vec[icrd] = fold(vec[icrd]);

						vec[icrd] += system->xr_s[atm_p2][icrd] - system->xr_s[atm_p1][icrd];
					}
				}

				system->rotvec(vec, vec, system->lavec_s);
				system->rotvec(vec, vec, system->rlavec_p);

				phase = vec[0] * xk_in[0] + vec[1] * xk_in[1] + vec[2] * xk_in[2];
				exp_phase = std::exp(im * phase);

				// 
				//                  if (std::abs(exp_phase.real()) < 1.0e-14) {
				//                      exp_phase = std::complex<double>(0.0, exp_phase.imag());
				//                  }
				//                  if (std::abs(exp_phase.imag()) < 1.0e-14) {
				//                      exp_phase = std::complex<double>(exp_phase.real(), 0.0);
				//              }


				for (icrd = 0; icrd < 3; ++icrd){
					for (jcrd = 0; jcrd < 3; ++jcrd){
						ctmp[icrd][jcrd] += fc2_in[i][atm_s2][icrd][jcrd] * exp_phase;
					}
				}
			}

			for (icrd = 0; icrd < 3; ++icrd){
				for (jcrd = 0; jcrd < 3; ++jcrd){
					dymat_out[3 * i + icrd][3 * j + jcrd] = ctmp[icrd][jcrd] / std::sqrt(system->mass[atm_p1] * system->mass[atm_p2]);
				}
			}
		}
	}

#ifdef _DEBUG

	// Check the Hermiticity of the dynamical matrix.

	double res = 0.0;

	for (i = 0; i < neval; ++i) {
		for (j = 0; j < neval; ++j) {

			res += std::norm(dymat_out[i][j] - std::conj(dymat_out[j][i]));
		}
	}

	if (std::sqrt(res)/static_cast<double>(neval) > eps12) {
		std::cout << "xk = " << xk_in[0] << " " << xk_in[1] << " " << xk_in[2] << std::endl;
		error->warn("calc_analytic_k", "Dynamical matrix is not hermitian");
	}
	//         std::cout << "D" << std::endl;
	//         for (icrd = 0; icrd < 6; ++icrd){
	//             for (jcrd = 0; jcrd < 6; ++jcrd){
	//                 std::cout << "(" << std::setw(15) << std::scientific << dymat_out[icrd][jcrd].real();
	//                 std::cout << std::setw(15) << std::scientific << dymat_out[icrd][jcrd].imag() << ") ";
	//             }
	//             std::cout << std::endl;
	//         }
	//         std::cout << "D - D^{\\dagger}" << std::endl;
	//         for (icrd = 0; icrd < 6; ++icrd){
	//             for (jcrd = 0; jcrd < 6; ++jcrd){
	//                 std::cout << "(" << std::setw(15) << std::scientific << dymat_out[icrd][jcrd].real() - dymat_out[jcrd][icrd].real() ;
	//                 std::cout << std::setw(15) << std::scientific << dymat_out[icrd][jcrd].imag() + dymat_out[jcrd][icrd].imag() << ") ";
	//             }
	//             std::cout << std::endl;
	//         }
#endif
}


void Dynamical::calc_analytic_k(double *xk_in, std::vector<FcsClassExtent> fc2_in, std::complex<double> **dymat_out)
{
	int i, j;
	unsigned int atm1_s, atm2_s;
	unsigned int atm1_p, atm2_p;
	unsigned int xyz1, xyz2;
	unsigned int icell;

	double vec[3];
	std::complex<double> phase;
	std::complex<double> im(0.0, 1.0);

	std::complex<double> **ctmp;

	memory->allocate(ctmp, 3*system->natmin, 3*system->natmin);

	for (i = 0; i < 3*system->natmin; ++i) {
		for (j = 0; j < 3*system->natmin; ++j) {
			dymat_out[i][j] = std::complex<double>(0.0, 0.0);
		}
	}

	for (std::vector<FcsClassExtent>::const_iterator it = fc2_in.begin(); it != fc2_in.end(); ++it) {

		atm1_p = (*it).atm1;
		atm2_s = (*it).atm2;
		xyz1 = (*it).xyz1;
		xyz2 = (*it).xyz2;

		icell = (*it).cell_s;

		atm1_s = system->map_p2s[atm1_p][0];

		for (i = 0; i < 3; ++i) {
			vec[i] = system->xr_s[atm1_s][i] - system->xr_s[atm2_s][i];
			vec[i] -= xshift_s[icell][i];
		}

		system->rotvec(vec, vec, system->lavec_s);
		system->rotvec(vec, vec, system->rlavec_p);

		phase = vec[0] * xk_in[0] + vec[1] * xk_in[1] + vec[2] * xk_in[2];

		atm2_p = system->map_s2p[atm2_s].atom_num;

		dymat_out[3 * atm1_p + xyz1][3 * atm2_p + xyz2] += (*it).fcs_val * std::exp(im * phase) / std::sqrt(system->mass[atm1_s] * system->mass[atm2_s]);
	}
}

void Dynamical::calc_nonanalytic_k(double *xk_in, double *kvec_na_in, double **dymat_na_out)
{
	unsigned int i, j;
	unsigned int iat, jat;
	unsigned int atm_p1, atm_p2;
	double kepsilon[3];
	double kz1[3], kz2[3];
	double denom, norm2;
	double born_tmp[3][3];
	double xk_tmp[3];
	double factor;

	for (i = 0; i < neval; ++i) {
		for (j = 0; j < neval; ++j) {
			dymat_na_out[i][j] = 0.0;
		}
	}

	system->rotvec(kepsilon, kvec_na_in, dielec);
	denom = kvec_na_in[0] * kepsilon[0] + kvec_na_in[1] * kepsilon[1] + kvec_na_in[2] * kepsilon[2];

	if (denom > eps) {

		for (iat = 0; iat < system->natmin; ++iat) {
			atm_p1 = system->map_p2s[iat][0];


			for (i = 0; i <3; ++i) {
				for (j = 0; j < 3; ++j) {
					born_tmp[i][j] = borncharge[iat][i][j];
				}
			}

			system->rotvec(kz1, kvec_na_in, born_tmp, 'T');

			for (jat = 0; jat < system->natmin; ++jat) {
				atm_p2 = system->map_p2s[jat][0];


				for (i = 0; i <3; ++i) {
					for (j = 0; j < 3; ++j) {
						born_tmp[i][j] = borncharge[jat][i][j];
					}
				}

				system->rotvec(kz2, kvec_na_in, born_tmp, 'T');

				for (i = 0; i < 3; ++i) {
					for (j = 0; j < 3; ++j) {

						dymat_na_out[3 * iat + i][3 * jat + j] = kz1[i] * kz2[j] / (denom * std::sqrt(system->mass[atm_p1] * system->mass[atm_p2]));

					}
				}
			}
		}
	}

	system->rotvec(xk_tmp, xk_in, system->rlavec_p, 'T');
	norm2 = xk_tmp[0] * xk_tmp[0] + xk_tmp[1] * xk_tmp[1] + xk_tmp[2] * xk_tmp[2];

	factor = 8.0 * pi / system->volume_p * std::exp(-norm2 / std::pow(na_sigma, 2));

	for (i = 0; i < neval; ++i) {
		for (j = 0; j < neval; ++j) {
			dymat_na_out[i][j] *= factor;
		}
	}
}

void Dynamical::diagonalize_dynamical_all()
{
	unsigned int ik, is;
	unsigned int nk = kpoint->nk;
	bool require_evec; 

	if (mympi->my_rank == 0) {
		std::cout << std::endl << "Diagonalizing dynamical matrices for all k-points ..." << std::endl;
	}

	memory->allocate(eval_phonon, nk, neval);

	if (eigenvectors) {
		require_evec = true;
		memory->allocate(evec_phonon, nk, neval, neval);
	} else {
		require_evec = false;
		memory->allocate(evec_phonon, nk, 1, 1);
	}

	// Calculate phonon eigenvalues and eigenvectors for all k-points

	for (ik = 0; ik < nk; ++ik){
		if (fcs_phonon->is_fc2_ext) {
			eval_k(kpoint->xk[ik], kvec_na[ik], fcs_phonon->fc2_ext, eval_phonon[ik], evec_phonon[ik], require_evec);
		} else {
			eval_k(kpoint->xk[ik], kvec_na[ik], fcs_phonon->fc2, eval_phonon[ik], evec_phonon[ik], require_evec);
		}

		// Phonon energy is the square-root of the eigenvalue 
		for (is = 0; is < neval; ++is){
			eval_phonon[ik][is] = freq(eval_phonon[ik][is]);
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);
	if (mympi->my_rank == 0) {
		timer->print_elapsed();
		std::cout << "done !" << std::endl;
	}

#ifdef _DEBUG


	// Check if D(k) - D(-k)^{*} is satisfied when KPMODE = 2.

	if (kpoint->kpoint_mode == 2) {
		int i, j;
		int nk_minus;
		std::complex<double> **dmat1, **dmat2;

		double res;
		// 	double *eval1, *eval2;
		// 		std::complex<double> **evec1, **evec2;
		double *xk1, *xk2;

		memory->allocate(dmat1, neval, neval);
		memory->allocate(dmat2, neval, neval);

		memory->allocate(xk1, 3);
		memory->allocate(xk2, 3);


		for (ik = 0; ik < nk; ++ik) {

			for (i = 0; i < 3; ++i) {
				xk1[i] = kpoint->xk[ik][i];
				xk2[i] = -kpoint->xk[ik][i];
			}

			nk_minus = kpoint->get_knum(xk2[0], xk2[1], xk2[2]);

			for (i = 0; i < 3; ++i) {
				xk2[i] = kpoint->xk[nk_minus][i];
			}

			// 		std::cout << "ik = " << ik << std::endl;
			// 		std::cout << "xk = " << xk[0] << " " << xk[1] << " " << xk[2] << std::endl;

			calc_analytic_k(xk1, fcs_phonon->fc2, dmat1);	  
			calc_analytic_k(xk2, fcs_phonon->fc2, dmat2);

			res = 0.0;

			for (i = 0; i < neval; ++i) {
				for (j = 0; j < neval; ++j) {

					res += std::norm(dmat1[i][j] - std::conj(dmat2[i][j]));
				}
			}

			if (std::sqrt(res)/static_cast<double>(neval) > eps12) {
				std::cout << "ik = " << ik << std::endl;
				error->warn("diagonalize_dynamical_all", "D(k) = D(-k)^{*} is not satisfied. This might imply a bug.");
			}


			// 	  std::cout << "D(k) - D(-k)^{*}" << std::endl;
			// 	  for (int icrd = 0; icrd < neval; ++icrd){
			// 		  for (int jcrd = 0; jcrd < neval; ++jcrd){
			// 			  std::cout << "(" << std::setw(15) << std::scientific << real(dmat1[icrd][jcrd] - std::conj(dmat2[icrd][jcrd]));
			// 			  std::cout << std::setw(15) << std::scientific << imag(dmat1[icrd][jcrd] - std::conj(dmat2[icrd][jcrd])) << ") ";
			// 		  }
			// 		  std::cout << std::endl;
			// 	  }
			// 	  std::cout << "Compare eigenvectors" << std::endl;
			// 	  for (int icrd = 0; icrd < neval; ++icrd) {
			// 		 for (int jcrd = 0; jcrd < neval; ++jcrd) {
			// 			 std::cout << evec1[icrd][jcrd] << " " << evec2[icrd][jcrd] << " " << evec1[icrd][jcrd] - std::conj(evec2[icrd][jcrd]) << std::endl;
			// 		 }
			// 		 std::cout << std::endl;
			// 	}
			// 	}

		}
		memory->deallocate(dmat1);
		memory->deallocate(dmat2);
		memory->deallocate(xk1);
		memory->deallocate(xk2);

	}
#endif
}


void Dynamical::load_born()
{
	// Read the dielectric tensor and born effective charges from file_born

	std::ifstream ifs_born;
	double sum_born[3][3];
	double res;

	ifs_born.open(file_born.c_str(), std::ios::in);
	if (!ifs_born) error->exit("load_born", "cannot open file_born");

	unsigned int i, j, k;

	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j) {
			ifs_born >> dielec[i][j];
		}
	}

	for (i = 0; i < system->natmin; ++i) {
		for (j = 0; j < 3; ++j) {
			for (k = 0; k < 3; ++k) {
				ifs_born >> borncharge[i][j][k];
			}
		}
	}

	std::cout << "Dielectric constant tensor in Cartesian coordinate" << std::endl;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j) {
			std::cout << std::setw(15) << dielec[i][j];
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
	std::cout << "Born effective charge tensor in Cartesian coordinate" << std::endl;
	for (i = 0; i < system->natmin; ++i) {
		std::cout << "Atom" << std::setw(5) << i + 1 << "(" << std::setw(3) << system->symbol_kd[i] << ") :" << std::endl;
		for (j = 0; j < 3; ++j) {
			for (k = 0; k < 3; ++k) {
				std::cout << std::setw(15) << borncharge[i][j][k];
			}
			std::cout << std::endl;
		}
	}

	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j) {
			sum_born[i][j] = 0.0;
		}
	}

	for (i = 0; i < system->natmin; ++i) {
		for (j = 0; j < 3; ++j) {
			for (k = 0; k < 3; ++k) {
				sum_born[j][k] += borncharge[i][j][k];
			}
		}
	}

	res = 0.0;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j) {
			res += std::pow(sum_born[i][j], 2);
		}
	}

	if (res > eps10) {
		std::cout << std::endl;
		std::cout << "WARNING: Born effective charges do not satisfy the acoustic sum rule." << std::endl;
		std::cout << "         The born effective charges will be modified as follows." << std::endl;

		for (i = 0; i < system->natmin; ++i) {
			for (j = 0; j < 3; ++j) {
				for (k = 0; k < 3; ++k) {
					borncharge[i][j][k] -= sum_born[j][k] / static_cast<double>(system->natmin);
				}
			}
		}
		std::cout << std::endl;
		std::cout << "New Born effective charge tensor in Cartesian coordinate." << std::endl;
		for (i = 0; i < system->natmin; ++i) {
			std::cout << "Atom" << std::setw(5) << i + 1 << "(" << std::setw(3) << system->symbol_kd[i] << ") :" << std::endl;
			for (j = 0; j < 3; ++j) {
				for (k = 0; k < 3; ++k) {
					std::cout << std::setw(15) << borncharge[i][j][k];
				}
				std::cout << std::endl;
			}
		}
	}
}

void Dynamical::setup_na_kvec()
{
	// Setup k-vector necessary for the non-analytic correction.

	unsigned int i, j;
	unsigned int nk = kpoint->nk;
	double norm;

	if (kpoint->kpoint_mode == 0 || kpoint->kpoint_mode == 2) {

		// DOS or Boltzmann

		for (i = 0; i < kpoint->nk; ++i) {
			for (j = 0; j < 3; ++j) {
				kvec_na[i][j] = kpoint->xk[i][j];
			}
			system->rotvec(kvec_na[i], kvec_na[i], system->rlavec_p, 'T');
			norm = std::sqrt(kvec_na[i][0] * kvec_na[i][0] + kvec_na[i][1] * kvec_na[i][1] + kvec_na[i][2] * kvec_na[i][2]);

			if (norm > eps) {
				for (j = 0; j < 3; ++j) kvec_na[i][j] /= norm;
			}
		}

	} else if (kpoint->kpoint_mode == 1) {

		// Band structure calculation

		for (i = 0; i < kpoint->nk; ++i) {
			for (j = 0; j < 3; ++j) {
				kvec_na[i][j] = kpoint->kpoint_direction[i][j];
			}
		}
	}
}


void Dynamical::calc_dynamical_matrix()
{
	// Calculate dynamical matrix of all k-points

	neval = 3 * system->natmin;
	memory->allocate(dymat, kpoint->nk, neval, neval);
	calc_analytic();

	std::complex<double> **dymat_tmp, **dymat_transpose;
	memory->allocate(dymat_tmp, neval, neval);
	memory->allocate(dymat_transpose, neval, neval);

	unsigned int nk = kpoint->nk;
	unsigned int i, j, ik;

	// Hermitize dynamical matrix

	for (ik = 0; ik < nk; ++ik){
		for (i = 0; i < neval; ++i){
			for (j = 0; j < neval; ++j){
				dymat_tmp[i][j] = dymat[ik][i][j];
				dymat_transpose[i][j] = dymat[ik][j][i];
			}
		}
		for (i = 0; i < neval; ++i){
			for (j = 0; j < neval; ++j){
				dymat[ik][i][j] = 0.5 * (dymat_tmp[i][j] + std::conj(dymat_transpose[i][j]));
			}
		}
	}

	memory->deallocate(dymat_tmp);
	memory->deallocate(dymat_transpose);
}

void Dynamical::calc_analytic()
{
	unsigned int i, j, itran, ik;
	unsigned int icrd, jcrd;

	unsigned int natmin = system->natmin;
	unsigned int ntran = system->ntran;

	unsigned int atm_p1, atm_p2, atm_s2;

	unsigned int nk = kpoint->nk;

	std::complex<double> ***ctmp;
	std::complex<double> im(0.0, 1.0);
	std::complex<double> exp_phase;

	double vec[3];

	double *phase;

	memory->allocate(ctmp, nk, 3, 3);
	memory->allocate(phase, nk);

	for (i = 0; i < natmin; ++i){

		atm_p1 = system->map_p2s[i][0];

		for (j = 0; j < natmin; ++j){

			atm_p2 = system->map_p2s[j][0];

			for(ik = 0; ik < nk; ++ik){
				for(icrd = 0; icrd < 3; ++icrd){
					for(jcrd = 0; jcrd < 3; ++jcrd){
						ctmp[ik][icrd][jcrd] = std::complex<double>(0.0, 0.0);
					}
				}
			}

			for(itran = 0; itran < ntran; ++itran){
				atm_s2 =system->map_p2s[j][itran];

				for(icrd = 0; icrd < 3; ++icrd){
					vec[icrd] = system->xr_s[atm_p1][icrd] - system->xr_s[atm_s2][icrd];
					vec[icrd] = fold(vec[icrd]);
				}

				system->rotvec(vec, vec, system->lavec_s);
				system->rotvec(vec, vec, system->rlavec_p);

				for (ik = 0; ik < nk; ++ik){
					phase[ik] = vec[0] * kpoint->xk[ik][0] + vec[1] * kpoint->xk[ik][1] + vec[2] * kpoint->xk[ik][2];
					exp_phase = std::exp(im * phase[ik]);

					for (icrd = 0; icrd < 3; ++icrd){
						for (jcrd = 0; jcrd < 3; ++jcrd){
							ctmp[ik][icrd][jcrd] += fcs_phonon->fc2[i][atm_s2][icrd][jcrd] * exp_phase;
						}
					}
				}
			}

			for (ik = 0; ik < nk; ++ik){
				for (icrd = 0; icrd < 3; ++icrd){
					for (jcrd = 0; jcrd < 3; ++jcrd){
						dymat[ik][3 * i + icrd][3 * j + jcrd] = ctmp[ik][icrd][jcrd] / std::sqrt(system->mass[atm_p1] * system->mass[atm_p2]);
					}
				}
			}

		}
	}

#ifdef _DEBUG
	for(ik = 0; ik < nk; ++ik){
		std::cout << "D" << std::endl;
		for (icrd = 0; icrd < 6; ++icrd){
			for (jcrd = 0; jcrd < 6; ++jcrd){
				std::cout << "(" << std::setw(15) << std::scientific << real(dymat[ik][icrd][jcrd]);
				std::cout << std::setw(15) << std::scientific << imag(dymat[ik][icrd][jcrd]) << ") ";
			}
			std::cout << std::endl;
		}
		std::cout << "D - D^{\\dagger}" << std::endl;
		for (icrd = 0; icrd < 6; ++icrd){
			for (jcrd = 0; jcrd < 6; ++jcrd){
				std::cout << "(" << std::setw(15) << std::scientific << real(dymat[ik][icrd][jcrd]) -real(dymat[ik][jcrd][icrd]) ;
				std::cout << std::setw(15) << std::scientific << imag(dymat[ik][icrd][jcrd]) + imag(dymat[ik][jcrd][icrd]) << ") ";
			}
			std::cout << std::endl;
		}
	} 
#endif

	memory->deallocate(ctmp);
	memory->deallocate(phase);
}

void Dynamical::diagonalize_dynamical()
{
	char JOBZ, UPLO = 'U';
	int INFO, LWORK;
	double *RWORK;
	std::complex<double> *WORK;

	if(eigenvectors){
		JOBZ = 'V';
	} else {
		JOBZ = 'N';
	}

	LWORK = (2 * neval - 1) * 10;
	memory->allocate(RWORK, 3*neval - 2);
	memory->allocate(WORK, LWORK);

	double *eval;
	std::complex<double> *amat;

	memory->allocate(eval, neval);
	memory->allocate(amat, neval * neval);

	int n = neval;
	unsigned int i, j;
	unsigned int ik, k;

	unsigned int nk = kpoint->nk;

	memory->allocate(eval_phonon, nk, neval);

	for (ik = 0; ik < nk; ++ik){
		k = 0;
		for(i = 0; i < neval; ++i){
			for (j = 0; j < neval; ++j){
				amat[k++] = dymat[ik][i][j];
			}
		}

		zheev_(&JOBZ, &UPLO, &n, amat, &n, eval, WORK, &LWORK, RWORK, &INFO);

		// save eigenvalues

		for (i = 0; i < neval; ++i){
			eval_phonon[ik][i] = eval[i];
		}

		// save eigenvectors if necessary

		if(eigenvectors){
			k = 0;
			for(i = 0; i < neval; ++i){
				for (j = 0; j < neval; ++j){
					dymat[ik][i][j] = amat[k++];
				}
			}
		}
	}
	memory->deallocate(eval);
	memory->deallocate(amat);
}

double Dynamical::fold(double x)
{
	if (x >= -0.5 && x < 0.5) {
		return x;
	} else if (x < 0.0) {
		return x + 1.0;
	} else {
		return x - 1.0;
	}
}

double Dynamical::freq(const double x) 
{
	if (x >= 0.0) {
		return std::sqrt(x);
	} else {
		if (std::abs(x) < eps) {
			return std::sqrt(-x);
		} else { 
			return -std::sqrt(-x);
		}
	}
}
