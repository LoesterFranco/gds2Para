#include "fdtd.hpp"
#include "hypreSolver.h"
#include <mkl_service.h>
#include <mkl_dfti.h>

void applyPrecond_P(myint* PRowId1, myint* PColId, double* Pval, myint leng_P, myint N, double* x1, double* x2);
int generate_bm(fdtdMesh* sys, sparse_matrix_t& V0dat, myint leng_v0d, sparse_matrix_t& V0cat, myint leng_v0c, double freq, double* J, double* bm, myint N);
int multi_matrixvec(fdtdMesh* sys, sparse_matrix_t& V0dt, sparse_matrix_t& V0dat, myint leng_v0d, sparse_matrix_t& V0ct, sparse_matrix_t& V0cat, myint leng_v0c, double freq, myint* LrowId, myint* LcolId, double* Lval, myint leng, double* x, double* y, myint N);
int realMatrixVec(fdtdMesh* sys, sparse_matrix_t& V0dt, sparse_matrix_t& V0dat, myint leng_v0d, sparse_matrix_t& V0ct, sparse_matrix_t& V0cat, myint leng_v0c, double freq, myint* LrowId, myint* LcolId, double* Lval, myint leng, double* x, double* y, myint N);
int imagMatrixVec(fdtdMesh* sys, myint leng_v0d, sparse_matrix_t& V0ct, sparse_matrix_t& V0cat, myint leng_v0c, double freq, double* x, double* y, myint N);
int applyPrecond_freq(fdtdMesh* sys, double* b1, complex<double>* b2, myint* L1RowId, myint* L1ColId, double* L1val, myint leng_L1, sparse_matrix_t& v0ct, sparse_matrix_t& v0cat, sparse_matrix_t& v0dt, sparse_matrix_t& v0dat, double freq);


static bool comp(pair<complex<double>, myint> a, pair<complex<double>, myint> b, sparse_matrix_t& v0dt, sparse_matrix_t& v0dat)
{
    return (pow(a.first.real(), 2) + pow(a.first.imag(), 2)) < (pow(b.first.real(), 2) + pow(b.first.imag(), 2));
};

int solveFreqIO(fdtdMesh* sys, int freqi, complex<double>* y, myint* MooRowId, myint* MooRowId1, myint* MooColId, double* Mooval, myint lengoo, myint* SioRowId, myint* SioColId, double* Sioval, myint lengio, myint* PRowId1, myint* PColId, double* Pval, myint leng_P) {
	/* solve the problem in frequency domain
	  [-omega^2*D_epsoo+Soo, Soi
	   Sio,                  i*omega*D_sigii+Sii]
	   sourcePort : which source is now used
	   y : the solution 
	   MooRowId1 : CSR format of (-omega^2*D_epsoo+Soo)'s RowId
	   MooColId : (-omega^2*D_epsoo+Soo)'s column id
	   Mooval1 : (-omega^2*D_epsoo+Soo)'s value */
	myint nedge = sys->N_edge - sys->bden, inside = nedge - sys->outside;
	ofstream out;

	/* Solve the outside part */
	double* Jo = (double*)calloc(sys->outside * sys->numPorts, sizeof(double));   // Jo
	for (int sourcePort = 0; sourcePort < sys->numPorts; ++sourcePort) {
		for (int sourcePortSide = 0; sourcePortSide < sys->portCoor[sourcePort].multiplicity; sourcePortSide++) {
			for (int indEdge = 0; indEdge < sys->portCoor[sourcePort].portEdge[sourcePortSide].size(); indEdge++) {
				/* Set current density for all edges within sides in port to prepare solver */
				Jo[sourcePort * sys->outside + sys->mapio[sys->mapEdge[sys->portCoor[sourcePort].portEdge[sourcePortSide][indEdge]]]] = sys->portCoor[sourcePort].portDirection[sourcePortSide];
			}
		}
	}

	/* solve oo part */
	double freq = sys->freqNo2freq(freqi);
	double* xo = (double*)calloc(sys->outside * sys->numPorts, sizeof(double));
	//clock_t t1 = clock();
	//solveOO(sys, freq, Jo, v0dt, v0dat, xo);   // xo should be imaginary add i later
	//solveOO_pardiso(sys, MooRowId1, MooColId, Mooval, lengoo, sys->outside, Jo, xo, sys->numPorts);   // use pardiso to solve (-omega^2*D_epsoo+Soo)*xo=Jo
	for (int ind = 0; ind < sys->numPorts; ++ind) {
		/* solve M*x=Jo, with P as the preconditioner */
		mkl_gmres_A_P(&Jo[ind * sys->outside], &xo[ind * sys->outside], MooRowId, MooColId, Mooval, lengoo, sys->outside, PRowId1, PColId, Pval, leng_P); 
	}
	//for (int ind = 0; ind < sys->outside * sys->numPorts; ++ind) {
	//	xo[ind] *= -freq * 2 * M_PI;   // xo is the solution from (-omega^2*D_epsoo+Soo)\(-omega*Jo)
	//}
	//cout << "Time to solve oo part is " << (clock() - t1) * 1.0 / CLOCKS_PER_SEC << endl;
	/* Begin to check the correctness of OO part */
	//out.open("Jo.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < sys->outside; ++ind) {
	//	out << Jo[ind] << endl;
	//}
	//out.close();
	//out.open("xo.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < sys->outside; ++ind) {
	//	out << setprecision(15) << xo[ind] << endl;
	//}
	//out.close();
	/* End of checking the correctness of OO part */

	/* solve ii part */
	//double* temp = (double*)calloc(inside * sys->numPorts, sizeof(double));
	//double* xi = (double*)calloc(inside * sys->numPorts, sizeof(double));
	//for (int sourcePort = 0; sourcePort < sys->numPorts; ++sourcePort) {
	//	sparseMatrixVecMul(SioRowId, SioColId, Sioval, lengio, &xo[sourcePort * sys->outside], &temp[sourcePort * inside]);   // temp=Sio*xo
	//}
	//for (myint indi = 0; indi < inside * sys->numPorts; ++indi) {
	//	xi[indi] = -temp[indi] / (2 * M_PI * freq * SIGMA);
	//}

	for (int sourcePort = 0; sourcePort < sys->numPorts; ++sourcePort) {
		for (myint indi = 0; indi < nedge; ++indi) {
			if (sys->markEdge[sys->mapEdgeR[indi]]) {
				//y[sourcePort * nedge + indi] = xi[sourcePort * inside + sys->mapio[indi] - sys->outside] + 1i * 0;
			}
			else {
				y[sourcePort * nedge + indi] = 0 + 1i * xo[sourcePort * sys->outside + sys->mapio[indi]];
			}
		}
	}
	//out.open("y.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int sourcePort = 0; sourcePort < sys->numPorts; ++sourcePort) {
	//	for (int indi = 0; indi < nedge; ++indi) {
	//		out << y[sourcePort * nedge + indi].real() << " " << y[sourcePort * nedge + indi].imag() << endl;
	//	}
	//}
	//out.close();

	free(Jo); Jo = NULL;
	free(xo); xo = NULL;
	//free(temp); temp = NULL;
	//free(xi); xi = NULL;
}


int find_Vh_back(fdtdMesh* sys, int sourcePort, sparse_matrix_t& v0ct, sparse_matrix_t& v0cat, sparse_matrix_t& v0dt, sparse_matrix_t& v0dat, myint* A12RowId, myint* A12ColId, double* A12val, myint leng_A12, myint* A21RowId, myint* A21ColId, double* A21val, myint leng_A21, myint* A22RowId, myint* A22ColId, double* A22val, myint leng_A22, myint* SRowId1, myint* SColId, double* Sval, sparse_matrix_t& Ll) {
	int t0n = 3;
	double dt = DT;
	double tau = 1.e-11;
	double t0 = t0n * tau;
	myint nt = 2 * t0 / dt;
	myint SG = 10;
	double zero_value = 1e10;    // smaller than this value is discarded as null-space eigenvector
	double eps1 = 1.e-8;    // weight
	double eps2 = 1.e-2;    // wavelength error tolerance
	double* rsc = (double*)calloc((sys->N_edge - sys->bden), sizeof(double));
	double* xr = (double*)calloc((sys->N_edge - sys->bden) * 3, sizeof(double));
	double eps = 1e-5;   // nn is the norm of each xr
    int l = 0;
    myint start, index;

    vector<double> U0_base(sys->N_edge - sys->bden, 0);
    vector<vector<double>> U0;   // Since I need to put in new vectors into U0, I need dynamic storage
    double *U0c, *m0;
    vector<vector<double>> temp, temp1, temp2;
    double nn, nnr, nni;
    double *Cr, *D_sigr, *D_epsr;
    vector<vector<double>> Cr_p, D_sigr_p, D_epsr_p;
    vector<double> Cr_base;
    double *Ar, *Br, *BBr;
    vector<pair<complex<double>, myint>> dp, dp1;

    double *vl, *vr, *vl1;
    double *vr1;
    lapack_int info;
    lapack_int n, m;
    char jobvl, jobvr;
    lapack_int lda, ldb;
    lapack_int ldvl, ldvr;
    double *alphar;
    double *alphai;
    double *beta;
    double *tmp, *tmp1, *tmp2;
    int i_re1, i_nre1;
    lapack_complex_double *V_re, *V_nre, *V_re1, *V_nre1;
    V_re = (lapack_complex_double*)calloc(1, sizeof(lapack_complex_double));
    V_nre = (lapack_complex_double*)calloc(1, sizeof(lapack_complex_double));
    V_re1 = (lapack_complex_double*)calloc(1, sizeof(lapack_complex_double));
    V_nre1 = (lapack_complex_double*)calloc(1, sizeof(lapack_complex_double));
    int ind_i, ind_j;
    int i_re, i_nre;
    vector<int> re, nre;    // used to store the No. of eigenvectors in re and nre
    char job, side;
    lapack_int ilo, ihi;
    double *lscale, *rscale;
    double nor;
    lapack_complex_double *tmp3, *tmp4;
    int status;
    lapack_complex_double *y_re;
    lapack_complex_double *y_nre, *y_nre1;
    double x_re, x_nre;
    double maxvalue = 1.e+100;
    lapack_complex_double *tau_qr;
    int U0_i = 0;
    double zero_norm = 1.e-8;
    lapack_complex_double *m_new;
    lapack_int *ipiv;
    lapack_int iter;
    double *V_nre_norm, *tau1;
    lapack_complex_double *y_new, *V_new;
    lapack_complex_double *y;
    int *select;
    double *q, *bm;
    lapack_complex_double *qc;
    ofstream outfile;
    lapack_complex_double *m_re;
    double scale = 1.e+14;
    lapack_complex_double *RR;
	myint noden = sys->leng_v0c + sys->leng_v0d1;
	myint edge = sys->N_edge - sys->bden;
	double* x;


	double normb;
	int inx, iny, inz;
	/* Allocate the space for the time response of each port */
	double** resp = (double**)malloc(sys->numPorts * sizeof(double*));
	for (int ind = 0; ind < sys->numPorts; ++ind) {
		resp[ind] = (double*)calloc(nt, sizeof(double));
	}
	/* End of allocating the space for the time response of each port */

	/* pardiso solve the backward difference matrix --- factorization */
	//pardisoSolve_factorize(SRowId1, SColId, Sval, sys->leng_S, edge);
	/* End of pardiso solve the backward difference matrix --- factorization */

	for (int ind = 1; ind <= nt; ind++) {    // time marching to find the repeated eigenvectors
		// Generate the right hand side
		for (int indi = 0; indi < edge; indi++) {
			rsc[indi] = 0;
			rsc[indi] -= sys->getEps(sys->mapEdgeR[indi]) * (-2 * xr[edge + indi] + xr[indi]);
			if (sys->markEdge[sys->mapEdgeR[indi]]) {
				rsc[indi] += dt * SIGMA * xr[edge + indi];
			}
		}
		for (int sourcePortSide = 0; sourcePortSide < sys->portCoor[sourcePort].multiplicity; sourcePortSide++) {
			for (int inde = 0; inde < sys->portCoor[sourcePort].portEdge[sourcePortSide].size(); inde++) {
				rsc[sys->mapEdge[sys->portCoor[sourcePort].portEdge[sourcePortSide][inde]]] -= (pow(dt, 2) * 2000 * exp(-pow((((dt * (ind + 1)) - t0) / tau), 2)) + pow(dt, 2) * 2000 * (dt * (ind + 1) - t0) * exp(-pow(((dt * (ind + 1) - t0) / tau), 2)) * (-2 * (dt * (ind + 1) - t0) / pow(tau, 2)));
			}
		}
		//outfile.open("rsc.txt", std::ofstream::out | std::ofstream::trunc);
		//for (int ind = 0; ind < sys->N_edge; ++ind) {
		//	outfile << rsc[ind] << endl;
		//}
		//outfile.close();

		/* backward difference, use HYPRE solve */
		//status = hypreSolve(sys->SRowId, sys->SColId, val, sys->leng_S, rsc, sys->N_edge - sys->bden, &xr[(sys->N_edge - sys->bden) * 2], 1, 3);   // reference backward by solving (D_eps+dt*D_sig+dt^2*S)
		//status = hypreSolve(LrowId, LcolId, val, leng, rsc, sys->N_edge - sys->bden, &xr[edge * 2], 0, 3);   // solve (D_eps + dt * D_sig + dt ^ 2 * L)
		/* End of backward difference, use HYPRE solve */

		/* Use pardiso to solve backward difference */
		status = pardisoSolve(SRowId1, SColId, Sval, rsc, &xr[edge * 2], edge);
		//outfile.open("xr.txt", std::ofstream::trunc | std::ofstream::out);
		//for (int indi = 0; indi < edge; ++indi) {
		//	outfile << xr[edge * 2 + indi] << endl;
		//}
		//outfile.close();
		/* End of using pardiso to solve backward difference */

		/* Start to solve [V0a'*(D_eps+dt*D_sig)*V0, V0a'*(D_eps+dt*D_sig)
		          (D_eps+dt*D_sig)*V0,      D_eps+dt*D_sig+dt^2*L]
				  for backward difference*/
		/* Generate [v0a' * b; b] */
		//bm = (double*)calloc(noden + edge, sizeof(double));
		//sys->generateLaplacianRight(bm, rsc);
		////outfile.open("bm.txt", std::ofstream::trunc | std::ofstream::out);
		////for (int ind = 0; ind < sys->N_edge - sys->bden + sys->leng_v0d1 + sys->leng_v0c; ++ind) {
		////	outfile << bm[ind] << endl;
		////}
		////outfile.close();

		///* Use MKL fgmres to solve the matrix solution */
		///* solve [v0a'*(D_eps+dt*D_sig)*v0, v0a'*(D_eps+dt*D_sig);
		//        (D_eps+dt*D_sig)*v0,      D_eps+dt*D_sig+dt^2*L] with diagonal as the preconditioner */
		////x = (double*)malloc((noden + sys->N_edge - sys->bden) * sizeof(double));
		////status = mkl_gmres(sys, bm, x, Ll, A22RowId, A22ColId, A22val, leng_A22, v0ct, v0cat, v0dt, v0dat);
		///* End of using MKL fgmres to solve the matrix solution */

		///* Use direct solver to solve the matrix solution */
		///* When dt^2*Loo is much larger than dt*D_sig+D_eps, this way is accurate
		// solve [v0a'*(D_eps+dt*D_sig)*v0, v0a'*(D_eps+dt*D_sig);
  //              (D_eps+dt*D_sig)*v0,      D_eps+dt*D_sig+dt^2*L] */
		//x = (double*)malloc((noden + sys->N_edge - sys->bden) * sizeof(double));
		//status = solveBackMatrix(sys, bm, x, v0ct, v0cat, v0dt, v0dat, A12RowId, A12ColId, A12val, leng_A12, A21RowId, A21ColId, A21val, leng_A21, A22RowId, A22ColId, A22val, leng_A22);
		// /* End of using direct solver to solve the matrix solution */

		///* Use pardiso to solve the matrix solution */
		////x = (double*)calloc((noden + sys->N_edge - sys->bden), sizeof(double));
		////status = pardisoSolve(sys->LlRowId, sys->LlColId, sys->Llval, bm, x, edge + noden);
		///* End of using pardiso to solve the matrix solution */
		//
		///* Compute V0 * y0 + xh */
		//status = combine_x(x, sys, &xr[edge * 2]);
		////outfile.open("x.txt", std::ofstream::trunc | std::ofstream::out);
		////for (int indi = 0; indi < edge; ++indi) {
		////	outfile << xr[edge * 2 + indi] << endl;
		////}
		////outfile.close();

		/* End of solving [V0a'*(D_eps+dt*D_sig)*V0, V0a'*(D_eps+dt*D_sig)
		(D_eps+dt*D_sig)*V0,      D_eps+dt*D_sig+dt^2*L]
		for backward difference*/
		
		cout << ind << endl;
		//outfile.open("xf.txt", std::ofstream::out | std::ofstream::trunc);
		//for (int ind = 0; ind < sys->N_edge - sys->bden; ++ind) {
		//	outfile << xr[sys->N_edge * 2 + ind] << endl;
		//}
		//outfile.close();

		//status = storeTimeRespValue(sys, resp, ind, &xr[edge]);
		

		nn = 0;
		for (int indi = 0; indi < edge; indi++) {
			//nn += pow(xr[2 * edge + indi], 2);
			xr[indi] = xr[edge + indi];
			xr[edge + indi] = xr[(edge) * 2 + indi];
			xr[(edge) * 2 + indi] = 0;
		}
		//nn = sqrt(nn);

		nn = sys->getVoltage(xr, sourcePort);   // get the voltage

		outfile << nn << endl;
		free(bm); bm = NULL;
		free(x); x = NULL;
	}
	outfile.close();

	free(rsc); rsc = NULL;
	free(xr); xr = NULL;

	/* FFT to transfer the time response to frequency domain */
	//complex<double>** freqV = (complex<double>**)malloc(sys->numPorts * sizeof(complex<double>));
	//for (int ind = 0; ind < sys->numPorts; ++ind) {
	//	freqV[ind] = (complex<double>*)calloc(nt, sizeof(complex<double>));
	//	status = mklFFT(sys, resp[ind], freqV[ind], nt);
	//}
	//cout << "mklFFT status is " << status << endl;
	//outfile.open("t.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < nt; ++ind) {
	//	outfile << resp[0][ind] << endl;
	//}
	//outfile.close();

	//outfile.open("f.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < nt; ++ind) {
	//	outfile << freqV[0][ind].real() << " " << freqV[0][ind].imag() << endl;
	//}
	//outfile.close();
	/* End of FFT to transfer the time response to frequency domain */

	for (int ind = 0; ind < sys->numPorts; ++ind) {
		free(resp[ind]); resp[ind] = NULL;
	}
	free(resp); resp = NULL;
    free(xr); xr = NULL;
}

