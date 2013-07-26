#include <fstream>
#include <boost/lexical_cast.hpp>
#include "writes.h"
#include "system.h"
#include "interaction.h"
#include "memory.h"
#include "symmetry.h"
#include "error.h"
#include "ewald.h"
#include "files.h"
#include "fcs.h"
#include "fitting.h"
#include "constraint.h"


using namespace ALM_NS;

Writes::Writes(ALM *alm): Pointers(alm){}

Writes::~Writes() {}

void Writes::write_input_vars()
{
	unsigned int i;

	std::cout << std::endl;
	std::cout << "Input variables below:" << std::endl;
	std::cout << "---------------------------------------------------" << std::endl;
	std::cout << "General:" << std::endl;
	std::cout << " PREFIX = " << files->job_title << std::endl;
	std::cout << " NAT = " << system->nat << "; NKD = " << system->nkd << std::endl;
	std::cout << " NSYM = " << symmetry->nsym << "; NNP = " << symmetry->nnp << std::endl;
	std::cout << " KD = ";
	for (i = 0; i < system->nkd; ++i) std::cout << std::setw(4) << system->kdname[i];
	std::cout << std::endl;
	std::cout << " MASS = ";
	for (i = 0; i < system->nkd; ++i) std::cout << std::setw(8) << system->mass_kd[i];
	std::cout << std::endl;
	std::cout << " PERIODIC = ";
	for (i = 0; i < 3; ++i) std::cout << std::setw(3) << interaction->is_periodic[i];
	std::cout << std::endl;
	std::cout << " INTERTYPE = " << interaction->interaction_type << std::endl;
	std::cout << std::endl;

	std::cout << "Interaction:" << std::endl;
	std::cout << " NORDER = " << interaction->maxorder << std::endl;
	std::cout << " NBODY = ";
	for (i = 0; i < interaction->maxorder; ++i) std::cout << std::setw(3) << interaction->nbody_include[i];
	std::cout << std::endl;
	std::cout << " ILONG = " << ewald->is_longrange << "; FLONG = " << ewald->file_longrange << std::endl;
	std::cout << std::endl;

	std::cout << "Fitting:" << std::endl;
	std::cout << " DFILE = " << files->file_disp << std::endl;
	std::cout << " FFILE = " << files->file_force << std::endl;
	std::cout << " NDATA = " << system->ndata << "; NSTART = " << system->nstart << "; NEND = " << system->nend << "; NSKIP = " << system->nskip << std::endl;
	std::cout << " NBOOT = " << fitting->nboot << std::endl;
	std::cout << " MULTDAT = " << symmetry->multiply_data << std::endl;
	std::cout << " ICONST = " << constraint->constraint_mode << std::endl;
	std::cout << " ROTAXIS = " << constraint->rotation_axis << std::endl;
	std::cout << " FC2INFO = " << constraint->fc2_file << std::endl;
	std::cout << " REFINFO = " << symmetry->refsys_file << std::endl;
	std::cout << std::endl;
	std::cout << "---------------------------------------------------" << std::endl;
	std::cout << std::endl;

}

void Writes::writeall()
{
	wrtfcs();

	ofs_info.open(files->file_info.c_str(), std::ios::out);
	if(!ofs_info) error->exit("writeall", "cannot open file_info");

	wrtmisc();

	ofs_info.close();
}