int find_Vh_central(fdtdMesh *sys, int sourcePort){
	myint edge = sys->N_edge - sys->bden;
    int t0n = 3;
    double dt = DT;
    double tau = 1.e-11;
    double t0 = t0n * tau;
    myint nt = 3 * t0 / dt;
    myint SG = 1000;
    double zero_value = 1e10;    // smaller than this value is discarded as null-space eigenvector
    double eps1 = 1.e-7;    // weight
    double eps2 = 1.e-2;    // wavelength error tolerance
    double *rsc = (double*)calloc((sys->N_edge - sys->bden), sizeof(double));
    double *xr = (double*)calloc((sys->N_edge - sys->bden) * 3, sizeof(double));
    myint start;
    myint index;
    int l = 0;
    
    vector<double> U0_base(sys->N_edge - sys->bden, 0);
    vector<vector<double>> U0;   // Since I need to put in new vectors into U0, I need dynamic storage
    double *U0c, *m0;
    vector<vector<double>> temp, temp1, temp2;
    double nn, nnr, nni;
    double *Cr, *D_sigr, *D_epsr;
    vector<vector<double>> Cr_p, D_sigr_p, D_epsr_p;
    vector<double> Cr_base;
    double *Ar, *Br, *BBr;
    vector<pair<complex<double>, myint>> dp, dp1;
    double *vl, *vr, *vl1;
    double *vr1;
    lapack_int info;
    lapack_int n, m;
    char jobvl, jobvr;
    lapack_int lda, ldb;
    lapack_int ldvl, ldvr;
    double *alphar;
    double *alphai;
    double *beta;
    double *tmp, *tmp1, *tmp2;
    int i_re1, i_nre1;
    lapack_complex_double *V_re, *V_nre, *V_re1, *V_nre1;
    V_re = (lapack_complex_double*)calloc(1, sizeof(lapack_complex_double));
    V_nre = (lapack_complex_double*)calloc(1, sizeof(lapack_complex_double));
    V_re1 = (lapack_complex_double*)calloc(1, sizeof(lapack_complex_double));
    V_nre1 = (lapack_complex_double*)calloc(1, sizeof(lapack_complex_double));
    int ind_i, ind_j;
    int i_re, i_nre;
    vector<int> re, nre;    // used to store the No. of eigenvectors in re and nre
    char job, side;
    lapack_int ilo, ihi;
    double *lscale, *rscale;
    double nor;
    lapack_complex_double *tmp3, *tmp4;
    int status;
    lapack_complex_double *y_re;
    lapack_complex_double *y_nre, *y_nre1;
    double x_re, x_nre;
    double maxvalue = 1.e+100;
    lapack_complex_double *tau_qr;
    int U0_i = 0;
    double zero_norm = 1.e-7;
    lapack_complex_double *m_new;
    lapack_int *ipiv;
    lapack_int iter;
    double *V_nre_norm, *tau1;
    lapack_complex_double *y_new, *V_new;
    lapack_complex_double *y;
    int *select;
    double *q;
    lapack_complex_double *qc;
    ofstream outfile;
    lapack_complex_double *m_re;
    double scale = 1.e+12;
    lapack_complex_double *RR;
    double eps = 1e-5;
	ofstream out;
	int inx, iny, inz;
	double leng;
	myint thisEdge;
	out.open("res_plasma.txt", std::ofstream::out | std::ofstream::app);
	int port;
	double vol;
	for (int ind = 1; ind <= nt; ind++) {    // time marching to find the repeated eigenvectors
		for (int sourcePortSide = 0; sourcePortSide < sys->portCoor[sourcePort].multiplicity; sourcePortSide++) {
			for (int inde = 0; inde < sys->portCoor[sourcePort].portEdge[sourcePortSide].size(); inde++) {
				rsc[sys->mapEdge[sys->portCoor[sourcePort].portEdge[sourcePortSide][inde]]] = 2000 * exp(-pow((((dt * ind) - t0) / tau), 2)) + 2000 * (dt * ind - t0) * exp(-pow(((dt * ind - t0) / tau), 2)) * (-2 * (dt * ind - t0) / pow(tau, 2));

			}
		}

		// central difference
		index = 0;
		nn = 0;
		while (index < sys->leng_S) {
			start = sys->SRowId[index];
			while (index < sys->leng_S && sys->SRowId[index] == start) {
				xr[2 * (sys->N_edge - sys->bden) + sys->SRowId[index]] += sys->Sval[index] * xr[1 * (sys->N_edge - sys->bden) + sys->SColId[index]] * (-2) * pow(dt, 2);
				index++;
			}
			if (sys->markEdge[sys->mapEdgeR[start]] != 0) {
				xr[2 * (sys->N_edge - sys->bden) + start] += -rsc[start] * 2 * pow(dt, 2) + dt * SIGMA * xr[start] - 2 * sys->getEps(sys->mapEdgeR[start]) * xr[start] + 4 * sys->getEps(sys->mapEdgeR[start]) * xr[1 * (sys->N_edge - sys->bden) + start];
				xr[2 * (sys->N_edge - sys->bden) + start] /= (2 * sys->getEps(sys->mapEdgeR[start]) + dt * SIGMA);
			}
			else {
				xr[2 * (sys->N_edge - sys->bden) + start] += -rsc[start] * 2 * pow(dt, 2) - 2 * sys->getEps(sys->mapEdgeR[start]) * xr[start] + 4 * sys->getEps(sys->mapEdgeR[start]) * xr[1 * (sys->N_edge - sys->bden) + start];
				xr[2 * (sys->N_edge - sys->bden) + start] /= (2 * sys->getEps(sys->mapEdgeR[start]));
			}
			nn += xr[1 * (sys->N_edge - sys->bden) + start] * xr[1 * (sys->N_edge - sys->bden) + start];
		}
		nn = sqrt(nn);
		//cout << "Step " << ind << "'s norm is " << nn << endl;
		cout << ind << endl;

		/* print out the voltage with time */
		for (port = 0; port < sys->numPorts; ++port) {
			vol = 0;
			for (int inde = 0; inde < sys->portCoor[port].portEdge[0].size(); inde++) {
				thisEdge = sys->portCoor[port].portEdge[0][inde];
				if (thisEdge % (sys->N_edge_s + sys->N_edge_v) >= sys->N_edge_s) {    // This edge is along the z-axis
					inz = thisEdge / (sys->N_edge_s + sys->N_edge_v);
					leng = sys->zn[inz + 1] - sys->zn[inz];
				}
				else {
					if (thisEdge % (sys->N_edge_s + sys->N_edge_v) >= (sys->N_cell_y) * (sys->N_cell_x + 1)) {    // This edge is along the x-axis
						inx = ((thisEdge % (sys->N_edge_s + sys->N_edge_v)) - (sys->N_cell_y) * (sys->N_cell_x + 1)) / (sys->N_cell_y + 1);
						leng = sys->xn[inx + 1] - sys->xn[inx];
					}
					else {    // This edge is along the y-axis
						iny = (thisEdge % (sys->N_edge_s + sys->N_edge_v)) % sys->N_cell_y;
						leng = sys->yn[iny + 1] - sys->yn[iny];
					}
				}
				vol -= xr[edge + sys->mapEdge[sys->portCoor[port].portEdge[0][inde]]] * leng * (sys->portCoor[port].portDirection[0] * 1.0);
			}
			out << vol << " ";
		}
		out << endl;
		for (myint inde = 0; inde < sys->N_edge - sys->bden; inde++) {
			xr[inde] = xr[1 * edge + inde];
			xr[1 * edge + inde] = xr[2 * edge + inde];
			xr[2 * edge + inde] = 0;
		}
	}
	free(xr); xr = NULL;
	out.close();


		//if (ind == SG){
		//    l++;
		//    U0.push_back(U0_base);
		//    temp.push_back(U0_base);
		//    temp1.push_back(U0_base);
		//    temp2.push_back(U0_base);
		//    Cr_p.push_back(Cr_base);
		//    D_sigr_p.push_back(Cr_base);
		//    D_epsr_p.push_back(Cr_base);
		//    for (myint inde = 0; inde < sys->N_edge - sys->bden; inde++){
		//        U0[U0_i][inde] = xr[1 * (sys->N_edge - sys->bden) + inde] / nn;
		//    }
		//    U0_i++;
		//    Cr = (double*)calloc(l * l, sizeof(double));
		//    D_sigr = (double*)calloc(l * l, sizeof(double));
		//    D_epsr = (double*)calloc(l * l, sizeof(double));
		//    index = 0;
		//    
		//    while (index < sys->leng_S){
		//        start = sys->SRowId[index];
		//        while (index < sys->leng_S && sys->SRowId[index] == start){
		//            temp[0][sys->SRowId[index]] += sys->Sval[index] * U0[U0_i - 1][sys->SColId[index]];
		//            index++;
		//        }
		//        if (sys->markEdge[sys->mapEdgeR[start]] != 0) {
		//            temp1[0][start] = sqrt(SIGMA) * U0[U0_i - 1][start];
		//            temp2[0][start] = sqrt(sys->getEps(sys->mapEdgeR[start])) * U0[U0_i - 1][start];
		//        }
		//        else {
		//            temp1[0][start] = 0;
		//            temp2[0][start] = sqrt(sys->getEps(sys->mapEdgeR[start])) * U0[U0_i - 1][start];
		//        }
		//    }
		//    for (myint inde = 0; inde < l; inde++){
		//        Cr_p.push_back(Cr_base);
		//        D_sigr_p.push_back(Cr_base);
		//        D_epsr_p.push_back(Cr_base);
		//        for (myint inde2 = 0; inde2 < l; inde2++){
		//            Cr_p[inde].push_back(0);
		//            D_sigr_p[inde].push_back(0);
		//            D_epsr_p[inde].push_back(0);
		//            for (myint inde3 = 0; inde3 < sys->N_edge - 2 * sys->N_edge_s; inde3++){
		//                Cr[inde * l + inde2] += U0[inde][inde3] * temp[inde2][inde3];
		//                D_sigr[inde * l + inde2] += temp1[inde][inde3] * temp1[inde2][inde3];
		//                D_epsr[inde * l + inde2] += temp2[inde][inde3] * temp2[inde2][inde3];
		//                Cr_p[inde][inde2] += U0[inde][inde3] * temp[inde2][inde3];
		//                D_sigr_p[inde][inde2] += temp1[inde][inde3] * temp1[inde2][inde3];
		//                D_epsr_p[inde][inde2] += temp2[inde][inde3] * temp2[inde2][inde3];
		//            }
		//        }
		//    }
		//    Ar = (double*)malloc(4 * l * l * sizeof(double));
		//    Br = (double*)malloc(4 * l * l * sizeof(double));
		//    for (myint inde = 0; inde < l; inde++){
		//        for (myint inde2 = 0; inde2 < l; inde2++){
		//            Ar[inde * 2 * l + inde2] = -Cr[inde * l + inde2];
		//            Br[inde * 2 * l + inde2] = D_sigr[inde * l + inde2] * scale;
		//        }
		//    }
		//    for (myint inde = 0; inde < l; inde++){
		//        for (myint inde2 = l; inde2 < 2 * l; inde2++){
		//            Ar[inde * 2 * l + inde2] = 0;
		//            Br[inde * 2 * l + inde2] = D_epsr[inde * l + inde2 - l] * pow(scale, 2);
		//        }
		//    }
		//    for (myint inde = l; inde < 2 * l; inde++){
		//        for (myint inde2 = 0; inde2 < l; inde2++){
		//            Ar[inde * 2 * l + inde2] = 0;
		//            Br[inde * 2 * l + inde2] = D_epsr[(inde - l) * l + inde2] * pow(scale, 2);
		//        }
		//    }
		//    for (myint inde = l; inde < 2 * l; inde++){
		//        for (myint inde2 = l; inde2 < 2 * l; inde2++){
		//            Ar[inde * 2 * l + inde2] = D_epsr[(inde - l) * l + inde2 - l] * pow(scale, 2);
		//            Br[inde * 2 * l + inde2] = 0;
		//        }
		//    }

		//    n = 2 * l;
		//    jobvl = 'N'; jobvr = 'V';
		//    lda = 2 * l; ldb = 2 * l;
		//    ldvl = 2 * l; ldvr = 2 * l;
		//    alphar = (double*)malloc(2 * l * sizeof(double));
		//    alphai = (double*)malloc(2 * l * sizeof(double));
		//    beta = (double*)malloc(2 * l * sizeof(double));
		//    vl = (double*)malloc(4 * l * l * sizeof(double));
		//    vr = (double*)malloc(4 * l * l * sizeof(double));
		//    job = 'B';    // scale only
		//    lscale = (double*)malloc(n * sizeof(double));
		//    rscale = (double*)malloc(n * sizeof(double));
		//    
		//    info = LAPACKE_dggev(LAPACK_COL_MAJOR, jobvl, jobvr, n, Ar, lda, Br, ldb, alphar, alphai, beta, vl, ldvl, vr, ldvr);
		//    for (myint inde = 0; inde < 2 * l; inde++){
		//        //if (alphai[inde] == 0){
		//        //    dp.push_back(make_pair(alphar[inde] / beta[inde] * scale, inde));
		//        //}
		//        //else{
		//        if (abs(alphai[inde] / beta[inde]) > eps) {   // only those imaginary part larger than 0 eigenvalues are considered
		//            dp.push_back(make_pair(alphar[inde] / beta[inde] * scale + 1i * alphai[inde] / beta[inde] * scale, inde));
		//            inde++;
		//            dp.push_back(make_pair(alphar[inde] / beta[inde] * scale + 1i * alphai[inde] / beta[inde] * scale, inde));
		//        }
		//    }
		//    
		//    sort(dp.begin(), dp.end(), comp);
		//    
		//    free(Cr); Cr = NULL;
		//    free(D_epsr); D_epsr = NULL;
		//    free(D_sigr); D_sigr = NULL;
		//    free(Ar); Ar = NULL;
		//    free(Br); Br = NULL;
		//    free(alphar); alphar = NULL;
		//    free(alphai); alphai = NULL;
		//    free(beta); beta = NULL;
		//}
		//else if (ind % SG == 0){
		//    tmp = (double*)malloc((sys->N_edge - sys->bden) * sizeof(double));
		//    tmp1 = (double*)calloc((sys->N_edge - sys->bden), sizeof(double));
		//    tmp2 = (double*)calloc(U0_i, sizeof(double));
		//    nn = 0;
		//    nor = 0;
		//    for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//        tmp[inde] = xr[1 * (sys->N_edge - sys->bden) + inde];
		//        nor += xr[1 * (sys->N_edge - sys->bden) + inde] * xr[1 * (sys->N_edge - sys->bden) + inde];
		//    }
		//    nor = sqrt(nor);

		//    

		//    /* modified Gran Schmidt */
		//    clock_t t1 = clock();
		//    /*for (myint inde = 0; inde < U0_i; inde++){
		//        nn = 0;
		//        for (myint inde1 = 0; inde1 < sys->N_edge - 2 * sys->N_edge_s; inde1++){
		//            nn += U0[inde][inde1] * U0[inde][inde1];
		//        }
		//        nn = sqrt(nn);
		//        for (myint inde1 = 0; inde1 < sys->N_edge - 2 * sys->N_edge_s; inde1++){
		//            U0[inde][inde1] = U0[inde][inde1] / nn;
		//        }
		//        for (myint inde1 = inde + 1; inde1 < U0_i; inde1++){
		//            nn = 0;
		//            for (myint inde2 = 0; inde2 < sys->N_edge - 2 * sys->N_edge_s; inde2++){
		//                nn += U0[inde][inde2] * U0[inde1][inde2];
		//            }
		//            q = (double*)malloc((sys->N_edge - 2 * sys->N_edge_s) * sizeof(double));
		//            for (myint inde2 = 0; inde2 < sys->N_edge - 2 * sys->N_edge_s; inde2++){
		//                q[inde2] = U0[inde][inde2] * nn;
		//                U0[inde1][inde2] -= q[inde2];
		//            }
		//            free(q); q = NULL;
		//        }
		//        nn = 0;
		//        for (myint inde1 = 0; inde1 < sys->N_edge - 2 * sys->N_edge_s; inde1++){
		//            nn += U0[inde][inde1] * tmp[inde1];
		//        }
		//        q = (double*)malloc((sys->N_edge - 2 * sys->N_edge_s) * sizeof(double));
		//        for (myint inde1 = 0; inde1 < sys->N_edge - 2 * sys->N_edge_s; inde1++){
		//            q[inde1] = U0[inde][inde1] * nn;
		//            tmp[inde1] = tmp[inde1] - q[inde1];
		//        }
		//        free(q); q = NULL;
		//    }
		//    nn = 0;
		//    for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
		//        nn += tmp[inde] * tmp[inde];
		//    }
		//    nn = sqrt(nn);*/
		//    
		//    for (myint inde = 0; inde < U0_i; inde++){
		//        nn = 0;
		//        for (myint inde1 = 0; inde1 < sys->N_edge - sys->bden; inde1++){
		//            nn += tmp[inde1] * U0[inde][inde1];
		//        }
		//        for (myint inde1 = 0; inde1 < sys->N_edge - sys->bden; inde1++){
		//            tmp[inde1] -= nn * U0[inde][inde1];
		//        }
		//    }
		//    nn = 0;
		//    for (myint inde = 0; inde < sys->N_edge - sys->bden; inde++){
		//        nn += tmp[inde] * tmp[inde];
		//    }
		//    nn = sqrt(nn);

		//    //cout << "Modified Gram Schmidt takes " << (clock() - t1) * 1.0 / CLOCKS_PER_SEC << endl;
		//    //cout << "U0_i is " << U0_i << endl;
		//    /*ofstream out;
		//    out.open("U0.txt", std::ofstream::out | std::ofstream::trunc);
		//    for (myint inde = 0; inde < (sys->N_edge - 2 * sys->N_edge_s); inde++){
		//        for (myint inde2 = 0; inde2 < U0_i; inde2++){
		//            out << U0[inde2][inde] << " ";
		//        }
		//        out << endl;
		//    }
		//    out.close();*/
		//    
		//    //cout << "New vector after deduction " << nn / nor << endl;
		//    if (nn / nor > zero_norm){     // this new vector contains new information
		//        U0.push_back(U0_base);
		//        temp.push_back(U0_base);
		//        temp1.push_back(U0_base);
		//        temp2.push_back(U0_base);
		//        for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//            U0[U0_i][inde] = tmp[inde] / nn;
		//        }
		//        U0_i++;
		//    }
		//    else{     // no new information go to the next loop
		//        for (myint inde = 0; inde < sys->N_edge - sys->bden; inde++){
		//            xr[inde] = xr[1 * (sys->N_edge - sys->bden) + inde];
		//            xr[1 * (sys->N_edge - sys->bden) + inde] = xr[2 * (sys->N_edge - sys->bden) + inde];
		//            xr[2 * (sys->N_edge - sys->bden) + inde] = 0;
		//        }
		//        free(tmp); tmp = NULL;
		//        free(tmp1); tmp1 = NULL;
		//        free(tmp2); tmp2 = NULL;
		//        continue;
		//    }
		//    /*tau_qr = (lapack_complex_double*)malloc(U0_i * sizeof(lapack_complex_double));

		//    info = LAPACKE_zgeqrf(LAPACK_COL_MAJOR, (sys->N_edge - 2 * sys->N_edge_s), U0_i, V_re, (sys->N_edge - 2 * sys->N_edge_s), tau_qr);

		//    info = LAPACKE_zungqr(LAPACK_COL_MAJOR, (sys->N_edge - 2 * sys->N_edge_s), i_re, i_re, V_re, (sys->N_edge - 2 * sys->N_edge_s), tau_qr);

		//    free(tau_qr); tau_qr = NULL;*/
		//    t1 = clock();
		//    free(tmp); tmp = NULL;
		//    free(tmp1); tmp1 = NULL;
		//    free(tmp2); tmp2 = NULL;
		//    l++;
		//    Cr = (double*)calloc(U0_i * U0_i, sizeof(double));
		//    D_sigr = (double*)calloc(U0_i * U0_i, sizeof(double));
		//    D_epsr = (double*)calloc(U0_i * U0_i, sizeof(double));
		//    index = 0;
		//    
		//    while (index < sys->leng_S){
		//        start = sys->SRowId[index];

		//        while (index < sys->leng_S && sys->SRowId[index] == start){
		//            temp[(U0_i - 1)][sys->SRowId[index]] += sys->Sval[index] * U0[(U0_i - 1)][sys->SColId[index]];
		//            index++;
		//        }
		//        if (sys->markEdge[sys->mapEdgeR[start]] != 0) {
		//            temp1[(U0_i - 1)][start] = sqrt(SIGMA) * U0[U0_i - 1][start];
		//            temp2[(U0_i - 1)][start] = sqrt(sys->getEps(sys->mapEdgeR[start])) * U0[U0_i - 1][start];
		//        }
		//        else {
		//            temp1[(U0_i - 1)][start] = 0;
		//            temp2[(U0_i - 1)][start] = sqrt(sys->getEps(sys->mapEdgeR[start])) * U0[U0_i - 1][start];
		//        }
		//    }
		//    index = U0_i - 1;
		//    Cr_p.push_back(Cr_base);
		//    D_sigr_p.push_back(Cr_base);
		//    D_epsr_p.push_back(Cr_base);
		//    for (myint inde2 = 0; inde2 < U0_i; inde2++){
		//        for (myint inde3 = 0; inde3 < (sys->N_edge - sys->bden); inde3++){
		//            Cr[index * U0_i + inde2] += U0[inde2][inde3] * temp[index][inde3];
		//            D_sigr[index * U0_i + inde2] += temp1[inde2][inde3] * temp1[index][inde3];
		//            D_epsr[index * U0_i + inde2] += temp2[inde2][inde3] * temp2[index][inde3];
		//        }
		//        Cr_p[index].push_back(Cr[index * U0_i + inde2]);
		//        D_sigr_p[index].push_back(D_sigr[index * U0_i + inde2]);
		//        D_epsr_p[index].push_back(D_epsr[index * U0_i + inde2]);

		//    }

		//    index = U0_i - 1;
		//    for (myint inde2 = 0; inde2 < U0_i - 1; inde2++){
		//        for (myint inde3 = 0; inde3 < (sys->N_edge - sys->bden); inde3++){
		//            Cr[inde2 * U0_i + index] += U0[index][inde3] * temp[inde2][inde3];
		//            D_sigr[inde2 * U0_i + index] += temp1[index][inde3] * temp1[inde2][inde3];
		//            D_epsr[inde2 * U0_i + index] += temp2[index][inde3] * temp2[inde2][inde3];
		//            
		//        }
		//        Cr_p[inde2].push_back(Cr[inde2 * U0_i + index]);
		//        D_sigr_p[inde2].push_back(D_sigr[inde2 * U0_i + index]);
		//        D_epsr_p[inde2].push_back(D_epsr[inde2 * U0_i + index]);
		//    }
		//    for (myint inde = 0; inde < U0_i - 1; inde++){
		//        for (myint inde2 = 0; inde2 < U0_i - 1; inde2++){
		//            Cr[inde * U0_i + inde2] += Cr_p[inde][inde2];
		//            D_sigr[inde * U0_i + inde2] += D_sigr_p[inde][inde2];
		//            D_epsr[inde * U0_i + inde2] += D_epsr_p[inde][inde2];
		//        }
		//    }
		//    
		//    Ar = (double*)malloc(4 * U0_i * U0_i * sizeof(double));
		//    Br = (double*)malloc(4 * U0_i * U0_i * sizeof(double));
		//    for (myint inde = 0; inde < U0_i; inde++){
		//        for (myint inde2 = 0; inde2 < U0_i; inde2++){
		//            Ar[inde * 2 * U0_i + inde2] = -Cr[inde * U0_i + inde2];
		//            Br[inde * 2 * U0_i + inde2] = D_sigr[inde * U0_i + inde2] * scale;
		//        }
		//    }
		//    for (myint inde = 0; inde < U0_i; inde++){
		//        for (myint inde2 = U0_i; inde2 < 2 * U0_i; inde2++){
		//            Ar[inde * 2 * U0_i + inde2] = 0;
		//            Br[inde * 2 * U0_i + inde2] = D_epsr[inde * U0_i + inde2 - U0_i] * pow(scale, 2);
		//        }
		//    }
		//    for (myint inde = U0_i; inde < 2 * U0_i; inde++){
		//        for (myint inde2 = 0; inde2 < U0_i; inde2++){
		//            Ar[inde * 2 * U0_i + inde2] = 0;
		//            Br[inde * 2 * U0_i + inde2] = D_epsr[(inde - U0_i) * U0_i + inde2] * pow(scale, 2);
		//        }
		//    }
		//    for (myint inde = U0_i; inde < 2 * U0_i; inde++){
		//        for (myint inde2 = U0_i; inde2 < 2 * U0_i; inde2++){
		//            Ar[inde * 2 * U0_i + inde2] = D_epsr[(inde - U0_i) * U0_i + inde2 - U0_i] * pow(scale, 2);
		//            Br[inde * 2 * U0_i + inde2] = 0;
		//        }
		//    }
		//    
		//    /*if (ind < 1000){
		//        cout << "Ar is " << endl;
		//        for (myint inde = 0; inde < 2 * U0_i; inde++){
		//            for (myint inde2 = 0; inde2 < 2 * U0_i; inde2++){
		//                cout << Ar[inde2 * 2 * U0_i + inde] << ", ";
		//            }
		//            cout << endl;
		//        }
		//        cout << endl;
		//        cout << "Br is " << endl;
		//        for (myint inde = 0; inde < 2 * U0_i; inde++){
		//            for (myint inde2 = 0; inde2 < 2 * U0_i; inde2++){
		//                cout << Br[inde2 * 2 * U0_i + inde] << ", ";
		//            }
		//            cout << endl;
		//        }
		//    }*/
		//    
		//    
		//    n = 2 * U0_i;
		//    jobvl = 'N'; jobvr = 'V';
		//    lda = 2 * U0_i; ldb = 2 * U0_i;
		//    ldvl = 2 * U0_i; ldvr = 2 * U0_i;
		//    alphar = (double*)malloc(2 * U0_i * sizeof(double));
		//    alphai = (double*)malloc(2 * U0_i * sizeof(double));
		//    beta = (double*)malloc(2 * U0_i * sizeof(double));
		//    vl1 = (double*)calloc(4 * U0_i * U0_i, sizeof(double));
		//    vr1 = (double *)calloc(4 * U0_i * U0_i, sizeof(double));
		//    //lscale = (double*)calloc(2 * U0_i, sizeof(double));
		//    //rscale = (double*)calloc(2 * U0_i, sizeof(double));
		//    //tau1 = (double*)calloc(2 * U0_i, sizeof(double));
		//    //RR = (double*)calloc(4 * U0_i * U0_i, sizeof(double));    // store the upper triangular matrix
		//    //QQ = (double*)calloc(4 * U0_i * U0_i, sizeof(double));
		//    //ZZ = (double*)calloc(4 * U0_i * U0_i, sizeof(double));
		//    //select = (int*)calloc(2 * U0_i, sizeof(int));
		//    dp1.clear();
		//    cout << "Generate eigenvalue problem time is " << (clock() - t1) * 1.0 / CLOCKS_PER_SEC << endl;
		//    t1 = clock();
		//    info = LAPACKE_dggev(LAPACK_COL_MAJOR, jobvl, jobvr, 2 * U0_i, Ar, lda, Br, ldb, alphar, alphai, beta, vl1, ldvl, vr1, ldvr);
		//    /*info = LAPACKE_dggbal(LAPACK_COL_MAJOR, 'B', 2 * U0_i, Ar, lda, Br, ldb, &ilo, &ihi, lscale, rscale);
		//    info = LAPACKE_dgeqrf(LAPACK_COL_MAJOR, 2 * U0_i, 2 * U0_i, Br, 2 * U0_i, tau1);
		//    for (myint inde = 0; inde < 2 * U0_i; inde++){
		//        for (myint inde2 = inde; inde2 < 2 * U0_i; inde2++){
		//            RR[inde2 * 2 * U0_i + inde] = Br[inde2 * U0_i + inde];
		//            Br[inde2 * 2 * U0_i + inde] = 0;
		//        }
		//    }
		//    info = LAPACKE_dormqr(LAPACK_COL_MAJOR, 'L', 'T', 2 * U0_i, 2 * U0_i, 2 * U0_i, Br, 2 * U0_i, tau1, Ar, 2 * U0_i);
		//    info = LAPACKE_dgghrd(LAPACK_COL_MAJOR, 'I', 'I', 2 * U0_i, ilo, ihi, Ar, 2 * U0_i, RR, 2 * U0_i, QQ, 2 * U0_i, ZZ, 2 * U0_i);
		//    info = LAPACKE_dhgeqz(LAPACK_COL_MAJOR, 'S', 'V', 'V', 2 * U0_i, ilo, ihi, Ar, 2 * U0_i, RR, 2 * U0_i, alphar, alphai, beta, QQ, 2 * U0_i, ZZ, 2 * U0_i);
		//    info = LAPACKE_dtgevc(LAPACK_COL_MAJOR, 'R', 'B', select, 2 * U0_i, Ar, 2 * U0_i, RR, 2 * U0_i, QQ, 2 * U0_i, ZZ, 2 * U0_i, 2 * U0_i, &m);
		//    info = LAPACKE_dggbak(LAPACK_COL_MAJOR, 'B', 'R', 2 * U0_i, ilo, ihi, lscale, rscale, m, vr1, 2 * U0_i);*/
		//    
		//    

		//    for (myint inde = 0; inde < 2 * U0_i; inde++){
		//        //if (alphai[inde] == 0){
		//        //    dp1.push_back(make_pair(alphar[inde] / beta[inde] * scale, inde));
		//        //}
		//        //else{
		//        if (abs(alphai[inde] / beta[inde]) > eps) {    // only those imaginary part larger than 0 eigenvalues are considered
		//            dp1.push_back(make_pair(alphar[inde] / beta[inde] * scale + 1i * alphai[inde] / beta[inde] * scale, inde));
		//            inde++;
		//            dp1.push_back(make_pair(alphar[inde] / beta[inde] * scale + 1i * alphai[inde] / beta[inde] * scale, inde));
		//        }
		//    }
		//    /*if (ind < 1000){
		//        

		//        outfile.open("vr.txt", std::ofstream::out | std::ofstream::trunc);
		//        for (myint inde = 0; inde < 2 * U0_i; inde++){
		//            for (myint inde1 = 0; inde1 < 2 * U0_i; inde1++){
		//                outfile << vr1[inde1 * (2 * U0_i) + inde] << " ";
		//            }
		//            outfile << endl;
		//        }
		//        outfile.close();

		//        for (myint inde = 0; inde < dp1.size(); inde++){
		//            cout << dp1[inde].first << " ";
		//        }
		//        cout << endl;
		//    }*/
		//    sort(dp1.begin(), dp1.end(), comp);
		//    
		//    ind_i = 0;
		//    ind_j = 0;
		//    i_re = 0;
		//    i_nre = 0;
		//    re.clear();
		//    nre.clear();
		//    
		//        /*for (myint inde = 0; inde < dp1.size(); inde++){
		//            cout << dp1[inde].first << " ";
		//        }
		//        cout << endl;*/
		//        /* for (myint inde = 0; inde < 2 * U0_i; inde++){
		//            for (myint inde2 = 0; inde2 < 2 * U0_i; inde2++){
		//                cout << vr1[inde2 * 2 * U0_i + inde] << " ";
		//            }
		//            cout << endl;
		//        }*/
		//        /*if (ind == 3 * SG)
		//            break;*/
		//    
		//    while (ind_i < dp.size() && abs(dp[ind_i].first) < zero_value){
		//        ind_i++;
		//    }
		//    while (ind_j < dp1.size() && abs(dp1[ind_j].first) < zero_value){
		//        ind_j++;
		//    }
		//    while (ind_i < dp.size() && ind_j < dp1.size()){
		//        if (abs(dp[ind_i].first) > maxvalue || abs(dp1[ind_j].first) > maxvalue){
		//            break;
		//        }
		//        if (abs(abs(dp[ind_i].first) - abs(dp1[ind_j].first)) / abs(dp[ind_i].first) < eps2){
		//            if (dp[ind_i].first.imag() == 0){    // if the eigenvalue is real, only corresponds to one eigenvector
		//                re.push_back(ind_i);    // put the eigenvector index into re
		//                ind_i++;
		//                i_re++;
		//            }
		//            else if (abs(dp[ind_i].first.imag() + dp[ind_i + 1].first.imag()) / abs(dp[ind_i].first.imag()) < 1.e-3){    // if the eigenvalue is imaginary, corresponds to two eigenvectors
		//                if (dp[ind_i].first.imag() > 0){    // first put the positive eigenvalue and then put the negative eigenvalue
		//                    re.push_back(ind_i);
		//                    re.push_back(ind_i + 1);
		//                }
		//                else if (dp[ind_i].first.imag() < 0){
		//                    re.push_back(ind_i + 1);
		//                    re.push_back(ind_i);
		//                }
		//                
		//                ind_i += 2;    // two conjugate eigenvalues
		//                i_re += 2;
		//            }
		//        }
		//        
		//        else if (abs(abs(dp[ind_i].first) - abs(dp1[ind_j].first)) / abs(dp[ind_i].first) >= eps2 && abs(dp[ind_i].first) > abs(dp1[ind_j].first)){
		//            ind_j++;
		//        }
		//        else if (abs(abs(dp[ind_i].first) - abs(dp1[ind_j].first)) / abs(dp[ind_i].first) >= eps2 && abs(dp[ind_i].first) < abs(dp1[ind_j].first)){
		//            if (dp[ind_i].first.imag() == 0){
		//                nre.push_back(ind_i);
		//                ind_i++;
		//                i_nre++;
		//            }
		//            else{
		//                if (abs(dp[ind_i].first.imag() + dp[ind_i + 1].first.imag()) / abs(dp[ind_i].first.imag()) < 1.e-3){
		//                    if (dp[ind_i].first.imag() > 0){    // first put the positive eigenvalue and then put the negative eigenvalue
		//                        nre.push_back(ind_i);
		//                        nre.push_back(ind_i + 1);
		//                    }
		//                    else if (dp[ind_i].first.imag() < 0){
		//                        nre.push_back(ind_i + 1);
		//                        nre.push_back(ind_i);
		//                    }
		//                }
		//                ind_i += 2;    // two conjugate eigenvalues
		//                i_nre += 2;
		//            }
		//        }
		//    }
		//    
		//    while (ind_i < dp.size()){
		//        if (dp[ind_i].first.imag() == 0){
		//            nre.push_back(ind_i);
		//            ind_i++;
		//            i_nre++;
		//        }
		//        else if (abs(dp[ind_i].first.imag() + dp[ind_i + 1].first.imag()) / abs(dp[ind_i].first.imag()) < 1.e-5){
		//            if (dp[ind_i].first.imag() > 0){    // first put the positive eigenvalue and then put the negative eigenvalue
		//                nre.push_back(ind_i);
		//                nre.push_back(ind_i + 1);
		//            }
		//            else if (dp[ind_i].first.imag() < 0){
		//                nre.push_back(ind_i + 1);
		//                nre.push_back(ind_i);
		//            }
		//            ind_i += 2;    // two conjugate eigenvalues
		//            i_nre += 2;
		//        }
		//        else if (abs(dp[ind_i].first) > maxvalue){
		//            nre.push_back(ind_i);
		//            ind_i++;
		//            i_nre++;
		//        }

		//    }
		//    /*for (myint inde = 0; inde < dp.size(); inde++){
		//        cout << dp[inde].first << " ";
		//    }
		//    cout << endl;*/
		//    free(V_re);
		//    V_re = (lapack_complex_double*)calloc((sys->N_edge - sys->bden) * i_re, sizeof(lapack_complex_double));
		//    free(V_nre);
		//    V_nre = (lapack_complex_double*)calloc((sys->N_edge - sys->bden) * i_nre, sizeof(lapack_complex_double));

		//    i_re = 0;
		//    
		//    for (myint rei = 0; rei < re.size(); rei++){
		//        if (dp[re[rei]].first.imag() == 0){    // one eigenvector corresponding to real eigenvalue
		//            for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//                for (myint inde3 = 0; inde3 < U0_i - 1; inde3++){
		//                    V_re[i_re * (sys->N_edge - sys->bden) + inde].real += U0[inde3][inde] * vr[dp[re[rei]].second * 2 * (U0_i - 1) + inde3];    // only get the first half eigenvectors
		//                }
		//            }
		//            i_re++;
		//        }
		//        else{    // two eigenvectors corresponding to imaginary eigenvalue
		//            if (dp[re[rei]].first.imag() > 0){
		//                for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//                    for (myint inde3 = 0; inde3 < U0_i - 1; inde3++){
		//                        V_re[i_re * (sys->N_edge - sys->bden) + inde].real = V_re[i_re * (sys->N_edge - sys->bden) + inde].real + U0[inde3][inde] * vr[dp[re[rei]].second * 2 * (U0_i - 1) + inde3];
		//                        V_re[i_re * (sys->N_edge - sys->bden) + inde].imag = V_re[i_re * (sys->N_edge - sys->bden) + inde].imag + U0[inde3][inde] * vr[dp[re[rei + 1]].second * 2 * (U0_i - 1) + inde3];
		//                        V_re[(i_re + 1) * (sys->N_edge - sys->bden) + inde].real = V_re[(i_re + 1) * (sys->N_edge - sys->bden) + inde].real + U0[inde3][inde] * vr[(dp[re[rei]].second) * 2 * (U0_i - 1) + inde3];
		//                        V_re[(i_re + 1) * (sys->N_edge - sys->bden) + inde].imag = V_re[(i_re + 1) * (sys->N_edge - sys->bden) + inde].imag - U0[inde3][inde] * vr[dp[re[rei + 1]].second * 2 * (U0_i - 1) + inde3];
		//                    }
		//                }
		//                rei++;
		//            }
		//            else if (dp[re[rei]].first.imag() < 0){
		//                for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//                    for (myint inde3 = 0; inde3 < U0_i - 1; inde3++){
		//                        V_re[i_re * (sys->N_edge - sys->bden) + inde].real = V_re[i_re * (sys->N_edge - sys->bden) + inde].real + U0[inde3][inde] * vr[dp[re[rei + 1]].second * 2 * (U0_i - 1) + inde3];
		//                        V_re[i_re * (sys->N_edge - sys->bden) + inde].imag = V_re[i_re * (sys->N_edge - sys->bden) + inde].imag - U0[inde3][inde] * vr[dp[re[rei]].second * 2 * (U0_i - 1) + inde3];
		//                        V_re[(i_re + 1) * (sys->N_edge - sys->bden) + inde].real = V_re[(i_re + 1) * (sys->N_edge - sys->bden) + inde].real + U0[inde3][inde] * vr[dp[re[rei + 1]].second * 2 * (U0_i - 1) + inde3];
		//                        V_re[(i_re + 1) * (sys->N_edge - sys->bden) + inde].imag = V_re[(i_re + 1) * (sys->N_edge - sys->bden) + inde].imag + U0[inde3][inde] * vr[dp[re[rei]].second * 2 * (U0_i - 1) + inde3];
		//                    }
		//                }
		//                rei++;
		//            }
		//            i_re += 2;
		//        }
		//    }

		//    
		//    i_nre = 0;
		//    for (myint nrei = 0; nrei < nre.size(); nrei++){
		//        if (abs(dp[nre[nrei]].first) > maxvalue){    // if this eigenvalue is infinity
		//            for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//                for (myint inde3 = 0; inde3 < U0_i - 1; inde3++){
		//                    V_nre[i_nre * (sys->N_edge - sys->bden) + inde].real += U0[inde3][inde] * vr[dp[nre[nrei]].second * 2 * (U0_i - 1) + inde3];
		//                }
		//            }
		//            i_nre++;
		//            continue;
		//        }
		//        if (dp[nre[nrei]].first.imag() == 0){    // one eigenvector corresponding to real eigenvalue
		//            for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//                for (myint inde3 = 0; inde3 < U0_i - 1; inde3++){
		//                    V_nre[i_nre * (sys->N_edge - sys->bden) + inde].real += U0[inde3][inde] * vr[dp[nre[nrei]].second * 2 * (U0_i - 1) + inde3];
		//                }
		//            }
		//            i_nre++;
		//        }
		//        else{    // two eigenvectors corresponding to imaginary eigenvalue
		//            if (dp[nre[nrei]].first.imag() > 0){
		//                for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//                    for (myint inde3 = 0; inde3 < U0_i - 1; inde3++){
		//                        V_nre[i_nre * (sys->N_edge - sys->bden) + inde].real = V_nre[i_nre * (sys->N_edge - sys->bden) + inde].real + U0[inde3][inde] * vr[dp[nre[nrei]].second * 2 * (U0_i - 1) + inde3];
		//                        V_nre[i_nre * (sys->N_edge - sys->bden) + inde].imag = V_nre[i_nre * (sys->N_edge - sys->bden) + inde].imag + U0[inde3][inde] * vr[dp[nre[nrei + 1]].second * 2 * (U0_i - 1) + inde3];
		//                        V_nre[(i_nre + 1) * (sys->N_edge - sys->bden) + inde].real = V_nre[(i_nre + 1) * (sys->N_edge - sys->bden) + inde].real + U0[inde3][inde] * vr[(dp[nre[nrei]].second) * 2 * (U0_i - 1) + inde3];
		//                        V_nre[(i_nre + 1) * (sys->N_edge - sys->bden) + inde].imag = V_nre[(i_nre + 1) * (sys->N_edge - sys->bden) + inde].imag - U0[inde3][inde] * vr[dp[nre[nrei + 1]].second * 2 * (U0_i - 1) + inde3];
		//                    }
		//                }
		//                nrei++;
		//            }
		//            else if (dp[nre[nrei]].first.imag() < 0){
		//                for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//                    for (myint inde3 = 0; inde3 < U0_i - 1; inde3++){
		//                        V_nre[i_nre * (sys->N_edge - sys->bden) + inde].real = V_nre[i_nre * (sys->N_edge - sys->bden) + inde].real + U0[inde3][inde] * vr[dp[nre[nrei + 1]].second * 2 * (U0_i - 1) + inde3];
		//                        V_nre[i_nre * (sys->N_edge - sys->bden) + inde].imag = V_nre[i_nre * (sys->N_edge - sys->bden) + inde].imag - U0[inde3][inde] * vr[dp[nre[nrei]].second * 2 * (U0_i - 1) + inde3];
		//                        V_nre[(i_nre + 1) * (sys->N_edge - sys->bden) + inde].real = V_nre[(i_nre + 1) * (sys->N_edge - sys->bden) + inde].real + U0[inde3][inde] * vr[dp[nre[nrei + 1]].second * 2 * (U0_i - 1) + inde3];
		//                        V_nre[(i_nre + 1) * (sys->N_edge - sys->bden) + inde].imag = V_nre[(i_nre + 1) * (sys->N_edge - sys->bden) + inde].imag + U0[inde3][inde] * vr[dp[nre[nrei]].second * 2 * (U0_i - 1) + inde3];
		//                    }
		//                }
		//                nrei++;
		//            }
		//            i_nre += 2;
		//        }
		//    }
		//    cout << "Solve generalized eigenvale problem time is " << (clock() - t1) * 1.0 / CLOCKS_PER_SEC << endl;
		//    /*cout << "The step No is " << ind << endl;
		//    cout << "The repeated eigenvalues are of No. " << i_re << endl;
		//    cout << "The non-repeated eigenvalues are of No. " << i_nre << endl;*/
		//    
		//    t1 = clock();
		//    // orthogonalized V_re
		//    if (i_re > 0){
		//        /*outfile.open("V_re.txt", std::ofstream::out | std::ofstream::trunc);
		//        for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
		//            for (myint inde2 = 0; inde2 < i_re; inde2++){
		//                outfile << V_re[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real << " " << V_re[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag << " ";
		//            }
		//            outfile << endl;
		//        }
		//        outfile.close();*/
		//        /*tau_qr = (lapack_complex_double*)malloc(i_re * sizeof(lapack_complex_double));
		//        RR = (lapack_complex_double*)calloc(i_re * i_re, sizeof(lapack_complex_double));

		//        info = LAPACKE_zgeqrf(LAPACK_COL_MAJOR, (sys->N_edge - 2 * sys->N_edge_s), i_re, V_re, (sys->N_edge - 2 * sys->N_edge_s), tau_qr);
		//        
		//        for (myint inde = 0; inde < i_re; inde++){
		//            for (myint inde1 = 0; inde1 <= inde; inde1++){
		//                RR[inde * i_re + inde1].real = V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde1].real;
		//                RR[inde * i_re + inde1].imag = V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde1].imag;
		//            }
		//        }
		//        info = LAPACKE_zungqr(LAPACK_COL_MAJOR, (sys->N_edge - 2 * sys->N_edge_s), i_re, i_re, V_re, (sys->N_edge - 2 * sys->N_edge_s), tau_qr);
		//        
		//        free(tau_qr); tau_qr = NULL;*/

		//       /* outfile.open("RR.txt", std::ofstream::out | std::ofstream::trunc);
		//        for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
		//            for (myint inde2 = 0; inde2 < i_re; inde2++){
		//                outfile << RR[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real << " " << RR[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag << " ";
		//            }
		//            outfile << endl;
		//        }
		//        outfile.close();*/
		//        

		//        /*i_re1 = 0;
		//        for (myint inde = 0; inde < i_re; inde++){
		//            if (sqrt(pow(RR[inde * i_nre + inde].real, 2) + pow(RR[inde * i_nre + inde].imag, 2)) > 1.e-12){
		//                i_re1++;
		//            }
		//        }
		//        free(V_re1);
		//        V_re1 = (lapack_complex_double*)calloc((sys->N_edge - 2 * sys->N_edge_s) * i_re1, sizeof(lapack_complex_double));
		//        i_re1 = 0;
		//        for (myint inde = 0; inde < i_re; inde++){
		//            if (sqrt(pow(RR[inde * i_nre + inde].real, 2) + pow(RR[inde * i_nre + inde].imag, 2)) > 1.e-12){
		//                for (myint inde1 = 0; inde1 < sys->N_edge - 2 * sys->N_edge_s; inde1++){
		//                    V_re1[i_re1 * (sys->N_edge - 2 * sys->N_edge_s) + inde1].real = V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde1].real;
		//                    V_re1[i_re1 * (sys->N_edge - 2 * sys->N_edge_s) + inde1].imag = V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde1].imag;
		//                }
		//                i_re1++;
		//            }
		//        }
		//        free(RR); RR = NULL;*/
		//        /*outfile.open("V_re1.txt", std::ofstream::out | std::ofstream::trunc);
		//        for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
		//            for (myint inde2 = 0; inde2 < i_re; inde2++){
		//                outfile << V_re[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real << " " << V_re[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag << " ";
		//            }
		//            outfile << endl;
		//        }
		//        outfile.close();*/


		//        for (myint inde = 0; inde < i_re; inde++){
		//            nn = 0;
		//            for (myint inde1 = 0; inde1 < sys->N_edge - sys->bden; inde1++){
		//                nn += pow(V_re[inde * (sys->N_edge - sys->bden) + inde1].real, 2) + pow(V_re[inde * (sys->N_edge - sys->bden) + inde1].imag, 2);
		//            }
		//            nn = sqrt(nn);
		//            for (myint inde1 = 0; inde1 < sys->N_edge - sys->bden; inde1++){
		//                V_re[inde * (sys->N_edge - sys->bden) + inde1].real = V_re[inde * (sys->N_edge - sys->bden) + inde1].real / nn;
		//                V_re[inde * (sys->N_edge - sys->bden) + inde1].imag = V_re[inde * (sys->N_edge - sys->bden) + inde1].imag / nn;
		//            }
		//            /*for (myint inde1 = inde + 1; inde1 < i_re; inde1++){
		//                nnr = 0;
		//                nni = 0;
		//                for (myint inde2 = 0; inde2 < sys->N_edge - 2 * sys->N_edge_s; inde2++){
		//                    nnr += V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real * V_re[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real
		//                        + V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag * V_re[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag;
		//                    nni += -V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag * V_re[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real
		//                        +V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real * V_re[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag;
		//                }
		//                
		//                qc = (lapack_complex_double*)calloc((sys->N_edge - 2 * sys->N_edge_s), sizeof(lapack_complex_double));
		//                for (myint inde2 = 0; inde2 < sys->N_edge - 2 * sys->N_edge_s; inde2++){
		//                    qc[inde2].real = V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real * nnr - nni * V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag;
		//                    qc[inde2].imag = nnr * V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag + nni * V_re[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real;
		//                    
		//                   
		//                    V_re[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real -= qc[inde2].real;
		//                    V_re[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag -= qc[inde2].imag;

		//                    
		//                }
		//                free(qc); qc = NULL;
		//            }*/
		//            
		//        }
		//        
		//        

		//        if (i_nre > 0){
		//            tmp3 = (lapack_complex_double*)calloc(i_re * i_nre, sizeof(lapack_complex_double));
		//            tmp4 = (lapack_complex_double*)calloc((sys->N_edge - sys->bden) * i_nre, sizeof(lapack_complex_double));
		//            m_re = (lapack_complex_double*)calloc(i_re * i_re, sizeof(lapack_complex_double));
		//            
		//            status = matrix_multi('T', V_re, (sys->N_edge - sys->bden), i_re, V_nre, (sys->N_edge - sys->bden), i_nre, tmp3);
		//            status = matrix_multi('T', V_re, (sys->N_edge - sys->bden), i_re, V_re, (sys->N_edge - sys->bden), i_re, m_re);
		//            ipiv = (lapack_int*)malloc((i_nre + i_re) * sizeof(lapack_int));
		//            info = LAPACKE_zgesv(LAPACK_COL_MAJOR, i_re, i_nre, m_re, i_re, ipiv, tmp3, i_re);// , y_nre1, i_nre + i_re, &iter);
		//            status = matrix_multi('N', V_re, (sys->N_edge - sys->bden), i_re, tmp3, i_re, i_nre, tmp4);
		//            free(ipiv); ipiv = NULL;
		//            free(m_re); m_re = NULL;

		//            //V_nre_norm = (double*)calloc(i_nre, sizeof(double));
		//            for (myint inde2 = 0; inde2 < i_nre; inde2++){
		//                for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//                    V_nre[inde2 * (sys->N_edge - sys->bden) + inde].real -= tmp4[inde2 * (sys->N_edge - sys->bden) + inde].real;
		//                    V_nre[inde2 * (sys->N_edge - sys->bden) + inde].imag -= tmp4[inde2 * (sys->N_edge - sys->bden) + inde].imag;
		//                    //V_nre_norm[inde2] += pow(V_nre[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real, 2) + pow(V_nre[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag, 2);
		//                }
		//                //V_nre_norm[inde2] = sqrt(V_nre_norm[inde2]);
		//                //cout << V_nre_norm[inde2] << endl;
		//            }

		//            // orthogonalize V_nre by using Modified GS
		//            for (myint inde = 0; inde < i_nre; inde++){
		//                nn = 0;
		//                for (myint inde1 = 0; inde1 < sys->N_edge - sys->bden; inde1++){
		//                    nn += pow(V_nre[inde * (sys->N_edge - sys->bden) + inde1].real, 2) + pow(V_nre[inde * (sys->N_edge - sys->bden) + inde1].imag, 2);
		//                }
		//                nn = sqrt(nn);
		//                for (myint inde1 = 0; inde1 < sys->N_edge - sys->bden; inde1++){
		//                    V_nre[inde * (sys->N_edge - sys->bden) + inde1].real = V_nre[inde * (sys->N_edge - sys->bden) + inde1].real / nn;
		//                    V_nre[inde * (sys->N_edge - sys->bden) + inde1].imag = V_nre[inde * (sys->N_edge - sys->bden) + inde1].imag / nn;
		//                }
		//                /*for (myint inde1 = inde + 1; inde1 < i_nre; inde1++){
		//                    nnr = 0;
		//                    nni = 0;
		//                    for (myint inde2 = 0; inde2 < sys->N_edge - 2 * sys->N_edge_s; inde2++){
		//                        nnr += V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real * V_nre[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real
		//                            + V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag * V_nre[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag;
		//                        nni += -V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag * V_nre[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real
		//                            + V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real * V_nre[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag;
		//                    }

		//                    qc = (lapack_complex_double*)calloc((sys->N_edge - 2 * sys->N_edge_s), sizeof(lapack_complex_double));
		//                    for (myint inde2 = 0; inde2 < sys->N_edge - 2 * sys->N_edge_s; inde2++){
		//                        qc[inde2].real = V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real * nnr - nni * V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag;
		//                        qc[inde2].imag = nnr * V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag + nni * V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real;


		//                        V_nre[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].real -= qc[inde2].real;
		//                        V_nre[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde2].imag -= qc[inde2].imag;


		//                    }
		//                    free(qc); qc = NULL;
		//                }*/

		//            }

		//           // make each column vector to be of norm 1
		//           /*for (myint inde2 = 0; inde2 < i_nre; inde2++){

		//               for (myint inde = 0; inde < (sys->N_edge - 2 * sys->N_edge_s); inde++){
		//                   V_nre[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real = V_nre[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real / V_nre_norm[inde2];
		//                   V_nre[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag = V_nre[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag / V_nre_norm[inde2];
		//                   
		//               }
		//           }
		//           free(V_nre_norm); V_nre_norm = NULL;*/
		//           
		//           /*tau_qr = (lapack_complex_double*)malloc(i_nre * sizeof(lapack_complex_double));
		//           RR = (lapack_complex_double*)calloc(i_nre * i_nre, sizeof(lapack_complex_double));
		//           info = LAPACKE_zgeqrf(LAPACK_COL_MAJOR, (sys->N_edge - 2 * sys->N_edge_s), i_nre, V_nre, (sys->N_edge - 2 * sys->N_edge_s), tau_qr);
		//           for (myint inde = 0; inde < i_nre; inde++){
		//               for (myint inde1 = 0; inde1 <= inde; inde1++){
		//                   RR[inde * i_nre + inde1].real = V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde1].real;
		//                   RR[inde * i_nre + inde1].imag = V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde1].imag;
		//               }
		//           }
		//           info = LAPACKE_zungqr(LAPACK_COL_MAJOR, (sys->N_edge - 2 * sys->N_edge_s), i_nre, i_nre, V_nre, (sys->N_edge - 2 * sys->N_edge_s), tau_qr);
		//           free(tau_qr); tau_qr = NULL;
		//           i_nre1 = 0;
		//           for (myint inde = 0; inde < i_nre; inde++){
		//               if (sqrt(pow(RR[inde * i_nre + inde].real, 2) + pow(RR[inde * i_nre + inde].imag, 2)) > 1.e-12){
		//                   i_nre1++;
		//               }
		//           }
		//           free(V_nre1);
		//           V_nre1 = (lapack_complex_double*)calloc((sys->N_edge - 2 * sys->N_edge_s) * i_nre1, sizeof(lapack_complex_double));
		//           i_nre1 = 0;
		//           for (myint inde = 0; inde < i_nre; inde++){
		//               if (sqrt(pow(RR[inde * i_nre + inde].real, 2) + pow(RR[inde * i_nre + inde].imag, 2)) > 1.e-12){
		//                   for (myint inde1 = 0; inde1 < sys->N_edge - 2 * sys->N_edge_s; inde1++){
		//                       V_nre1[i_nre1 * (sys->N_edge - 2 * sys->N_edge_s) + inde1].real = V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde1].real;
		//                       V_nre1[i_nre1 * (sys->N_edge - 2 * sys->N_edge_s) + inde1].imag = V_nre[inde * (sys->N_edge - 2 * sys->N_edge_s) + inde1].imag;
		//                   }
		//                   i_nre1++;
		//               }
		//           }
		//           free(RR); RR = NULL;*/
		//                /*outfile.open("V_nre.txt", std::ofstream::out | std::ofstream::trunc);
		//                for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
		//                    for (myint inde2 = 0; inde2 < i_nre; inde2++){
		//                        outfile << V_nre[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real << " " << V_nre[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag << " ";
		//                    }
		//                    outfile << endl;
		//                }
		//                outfile.close();*/


		//            free(tmp3); tmp3 = NULL;
		//            free(tmp4); tmp4 = NULL;
		//            cout << "Repeated eigenvalues number is " << i_re << " and non-repeated eigenvalues number is " << i_nre << endl;
		//        }
		//    }
		//    cout << "The time to generate orthogonalized V_re and V_nre is " << (clock() - t1) * 1.0 / CLOCKS_PER_SEC << endl;
		//    
		//    if (i_re == 0){
		//        dp.clear();
		//        for (myint inde = 0; inde < dp1.size(); inde++){
		//            dp.push_back(dp1[inde]);
		//        }
		//        free(vr); vr = NULL;
		//        vr = (double*)malloc(4 * U0_i * U0_i * sizeof(double));
		//        for (myint inde = 0; inde < 2 * U0_i; inde++){
		//            for (myint inde2 = 0; inde2 < 2 * U0_i; inde2++){
		//                vr[inde * 2 * U0_i + inde2] = vr1[inde * 2 * U0_i + inde2];
		//            }
		//        }
		//        free(vr1); vr1 = NULL;
		//        free(vl1); vl1 = NULL;
		//        //free(V_re); V_re = NULL;
		//        //free(V_nre); V_nre = NULL;
		//        free(Cr); Cr = NULL;
		//        free(D_epsr); D_epsr = NULL;
		//        free(D_sigr); D_sigr = NULL;
		//        free(Ar); Ar = NULL;
		//        free(Br); Br = NULL;
		//        free(alphar); alphar = NULL;
		//        free(alphai); alphai = NULL;
		//        free(beta); beta = NULL;
		//        for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//            xr[inde] = xr[1 * (sys->N_edge - sys->bden) + inde];
		//            xr[1 * (sys->N_edge - sys->bden) + inde] = xr[2 * (sys->N_edge - sys->bden) + inde];
		//            xr[2 * (sys->N_edge - sys->bden) + inde] = 0;
		//        }
		//        /*if (ind >= 5000)
		//            break;*/
		//        cout << endl;
		//        continue;
		//    }
		//    else if (i_nre == 0){    // all the eigenvalues are repeated
		//        //free(V_nre); V_nre = NULL;
		//        //free(V_re); V_re = NULL;
		//        //break;
		//        dp.clear();
		//        for (myint inde = 0; inde < dp1.size(); inde++){
		//            dp.push_back(dp1[inde]);
		//        }
		//        free(vr); vr = NULL;
		//        vr = (double*)malloc(4 * U0_i * U0_i * sizeof(double));
		//        for (myint inde = 0; inde < 2 * U0_i; inde++){
		//            for (myint inde2 = 0; inde2 < 2 * U0_i; inde2++){
		//                vr[inde * 2 * U0_i + inde2] = vr1[inde * 2 * U0_i + inde2];
		//            }
		//        }
		//        free(vr1); vr1 = NULL;
		//        free(vl1); vl1 = NULL;
		//        //free(V_re); V_re = NULL;
		//        //free(V_nre); V_nre = NULL;
		//        free(Cr); Cr = NULL;
		//        free(D_epsr); D_epsr = NULL;
		//        free(D_sigr); D_sigr = NULL;
		//        free(Ar); Ar = NULL;
		//        free(Br); Br = NULL;
		//        free(alphar); alphar = NULL;
		//        free(alphai); alphai = NULL;
		//        free(beta); beta = NULL;
		//        for (myint inde = 0; inde < (sys->N_edge - sys->bden); inde++){
		//            xr[inde] = xr[1 * (sys->N_edge - sys->bden) + inde];
		//            xr[1 * (sys->N_edge - sys->bden) + inde] = xr[2 * (sys->N_edge - sys->bden) + inde];
		//            xr[2 * (sys->N_edge - sys->bden) + inde] = 0;
		//        }
		//        /*if (ind >= 5000)
		//            break;*/
		//        cout << endl;
		//        continue;
		//    }
		//    else{
		//        t1 = clock();
		//        y_new = (lapack_complex_double*)calloc((i_re + i_nre), sizeof(lapack_complex_double));
		//        for (myint inde = 0; inde < i_re; inde++){
		//            for (myint inde2 = 0; inde2 < (sys->N_edge - sys->bden); inde2++){
		//                y_new[inde].real = y_new[inde].real + (V_re[inde * (sys->N_edge - sys->bden) + inde2].real) * xr[1 * (sys->N_edge - sys->bden) + inde2];    //conjugate transpose
		//                y_new[inde].imag = y_new[inde].imag - (V_re[inde * (sys->N_edge - sys->bden) + inde2].imag) * xr[1 * (sys->N_edge - sys->bden) + inde2];    //conjugate transpose

		//            }
		//        }
		//        for (myint inde = 0; inde < i_nre; inde++){
		//            for (myint inde2 = 0; inde2 < (sys->N_edge - sys->bden); inde2++){
		//                y_new[inde + i_re].real = y_new[inde + i_re].real + V_nre[inde * (sys->N_edge - sys->bden) + inde2].real * xr[1 * (sys->N_edge - sys->bden) + inde2];    // conjugate transpose
		//                y_new[inde + i_re].imag = y_new[inde + i_re].imag - V_nre[inde * (sys->N_edge - sys->bden) + inde2].imag * xr[1 * (sys->N_edge - sys->bden) + inde2];
		//            }
		//        }
		//        
		//        
		//        V_new = (lapack_complex_double*)malloc((sys->N_edge - sys->bden) * (i_re + i_nre) * sizeof(lapack_complex_double));
		//        for (myint inde = 0; inde < i_re; inde++){
		//            for (myint inde2 = 0; inde2 < sys->N_edge - sys->bden; inde2++){
		//                V_new[inde * (sys->N_edge - sys->bden) + inde2].real = V_re[inde * (sys->N_edge - sys->bden) + inde2].real;
		//                V_new[inde * (sys->N_edge - sys->bden) + inde2].imag = V_re[inde * (sys->N_edge - sys->bden) + inde2].imag;
		//            }
		//        }
		//        for (myint inde = i_re; inde < i_re + i_nre; inde++){
		//            for (myint inde2 = 0; inde2 < sys->N_edge - sys->bden; inde2++){
		//                V_new[inde * (sys->N_edge - sys->bden) + inde2].real = V_nre[(inde - i_re) * (sys->N_edge - sys->bden) + inde2].real;
		//                V_new[inde * (sys->N_edge - sys->bden) + inde2].imag = V_nre[(inde - i_re) * (sys->N_edge - sys->bden) + inde2].imag;
		//            }
		//        }
		//        

		//        m_new = (lapack_complex_double*)calloc((i_nre + i_re) * (i_nre + i_re), sizeof(lapack_complex_double));
		//        ipiv = (lapack_int*)malloc((i_nre + i_re) * sizeof(lapack_int));
		//        //y_nre1 = (lapack_complex_double*)calloc(i_nre + i_re, sizeof(lapack_complex_double));
		//        status = matrix_multi('T', V_new, (sys->N_edge - sys->bden), i_nre + i_re, V_new, (sys->N_edge - sys->bden), i_nre + i_re, m_new);
		//        
		//        info = LAPACKE_zgesv(LAPACK_COL_MAJOR, i_nre + i_re, 1, m_new, i_nre + i_re, ipiv, y_new, i_nre + i_re);// , y_nre1, i_nre + i_re, &iter);
		//        x_re = 0;
		//        x_nre = 0;
		//        for (myint inde = 0; inde < i_re; inde++){
		//            x_re += pow(y_new[inde].real, 2) + pow(y_new[inde].imag, 2);
		//        }
		//        for (myint inde = i_re; inde < i_nre + i_re; inde++){
		//            x_nre += pow(y_new[inde].real, 2) + pow(y_new[inde].imag, 2);
		//        }
		//        cout << "The weight on step " << ind << " is " << x_nre / x_re << endl;
		//        //cout << "U0 size is " << U0_i << endl;
		//        cout << "Weight calculation time is " << (clock() - t1) * 1.0 / CLOCKS_PER_SEC << endl;
		//        cout << endl;
		//        free(V_new); V_new = NULL;
		//        free(y_new); y_new = NULL;
		//        
		//        //free(y_nre1); y_nre1 = NULL;
		//        free(ipiv); ipiv = NULL;
		//        
		//        free(m_new); m_new = NULL;
		//        
		//        //free(V_nre); V_nre = NULL;
		//        free(Cr); Cr = NULL;
		//        
		//        free(D_epsr); D_epsr = NULL;
		//        
		//        free(D_sigr); D_sigr = NULL;
		//        free(Ar); Ar = NULL;
		//        free(Br); Br = NULL;
		//        free(alphar); alphar = NULL;
		//        free(alphai); alphai = NULL;
		//        free(beta); beta = NULL;
		//        if  ((x_nre / x_re) < eps1){
		//            free(vr1); vr1 = NULL;
		//            free(vl1); vl1 = NULL;
		//            break;
		//        }
		//        else{
		//            dp.clear();
		//            for (myint inde = 0; inde < dp1.size(); inde++){
		//                dp.push_back(dp1[inde]);
		//            }
		//            free(vr); vr = NULL;
		//            vr = (double*)malloc(4 * U0_i * U0_i * sizeof(double));
		//            for (myint inde = 0; inde < 2 * U0_i; inde++){
		//                for (myint inde2 = 0; inde2 < 2 * U0_i; inde2++){
		//                    vr[inde * 2 * U0_i + inde2] = vr1[inde * 2 * U0_i + inde2];
		//                }
		//            }
		//            free(vr1); vr1 = NULL;
		//            free(vl1); vl1 = NULL;
		//            //free(V_re); V_re = NULL;
		//            
		//        }
		//        
		//    }
		//    
		//}
	//}


        
        
	//out.close();
    //temp.clear();
    //temp1.clear();
    //temp2.clear();
    //U0.clear();
    /* deduct the u0 space */
    /*outfile.open("u0.txt", std::ofstream::out | std::ofstream::trunc);
    for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
        for (myint inde1 = 0; inde1 < 2; inde1++){
            outfile << u0[inde1 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real << " ";
        }
        outfile << endl;
    }
    outfile.close();*/
    
    //sys->Vh = (lapack_complex_double*)malloc((sys->N_edge - sys->bden) * i_re * sizeof(lapack_complex_double));
    //lapack_complex_double *V_re2 = (lapack_complex_double*)malloc((sys->N_edge - sys->bden) * i_re * sizeof(lapack_complex_double));
    //for (myint inde = 0; inde < sys->N_edge - sys->bden; inde++){    // A*V_re
    //    for (myint inde2 = 0; inde2 < i_re; inde2++){
    //        sys->Vh[inde2 * (sys->N_edge - sys->bden) + inde].real = V_re[inde2 * (sys->N_edge - sys->bden) + inde].real;
    //        sys->Vh[inde2 * (sys->N_edge - sys->bden) + inde].imag = V_re[inde2 * (sys->N_edge - sys->bden) + inde].imag;
    //        //V_re2[inde2 * (sys->N_edge - sys->bden) + inde].real = V_re[inde2 * (sys->N_edge - sys->bden) + inde].real * (-sys->freqStart * sys->freqUnit * sys->freqStart * sys->freqUnit * 4 * pow(M_PI, 2)) * sys->eps[inde + sys->N_edge_s]
    //        //    - 2 * M_PI * sys->freqStart * sys->freqUnit * V_re[inde2 * (sys->N_edge - sys->bden) + inde].imag * sys->sig[inde + sys->N_edge_s];
    //        //V_re2[inde2 * (sys->N_edge - sys->bden) + inde].imag = V_re[inde2 * (sys->N_edge - sys->bden) + inde].real * (sys->freqStart * sys->freqUnit * 2 * M_PI) * sys->sig[inde + sys->N_edge_s]
    //        //    - V_re[inde2 * (sys->N_edge - sys->bden) + inde].imag * (sys->freqStart * sys->freqUnit * sys->freqStart * sys->freqUnit * 4 * pow(M_PI, 2)) * sys->eps[inde + sys->N_edge_s];
    //    }
    //}

    //y_re = (lapack_complex_double*)calloc(2 * i_re, sizeof(lapack_complex_double));    // u0a'*A*V_re
    //status = matrix_multi('T', u0a, (sys->N_edge - 2 * sys->N_edge_s), 2, V_re2, (sys->N_edge - 2 * sys->N_edge_s), i_re, y_re);

    //tmp3 = (lapack_complex_double*)calloc((sys->N_edge - 2 * sys->N_edge_s) * 2, sizeof(lapack_complex_double));
    //for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){    // A*u0
    //    for (myint inde2 = 0; inde2 < 2; inde2++){
    //        tmp3[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real = u0[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real * (-sys->freqStart * sys->freqUnit * sys->freqStart * sys->freqUnit * 4 * pow(M_PI, 2)) * sys->eps[inde + sys->N_edge_s]
    //            - 2 * M_PI * sys->freqStart * sys->freqUnit * u0[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag * sys->sig[inde + sys->N_edge_s];
    //        tmp3[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag = u0[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real * (sys->freqStart * sys->freqUnit * 2 * M_PI) * sys->sig[inde + sys->N_edge_s]
    //            - u0[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag * (sys->freqStart * sys->freqUnit * sys->freqStart * sys->freqUnit * 4 * pow(M_PI, 2)) * sys->eps[inde + sys->N_edge_s];
    //    }
    //}

    //tmp4 = (lapack_complex_double*)calloc(2 * 2, sizeof(lapack_complex_double));    // u0a'*A*u0
    //status = matrix_multi('T', u0a, (sys->N_edge - 2 * sys->N_edge_s), 2, tmp3, (sys->N_edge - 2 * sys->N_edge_s), 2, tmp4);    // u0a'*A*u0
    //ipiv = (lapack_int*)malloc(2 * sizeof(lapack_int));
    //y_new = (lapack_complex_double*)calloc(2 * i_re, sizeof(lapack_complex_double));
    ///*outfile.open("ma.txt", std::ofstream::out | std::ofstream::trunc);
    //for (myint inde = 0; inde < 2; inde++){
    //    for (myint inde1 = 0; inde1 < 2; inde1++){
    //        outfile << tmp4[inde1 * 2 + inde].real << " " << tmp4[inde1 * 2 + inde].imag << " ";
    //    }
    //    outfile << endl;
    //}*/
    //info = LAPACKE_zcgesv(LAPACK_COL_MAJOR, 2, i_re, tmp4, 2, ipiv, y_re, 2, y_new, 2, &iter);    // (u0a'*A*u0)\(u0a'*A*V_re)
    //m_new = (lapack_complex_double*)calloc((sys->N_edge - 2 * sys->N_edge_s) * i_re, sizeof(lapack_complex_double));
    //status = matrix_multi('N', u0, (sys->N_edge - 2 * sys->N_edge_s), 2, y_new, 2, i_re, m_new);    // u0*((u0a'*A*u0)\(u0a'*A*V_re))
    //for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
    //    for (myint inde2 = 0; inde2 < i_re; inde2++){
    //        sys->Vh[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real = sys->Vh[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real -m_new[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real;
    //        sys->Vh[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag = sys->Vh[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag -m_new[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag;
    //    }
    //}
    //ofstream out;
    
    /*out.open("Vh.txt", std::ofstream::out | std::ofstream::trunc);
    for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
        for (myint inde2 = 0; inde2 < i_re; inde2++){
            out << sys->Vh[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real << " " << sys->Vh[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag << " ";
        }
        out << endl;
    }
    out.close();*/

    /*out.open("u0.txt", std::ofstream::out | std::ofstream::trunc);
    for (myint inde = 0; inde < sys->N_edge - 2 * sys->N_edge_s; inde++){
        for (myint inde2 = 0; inde2 < 2; inde2++){
            out << u0[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].real << " " << u0[inde2 * (sys->N_edge - 2 * sys->N_edge_s) + inde].imag << " ";
        }
        out << endl;
    }
    out.close();*/


    //sys->leng_Vh = i_re;

    //free(y_re); y_re = NULL;
    //free(V_re2); V_re2 = NULL;
    //free(tmp3); tmp3 = NULL;
    //free(tmp4); tmp4 = NULL;
    //free(ipiv); ipiv = NULL;
    //free(y_new); y_new = NULL;
    //free(m_new); m_new = NULL;
    

    return 0;

}

int matrix_multi(char operation, lapack_complex_double *a, myint arow, myint acol, lapack_complex_double *b, myint brow, myint bcol, lapack_complex_double *tmp3){
    /* operation = 'T' is first matrix conjugate transpose, operation = 'N' is first matrix non-conjugate-transpose*/
    if (operation == 'T'){
        for (myint ind = 0; ind < acol; ind++){
            for (myint ind1 = 0; ind1 < bcol; ind1++){
                tmp3[ind1 * acol + ind].real = 0;
                tmp3[ind1 * acol + ind].imag = 0;
                for (myint ind2 = 0; ind2 < arow; ind2++){
                    tmp3[ind1 * acol + ind].real = tmp3[ind1 * acol + ind].real + a[ind * arow + ind2].real * b[ind1 * brow + ind2].real + a[ind * arow + ind2].imag * b[ind1 * brow + ind2].imag;
                    tmp3[ind1 * acol + ind].imag = tmp3[ind1 * acol + ind].imag - a[ind * arow + ind2].imag * b[ind1 * brow + ind2].real + a[ind * arow + ind2].real * b[ind1 * brow + ind2].imag;
                }
            }
        }
    }
    else if (operation == 'N'){
        for (myint ind = 0; ind < arow; ind++){
            for (myint ind1 = 0; ind1 < bcol; ind1++){
                tmp3[ind1 * arow + ind].real = 0;
                tmp3[ind1 * arow + ind].imag = 0;
                for (myint ind2 = 0; ind2 < acol; ind2++){
                    tmp3[ind1 * arow + ind].real = tmp3[ind1 * arow + ind].real + a[ind2 * arow + ind].real * b[ind1 * brow + ind2].real - a[ind2 * arow + ind].imag * b[ind1 * brow + ind2].imag;
                    tmp3[ind1 * arow + ind].imag = tmp3[ind1 * arow + ind].imag + a[ind2 * arow + ind].imag * b[ind1 * brow + ind2].real + a[ind2 * arow + ind].real * b[ind1 * brow + ind2].imag;
                }
            }
        }
    }
    return 0;
}