void Writes::wrtfcs()
{
	int i, j, k, l, m;
	int iat, jat;
	unsigned int ui;

	int maxorder = interaction->maxorder;
	std::string *str_fcs;

	memory->allocate(str_fcs, maxorder);

	std::string str_tmp;

	std::ofstream ofs_fcs;
	ofs_fcs.open(files->file_fcs.c_str(), std::ios::out);
	if(!ofs_fcs) error->exit("openfiles", "cannot open fcs file");

	for (i = 0; i < maxorder; ++i){
		str_fcs[i] = "*FC" + boost::lexical_cast<std::string>(i + 2);
	}

	ofs_fcs <<  "********************Force Constants (FCs)********************" << std::endl;
	ofs_fcs <<  "!     Force Constants will be printed in atomic unit        !" << std::endl;
	ofs_fcs <<  "!     FC2: Ry/a0^2     FC3: Ry/a0^3     FC4: Ry/a0^4   etc. !" << std::endl;
	ofs_fcs <<  "!     FC?: Ry/a0^?                                          !" << std::endl;
	ofs_fcs <<  "!     a0= Bohr radius                                       !" << std::endl;
	ofs_fcs << "*************************************************************"  << std::endl << std::endl;

	ofs_fcs << "---------------Symmetrically Independent FCs---------------" << std::endl;
	ofs_fcs << " Indices (Global, Local)      FCs      Pairs       Distance (for IFC2)    Multiplicity (for IFC2)"<< std::endl;

	k = 0;

	ofs_fcs.setf(std::ios::scientific);

	for (i = 0; i < maxorder; ++i){

		m = 0;

		if(fcs->ndup[i].size() > 0) {

			ofs_fcs << std::endl << std::setw(6) << str_fcs[i] << std::endl;

			for (ui = 0; ui < fcs->ndup[i].size(); ++ui){

				ofs_fcs << std::setw(6) << k + 1 << std::setw(6) << ui + 1 << std::setw(16) <<  fitting->params[k];
				for (l = 0; l < i + 2; ++l){
					ofs_fcs << std::setw(7) << fcs->easyvizint(fcs->fc_set[i][m].elems[l]);    
				}
				if(i==0) {
					iat = fcs->fc_set[i][m].elems[0] / 3;
					jat = fcs->fc_set[i][m].elems[1] / 3;
					j = symmetry->map_s2p[iat].atom_num;
					ofs_fcs << std::setw(15) << interaction->distlist[fcs->fc_set[i][m].elems[0]/3][fcs->fc_set[i][m].elems[1]/3];
					ofs_fcs << std::setw(15) << interaction->mindist_pairs[j][jat].size();
				}
				ofs_fcs << std::endl;
				m += fcs->ndup[i][ui];
				++k;
			}
		}
	}

	ofs_fcs << std::endl;

	if (constraint->extra_constraint_from_symmetry) {
		ofs_fcs << "---------------Constraint from Crystal Symmetry---------------" << std::endl;
		for (i = 0; i < maxorder; ++i){
			int nparam = fcs->ndup[i].size();


			for (std::set<ConstraintClass>::iterator p = constraint->const_symmetry[i].begin(); p != constraint->const_symmetry[i].end(); ++p){
				ofs_fcs << "  0 = ";
				ConstraintClass const_pointer = *p;
				for (j = 0; j < nparam; ++j){
					if (std::abs(const_pointer.w_const[j]) > eps8) {
						str_tmp = "(FC" + boost::lexical_cast<std::string>(i + 2) + "_" + boost::lexical_cast<std::string>(j + 1) + ")";
						ofs_fcs << std::setw(15) << std::showpos << const_pointer.w_const[j];
						ofs_fcs << std::setw(12) << std::left << str_tmp;
					}
				}
				ofs_fcs << std::endl;
			}
			ofs_fcs << std::endl;
		}
		ofs_fcs << std::endl;  
	}

	ofs_fcs.unsetf(std::ios::showpos);

	for (i = 0; i < maxorder; ++i){
		str_fcs[i] = "**FC" + boost::lexical_cast<std::string>(i + 2);
	}

	ofs_fcs << std::endl << std::endl
		<< "---------------All FCs below---------------" << std::endl;

	int ip = 0;
	int id;

	for (i = 0; i < maxorder; ++i){

		id = 0;

		if(fcs->ndup[i].size() > 0){
			ofs_fcs << std::endl << std::setw(6) << str_fcs[i] << std::endl;

			for (unsigned int iuniq = 0; iuniq < fcs->ndup[i].size(); ++iuniq){

				str_tmp = "# FC" + boost::lexical_cast<std::string>(i + 2) + "_";
				str_tmp += boost::lexical_cast<std::string>(iuniq + 1);

				ofs_fcs << str_tmp << std::setw(6) << fcs->ndup[i][iuniq] << std::setw(16) << fitting->params[ip] << std::endl;

				for (j = 0; j < fcs->ndup[i][iuniq]; ++j){
					ofs_fcs << std::setw(5) << j + 1 << std::setw(16) << fcs->fc_set[i][id].coef;
					for (k = 0; k < i + 2; ++k){
						ofs_fcs << std::setw(6) << fcs->easyvizint(fcs->fc_set[i][id].elems[k]);
					}
					ofs_fcs << std::endl;
					++id;
				}
				ofs_fcs << std::endl;
				++ip;
			}

		}
	}

	memory->deallocate(str_fcs);
	ofs_fcs.close();

	std::cout << std::endl << "Force Constants are written to file: " << files->file_fcs << std::endl;
}