void get1122Block_count(myint leng11, myint leng22, fdtdMesh* sys, myint& leng_A12, myint& leng_A21,  myint &leng_A22) {
	myint ind;

	for (ind = 0; ind < sys->leng_Ll; ind++) {
		if (sys->LlRowIdo[ind] >= 0 && sys->LlRowIdo[ind] < leng11 && sys->LlColId[ind] >= leng11 && sys->LlColId[ind] < leng22) {
			leng_A12++;
		}
		else if (sys->LlRowIdo[ind] >= leng11 && sys->LlRowIdo[ind] < leng22 && sys->LlColId[ind] >= 0 && sys->LlColId[ind] < leng11) {
			leng_A21++;
		}
		else if (sys->LlRowIdo[ind] >= leng11 && sys->LlRowIdo[ind] < leng22 && sys->LlColId[ind] >= leng11 && sys->LlColId[ind] < leng22) {
			leng_A22++;
		}
	}
}

void get1122Block(myint leng11, myint leng22, fdtdMesh* sys, myint* A12RowId, myint* A12ColId, double* A12val, myint* A21RowId, myint* A21ColId, double* A21val, myint* A22RowId, myint* A22ColId, double* A22val) {
	myint ind, leng_A12 = 0, leng_A21 = 0, leng_A22 = 0;
	

	for (ind = 0; ind < sys->leng_Ll; ind++) {
		if (sys->LlRowIdo[ind] >= 0 && sys->LlRowIdo[ind] < leng11 && sys->LlColId[ind] >= leng11 && sys->LlColId[ind] < leng22) {
			A12RowId[leng_A12] = sys->LlRowIdo[ind];
			A12ColId[leng_A12] = sys->LlColId[ind] - leng11;
			A12val[leng_A12] = sys->Llval[ind];
			leng_A12++;
		}
		else if (sys->LlRowIdo[ind] >= leng11 && sys->LlRowIdo[ind] < leng22 && sys->LlColId[ind] >= 0 && sys->LlColId[ind] < leng11) {
			A21RowId[leng_A21] = sys->LlRowIdo[ind] - leng11;
			A21ColId[leng_A21] = sys->LlColId[ind];
			A21val[leng_A21] = sys->Llval[ind];
			leng_A21++;
		}
		else if (sys->LlRowIdo[ind] >= leng11 && sys->LlRowIdo[ind] < leng22 && sys->LlColId[ind] >= leng11 && sys->LlColId[ind] < leng22) {
			A22RowId[leng_A22] = sys->LlRowIdo[ind] - leng11;
			A22ColId[leng_A22] = sys->LlColId[ind] - leng11;
			A22val[leng_A22] = sys->Llval[ind];
			leng_A22++;
		}
	}

}