void Writes::wrtmisc(){

	// Write miscellaneous information to file_info 
	// for subsequent calculations (phonons, md, alm)

	int i, j, k, m;
	int iat;
	int ihead, order;
	unsigned int ui;

	ofs_info << "##SYSTEM INFO" << std::endl;
	ofs_info << "Lattice Vector (in Bohr unit)" << std::endl;
	for (j = 0; j < 3; ++j){
		for(i = 0; i < 3; ++i){
			ofs_info <<  std::setw(25) << std::setprecision(16) << system->lavec[i][j]; // Be careful to the transpose of (i,j)
		}
		ofs_info << std::endl;
	}

	ofs_info << "Atomic Species" << std::endl;
	ofs_info << std::setw(6) << system->nkd << std::endl;

	for(i = 0; i < system->nkd; ++i){
		ofs_info << std::setw(6) << i + 1 << std::setw(5) << system->kdname[i] << std::setw(20) << system->mass_kd[i] << std::endl;
	}
	ofs_info << "Translational Symmetry Information" << std::endl;
	ofs_info << std::setw(6) << system->nat << std::setw(6) << symmetry->natmin << std::setw(6) << symmetry->ntran << std::endl;
	ofs_info << std::setw(11) << "'Atoms'" << std::setw(11) << "'Species'" 
		<< std::setw(75) <<  "'Atomic Coordinates (Fractional)'                      " 
		<< std::setw(15) << "'TRANSLATION'" << std::setw(15) << "'INDEX IN THE CELL'" << std::endl;
	for(i = 0; i < system->nat; ++i){
		ofs_info << std::setw(11) << i + 1 << std::setw(11) << system->kd[i];
		for(j = 0; j < 3; ++j){
			ofs_info << std::scientific << std::setprecision(16) << std::setw(25) << system->xcoord[i][j];
		}
		ofs_info << std::setw(15) << symmetry->map_s2p[i].tran_num + 1 
			<< std::setw(15) << symmetry->map_s2p[i].atom_num + 1 << std::endl;
	}

	ofs_info << "##HARMONIC FORCE CONSTANTS" << std::endl;
	ofs_info << fcs->ndup[0].size() << std::endl;


	ihead = 0;
	k = 0;
	for (ui = 0; ui < fcs->ndup[0].size(); ++ui){

		ofs_info << std::scientific << std::setprecision(16) << std::setw(25) <<  fitting->params[k];
		for (i = 0; i < 2; ++i){
			ofs_info << std::setw(7) << fcs->fc_set[0][ihead].elems[i];    
		}
		ofs_info << std::endl;
		ihead += fcs->ndup[0][ui];
		++k;
	}

	ofs_info << "##INTERACTION LISTS" << std::endl;
	ofs_info << "Interaction List and Reference Vectors(Cartesian) for each order" << std::endl;

	if (interaction->interaction_type == 0 || interaction->interaction_type == 1) {
		for (order = 0; order < interaction->maxorder; ++order){
			ofs_info << "#LIST_" + interaction->str_order[order] << std::endl;

			for (k = 0; k < symmetry->natmin; ++k) ofs_info << std::setw(6) << interaction->ninter[k][order];
			ofs_info << std::endl;

			for (k = 0; k < symmetry->natmin; ++k){
				iat = symmetry->map_p2s[k][0];
				for (m = 0; m < interaction->ninter[k][order]; ++m){
					ofs_info << std::setw(6) << iat + 1 << std::setw(6) << interaction->intpairs[k][order][m] + 1;
					for (i = 0; i < 3; ++i){
						ofs_info << std::scientific << std::setprecision(16) << std::setw(25) << interaction->relvec[k][order][m][i];
					}
					ofs_info << std::endl;
				}
			}
		}
	} else if (interaction->interaction_type == 2) {

		// Special treatment for harmonic terms

		int ninter_tmp;
		ninter_tmp = 0;
		ofs_info << "#LIST_HARMONIC" << std::endl;

		for (i = 0; i < symmetry->natmin; ++i) {
			ninter_tmp = 0;
			for (j = 0; j < system->nat; ++j) {
				ninter_tmp += interaction->mindist_pairs[i][j].size();
			}
			ofs_info << std::setw(6) << ninter_tmp;
		}
		ofs_info << std::endl;

		for (i = 0; i < symmetry->natmin; ++i) {
			iat = symmetry->map_p2s[i][0];
			for (j = 0; j < system->nat; ++j) {
				for (k = 0; k < interaction->mindist_pairs[i][j].size(); ++k) {
					ofs_info << std::setw(6) << iat + 1 << std::setw(6) << j + 1;
					for (m = 0; m < 3; ++m) {
						ofs_info << std::scientific << std::setprecision(16) << std::setw(25) << interaction->mindist_pairs[i][j][k].relvec[m];
					}
					ofs_info << std::endl;
				}
			}
		}

		for (order = 1; order < interaction->maxorder; ++order){
			ofs_info << "#LIST_" + interaction->str_order[order] << std::endl;

			for (k = 0; k < symmetry->natmin; ++k) ofs_info << std::setw(6) << interaction->ninter[k][order];
			ofs_info << std::endl;

			for (k = 0; k < symmetry->natmin; ++k){
				iat = symmetry->map_p2s[k][0];
				for (m = 0; m < interaction->ninter[k][order]; ++m){
					ofs_info << std::setw(6) << iat + 1 << std::setw(6) << interaction->intpairs[k][order][m] + 1;
					for (i = 0; i < 3; ++i){
						ofs_info << std::scientific << std::setprecision(16) << std::setw(25) << interaction->relvec[k][order][m][i];
					}
					ofs_info << std::endl;
				}
			}
		}
	} else {
		error->exit("wrtmisc", "This cannot happen.");
	}

	int *ncount;
	int ind_tmp;
	int id;

	int ip = 0;
	int ishift = 0;
	int *pair_tmp;

	memory->allocate(ncount, 3*symmetry->natmin);
	memory->allocate(pair_tmp, interaction->maxorder + 1);

	ofs_info << "##FORCE CONSTANTS" << std::endl;
	ofs_info << "All force constants and interaction info" << std::endl;


	for(int order = 0; order < interaction->maxorder; ++order){
		ofs_info << "#FCS_" + interaction->str_order[order] << std::endl;

		int nelem = 0;
		for(std::vector<int>::iterator it = fcs->ndup[order].begin(); it != fcs->ndup[order].end(); ++it){
			nelem += *it;
		}
		ofs_info << std::setw(10) << nelem << std::endl;

		for(i = 0; i < 3*symmetry->natmin; ++i) ncount[i] = 0;

		id = 0;

		for(ui = 0; ui < fcs->ndup[order].size(); ++ui){
			for(j = 0; j < fcs->ndup[order][ui]; ++j){
				ind_tmp = fcs->fc_set[order][id].elems[0];
				for(k = 0; k < symmetry->natmin; ++k){
					if(ind_tmp / 3 == symmetry->map_p2s[k][0]) {
						++ncount[3 * k + ind_tmp % 3];
						break;
					}
				}
				++id;
			}
		}

		for(i = 0; i < 3*symmetry->natmin; ++i){
			ofs_info << std::setw(6) << ncount[i];
		}
		ofs_info << std::endl;

		// This sorting is necessary for linking to molecular dynamics program.
		std::sort(fcs->fc_set[order].begin(), fcs->fc_set[order].end());

		for(std::vector<FcProperty>::iterator it = fcs->fc_set[order].begin(); it != fcs->fc_set[order].end(); ++it){
			FcProperty fctmp = *it;
			ip = fctmp.mother + ishift;
			ofs_info << std::scientific << std::setprecision(16) << std::setw(25) << fitting->params[ip]*fctmp.coef << std::endl;
			for(k = 0; k < order + 2; ++k){
				ofs_info << std::setw(5) << fcs->easyvizint(fctmp.elems[k]);
			}
			ofs_info << std::endl;
		}

		ishift += fcs->ndup[order].size();
	}

	if (interaction->interaction_type == 2) {

		ofs_info << "#FCS_HARMONIC_EXT" << std::endl;

		for (i = 0; i < 3*symmetry->natmin; ++i) ncount[i] = 0;

		for (std::vector<FcProperty>::iterator it = fcs->fc_set[0].begin(); it != fcs->fc_set[0].end(); ++it) {
			FcProperty fctmp = *it;

			for (k = 0; k < 2; ++k) {
				pair_tmp[k] = fctmp.elems[k] / 3;
			}
			j = symmetry->map_s2p[pair_tmp[0]].atom_num;
			ncount[3 * j + fctmp.elems[0] % 3] += interaction->mindist_pairs[j][pair_tmp[1]].size();
		}


		int nelem = 0;
		for (i = 0; i < 3*symmetry->natmin; ++i) nelem += ncount[i];

		ofs_info << std::setw(10) << nelem << std::endl;


		for(i = 0; i < 3*symmetry->natmin; ++i){
			ofs_info << std::setw(6) << ncount[i];
		}
		ofs_info << std::endl;


		for (std::vector<FcProperty>::iterator it = fcs->fc_set[0].begin(); it != fcs->fc_set[0].end(); ++it) {
			FcProperty fctmp = *it;
			ip = fctmp.mother;

			for (k = 0; k < 2; ++k) {
				pair_tmp[k] = fctmp.elems[k] / 3;
			}
			j = symmetry->map_s2p[pair_tmp[0]].atom_num;
			for (std::vector<DistInfo>::iterator it2 = interaction->mindist_pairs[j][pair_tmp[1]].begin(); it2 != interaction->mindist_pairs[j][pair_tmp[1]].end(); ++it2) {
				ofs_info << std::setw(5) << j << std::setw(5) << fctmp.elems[0] % 3;
				ofs_info << std::setw(8) << pair_tmp[1] << std::setw(5) <<  fctmp.elems[1] % 3;
				ofs_info << std::setw(5) << (*it2).cell;
				ofs_info << std::scientific << std::setprecision(16) << std::setw(25) << fitting->params[ip]*fctmp.coef / static_cast<double>(interaction->mindist_pairs[j][pair_tmp[1]].size()) << std::endl;
			}
		}

	}


	memory->deallocate(ncount);
	memory->deallocate(pair_tmp);

	std::cout << std::endl << "Miscellaneous information needed for post-process was stored to file: " << files->file_info << std::endl;
}