int mkl_gmres(fdtdMesh* sys, double* bm, double* x, sparse_matrix_t Ll, myint* A22RowId, myint* A22ColId, double* A22val, myint leng_A22, sparse_matrix_t v0ct, sparse_matrix_t v0cat, sparse_matrix_t v0dt, sparse_matrix_t v0dat) {
	/* Use MKL fgmres to generate the matrix solution of Ll * x = bm 
	   Ll = [V0a' * (D_eps + dt * D_sig) * V0, V0a' * (D_eps + dt *  D_sig);
	         (D_eps + dt * D_sig) * V0, (D_eps + dt * D_sig + dt ^ 2 * L)] 
	   bm = [V0a' * b; 
	         b]
	   x solution (which solution? final solution or the solution from the matrix equation)*/


	myint N = sys->leng_v0d1 + sys->leng_v0c + sys->N_edge - sys->bden;
	cout << "boundary edge number is " << sys->bden << endl;
	myint size = 128;

	/*------------------------------------------------------------------------------------
	/* Allocate storage for the ?par parameters and the solution/rhs/residual vectors
	/*------------------------------------------------------------------------------------*/
	MKL_INT ipar[size];
	ipar[14] = 100;
	//double b[N];
	//double expected_solution[N];
	//double computed_solution[N];
	//double residual[N];
	double* b = (double*)malloc(N * sizeof(double));
	double* expected_solution = (double*)malloc(N * sizeof(double));
	double* computed_solution = (double*)malloc(N * sizeof(double));
	double* residual = (double*)malloc(N * sizeof(double));
	double dpar[size];
	double* tmp = (double*)calloc(N*(2 * ipar[14] + 1) + (ipar[14] * (ipar[14] + 9)) / 2 + 1, sizeof(double));
	/*---------------------------------------------------------------------------
	/* Some additional variables to use with the RCI (P)FGMRES solver
	/*---------------------------------------------------------------------------*/
	MKL_INT itercount;
	MKL_INT RCI_request, i, ivar;
	ivar = N;
	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;
	sparse_status_t s;
	double dvar;
	int status;
	ofstream out;
	/*---------------------------------------------------------------------------
	/* Save the right-hand side in vector b for future use
	/*---------------------------------------------------------------------------*/
	i = 0;
	for (i = 0; i < N; ++i) {
		b[i] = bm[i];
	}
	/*--------------------------------------------------------------------------
	/* Initialize the initial guess
	/*--------------------------------------------------------------------------*/
	for (i = 0; i < N; ++i) {
		computed_solution[i] =  bm[i];
	}
	double bmn = 0;

	/*--------------------------------------------------------------------------
	/* Initialize the solver
	/*--------------------------------------------------------------------------*/
	dfgmres_init(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	if (RCI_request != 0) goto FAILED;
	ipar[10] = 1;   // the Preconditioned FGMRES iterations will be performed
	ipar[14] = 100;   // restart number
	ipar[7] = 0;
	dpar[0] = 1.0E-3;
	/*---------------------------------------------------------------------------
	/* Check the correctness and consistency of the newly set parameters
	/*---------------------------------------------------------------------------*/
	dfgmres_check(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	if (RCI_request != 0) goto FAILED;
ONE: dfgmres(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	//bmn = 0;
	//for (int ind = 0; ind < ivar; ++ind) {
	//	bmn += computed_solution[ind] * computed_solution[ind];
	//}
	//cout << "Right hand side norm is " << bmn << endl;
	if (RCI_request == 0) {
		goto COMPLETE;
	}

	/*---------------------------------------------------------------------------
	/* If RCI_request=1, then compute the vector A*tmp[ipar[21]-1]
	/* and put the result in vector tmp[ipar[22]-1]
	/*---------------------------------------------------------------------------*/
	if (RCI_request == 1) {
		s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, Ll, descr, &tmp[ipar[21] - 1], beta, &tmp[ipar[22] - 1]);
		//out.open("x1.txt", std::ofstream::trunc | std::ofstream::out);
		//for (int indi = 0; indi < sys->leng_v0d1 + sys->leng_v0c + sys->N_edge; ++indi) {
		//	out << tmp[ipar[21] - 1 + indi] << endl;
		//}
		//out.close();

		//out.open("b1.txt", std::ofstream::trunc | std::ofstream::out);
		//for (int indi = 0; indi < sys->leng_v0d1 + sys->leng_v0c + sys->N_edge; ++indi) {
		//	out << tmp[ipar[22] - 1 + indi] << endl;
		//}
		//out.close();

		goto ONE;
	}

	/* do the user-defined stopping test */
	if (RCI_request == 2) {
		ipar[12] = 1;
		/* Get the current FGMRES solution in the vector b[N] */
		dfgmres_get(&ivar, computed_solution, b, &RCI_request, ipar, dpar, tmp, &itercount);
		/* Compute the current true residual via MKL (Sparse) BLAS routines */
		s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, Ll, descr, &b[0], beta, &residual[0]);
		dvar = -1.0E0;
		i = 1;
		daxpy(&ivar, &dvar, bm, &i, residual, &i);
		dvar = cblas_dnrm2(ivar, residual, i) / cblas_dnrm2(ivar, bm, i);    // relative residual
		//bmn = 0;
		//for (int ind = 0; ind < ivar; ++ind) {
		//		bmn += b[ind] * b[ind];
		//}
		//cout << "The solution is " << bmn << endl;

		cout << "The relative residual is " << dvar << " with iteration number " << itercount << endl;
		if (dvar < 0.0001) goto COMPLETE;
		else goto ONE;
	}

	/* apply the preconditioner on the vector tmp[ipar[21]-1] and put the result in vector tmp[ipar[22]-1] */
	if (RCI_request == 3) {
		
		status = applyPrecond(sys, &tmp[ipar[21] - 1], &tmp[ipar[22] - 1], A22RowId, A22ColId, A22val, leng_A22, v0ct, v0cat, v0dt, v0dat, 2);
		goto ONE;
	}

	/* check if the norm of the next generated vector is not zero up to rounding and computational errors. */
	if (RCI_request == 4) {
		//if (dpar[6] < 1.0E-12) goto COMPLETE;
		//else goto ONE;
		goto ONE;
	}

	else {
		goto FAILED;
	}

COMPLETE: ipar[12] = 0;
	dfgmres_get(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp, &itercount);
	cout << "Number of iterations: " << itercount << endl;
	for (i = 0; i < ivar; ++i) {
		x[i] = computed_solution[i];
	}
	goto SUCCEDED;
FAILED: cout << "The solver has returned the ERROR code " << RCI_request << endl;

SUCCEDED: free(b); b = NULL;
	free(expected_solution); expected_solution = NULL;
	free(computed_solution); computed_solution = NULL;
	free(residual); residual = NULL;
	return 0;
}

int mkl_gmres_A(double* bm, double* x, myint* ARowId, myint* AColId, double* Aval, myint leng_A, myint N) {
	/* Use MKL fgmres to generate the matrix solution of (D_epsoo + dt^2 * Loo) with no preconditioner
	(D_epsoo + dt^2 * Loo)
	x solution 
	N : size of the matrix*/


	myint size = 128;

	/*------------------------------------------------------------------------------------
	/* Allocate storage for the ?par parameters and the solution/rhs/residual vectors
	/*------------------------------------------------------------------------------------*/
	MKL_INT ipar[size];
	ipar[14] = 200;
	//double b[N];
	//double expected_solution[N];
	//double computed_solution[N];
	//double residual[N];
	double* b = (double*)malloc(N * sizeof(double));
	double* expected_solution = (double*)malloc(N * sizeof(double));
	double* computed_solution = (double*)malloc(N * sizeof(double));
	double* residual = (double*)malloc(N * sizeof(double));
	double dpar[size];
	double* tmp = (double*)calloc(N*(2 * ipar[14] + 1) + (ipar[14] * (ipar[14] + 9)) / 2 + 1, sizeof(double));
	cout << "    The size of tmp is " << (N*(2 * ipar[14] + 1) + (ipar[14] * (ipar[14] + 9)) / 2 + 1) * 8 << endl;
	/*---------------------------------------------------------------------------
	/* Some additional variables to use with the RCI (P)FGMRES solver
	/*---------------------------------------------------------------------------*/
	MKL_INT itercount;
	MKL_INT RCI_request, i, ivar;
	ivar = N;
	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;
	sparse_status_t s;
	double dvar;
	int status, maxit = 200;
	ofstream out;
	out.open("iteration_error_S.txt", std::ofstream::out | std::ofstream::trunc);
	/*---------------------------------------------------------------------------
	/* Save the right-hand side in vector b for future use
	/*---------------------------------------------------------------------------*/
	i = 0;
	for (i = 0; i < N; ++i) {
		b[i] = bm[i];
	}
	/*--------------------------------------------------------------------------
	/* Initialize the initial guess
	/*--------------------------------------------------------------------------*/
	for (i = 0; i < N; ++i) {
		computed_solution[i] = 0;// bm[i];
	}
	double bmn = 0;

	/*--------------------------------------------------------------------------
	/* Initialize the solver
	/*--------------------------------------------------------------------------*/
	dfgmres_init(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	if (RCI_request != 0) goto FAILED;
	ipar[10] = 0;   // preconditioner
	ipar[14] = 200;   // restart number
	ipar[7] = 0;
	dpar[0] = 1.0E-2;
	/*---------------------------------------------------------------------------
	/* Check the correctness and consistency of the newly set parameters
	/*---------------------------------------------------------------------------*/
	dfgmres_check(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	if (RCI_request != 0) goto FAILED;
ONE: dfgmres(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	//bmn = 0;
	//for (int ind = 0; ind < ivar; ++ind) {
	//	bmn += computed_solution[ind] * computed_solution[ind];
	//}
	//cout << "Right hand side norm is " << bmn << endl;
	if (RCI_request == 0) {
		goto COMPLETE;
	}

	/*---------------------------------------------------------------------------
	/* If RCI_request=1, then compute the vector A*tmp[ipar[21]-1]
	/* and put the result in vector tmp[ipar[22]-1]
	/*---------------------------------------------------------------------------*/
	if (RCI_request == 1) {
		for (myint ind = 0; ind < N; ++ind) {
			tmp[ipar[22] - 1 + ind] = 0;    // before using sparseMatrixVecMul, the resultant vector should be first initialized
		}
		sparseMatrixVecMul(ARowId, AColId, Aval, leng_A, &tmp[ipar[21] - 1], &tmp[ipar[22] - 1]);
		

		goto ONE;
	}

	/* do the user-defined stopping test */
	if (RCI_request == 2) {
		ipar[12] = 1;
		/* Get the current FGMRES solution in the vector b[N] */
		dfgmres_get(&ivar, computed_solution, b, &RCI_request, ipar, dpar, tmp, &itercount);
		/* Compute the current true residual via MKL (Sparse) BLAS routines */
		
		for (myint ind = 0; ind < N; ++ind) {
			residual[ind] = 0;   // before using sparseMatrixVecMul, the resultant vector should be first initialized
		}
		sparseMatrixVecMul(ARowId, AColId, Aval, leng_A, &b[0], &residual[0]);
		dvar = -1.0E0;
		i = 1;
		daxpy(&ivar, &dvar, bm, &i, residual, &i);
		dvar = cblas_dnrm2(ivar, residual, i) / cblas_dnrm2(ivar, bm, i);    // relative residual
		out << itercount << " " << dvar << endl;
		//cout << "The relative residual is " << dvar << " with iteration number " << itercount << endl;
		if (dvar < dpar[0] || itercount > maxit) goto COMPLETE;
		else goto ONE;
	}

	/* apply the preconditioner on the vector tmp[ipar[21]-1] and put the result in vector tmp[ipar[22]-1] */
	if (RCI_request == 3) {
		/* diagonal acts as the preconditioner */
		for (int ind = 0; ind < leng_A; ++ind) {
			if (ARowId[ind] == AColId[ind]) {   // the diagonal part
				tmp[ipar[22] - 1 + ARowId[ind]] = tmp[ipar[21] - 1 + ARowId[ind]] / Aval[ind];
			}
		}
		goto ONE;
	}

	/* check if the norm of the next generated vector is not zero up to rounding and computational errors. */
	if (RCI_request == 4) {
		//if (dpar[6] < 1.0E-12) goto COMPLETE;
		//else goto ONE;
		goto ONE;
	}

	else {
		goto FAILED;
	}

COMPLETE: ipar[12] = 0;
	dfgmres_get(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp, &itercount);
	cout << endl << "      The relative residual is " << dvar << " with iteration number " << itercount << endl << endl;
	for (i = 0; i < ivar; ++i) {
		x[i] = computed_solution[i];
	}
	goto SUCCEDED;
FAILED: cout << "The solver has returned the ERROR code " << RCI_request << endl;

SUCCEDED: free(b); b = NULL;
	free(expected_solution); expected_solution = NULL;
	free(computed_solution); computed_solution = NULL;
	free(residual); residual = NULL;
	free(tmp); tmp = NULL;
	out.close();
	return 0;
}



int combine_x(double* x, fdtdMesh* sys, double* xr) {
	/* Compute xr = V0 * y0 + xh 
	   x = [y0; xh]
	   xr is the destination */
	myint ind;

	for (ind = 0; ind < sys->v0d1num; ++ind) {
		xr[sys->mapEdge[sys->v0d1RowId[ind]]] += sys->v0d1valo[ind] * x[sys->v0d1ColIdo[ind]] / sys->v0dn[sys->v0d1ColIdo[ind]];
	}
	for (ind = 0; ind < sys->v0cnum; ++ind) {
		xr[sys->mapEdge[sys->v0cRowId[ind]]] += sys->v0cvalo[ind] * x[sys->leng_v0d1 + sys->v0cColIdo[ind]] / sys->v0cn[sys->v0cColIdo[ind]];
	}
	for (ind = 0; ind < sys->N_edge - sys->bden; ++ind) {
		xr[ind] += x[sys->leng_v0c + sys->leng_v0d1 + ind];
	}
	return 0;
}

int applyPrecond(fdtdMesh* sys, double* b1, double* b2, myint* A22RowId, myint* A22ColId, double* A22val, myint leng_A22, sparse_matrix_t v0ct, sparse_matrix_t v0cat, sparse_matrix_t v0dt, sparse_matrix_t v0dat, int choice) {
	/* b1 : right hand side
	   b2 : solution */
	ofstream out;
	if (choice == 1) {
		/* Preconditioner : [v0da'*D_eps*v0d,v0da'*D_eps*v0c,   0;
							 0,              v0ca'*dt*D_sig*v0c,0;
							 0,              0,                ,D_eps+dt*D_sig+dt^2*L] */
		int status;
		double alpha = 1, beta = 0;
		struct matrix_descr descr;
		descr.type = SPARSE_MATRIX_TYPE_GENERAL;
		sparse_status_t s;


		/* Apply the preconditioner to a vector b1 and get b2 */
		double* temp = (double*)calloc((sys->N_edge - sys->bden), sizeof(double));
		double* temp1 = (double*)calloc(sys->leng_v0d1, sizeof(double));
		double* temp2 = (double*)calloc(sys->leng_v0c, sizeof(double));
		status = hypreSolve(A22RowId, A22ColId, A22val, leng_A22, &b1[sys->leng_v0d1 + sys->leng_v0c], sys->N_edge - sys->bden, &b2[sys->leng_v0d1 + sys->leng_v0c], 0, 3);

		/* (V0ca'*(D_sig*dt)*V0c)*y0c=b2 */
		for (myint indi = 0; indi < sys->leng_v0c; ++indi) {   // Ac is not normalized with V0ca and V0c
			temp2[indi] = b1[sys->leng_v0d1 + indi] * sys->v0can[indi];
		}
		status = hypreSolve(sys->AcRowId, sys->AcColId, sys->Acval, sys->leng_Ac, temp2, sys->leng_v0c, &b2[sys->leng_v0d1], 0, 3);
		for (myint indi = 0; indi < sys->leng_v0c; ++indi) {
			b2[sys->leng_v0d1 + indi] /= (DT / sys->v0cn[indi]);
		}

		/* (V0da'*D_eps*V0d)*y0d+V0da'*D_eps*V0c*y0c=b1 */
		for (myint indi = 0; indi < sys->leng_v0c; ++indi) {
			temp2[indi] = b2[sys->leng_v0d1 + indi] / sys->v0cn[indi];
		}
		s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, v0ct, descr, temp2, beta, temp);
		for (myint indi = 0; indi < sys->N_edge - sys->bden; ++indi) {
			temp[indi] *= sys->getEps(sys->mapEdgeR[indi]);
		}

		s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0dat, descr, temp, beta, temp1); // temp1 = V0da'*D_eps*normalized(V0c)*yc
		for (myint indi = 0; indi < sys->leng_v0d1; ++indi) {
			temp1[indi] = b1[indi] - temp1[indi] / sys->v0dan[indi];   // temp1 = b1 - normalized(V0da)'*D_eps*normalized(V0c)*yc
		}
		status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, temp1, sys->leng_v0d1, b2, 0, 3);

		//out.open("b1.txt", std::ofstream::trunc | std::ofstream::out);
		//for (int indi = 0; indi < sys->leng_v0d1 + sys->leng_v0c + sys->N_edge; ++indi) {
		//	out << b1[indi] << endl;
		//}
		//out.close();

		//out.open("x1.txt", std::ofstream::trunc | std::ofstream::out);
		//for (int indi = 0; indi < sys->leng_v0d1 + sys->leng_v0c + sys->N_edge; ++indi) {
		//	out << b2[indi] << endl;
		//}
		//out.close();



		free(temp); temp = NULL;
		free(temp1); temp1 = NULL;
		free(temp2); temp2 = NULL;

	}
	else if (choice == 2) {


		/* Preconditioner : [v0da'*D_eps*v0d,v0da'*D_eps*v0c,   v0da'*D_eps;
							 0,              v0ca'*dt*D_sig*v0c,v0ca'*(D_eps+dt*D_sig);
							 0,              0,                 D_eps+dt*D_sig+dt^2*L] */
		/* Solve (D_eps+dt*D_sig+dt^2*L) */
		int status;
		double alpha = 1, beta = 0;
		struct matrix_descr descr;
		descr.type = SPARSE_MATRIX_TYPE_GENERAL;
		sparse_status_t s;

		status = hypreSolve(A22RowId, A22ColId, A22val, leng_A22, &b1[sys->leng_v0d1 + sys->leng_v0c], sys->N_edge - sys->bden, &b2[sys->leng_v0d1 + sys->leng_v0c], 0, 3);  // solve y3

		/* v0ca' line */
		double* temp = (double*)malloc((sys->N_edge - sys->bden) * sizeof(double));
		double* temp1 = (double*)malloc(sys->leng_v0c * sizeof(double));
		for (int ind = 0; ind < sys->N_edge - sys->bden; ++ind) {   // temp = (D_eps+dt*D_sig)*y3;
			if (sys->markEdge[sys->mapEdgeR[ind]]) {
				temp[ind] = b2[sys->leng_v0d1 + sys->leng_v0c + ind] * (sys->getEps(sys->mapEdgeR[ind]) + DT * SIGMA);
			}
			else {
				temp[ind] = b2[sys->leng_v0d1 + sys->leng_v0c + ind] * (sys->getEps(sys->mapEdgeR[ind]));
			}
		}
		s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0cat, descr, temp, beta, temp1);   // temp1 = v0ca'*(D_eps+dt*D_sig)*y3
		for (int ind = 0; ind < sys->leng_v0c; ++ind) {
			temp1[ind] /= sys->v0can[ind];   // normalized v0ca
		}
		// b2-v0ca'*(D_eps+dt*D_sig)*y3
		for (int ind = 0; ind < sys->leng_v0c; ++ind) {
			temp1[ind] = b1[sys->leng_v0d1 + ind] - temp1[ind];   // temp1 = b2-v0ca'*(D_eps+dt*D_sig)*y3
		}
		for (int indi = 0; indi < sys->leng_v0c; ++indi) {   // Ac is not normalized with V0ca and V0c
			temp1[indi] *= sys->v0can[indi];
		}

		// solve (v0ca'*dt*D_sig*v0c)
		status = hypreSolve(sys->AcRowId, sys->AcColId, sys->Acval, sys->leng_Ac, temp1, sys->leng_v0c, &b2[sys->leng_v0d1], 0, 3);
		for (int indi = 0; indi < sys->leng_v0c; ++indi) {
			b2[sys->leng_v0d1 + indi] /= (DT / sys->v0cn[indi]);
		}

		/* v0da' line */
		double* temp2 = (double*)malloc(sys->leng_v0d1 * sizeof(double));
		double* temp3 = (double*)malloc(sys->leng_v0d1 * sizeof(double));
		for (int ind = 0; ind < sys->N_edge - sys->bden; ++ind) {
			temp[ind] = b2[sys->leng_v0d1 + sys->leng_v0c + ind] * (sys->getEps(sys->mapEdgeR[ind]));   // temp = D_eps*y3
		}
		s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0dat, descr, temp, beta, temp2);   // temp2 = v0da'*D_eps*y3
		for (int ind = 0; ind < sys->leng_v0d1; ++ind) {
			temp2[ind] /= sys->v0dan[ind];    // normalized v0da
		}
		for (int ind = 0; ind < sys->leng_v0c; ++ind) {
			temp1[ind] = b2[sys->leng_v0d1 + ind] / sys->v0cn[ind];   //temp1 = y2 first normalized
		}

		s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, v0ct, descr, temp1, beta, temp);   // temp = v0c*y2
		for (int ind = 0; ind < sys->N_edge - sys->bden; ++ind) {
			temp[ind] *= (sys->getEps(sys->mapEdgeR[ind]));   // temp = D_eps*V0c*y2
		}
		s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0dat, descr, temp, beta, temp3);   // temp = v0da'*D_eps*v0c*y2
		for (int ind = 0; ind < sys->leng_v0d1; ++ind) {
			temp3[ind] /= sys->v0dan[ind];   // v0da is normalized
		}

		for (int ind = 0; ind < sys->leng_v0d1; ++ind) {
			temp2[ind] += temp3[ind];   // temp2 = v0da'*D_eps*v0c*y2+v0da'*D_eps*y3
			temp2[ind] = b1[ind] - temp2[ind];   // temp2 = b1-v0da1'*D_eps*v0c*y2-v0da'*D_eps*y3
		}
		status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, temp2, sys->leng_v0d1, b2, 0, 3);   // solve y1


		free(temp);
		free(temp1);
		free(temp2);
		free(temp3);
	}
	else if (choice == 3) {
		/* Preconditioner : [v0da'*D_eps*v0d, v0da'*D_eps*v0c,   0;
							 V0ca'*D_eps*V0c, v0ca'*dt*D_sig*v0c,0;
							 0,               0,                 D_eps+dt*D_sig+dt^2*L] */
		/* Solve (D_eps+dt*D_sig+dt^2*L) */
		int status;
		double alpha = 1, beta = 0;
		struct matrix_descr descr;
		descr.type = SPARSE_MATRIX_TYPE_GENERAL;
		sparse_status_t s;
		myint edge = sys->N_edge - sys->bden;

		status = hypreSolve(A22RowId, A22ColId, A22val, leng_A22, &b1[sys->leng_v0d1 + sys->leng_v0c], sys->N_edge - sys->bden, &b2[sys->leng_v0d1 + sys->leng_v0c], 0, 3);  // solve y3

		double* temp = (double*)calloc(sys->leng_v0d1, sizeof(double));
		double* temp1 = (double*)calloc(sys->leng_v0c, sizeof(double));
		double* temp2 = (double*)calloc(sys->leng_v0c, sizeof(double));
		double* temp3 = (double*)calloc(edge, sizeof(double));
		double* xd = (double*)calloc(sys->leng_v0d1, sizeof(double));
		double* xc = (double*)calloc(sys->leng_v0c, sizeof(double));
		for (int i = 0; i < sys->leng_v0c; ++i) {
			temp1[i] = b1[sys->leng_v0d1 + i] * sys->v0can[i];
		}
		status = hypreSolve(sys->AcRowId, sys->AcColId, sys->Acval, sys->leng_Ac, temp1, sys->leng_v0c, temp2, 0, 3);
		for (int indi = 0; indi < sys->leng_v0c; ++indi) {
			temp2[indi] /= (DT / sys->v0cn[indi]);   // temp2 = (V0ca'*D_sig*dt*V0c)\(bc)
		}

		for (int indi = 0; indi < sys->leng_v0c; ++indi) {
			temp2[indi] /= sys->v0cn[indi];
		}
		s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, v0ct, descr, temp2, beta, temp3);   // temp3 = V0c*(V0ca'*D_sig*V0c)\(bc)
		for (int indi = 0; indi < edge; ++indi) {
			temp3[indi] *= sys->getEps(sys->mapEdgeR[indi]);    // temp3 = D_eps*V0c*(V0ca'*D_sig*dt*V0c)\bc
		}
		s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0dat, descr, temp3, beta, temp);   // temp = V0da'*D_eps*V0c*(V0ca'*D_sig*V0c)\(bc)
		for (int indi = 0; indi < sys->leng_v0d1; ++indi) {
			temp[indi] = b1[indi] - temp[indi] / sys->v0dan[indi];   // temp = bd-V0da'*D_eps*V0c*(V0ca'*D_sig*V0c)\(bc)
		}
		status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, temp, sys->leng_v0d1, xd, 0, 3);   // xd=(V0da'*D_eps*V0d)\(bd-V0da'*D_eps*V0c*((V0ca'*D_sig*dt*V0c)\bc))

		for (int indi = 0; indi < sys->leng_v0d1; ++indi) {
			b2[indi] = xd[indi];
			temp[indi] = xd[indi] / sys->v0dn[indi];
		}
		s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, v0dt, descr, temp, beta, temp3);   // temp3 = V0d*xd
		for (int indi = 0; indi < edge; ++indi) {
			temp3[indi] *= sys->getEps(sys->mapEdgeR[indi]);   // temp3 = D_eps*V0d*xd
		}
		s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0cat, descr, temp3, beta, temp1);   // temp1 = V0ca'*D_eps*V0d*xd
		for (int indi = 0; indi < sys->leng_v0c; ++indi) {
			temp2[indi] = b1[sys->leng_v0d1 + indi] - temp1[indi] / sys->v0can[indi];   // temp2 = bc-V0ca'*D_eps*V0d*xd
		}
		for (int indi = 0; indi < sys->leng_v0c; ++indi) {
			temp2[indi] *= sys->v0can[indi];
		}
		status = hypreSolve(sys->AcRowId, sys->AcColId, sys->Acval, sys->leng_Ac, temp2, sys->leng_v0c, xc, 0, 3);
		for (int indi = 0; indi < sys->leng_v0c; ++indi) {
			b2[sys->leng_v0d1 + indi] = xc[indi] / (DT / sys->v0cn[indi]);   // xc = (V0ca'*D_sig*dt*V0c)\(bc-V0ca'*D_eps*V0d*xd)
		}
		free(temp); temp = NULL;
		free(temp1); temp1 = NULL;
		free(temp2); temp2 = NULL;
		free(temp3); temp3 = NULL;
		free(xd); xd = NULL;
		free(xc); xc = NULL;
	}
	
}

void applyPrecond_P(myint* PRowId1, myint* PColId, double* Pval, myint leng_P, myint N, double* x1, double* x2) {
	/* Apply the preconditioner P
	PRowId1 : CSR format of the rowId of matrix P
	PColId : matrix P's colId (the Id is in ascending order in one row)
	Pval : matrix P's value
	leng_P : nnz in P
	N : size of P
	x1 : the right hand side
	x2 : the result */
	pardisoSolve(PRowId1, PColId, Pval, x1, x2, N);
}

int solveBackMatrix(fdtdMesh* sys, double* bm, double* x, sparse_matrix_t& v0ct, sparse_matrix_t& v0cat, sparse_matrix_t& v0dt, sparse_matrix_t& v0dat, myint* A12RowId, myint* A12ColId, double* A12val, myint leng_A12, myint* A21RowId, myint* A21ColId, double* A21val, myint leng_A21, myint* A22RowId, myint* A22ColId, double* A22val, myint leng_A22) {
	/* solve the backward matrix inverse solution
	[V0a'*(D_eps+dt*D_sig)*V0, V0a'*(D_eps+dt*D_sig);
	 (D_eps+dt*D_sig)*V0     , (D_eps+dt*D_sig+dt^2*L)]
	   bm : right hand side
	   x : solution
	   Others are system matrices */
	int status;
	int lengv0 = sys->leng_v0c + sys->leng_v0d1;
	int edge = sys->N_edge - sys->bden;

	/* Generate x2 */
	double* temp = (double*)malloc(lengv0 * sizeof(double));
	double* temp1 = (double*)calloc(edge, sizeof(double));
	double* x1 = (double*)malloc(lengv0 * sizeof(double));
	ofstream out;
	//out.open("bm.txt", std::ofstream::trunc | std::ofstream::out);
	//for (int i = 0; i < lengv0; ++i) {
	//	out << bm[i] << endl;
	//}
	//out.close();
	status = solveA11Matrix(sys, bm, v0ct, v0cat, v0dt, v0dat, temp);   // temp = A11^(-1)*b1
	//out.open("te.txt", std::ofstream::trunc | std::ofstream::out);
	//for (int i = 0; i < lengv0; ++i) {
	//	out << temp[i] << endl;
	//}
	//out.close();
	status = sparseMatrixVecMul(A21RowId, A21ColId, A21val, leng_A21, temp, temp1);    // temp1 = A21*A11^(-1)*b1
	for (int i = 0; i < edge; ++i) {
		temp1[i] = bm[lengv0 + i] - temp1[i];    // temp1 = b2-A21*A11^(-1)*b1
	}
	status = hypreSolve(A22RowId, A22ColId, A22val, leng_A22, temp1, edge, temp1, 1, 3);    // temp1 (x2) = A22^(-1)*(b2-A21*A11^(-1)*b1)
	free(temp); temp = (double*)calloc(lengv0, sizeof(double));
	status = sparseMatrixVecMul(A12RowId, A12ColId, A12val, leng_A12, temp1, temp);    // temp = A12*x2
	for (int i = 0; i < lengv0; ++i) {
		temp[i] = bm[i] - temp[i];    // temp = b1-A12*x2
	}
	status = solveA11Matrix(sys, temp, v0ct, v0cat, v0dt, v0dat, x1);    // x1 = A11^(-1)*(b1-A12*x2)
	for (int i = 0; i < lengv0; ++i) {
		x[i] = x1[i];   // x upper is x1
	}
	for (int i = 0; i < edge; ++i) {
		x[i + lengv0] = temp1[i];    // x lower is x2
	}
	free(temp); temp = NULL;
	free(temp1); temp1 = NULL;
	free(x1); x1 = NULL;
	return 0;
}

int solveA11Matrix(fdtdMesh* sys, double* rhs, sparse_matrix_t& v0ct, sparse_matrix_t& v0cat, sparse_matrix_t& v0dt, sparse_matrix_t& v0dat, double* sol) {
	/* Solve [V0a'*(D_eps+dt*D_sig)*V0]^(-1)
	The matrix is [V0da'*D_eps*V0d, V0da'*D_eps*V0c;
	               V0ca'*D_eps*V0d, V0ca'*(D_eps+dt*D_sig)*V0c] */
	int status;
	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;
	sparse_status_t s;
	myint edge = sys->N_edge - sys->bden;    // reference this value
	ofstream out;

	double* bd1 = (double*)malloc(sys->leng_v0d1 * sizeof(double));
	double* temp = (double*)malloc(edge * sizeof(double));
	double* temp1 = (double*)malloc(sys->leng_v0c * sizeof(double));
	double* xc = (double*)malloc(sys->leng_v0c * sizeof(double));
	status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, rhs, sys->leng_v0d1, bd1, 1, 3);   // bd1 = (v0da'*D_eps*v0d)^(-1)*(bd)
	//out.open("bd1.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < sys->leng_v0d1; ind++) {
	//	out << bd1[ind] << endl;
	//}
	//out.close();
	for (myint i = 0; i < sys->leng_v0d1; ++i) {
		bd1[i] /= sys->v0dn[i];    // v0d is normalized
	}
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, v0dt, descr, bd1, beta, temp); // temp = v0d*(v0da'*D_eps*v0d)^(-1)*bd
	//out.open("temp.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < edge; ++ind) {
	//	out << temp[ind] << endl;
	//}
	//out.close();
	for (myint i = 0; i < edge; ++i) {
		temp[i] = sys->getEps(sys->mapEdgeR[i]) * temp[i];    // temp = D_eps*v0d*(v0da'*D_eps*v0d)^(-1)*bd
	}
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0cat, descr, temp, beta, temp1); // temp1 = v0ca'*D_eps*v0d*(v0da'*D_eps*v0d)^(-1)*bd
	for (myint i = 0; i < sys->leng_v0c; ++i) {
		temp1[i] /= sys->v0can[i];    // v0ca is normalized
	}
	for (myint i = 0; i < sys->leng_v0c; ++i) {
		temp1[i] = rhs[sys->leng_v0d1 + i] - temp1[i];   // temp1 = bc-v0ca'*D_eps*v0d*(v0da'*D_eps*v0d)^(-1)*bd
	}
	
	/* solve v0ca'*D_sig*dt*v0c */
	for (myint i = 0; i < sys->leng_v0c; ++i) {   // Ac is not normalized with V0ca and V0c
		temp1[i] = temp1[i] * sys->v0can[i];
	}
	status = hypreSolve(sys->AcRowId, sys->AcColId, sys->Acval, sys->leng_Ac, temp1, sys->leng_v0c, xc, 1, 3);
	for (myint i = 0; i < sys->leng_v0c; ++i) {
		xc[i] /= (DT / sys->v0cn[i]);    //xc = (v0ca'*dt*D_sig*v0c)^(-1)*(bc-v0ca'*D_eps*v0d*(v0da'*D_eps*v0d)^(-1)*bd)
	}

	for (myint i = 0; i < sys->leng_v0c; ++i) {
		temp1[i] = xc[i] / sys->v0cn[i];    // v0c is normalized
	}
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, v0ct, descr, temp1, beta, temp); // temp = v0c*xc
	for (myint i = 0; i < edge; ++i) {
		temp[i] = sys->getEps(sys->mapEdgeR[i]) * temp[i];    // temp = D_eps*v0c*xc
	}
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0dat, descr, temp, beta, bd1); // bd1 = v0da'*D_eps*v0c*xc
	for (myint i = 0; i < sys->leng_v0d1; ++i) {
		bd1[i] /= sys->v0dan[i];
	}
	for (myint i = 0; i < sys->leng_v0d1; ++i) {
		bd1[i] = rhs[i] - bd1[i];  // bd1 = bd-v0da'*D_eps*v0c*xc
	}
	status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, bd1, sys->leng_v0d1, bd1, 1, 3);   // bd1 (xd) = (v0da'*D_eps*v0d)^(-1)*(bd-v0da'*D_eps*v0c*xc)

	for (myint i = 0; i < sys->leng_v0d1; ++i) {
		sol[i] = bd1[i];
	}
	for (myint i = 0; i < sys->leng_v0c; ++i) {
		sol[i + sys->leng_v0d1] = xc[i];
	}
	free(bd1); bd1 = NULL;
	free(temp); temp = NULL;
	free(temp1); temp1 = NULL;
	free(xc); xc = NULL;
	return 0;
}

int sparseMatrixVecMul(myint* rowId, myint* colId, double* val, myint leng, double* v1, double* v2) {
	/* sparse matrix multiply a dense vector
	   rowId : rowId of this sparse matrix
	   colId : colId of this sparse matrix
	   val : value of this sparse matrix
	   leng : nnz in the matrix
	   v1 : vector
	   v2 : result vector, 
	   Note: v2 need to initialize to be 0 first */
	for (myint i = 0; i < leng; ++i) {

		v2[rowId[i]] += val[i] * v1[colId[i]];
	}
	return 0;
}

int pardisoSolve(myint* rowId, myint* colId, double* val, double* rsc, double* xsol, myint size) {
	/* Use pardiso to solve the matrix solution
	   rowId : matrix rowId, csr form, the start index for each row
	   colId : matrix colId
	   val : matrix values
	   rsc : right hand side
	   xsol : solution, output
	   size : dimension size of the problem 
	   nnz : non*/

	myint mtype = 11;    /* Real and unsymmetric matrix */
	myint nrhs = 1;    /* Number of right hand sides */
	void* pt[64];

	/* Pardiso control parameters */
	myint iparm[64];
	myint maxfct, mnum, phase, error, msglvl, solver;
	double dparm[64];
	int v0csin;
	myint perm;

	/* Auxiliary variables */
	char* var;

	msglvl = 0;    /* print statistical information */
	solver = 0;    /* use sparse direct solver */
	error = 0;
	maxfct = 1;
	mnum = 1;
	phase = 13;

	pardisoinit(pt, &mtype, iparm);
	iparm[38] = 1;
	iparm[34] = 1;    // 0-based indexing
	//iparm[3] = 2;    // number of processors
	//iparm[59] = 2;    // out of core version to solve very large problem
	//iparm[10] = 0;        /* Use nonsymmetric permutation and scaling MPS */

	//cout << "Begin to solve (-w^2*D_eps+iwD_sig+S)x=-iwJ\n";
	double ddum;

	pardiso(pt, &maxfct, &mnum, &mtype, &phase, &size, val, rowId, colId, &perm, &nrhs, iparm, &msglvl, rsc, xsol, &error);
	if (error != 0) {
		printf("\nERROR during numerical factorization: %d", error);
		exit(2);
	}

	phase = -1;     // Release internal memory
	pardiso(pt, &maxfct, &mnum, &mtype, &phase, &size, &ddum, rowId, colId, &perm, &nrhs, iparm, &msglvl, &ddum, &ddum, &error);

	return 0;
}

int pardisoSolve_c(myint* rowId, myint* colId, complex<double>* val, complex<double>* rsc, complex<double>* xsol, myint size) {
	/* Use pardiso to solve the complex matrix solution
	rowId : matrix rowId, csr form, the start index for each row
	colId : matrix colId
	val : matrix values
	rsc : right hand side
	xsol : solution, output
	size : dimension size of the problem
	nnz : non*/

	myint mtype = 13;    /* complex and unsymmetric matrix */
	myint nrhs = 1;    /* Number of right hand sides */
	void* pt[64];

	/* Pardiso control parameters */
	myint iparm[64];
	myint maxfct, mnum, phase, error, msglvl, solver;
	double dparm[64];
	int v0csin;
	myint perm;

	/* Auxiliary variables */
	char* var;

	msglvl = 0;    /* print statistical information */
	solver = 0;    /* use sparse direct solver */
	error = 0;
	maxfct = 1;
	mnum = 1;
	phase = 13;

	pardisoinit(pt, &mtype, iparm);
	iparm[38] = 1;
	iparm[34] = 1;    // 0-based indexing
					  //iparm[3] = 2;    // number of processors
					  //iparm[59] = 2;    // out of core version to solve very large problem
					  //iparm[10] = 0;        /* Use nonsymmetric permutation and scaling MPS */

					  //cout << "Begin to solve (-w^2*D_eps+iwD_sig+S)x=-iwJ\n";
	double ddum;

	pardiso(pt, &maxfct, &mnum, &mtype, &phase, &size, val, rowId, colId, &perm, &nrhs, iparm, &msglvl, rsc, xsol, &error);
	if (error != 0) {
		printf("\nERROR during numerical factorization: %d", error);
		exit(2);
	}

	phase = -1;     // Release internal memory
	pardiso(pt, &maxfct, &mnum, &mtype, &phase, &size, &ddum, rowId, colId, &perm, &nrhs, iparm, &msglvl, &ddum, &ddum, &error);

	return 0;
}

int storeTimeRespValue(fdtdMesh* sys, double** resp, int ind, double* xr) {
	/* Store the time response voltage
	   resp : space used to store the voltage
	   ind : the step number of the time marching
	   xr : the field distribution
	   Note : portDirection is the relative position of the port to the ground. E.g., ground is on the top, then portDirection = -1 */
	int inz, inx, iny;
	double leng;

	for (int port = 0; port < sys->numPorts; ++port) {
		for (int inde = 0; inde < sys->portCoor[port].portEdge[0].size(); inde++) {
			myint thisEdge = sys->portCoor[port].portEdge[0][inde];
			if (thisEdge % (sys->N_edge_s + sys->N_edge_v) >= sys->N_edge_s) {    // This edge is along the z-axis
				inz = thisEdge / (sys->N_edge_s + sys->N_edge_v);
				leng = sys->zn[inz + 1] - sys->zn[inz];
			}
			else if (thisEdge % (sys->N_edge_s + sys->N_edge_v) >= (sys->N_cell_y) * (sys->N_cell_x + 1)) {    // This edge is along the x-axis
				inx = ((thisEdge % (sys->N_edge_s + sys->N_edge_v)) - (sys->N_cell_y) * (sys->N_cell_x + 1)) / (sys->N_cell_y + 1);
				leng = sys->xn[inx + 1] - sys->xn[inx];
			}
			else {    // This edge is along the y-axis
				iny = (thisEdge % (sys->N_edge_s + sys->N_edge_v)) % sys->N_cell_y;
				leng = sys->yn[iny + 1] - sys->yn[iny];
			}
			resp[port][ind] -= xr[sys->mapEdge[sys->portCoor[port].portEdge[0][inde]]] * leng * (sys->portCoor[port].portDirection[0] * 1.0);
		}
	}
}

int mklFFT(fdtdMesh* sys, double* time, complex<double>* freq, int N) {
	/* transfer the time domain response to frequency domain
	   time : time domain signal, size N
	   freq : transfered frequecy domain signal, size N
	   N : number of points in time domain */

	/* Arbitrary harmonic used to verify FFT */
	int H = -1;

	/* Execution status */
	MKL_LONG status = 0;

	/* Pointer to input and output data */
	MKL_Complex16* x_cmplx;

	DFTI_DESCRIPTOR_HANDLE hand = 0;

	char version[DFTI_VERSION_LENGTH];

	DftiGetValue(0, DFTI_VERSION, version);

	/* Create DFIT descriptor */
	status = DftiCreateDescriptor(&hand, DFTI_DOUBLE, DFTI_COMPLEX, 1, (MKL_LONG)N);
	if (status != 0) goto failed;

	/* Set configuration : out-of-place */
	status = DftiSetValue(hand, DFTI_PLACEMENT, DFTI_NOT_INPLACE);
	if (status != 0) goto failed;

	/* Set configuration */
	status = DftiSetValue(hand, DFTI_CONJUGATE_EVEN_STORAGE, DFTI_COMPLEX_COMPLEX);
	if (status != 0) goto failed;

	/* allocate data arrays */
	x_cmplx = (MKL_Complex16*)mkl_malloc((N / 2 + 1) * sizeof(MKL_Complex16), 64);

	/* Compute forward transform */
	status = DftiComputeForward(hand, time, x_cmplx);
	if (status != 0) goto failed;

	/* transfer x_cmplx to freq */
	for (int ind = 0; ind <= N / 2; ++ind) {
		freq[ind] = x_cmplx[ind].real + 1i * x_cmplx[ind].imag;
	}
	for (int ind = N / 2 + 1; ind < N; ++ind) {
		freq[ind] = x_cmplx[(N - ind)].real - 1i * x_cmplx[(N - ind)].imag;
	}
cleanup:
	DftiFreeDescriptor(&hand);
	mkl_free(x_cmplx);
	return status;

failed:
	status = 1;
	goto cleanup;
}

void solveOO(fdtdMesh* sys, double freq, double* Jo, sparse_matrix_t& v0dt, sparse_matrix_t& v0dat, double* xo) {
	/* solve (-omega^2*D_epsoo+Soo)xo=bo
	  freq : frequency
	  Jo : bo=-i*omega*Jo
	  xo : solution
	  first solve with Jo in real domain, at the end solve xo*=-1i*omega
	  [v0da'*(-omega^2*D_epsoo)*v0d, v0da'*(-omega^2*D_epsoo);
	   -omega^2*D_epsoo*v0d,         -omega^2*D_epsoo+Loo]*
	   [yd;xh]=
	   [v0da'*Jo;Jo] */

	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;
	sparse_status_t s;
	int status;
	ofstream out;

	/* xh */
	double* temp = (double*)calloc(sys->leng_v0d1, sizeof(double));
	double* temp1 = (double*)calloc(sys->leng_v0d1, sizeof(double));
	double* temp2 = (double*)calloc(sys->outside, sizeof(double));
	double* xh = (double*)calloc(sys->outside, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0dat, descr, Jo, beta, temp);   // temp=v0da'*Jo
	status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, temp, sys->leng_v0d1, temp1, 1, 3);   // temp1=(v0da'*(D_epsoo)*v0d)^(-1)*v0da'*Jo
	for (myint ind = 0; ind < sys->leng_v0d1; ++ind) {
		temp1[ind] /= (-freq * freq * M_PI * M_PI * 4);    // temp1=(v0da'*(-omega^2*D_epsoo)*v0d)^(-1)*v0da'*Jo
	}
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, v0dt, descr, temp1, beta, temp2);   // temp2=v0d*(v0da'*(-omega^2*D_epsoo)*v0d)^(-1)*v0da'*Jo
	for (myint ind = 0; ind < sys->outside; ++ind) {
		temp2[ind] *= -freq * freq * M_PI * M_PI * 4 * sys->getEps(sys->mapEdgeR[sys->mapioR[ind]]);   // temp2=(-omega^2*D_epsoo*v0d)*(v0da'*(-omega^2*D_epsoo)*v0d)^(-1)*v0da'*Jo
		temp2[ind] = Jo[ind] - temp2[ind];   // temp2=Jo-(-omega^2*D_epsoo*v0d)*(v0da'*(-omega^2*D_epsoo)*v0d)^(-1)*v0da'*Jo
	}
	double* Looval = (double*)calloc(sys->leng_Loo, sizeof(double));
	for (myint ind = 0; ind < sys->leng_Loo; ++ind) {   // define Looval for (-omega^2*D_epsoo+Loo)
		Looval[ind] = sys->Looval[ind];
		if (sys->LooRowId[ind] == sys->LooColId[ind]) {
			Looval[ind] += -pow(freq * M_PI * 2, 2) * sys->getEps(sys->mapEdgeR[sys->mapioR[sys->LooRowId[ind]]]);
		}
	}
	status = mkl_gmres_A(temp2, xh, sys->LooRowId, sys->LooColId, Looval, sys->leng_Loo, sys->outside);   // gmres to solve (dt^2*Loo+D_epsoo), xh is got!
	//myint* LooRowId1 = (myint*)malloc((sys->outside + 1) * sizeof(myint));
	//status = COO2CSR_malloc(sys->LooRowId, sys->LooColId, sys->leng_Loo, sys->outside, LooRowId1);
	//solveOO_pardiso(sys, LooRowId1, sys->LooColId, Looval, sys->leng_Loo, sys->outside, temp2, xh, 1);
	/* Begin to output (Loo-omega^2*D_epsoo) */
	//out.open("Loo.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < sys->leng_Loo; ++ind) {
	//	out << sys->LooRowId[ind] + 1 << " " << sys->LooColId[ind] + 1 << " " << setprecision(15) << Looval[ind] << endl;
	//}
	//out.close();
	//out.open("temp2.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < sys->outside; ++ind) {
	//	out << setprecision(15) << temp2[ind] << endl;
	//}
	//out.close();
	//out.open("xh.txt", std::ofstream::out | std::ofstream::trunc);
	//for (int ind = 0; ind < sys->outside; ++ind) {
	//	out << setprecision(15) << xh[ind] << endl;
	//}
	//out.close();
	/* End of outputting (Loo-omega^2*(D_epsoo) */

	for (myint ind = 0; ind < sys->outside; ++ind) {
		temp2[ind] = xh[ind] * (-pow(freq * M_PI * 2, 2)) * sys->getEps(sys->mapEdgeR[sys->mapioR[ind]]);   // temp2=(-omega^2*D_epsoo)*xh
		temp2[ind] = Jo[ind] - temp2[ind];   // temp2=b-(-omega^2*D_epsoo)*xh
	}
	free(temp); temp = (double*)calloc(sys->leng_v0d1, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, v0dat, descr, temp2, beta, temp);   // temp=v0da'*(b-(-omega^2*D_epsoo)*xh)
	double* yd = (double*)calloc(sys->leng_v0d1, sizeof(double));
	status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, temp, sys->leng_v0d1, yd, 1, 3);   // get yd
	for (myint ind = 0; ind < sys->leng_v0d1; ++ind) {
		yd[ind] /= (-pow(freq * 2 * M_PI, 2));
	}

	/* combine to get the final xo */
	double* xo1 = (double*)calloc(sys->outside, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, v0dt, descr, yd, beta, xo1);   // xo1=v0d*yd
	for (myint ind = 0; ind < sys->outside; ++ind) {
		xo[ind] = - freq * 2 * M_PI * (xh[ind] + xo1[ind]);
	}

	free(temp); temp = NULL;
	free(temp1); temp1 = NULL;
	free(temp2); temp2 = NULL;
	free(xh); xh = NULL;
	free(Looval); Looval = NULL;
	free(yd); yd = NULL;
	free(xo1); xo1 = NULL;
	
}

void solveOO_pardiso(fdtdMesh* sys, myint* MooRowId1, myint* MooColId, double* Mooval, myint leng, myint N, double* Jo, double* xo, int rhs_s) {
	/* Use pardiso to solve Moo*xo=Jo
	MooRowId1 : Moo's CSR rowId
	MooColId : Moo's column id
	Mooval : Moo's value
	leng : nnz in the matrix Moo
	N : size of the matrix Moo
	Jo : right hand side is -1i*omega*Jo
	xo : outside solution
	rhs_s : right hand side size*/

	myint size = N;
	myint mtype = 11;    /* Real complex unsymmetric matrix */
	myint nrhs = rhs_s;    /* Number of right hand sides */
	void* pt[64];

	/* Pardiso control parameters */
	myint iparm[64];
	myint maxfct, mnum, phase, error, msglvl, solver;
	double dparm[64];
	int v0csin;
	myint perm;

	/* Auxiliary variables */
	char* var;

	msglvl = 0;    /* print statistical information */
	solver = 0;    /* use sparse direct solver */
	error = 0;
	maxfct = 1;
	mnum = 1;
	phase = 13;

	pardisoinit(pt, &mtype, iparm);
	iparm[38] = 1;
	iparm[34] = 1;    // 0-based indexing

	//cout << "Begin to solve (-w^2*D_epsoo+Soo)x=-iwJ\n";
	complex<double> ddum;

	pardiso(pt, &maxfct, &mnum, &mtype, &phase, &size, Mooval, MooRowId1, MooColId, &perm, &nrhs, iparm, &msglvl, Jo, xo, &error);
	if (error != 0) {
		printf("\nERROR during numerical factorization: %d", error);
		exit(2);
	}

	phase = -1;     // Release internal memory
	pardiso(pt, &maxfct, &mnum, &mtype, &phase, &size, &ddum, MooRowId1, MooColId, &perm, &nrhs, iparm, &msglvl, &ddum, &ddum, &error);

}

int mkl_gmres_A_P(double* bm, double* x, myint* ARowId, myint* AColId, double* Aval, myint leng_A, myint N, myint* PRowId1, myint* PColId, double* Pval, myint leng_P) {
	/* Use MKL fgmres to generate the matrix solution of A with preconditioner
	bm : rhs
	x solution
	N : size of the matrix
	ARowId : rowId of the matrix A
	AColId : colId of the matrix A
	Aval : value of the matrix A
	PRowId1 : Preconditioner matrix rowId, CSR format of the rowId
	PColId : Preconditioner matrix colId
	Pval : Preconditioner matrix value */


	myint size = 128;
	ofstream out;
	out.open("iteration_erroro.txt", ofstream::out | ofstream::trunc);
	/*------------------------------------------------------------------------------------
	/* Allocate storage for the ?par parameters and the solution/rhs/residual vectors
	/*------------------------------------------------------------------------------------*/
	MKL_INT ipar[size];
	ipar[14] = 1000;
	//double b[N];
	//double expected_solution[N];
	//double computed_solution[N];
	//double residual[N];
	double* b = (double*)malloc(N * sizeof(double));
	double* expected_solution = (double*)malloc(N * sizeof(double));
	double* computed_solution = (double*)malloc(N * sizeof(double));
	double* residual = (double*)malloc(N * sizeof(double));
	double dpar[size];
	double* tmp = (double*)calloc(N*(2 * ipar[14] + 1) + (ipar[14] * (ipar[14] + 9)) / 2 + 1, sizeof(double));
	/*---------------------------------------------------------------------------
	/* Some additional variables to use with the RCI (P)FGMRES solver
	/*---------------------------------------------------------------------------*/
	MKL_INT itercount;
	MKL_INT RCI_request, i, ivar;
	ivar = N;
	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;
	sparse_status_t s;
	double dvar;
	int status, maxit = 1000;
	/*---------------------------------------------------------------------------
	/* Save the right-hand side in vector b for future use
	/*---------------------------------------------------------------------------*/
	i = 0;
	for (i = 0; i < N; ++i) {
		b[i] = bm[i];
	}
	/*--------------------------------------------------------------------------
	/* Initialize the initial guess
	/*--------------------------------------------------------------------------*/
	for (i = 0; i < N; ++i) {
		computed_solution[i] = 0;// bm[i];
	}
	double bmn = 0;

	/*--------------------------------------------------------------------------
	/* Initialize the solver
	/*--------------------------------------------------------------------------*/
	dfgmres_init(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	if (RCI_request != 0) goto FAILED;
	ipar[10] = 0;   // preconditioner
	ipar[14] = 1000;   // restart number
	ipar[7] = 0;
	dpar[0] = 0.01;
	/*---------------------------------------------------------------------------
	/* Check the correctness and consistency of the newly set parameters
	/*---------------------------------------------------------------------------*/
	dfgmres_check(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	if (RCI_request != 0) goto FAILED;
ONE: dfgmres(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
	//bmn = 0;
	//for (int ind = 0; ind < ivar; ++ind) {
	//	bmn += computed_solution[ind] * computed_solution[ind];
	//}
	//cout << "Right hand side norm is " << bmn << endl;
	if (RCI_request == 0) {
		goto COMPLETE;
	}

	/*---------------------------------------------------------------------------
	/* If RCI_request=1, then compute the vector A*tmp[ipar[21]-1]
	/* and put the result in vector tmp[ipar[22]-1]
	/*---------------------------------------------------------------------------*/
	if (RCI_request == 1) {
		for (myint ind = 0; ind < N; ++ind) {
			tmp[ipar[22] - 1 + ind] = 0;    // before using sparseMatrixVecMul, the resultant vector should be first initialized
		}
		sparseMatrixVecMul(ARowId, AColId, Aval, leng_A, &tmp[ipar[21] - 1], &tmp[ipar[22] - 1]);


		goto ONE;
	}

	/* do the user-defined stopping test */
	if (RCI_request == 2) {
		ipar[12] = 1;
		/* Get the current FGMRES solution in the vector b[N] */
		dfgmres_get(&ivar, computed_solution, b, &RCI_request, ipar, dpar, tmp, &itercount);
		/* Compute the current true residual via MKL (Sparse) BLAS routines */

		for (myint ind = 0; ind < N; ++ind) {
			residual[ind] = 0;   // before using sparseMatrixVecMul, the resultant vector should be first initialized
		}
		sparseMatrixVecMul(ARowId, AColId, Aval, leng_A, &b[0], &residual[0]);
		dvar = -1.0E0;
		i = 1;
		daxpy(&ivar, &dvar, bm, &i, residual, &i);
		dvar = cblas_dnrm2(ivar, residual, i) / cblas_dnrm2(ivar, bm, i);    // relative residual
		out << itercount << " " << dvar << endl;
		if (dvar < dpar[0] || itercount > maxit) goto COMPLETE;
		else goto ONE;
	}

	/* apply the preconditioner on the vector tmp[ipar[21]-1] and put the result in vector tmp[ipar[22]-1] */
	if (RCI_request == 3) {
		applyPrecond_P(PRowId1, PColId, Pval, leng_P, N, &tmp[ipar[21] - 1], &tmp[ipar[22] - 1]);    // apply the preconditioner to tmp[ipar[21] - 1] and put the result in tmp[ipar[22] - 1]
		//hypreSolve(PRowId, PColId, Pval, leng_P, &tmp[ipar[21] - 1], N, &tmp[ipar[22] - 1], 0, 3);
		goto ONE;
	}

	/* check if the norm of the next generated vector is not zero up to rounding and computational errors. */
	if (RCI_request == 4) {
		//if (dpar[6] < 1.0E-12) goto COMPLETE;
		//else goto ONE;
		goto ONE;
	}

	else {
		goto FAILED;
	}

COMPLETE: ipar[12] = 0;
	dfgmres_get(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp, &itercount);
	cout << endl << "      The relative residual is " << dvar << " with iteration number " << itercount << endl << endl;
	for (i = 0; i < ivar; ++i) {
		x[i] = computed_solution[i];
	}
	goto SUCCEDED;
FAILED: cout << "The solver has returned the ERROR code " << RCI_request << endl;

SUCCEDED: free(b); b = NULL;
	free(expected_solution); expected_solution = NULL;
	free(computed_solution); computed_solution = NULL;
	free(residual); residual = NULL;
	return 0;
}


int pardisoSolve_factorize(myint* RowId1, myint* ColId, double* val, myint leng, myint size, myint nrhs) {
	/* Use pardiso to solve the matrix solution ----factorization
	RowId1 : matrix rowId, csr form, the start index for each row
	ColId : matrix colId
	val : matrix values
	leng : nnz in the matrix
	size : dimension size of the problem
	nrhs : number of right hand side
	*/

	myint mtype = 11;    /* Real and unsymmetric matrix */
	void* pt[64];

	/* Pardiso control parameters */
	myint iparm[64];
	myint maxfct, mnum, phase, error, msglvl, solver;
	double dparm[64];
	int v0csin;
	myint perm;

	/* Auxiliary variables */
	char* var;

	msglvl = 0;    /* print statistical information */
	solver = 0;    /* use sparse direct solver */
	error = 0;
	maxfct = 1;
	mnum = 1;
	phase = 12;   /* analysis and numerical factorization */

	pardisoinit(pt, &mtype, iparm);
	iparm[38] = 1;
	iparm[34] = 1;    // 0-based indexing
	iparm[3] = 2;    // number of processors
					 //iparm[59] = 2;    // out of core version to solve very large problem
					 //iparm[10] = 0;        /* Use nonsymmetric permutation and scaling MPS */

					 //cout << "Begin to solve (-w^2*D_eps+iwD_sig+S)x=-iwJ\n";
	double ddum;

	pardiso(pt, &maxfct, &mnum, &mtype, &phase, &size, val, RowId1, ColId, &perm, &nrhs, iparm, &msglvl, &ddum, &ddum, &error);
	if (error != 0) {
		printf("\nERROR during numerical factorization: %d", error);
		exit(2);
	}


	return 0;

}

int solveV0L(fdtdMesh* sys, int freqNo, myint* LrowId, myint* LcolId, double* Lval, myint leng, myint N, sparse_matrix_t& V0dt, sparse_matrix_t& V0dat, sparse_matrix_t& V0ct, sparse_matrix_t& V0cat) {
	/* solve matrix [V0da'*(-omega^2*D_eps)*V0d, V0da'*(-omega^2*D_eps)*V0c, V0da'*(-omega^2*D_eps)*0;
	                 V0ca'*(-omega^2*D_eps)*V0d, V0ca'*(-omega^2*D_eps+1i*omega*D_sig)*V0c, V0ca'*(-omega^2*D_eps+1i*omega*D_sig);
					 (-omega^2*D_eps)*V0d, (-omega^2*D_eps+1i*omega*D_sig)*V0c, -omega^2*D_eps+1i*omega*D_sig+L] with upper matrix as the preconditioner
				b = [V0da'*(-1i*omega*J); V0ca'*(-1i*omega*J); -1i*omega*J]
				N is this matrix size * 2
				*/
	myint nedge = sys->N_edge - sys->bden;
	double freq = sys->freqNo2freq(freqNo);
	/* Start to generate matrix [-omega^2*D_eps+L, -omega*D_sig;
								omega*D_sig,      -omega^2*D_eps+L] */
	myint leng_L1 = (leng + sys->inside - sys->outside) * 2;
	myint* L1RowId = (myint*)malloc(leng_L1 * sizeof(myint));
	myint* L1ColId = (myint*)malloc(leng_L1 * sizeof(myint));
	double* L1val = (double*)malloc(leng_L1 * sizeof(double));
	ofstream out;
	generateL1(sys, freq, LrowId, LcolId, Lval, leng, nedge, L1RowId, L1ColId, L1val, leng_L1);

	double* J;
	for (int sourcePort = 0; sourcePort < sys->numPorts; ++sourcePort) {
		J = (double*)calloc((sys->N_edge - sys->bden), sizeof(double));   // J
		for (int sourcePortSide = 0; sourcePortSide < sys->portCoor[sourcePort].multiplicity; sourcePortSide++) {
			for (int indEdge = 0; indEdge < sys->portCoor[sourcePort].portEdge[sourcePortSide].size(); indEdge++) {
				/* Set current density for all edges within sides in port to prepare solver */
				J[sys->mapEdge[sys->portCoor[sourcePort].portEdge[sourcePortSide][indEdge]]] = sys->portCoor[sourcePort].portDirection[sourcePortSide];
			}
		}


		myint size = 128;

		/*------------------------------------------------------------------------------------
		/* Allocate storage for the ?par parameters and the solution/rhs/residual vectors
		/*------------------------------------------------------------------------------------*/
		MKL_INT ipar[size];
		ipar[14] = 100;
		//double b[N];
		//double expected_solution[N];
		//double computed_solution[N];
		//double residual[N];
		double* b = (double*)calloc(N, sizeof(double));
		double* bm = (double*)calloc(N, sizeof(double));
		double* expected_solution = (double*)malloc(N * sizeof(double));
		double* computed_solution = (double*)malloc(N * sizeof(double));
		double* residual = (double*)malloc(N * sizeof(double));
		double* x = (double*)calloc(N, sizeof(double));
		double dpar[size];
		double* tmp = (double*)calloc(N*(2 * ipar[14] + 1) + (ipar[14] * (ipar[14] + 9)) / 2 + 1, sizeof(double));
		generate_bm(sys, V0dat, sys->leng_v0d1, V0cat, sys->leng_v0c, freq, J, bm, N);   // bm = [0; 0; 0; V0da'*(-omega*J); V0ca'*(-omega*J); -omega*J] the upper part is real and lower part is imaginary
		/*---------------------------------------------------------------------------
		/* Some additional variables to use with the RCI (P)FGMRES solver
		/*---------------------------------------------------------------------------*/
		MKL_INT itercount;
		MKL_INT RCI_request, i, ivar;
		ivar = N;
		double alpha = 1, beta = 0;
		struct matrix_descr descr;
		descr.type = SPARSE_MATRIX_TYPE_GENERAL;
		sparse_status_t s;
		double dvar;
		int status, maxit = 500;
		ofstream out;
		/*---------------------------------------------------------------------------
		/* Save the right-hand side in vector b for future use
		/*---------------------------------------------------------------------------*/
		i = 0;
		//double nn = 0;
		for (i = 0; i < N; ++i) {
			b[i] = bm[i];
			//nn += b[i] * b[i];
		}
		//cout << "Norm of b is " << nn << endl;
		/*--------------------------------------------------------------------------
		/* Initialize the initial guess
		/*--------------------------------------------------------------------------*/
		for (i = 0; i < N; ++i) {
			computed_solution[i] = 0;// bm[i];
		}
		double bmn = 0;

		/*--------------------------------------------------------------------------
		/* Initialize the solver
		/*--------------------------------------------------------------------------*/
		dfgmres_init(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
		if (RCI_request != 0) goto FAILED;
		ipar[10] = 1;   // preconditioner
		ipar[14] = 100;   // restart number
		ipar[7] = 0;
		dpar[0] = 1.0E-3;
		/*---------------------------------------------------------------------------
		/* Check the correctness and consistency of the newly set parameters
		/*---------------------------------------------------------------------------*/
		dfgmres_check(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
		if (RCI_request != 0) goto FAILED;
	ONE: dfgmres(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp);
		//bmn = 0;
		//for (int ind = 0; ind < ivar; ++ind) {
		//	bmn += computed_solution[ind] * computed_solution[ind];
		//}
		//cout << "Right hand side norm is " << bmn << endl;
		if (RCI_request == 0) {
			goto COMPLETE;
		}

		/*---------------------------------------------------------------------------
		/* If RCI_request=1, then compute the vector A*tmp[ipar[21]-1]
		/* and put the result in vector tmp[ipar[22]-1]
		/*---------------------------------------------------------------------------*/
		if (RCI_request == 1) {
			/* do the specific matrix vector multiplication and put the result in tmp[ipar[22] - 1] */
			multi_matrixvec(sys, V0dt, V0dat, sys->leng_v0d1, V0ct, V0cat, sys->leng_v0c, freq, LrowId, LcolId, Lval, leng, &tmp[ipar[21] - 1], &tmp[ipar[22] - 1], N);
			double nn = 0;
			//out.open("x1.txt", std::ofstream::out | std::ofstream::trunc);
			//for (int ind = 0; ind < N; ++ind) {
			//	out << tmp[ipar[21] - 1 + ind] << " " << tmp[ipar[22] - 1 + ind] << endl;
			//	nn += pow(tmp[ipar[21] - 1 + ind], 2);
			//}
			//out.close();
			cout << nn << endl;
			goto ONE;
		}

		/* do the user-defined stopping test */
		if (RCI_request == 2) {
			ipar[12] = 1;
			/* Get the current FGMRES solution in the vector b[N] */
			dfgmres_get(&ivar, computed_solution, b, &RCI_request, ipar, dpar, tmp, &itercount);
			/* Compute the current true residual via MKL (Sparse) BLAS routines */

			for (myint ind = 0; ind < N; ++ind) {
				residual[ind] = 0;   // before using sparseMatrixVecMul, the resultant vector should be first initialized
			}
			multi_matrixvec(sys, V0dt, V0dat, sys->leng_v0d1, V0ct, V0cat, sys->leng_v0c, freq, LrowId, LcolId, Lval, leng, b, residual, N);
			dvar = -1.0E0;
			i = 1;
			daxpy(&ivar, &dvar, bm, &i, residual, &i);
			dvar = cblas_dnrm2(ivar, residual, i) / cblas_dnrm2(ivar, bm, i);    // relative residual
			cout << "The relative residual is " << dvar << " with iteration number " << itercount << endl;
			if (dvar < 0.001 || itercount > maxit) goto COMPLETE;
			else goto ONE;
		}

		/* apply the preconditioner on the vector tmp[ipar[21]-1] and put the result in vector tmp[ipar[22]-1] */
		if (RCI_request == 3) {
			complex<double>* realRhsSoln = (complex<double>*)calloc(N / 2, sizeof(complex<double>));
			complex<double>* imagRhsSoln = (complex<double>*)calloc(N / 2, sizeof(complex<double>));
			applyPrecond_freq(sys, &tmp[ipar[21] - 1], realRhsSoln, L1RowId, L1ColId, L1val, leng_L1, V0ct, V0cat, V0dt, V0dat, freq);   // use upper triangular matrix as the preconditioner [V0da'*(-omega^2*D_eps)*V0d, V0da'*(-omega^2*D_eps)*V0c, 0;
			// 0, V0ca'*(1i*omega*D_sig)*V0c, V0ca'*(1i*omega*D_sig);
			// 0, 0, -omega ^ 2 * D_eps + 1i*omega*D_sig + L]
			applyPrecond_freq(sys, &tmp[ipar[21] - 1 + N / 2], imagRhsSoln, L1RowId, L1ColId, L1val, leng_L1, V0ct, V0cat, V0dt, V0dat, freq);

			for (myint ind = 0; ind < N / 2; ++ind) {
				tmp[ipar[22] - 1 + ind] = realRhsSoln[ind].real() - imagRhsSoln[ind].imag();
				tmp[ipar[22] - 1 + ind + N / 2] = imagRhsSoln[ind].real() + realRhsSoln[ind].imag();
			}
			goto ONE;
		}

		/* check if the norm of the next generated vector is not zero up to rounding and computational errors. */
		if (RCI_request == 4) {
			//if (dpar[6] < 1.0E-12) goto COMPLETE;
			//else goto ONE;
			goto ONE;
		}

		else {
			goto FAILED;
		}

	COMPLETE: ipar[12] = 0;
		dfgmres_get(&ivar, computed_solution, bm, &RCI_request, ipar, dpar, tmp, &itercount);
		
		for (i = 0; i < ivar; ++i) {
			x[i] = computed_solution[i];
		}
		goto SUCCEDED;
	FAILED: cout << "The solver has returned the ERROR code " << RCI_request << endl;

	SUCCEDED: free(b); b = NULL;
		free(expected_solution); expected_solution = NULL;
		free(computed_solution); computed_solution = NULL;
		free(residual); residual = NULL;
		free(J); J = NULL;
		//complex<double>* xc = (complex<double>*)calloc(N / 2, sizeof(complex<double>));
		//for (myint ind = 0; ind < N / 2; ++ind) {
		//	xc[ind] = x[ind] + 1i * x[N / 2 + ind];   // combine the real and imaginary parts to get the final complex vector
		//}
		//sys->Construct_Z_V0_Vh(xc, freqNo, sourcePort);
		//free(xc); xc = NULL;
		free(x); x = NULL;
	}
	return 0;
}

int applyPrecond_freq(fdtdMesh* sys, double* b1, complex<double>* b2, myint* L1RowId, myint* L1ColId, double* L1val, myint leng_L1, sparse_matrix_t& V0ct, sparse_matrix_t& V0cat, sparse_matrix_t& V0dt, sparse_matrix_t& V0dat, double freq) {
	/* Preconditioner [V0da'*(-omega^2*D_eps)*V0d, V0da'*(-omega^2*D_eps)*V0c, 0;
	0, V0ca'*(1i*omega*D_sig)*V0c, V0ca'*(1i*omega*D_sig);
	0, 0, -omega^2*D_eps+1i*omega*D_sig+L] */

	/* solve (-omega^2*D_eps+1i*omega*D_sig+L)^(-1)*b3 */
	myint nedge = sys->N_edge - sys->bden;
	int status;
	double nn = 0;
	double* r3 = (double*)calloc(2 * nedge, sizeof(double));
	double* x3 = (double*)calloc(2 * nedge, sizeof(double));   // upper part is real and lower part is imaginary
	for (int ind = 0; ind < nedge; ++ind) {
		r3[ind] = b1[sys->leng_v0d1 + sys->leng_v0c + ind];
		nn += r3[ind] * r3[ind];
	}
	if (nn != 0)
		mkl_gmres_A(r3, x3, L1RowId, L1ColId, L1val, leng_L1, 2 * nedge);

	double* sigx3 = (double*)calloc(nedge * 2, sizeof(double));
	for (int ind = 0; ind < nedge; ++ind) {
		if (sys->markEdge[sys->mapEdgeR[ind]]) {
			sigx3[ind] = freq * 2 * M_PI * SIGMA * x3[ind];   // sigx = omega*D_sig*x3, upper is imaginary and lower is real
			sigx3[ind + nedge] = -freq * 2 * M_PI * SIGMA * x3[ind + nedge];
		}
		else {
			sigx3[ind] = 0;
			sigx3[ind + nedge] = 0;
		}
	}
	double* r2r = (double*)calloc(sys->leng_v0c, sizeof(double));
	double* r2i = (double*)calloc(sys->leng_v0c, sizeof(double));
	double* x2r = (double*)calloc(sys->leng_v0c, sizeof(double));
	double* x2i = (double*)calloc(sys->leng_v0c, sizeof(double));
	sparse_status_t s;
	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0cat, descr, &sigx3[nedge], beta, r2r);   // r2r = right hand side real part for V0c line
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0cat, descr, sigx3, beta, r2i);   // r2i = right hand side imag part for V0c line
	for (int ind = 0; ind < sys->leng_v0c; ++ind) {
		r2r[ind] /= sys->v0can[ind];
		r2i[ind] /= sys->v0can[ind];
	}
	for (int ind = 0; ind < sys->leng_v0c; ++ind) {
		r2r[ind] = b1[sys->leng_v0d1 + ind] - r2r[ind];
		r2i[ind] *= -1;
	}
	for (myint indi = 0; indi < sys->leng_v0c; ++indi) {   // Ac is not normalized with V0ca and V0c
		r2r[indi] *= sys->v0can[indi];
		r2i[indi] *= sys->v0can[indi];
	}
	status = hypreSolve(sys->AcRowId, sys->AcColId, sys->Acval, sys->leng_Ac, r2r, sys->leng_v0c, x2i, 0, 3);
	status = hypreSolve(sys->AcRowId, sys->AcColId, sys->Acval, sys->leng_Ac, r2i, sys->leng_v0c, x2r, 0, 3);
	for (myint indi = 0; indi < sys->leng_v0c; ++indi) {
		x2i[indi] /= (-freq * 2 * M_PI / sys->v0cn[indi]);   // x2 = (V0ca'*(1i*omega)*V0c)\(b2-V0ca'*(1i*omega*D_sig)*x3)
		x2r[indi] /= (freq * 2 * M_PI / sys->v0cn[indi]);   // x2r is real part and x2i is imaginary part
	}

	double* x2rc = (double*)calloc((unsigned)sys->leng_v0c, sizeof(double));   // x2rc is x2r's copy which is used to divide v0cn and then multiply V0c
	double* x2ic = (double*)calloc((unsigned)sys->leng_v0c, sizeof(double));   // x2ic is x2i's copy which is used to divide v0cn and then multiply V0c
	for (myint ind = 0; ind < sys->leng_v0c; ++ind) {
		x2rc[ind] = x2r[ind] / sys->v0cn[ind];
		x2ic[ind] = x2i[ind] / sys->v0cn[ind];
	}
	double* V0cx2r = (double*)calloc(nedge, sizeof(double));
	double* V0cx2i = (double*)calloc(nedge, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, V0ct, descr, x2rc, beta, V0cx2r);   // V0cx2r = V0c*x2r the real part
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, V0ct, descr, x2ic, beta, V0cx2i);   // V0cx2i = V0c*x2i the imaginary part
	for (myint ind = 0; ind < nedge; ++ind) {
		V0cx2r[ind] *= (-pow(freq * 2 * M_PI, 2) * sys->getEps(sys->mapEdgeR[ind]));
		V0cx2i[ind] *= (-pow(freq * 2 * M_PI, 2) * sys->getEps(sys->mapEdgeR[ind]));
	}
	double* r1r = (double*)calloc(sys->leng_v0d1, sizeof(double));
	double* r1i = (double*)calloc(sys->leng_v0d1, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0dat, descr, V0cx2r, beta, r1r);   // r1r = V0da'*(-omega^2*D_eps)*V0c*x2r the real part
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0dat, descr, V0cx2i, beta, r1i);   // r1i = V0da'*(-omega^2*D_eps)*V0c*x2i the imaginary part
	for (myint ind = 0; ind < sys->leng_v0d1; ++ind) {
		r1r[ind] /= sys->v0dan[ind];
		r1i[ind] /= sys->v0dan[ind];
		r1r[ind] = b1[ind] - r1r[ind];
		r1i[ind] = -r1i[ind];
	}
	double* x1r = (double*)calloc(sys->leng_v0d1, sizeof(double));
	double* x1i = (double*)calloc(sys->leng_v0d1, sizeof(double));
	status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, r1r, sys->leng_v0d1, x1r, 0, 3);
	status = hypreSolve(sys->AdRowId, sys->AdColId, sys->Adval, sys->leng_Ad, r1i, sys->leng_v0d1, x1i, 0, 3);
	for (myint ind = 0; ind < sys->leng_v0d1; ++ind) {
		x1r[ind] /= (-pow(freq * 2 * M_PI, 2));
		x1i[ind] /= (-pow(freq * 2 * M_PI, 2));
	}

	for (myint ind = 0; ind < sys->leng_v0d1; ++ind) {
		b2[ind] = x1r[ind] + 1i * x1i[ind];
	}
	for (myint ind = 0; ind < sys->leng_v0c; ++ind) {
		b2[ind] = x2r[ind] + 1i * x2i[ind];
	}
	for (myint ind = 0; ind < nedge; ++ind) {
		b2[ind] = x3[ind] + 1i * x3[nedge + ind];
	}

	free(r3); r3 = NULL;
	free(x3); x3 = NULL;
	free(sigx3); sigx3 = NULL;
	free(r2r); r2r = NULL;
	free(r2i); r2i = NULL;
	free(x2r); x2r = NULL;
	free(x2i); x2i = NULL;
	free(x2rc); x2rc = NULL;
	free(x2ic); x2ic = NULL;
	free(V0cx2r); V0cx2r = NULL;
	free(V0cx2i); V0cx2i = NULL;
	free(r1r); r1r = NULL;
	free(r1i); r1i = NULL;
	free(x1r); x1r = NULL;
	free(x1i); x1i = NULL;
	return 0;
}

int generate_bm(fdtdMesh* sys, sparse_matrix_t& V0dat, myint leng_v0d, sparse_matrix_t& V0cat, myint leng_v0c, double freq, double* J, double* bm, myint N) {
	/* generate [0; 0; 0; V0da'*(-omega*J); V0ca'*(-omega*J); -omega*J]
	bm : resultant vector
	N : length of the vector */
	double* V0daJ = (double*)calloc(leng_v0d, sizeof(double));
	sparse_status_t s;
	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0dat, descr, J, beta, V0daJ);

	double* V0caJ = (double*)calloc(leng_v0c, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0cat, descr, J, beta, V0caJ);

	for (int ind = N / 2; ind < N / 2 + leng_v0d; ++ind) {
		bm[ind] = -freq * 2 * M_PI * V0daJ[ind - N / 2] / sys->v0dan[ind - N / 2];
	}
	for (int ind = N / 2 + leng_v0d; ind < N / 2 + leng_v0d + leng_v0c; ++ind) {
		bm[ind] = -freq * 2 * M_PI * V0caJ[ind - N / 2 - leng_v0d] / sys->v0can[ind - N / 2 - leng_v0d];
	}
	for (int ind = N / 2 + leng_v0d + leng_v0c; ind < N; ++ind) {
		bm[ind] = -freq * 2 * M_PI * J[ind - N / 2 - leng_v0d - leng_v0c];
	}
	free(V0daJ); V0daJ = NULL;
	free(V0caJ); V0caJ = NULL;
	return 0;
}

int multi_matrixvec(fdtdMesh* sys, sparse_matrix_t& V0dt, sparse_matrix_t& V0dat, myint leng_v0d, sparse_matrix_t& V0ct, sparse_matrix_t& V0cat, myint leng_v0c, double freq, myint* LrowId, myint* LcolId, double* Lval, myint leng, double* x, double* y, myint N) {
	/* do the matrix vector multiplication
	[V0da'*(-omega^2*D_eps)*V0d, V0da'*(-omega^2*D_eps)*V0c, 0, 0, 0, 0;
	V0ca'*(-omega^2*D_eps)*V0d, V0ca'*(-omega^2*D_eps)*V0c, V0ca'*(-omega^2*D_eps), 0, -V0ca'*omega*D_sig*V0c, -V0ca'*omega*D_sig;
	0, -omega^2*D_eps*V0c, -omega^2*D_eps+L, 0, -omega*D_sig*V0c, -omega*D_sig;
	0, 0, 0, V0da'*(-omega^2*D_eps)*V0d, V0da'*(-omega^2*D_eps)*V0c, 0;
	0, V0ca'*omega*D_sig*V0c, V0ca'*omega*D_sig, V0ca'*(-omega^2*D_eps)*V0d, V0ca'*(-omega^2*D_eps)*V0c, V0ca'*(-omega^2*D_eps);
	0, omega*D_sig*V0c, omega*D_sig, 0, -omega^2*D_eps*V0c, -omega^2*D_eps+L]
	x : the vector
	y : the resultant vector */
	double* y1 = (double*)calloc(N / 2, sizeof(double));
	double* y2 = (double*)calloc(N / 2, sizeof(double));
	realMatrixVec(sys, V0dt, V0dat, leng_v0d, V0ct, V0cat, leng_v0c, freq, LrowId, LcolId, Lval, leng, x, y1, N / 2);   // A11*x1
	imagMatrixVec(sys, leng_v0d, V0ct, V0cat, leng_v0c, freq, &x[N / 2], y2, N / 2);   // A12*x2
	for (int ind = 0; ind < N / 2; ++ind) {
		y[ind] = y1[ind] - y2[ind];
	}

	imagMatrixVec(sys, leng_v0d, V0ct, V0cat, leng_v0c, freq, x, y1, N / 2);   // A21*x1
	realMatrixVec(sys, V0dt, V0dat, leng_v0d, V0ct, V0cat, leng_v0c, freq, LrowId, LcolId, Lval, leng, &x[N / 2], y2, N / 2);   // A22*x2
	for (int ind = N / 2; ind < N; ++ind) {
		y[ind] = y1[ind - N / 2] + y2[ind - N / 2];
	}

	free(y1); y1 = NULL;
	free(y2); y2 = NULL;
	return 0;
}

int realMatrixVec(fdtdMesh* sys, sparse_matrix_t& V0dt, sparse_matrix_t& V0dat, myint leng_v0d, sparse_matrix_t& V0ct, sparse_matrix_t& V0cat, myint leng_v0c, double freq, myint* LrowId, myint* LcolId, double* Lval, myint leng, double* x, double* y, myint N) {
	/* [V0da'*(-omega^2*D_eps)*V0d, V0da'*(-omega^2*D_eps)*V0c, 0;
	    V0ca'*(-omega^2*D_eps)*V0c, V0ca'*(-omega^2*D_eps)*V0c, V0ca'*(-omega^2*D_eps);
		0, -omega^2*D_eps*V0c, -omega^2*D_eps+L] do matrix vector multiplication */
	double* x1 = (double*)calloc(N, sizeof(double));
	for (int ind = 0; ind < leng_v0d; ++ind) {
		x1[ind] = x[ind] / sys->v0dn[ind];
	}
	for (int ind = 0; ind < leng_v0c; ++ind) {
		x1[ind + leng_v0d] = x[ind + leng_v0d] / sys->v0cn[ind];
	}
	sparse_status_t s;
	int status;
	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;
	double* V0dx = (double*)calloc(sys->N_edge, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, V0dt, descr, &x1[0], beta, V0dx);   // V0dx = V0d*x1
	double* epsV0dx = (double*)calloc(sys->N_edge, sizeof(double));
	for (int ind = 0; ind < sys->N_edge; ++ind) {
		epsV0dx[ind] = V0dx[ind] * (-pow(freq * 2 * M_PI, 2) * sys->getEps(ind));   // epsV0dx = (-omega^2*D_eps)*V0d*x1
	}
	double* V0daepsV0d = (double*)calloc(leng_v0d, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0dat, descr, epsV0dx, beta, V0daepsV0d);   // V0daepsV0d = V0da'*(-omega^2*D_eps)*V0d*x1
	for (int ind = 0; ind < leng_v0d; ++ind) {
		V0daepsV0d[ind] /= sys->v0dan[ind];
	}
	double* V0caepsV0d = (double*)calloc(leng_v0c, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0cat, descr, epsV0dx, beta, V0caepsV0d);   // V0caepsV0d = V0ca'*(-omega^2*D_eps)*V0d*x1
	for (int ind = 0; ind < leng_v0c; ++ind) {
		V0caepsV0d[ind] /= sys->v0can[ind];
	}

	double* V0cx = (double*)calloc(sys->N_edge, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, V0ct, descr, &x1[leng_v0d], beta, V0cx);   // V0cx = V0c*x2
	double* epsV0cx = (double*)calloc(sys->N_edge, sizeof(double));
	for (int ind = 0; ind < sys->N_edge; ++ind) {
		epsV0cx[ind] = V0cx[ind] * (-pow(freq * 2 * M_PI, 2) * sys->getEps(ind));   // epsV0cx = (-omega^2*D_eps)*V0c*x2
	}
	double* V0daepsV0c = (double*)calloc(leng_v0d, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0dat, descr, epsV0cx, beta, V0daepsV0c);   // V0daepsV0c = V0da'*(-omega^2*D_eps)*V0c*x2
	for (int ind = 0; ind < leng_v0d; ++ind) {
		V0daepsV0c[ind] /= sys->v0dan[ind];
	}
	double* V0caepsV0c = (double*)calloc(leng_v0c, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0cat, descr, epsV0cx, beta, V0caepsV0c);   // V0caepsV0c = V0ca'*(-omega^2*D_eps)*V0c*x2
	for (int ind = 0; ind < leng_v0c; ++ind) {
		V0caepsV0c[ind] /= sys->v0can[ind];
	}

	double* Lx = (double*)calloc(sys->N_edge, sizeof(double));
	status = sparseMatrixVecMul(LrowId, LcolId, Lval, leng, &x1[leng_v0d + leng_v0c], Lx);   // Lx = L*x3
	double* epsx = (double*)calloc(sys->N_edge, sizeof(double));
	for (int ind = 0; ind < sys->N_edge; ++ind) {
		epsx[ind] = (-pow(freq * 2 * M_PI, 2) * sys->getEps(ind)) * x1[leng_v0d + leng_v0c + ind] + Lx[ind];
	}
	double* V0caeps = (double*)calloc(leng_v0c, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0cat, descr, epsx, beta, V0caeps);
	for (int ind = 0; ind < leng_v0c; ++ind) {
		V0caeps[ind] /= sys->v0can[ind];
	}

	for (int ind = 0; ind < leng_v0d; ++ind) {
		y[ind] = V0daepsV0d[ind] + V0daepsV0c[ind];
	}
	for (int ind = 0; ind < leng_v0c; ++ind) {
		y[ind + leng_v0d] = V0caepsV0d[ind] + V0caepsV0c[ind] + V0caeps[ind];
	}
	for (int ind = 0; ind < sys->N_edge; ++ind) {
		y[ind + leng_v0d + leng_v0c] = epsx[ind] + epsV0cx[ind];
	}
	free(x1); x1 = NULL;
	free(V0dx); V0dx = NULL;
	free(epsV0dx); epsV0dx = NULL;
	free(V0daepsV0d); V0daepsV0d = NULL;
	free(V0caepsV0d); V0caepsV0d = NULL;
	free(V0cx); V0cx = NULL;
	free(epsV0cx); epsV0cx = NULL;
	free(V0daepsV0c); V0daepsV0c = NULL;
	free(V0caepsV0c); V0caepsV0c = NULL;
	free(Lx); Lx = NULL;
	free(epsx); epsx = NULL;
	free(V0caeps); V0caeps = NULL;

	return 0;
}

int imagMatrixVec(fdtdMesh* sys, myint leng_v0d, sparse_matrix_t& V0ct, sparse_matrix_t& V0cat, myint leng_v0c, double freq, double* x, double* y, myint N) {
	/* [0, 0, 0;
	    0, V0ca'*omega*D_sig*V0c, V0ca'*omega*D_sig;
		0, omega*D_sig*V0c, omega*D_sig] matrix vector multiplication */
	sparse_status_t s;
	double alpha = 1, beta = 0;
	struct matrix_descr descr;
	descr.type = SPARSE_MATRIX_TYPE_GENERAL;

	double* x1 = (double*)calloc(N, sizeof(double));
	for (int ind = 0; ind < leng_v0d; ++ind) {
		x1[ind] = x[ind] / sys->v0dn[ind];
	}
	for (int ind = 0; ind < leng_v0c; ++ind) {
		x1[ind + leng_v0d] = x[ind + leng_v0d] / sys->v0cn[ind];
	}
	double* V0cx = (double*)calloc(sys->N_edge, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_TRANSPOSE, alpha, V0ct, descr, &x1[leng_v0d], beta, V0cx);   // V0cx = V0c*x2
	double* sigV0cx = (double*)calloc(sys->N_edge, sizeof(double));
	for (int ind = 0; ind < sys->N_edge; ++ind) {
		if (sys->markEdge[ind]) {
			sigV0cx[ind] = freq * 2 * M_PI * SIGMA * V0cx[ind];   // sigV0cx = omega*D_sig*V0c*x2
		}
		else {
			sigV0cx[ind] = 0;
		}
	}
	double* V0casigV0c = (double*)calloc(leng_v0c, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0cat, descr, sigV0cx, beta, V0casigV0c);   // V0casigV0c = V0ca'*omega*D_sig*V0c*x2
	for (int ind = 0; ind < leng_v0c; ++ind) {
		V0casigV0c[ind] /= sys->v0can[ind];
	}

	double* sigx = (double*)calloc(sys->N_edge, sizeof(double));
	for (int ind = 0; ind < sys->N_edge; ++ind) {
		if (sys->markEdge[ind]) {
			sigx[ind] = x[leng_v0d + leng_v0c + ind] * SIGMA * freq * 2 * M_PI;   // sigx = omega*D_sig*x3
		}
		else {
			sigx[ind] = 0;
		}
	}
	double* V0casig = (double*)calloc(leng_v0c, sizeof(double));
	s = mkl_sparse_d_mv(SPARSE_OPERATION_NON_TRANSPOSE, alpha, V0cat, descr, sigx, beta, V0casig);   // V0casig = V0ca'*omega*D_sig*x3
	for (int ind = 0; ind < leng_v0c; ++ind) {
		V0casigV0c[ind] /= sys->v0can[ind];
	}

	for (int ind = 0; ind < leng_v0c; ++ind) {
		y[ind + leng_v0d] = V0casigV0c[ind] + V0casig[ind];
	}
	for (int ind = 0; ind < sys->N_edge; ++ind) {
		y[ind + leng_v0d + leng_v0c] = sigV0cx[ind] + sigx[ind];
	}

	free(x1); x1 = NULL;
	free(V0cx); V0cx = NULL;
	free(sigV0cx); sigV0cx = NULL;
	free(V0casigV0c); V0casigV0c = NULL;
	free(sigx); sigx = NULL;
	free(V0casig); V0casig = NULL;

	return 0;
}