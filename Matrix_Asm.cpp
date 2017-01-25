//
// This file is part of 3d_bem.
//
// Created by D. Nikolski on 1/24/2017.
// Copyright (c) ECOLE POLYTECHNIQUE FEDERALE DE LAUSANNE, Switzerland,
// Geo-Energy Laboratory, 2016-2017.  All rights reserved.
// See the LICENSE.TXT file for more details. 
//

#include <complex>
#include <il/Array2D.h>
#include <il/StaticArray.h>
#include <il/StaticArray2D.h>
#include <il/linear_algebra/dense/blas/dot.h>
#include "Matrix_Asm.h"
#include "Tensor_Oper.h"
#include "Ele_Base.h"
#include "Elast_Ker_Int.h"
//#include "H_Potential.h"

namespace hfp3d {

// "Global" matrix assembly

    //template<typename C_array, typename N_array>
    il::Array2D<double> BEMatrix_S
            (double Mu, double Nu, double beta,
             il::Array2D<il::int_t> &Conn_Mtr,
             il::Array2D<double> &Node_Crd) {
        // BEM matrix assembly from boundary mesh geometry data:
        // mesh connectivity (Conn_Mtr) and nodes' coordinates (Node_Crd)
        // Naive way: no parallelization, no ACA

        IL_ASSERT(Conn_Mtr.size(0) >= 3);
        IL_ASSERT(Conn_Mtr.size(1) >= 1); // at least 1 element
        IL_ASSERT(Node_Crd.size(0) >= 3);
        IL_ASSERT(Node_Crd.size(1) >= 3); // at least 3 nodes

        const il::int_t Num_El = Conn_Mtr.size(1);
        const il::int_t Num_DOF = 18 * Num_El;

        //IL_ASSERT(Global_Matrix_H.size(0) == 18*Num_El);
        //IL_ASSERT(Global_Matrix_H.size(1) == 18*Num_El);

        il::Array2D<double> Global_Matrix_H(Num_DOF, Num_DOF);
        //il::StaticArray2D<double, Num_DOF, Num_DOF> Global_Matrix_H;
        //il::StaticArray<double, Num_DOF> RHS;

        il::StaticArray2D<std::complex<double>, 6, 6> SFM{0.0};
        //il::StaticArray<double, 6> VW_S, VW_T;
        il::StaticArray<std::complex<double>, 3> tau;
        il::StaticArray2D<double, 3, 3> EV_S, EV_T;
        il::StaticArray2D<double, 3, 3> RT_S, RT_S_t, RT_T; //, RT_T_t;
        il::StaticArray2D<double, 3, 3> TI_NN, TI_NN_G; //, TI_NN_I;
        il::StaticArray<il::StaticArray<double, 3>, 6> CP_T;
        il::StaticArray<double, 3> N_CP, N_CP_L;
        il::Array2D<double> V_S(3, 1), V_T(3, 1);
        il::StaticArray2D<double, 6, 18> S_H_CP_L; //, S_H_CP_G;
        il::StaticArray2D<double, 3, 18> T_H_CP_L, T_H_CP_G;
        il::StaticArray2D<double, 18, 18> IM_H_L;
        //il::StaticArray2D<double, 18, 18> IM_T_L;

        // Loop over "source" elements
        for (il::int_t S_El = 0; S_El < Num_El; ++S_El) {
            // Vertices' coordinates
            for (il::int_t j = 0; j < 3; ++j) {
                il::int_t S_N = Conn_Mtr(j, S_El);
                for (il::int_t k = 0; k < 3; ++k) {
                    EV_S(k, j) = Node_Crd(k, S_N);
                }
                // set VW_S[j]
            }

            // Basis (shape) functions and rotation tensor (RT_S) of the el-t
            SFM = El_SFM_S(EV_S, il::io, RT_S);
            //SFM = El_SFM_N(RT, EV, VW_S);

            // Inverse (transposed) rotation tensor
            for (int j = 0; j < 3; ++j) {
                for (int k = 0; k < 3; ++k) {
                    RT_S_t(k, j) = RT_S(j, k);
                }
            }

            // Complex-valued positions of "source" element nodes
            tau = El_RT_Tr(EV_S, RT_S);

            // Loop over "Target" elements
            for (il::int_t T_El = 0; T_El < Num_El; ++T_El) {
                // Vertices' coordinates
                for (il::int_t j = 0; j < 3; ++j) {
                    il::int_t T_N = Conn_Mtr(j, T_El);
                    for (il::int_t k = 0; k < 3; ++k) {
                        EV_T(k, j) = Node_Crd(k, T_N);
                    }
                    // set VW_T[j]
                }

                // Rotation tensor for the target element
                RT_T = El_LB_RT(EV_T);

                // Inverse (transposed) rotation tensor
                //for (int j=0; j < 3; ++j) {
                //    for (int k=0; k < 3; ++k) {
                //        RT_T_t(k,j) = RT_T(j,k);
                //    }
                //}

                // Normal vector at collocation point (x)
                for (int j = 0; j < 3; ++j) {
                    N_CP[j] = -RT_T(j, 2);
                }

                // Collocation points' coordinates
                CP_T = El_CP_S(EV_T, beta);
                //CP_T = El_CP_N(EV_T,VW_T,beta);

                // Loop over nodes of the "target" element
                for (int n_T = 0; n_T < 6; ++n_T) {
                    // Shifting to the n_T-th collocation pt
                    el_x_cr hz = El_X_CR(EV_S, CP_T[n_T], RT_S);

                    // Calculating DD-to stress influence
                    // w.r. to the source element's local coordinate system
                    S_H_CP_L = Local_IM(1, Mu, Nu, hz.h, hz.z, tau, SFM);

                    // Multiplication by N_CP

                    // Alternative 1: rotating stress at CP
                    // to the reference ("global") coordinate system
                    //S_H_CP_G = hfp3d::SIM_P_R(RT_S, RT_S_t, S_H_CP_L);
                    //T_H_CP_G = hfp3d::N_dot_SIM(N_CP, S_H_CP_G);

                    // Alternative 2: rotating N_CP to
                    // the source element's local coordinate system
                    // same as il::dot(RT_S_transposed, N_CP)
                    N_CP_L = il::dot(N_CP, RT_S);
                    T_H_CP_L = N_dot_SIM(N_CP_L, S_H_CP_L);
                    T_H_CP_G = il::dot(RT_S, T_H_CP_L);

                    // Alternative 3: keeping everything
                    // in terms of local coordinates
                    //T_H_CP_X = il::dot(RT_T_t, T_H_CP_G);

                    // Re-relating DD-to traction influence to DD
                    // w.r. to the reference coordinate system
                    for (int n_S = 0; n_S < 6; ++n_S) {
                        // taking a block (one node of the "source" element)
                        for (int j = 0; j < 3; ++j) {
                            for (int k = 0; k < 3; ++k) {
                                TI_NN(k, j) = T_H_CP_G(k, 3 * n_S + j);
                            }
                        }

                        // Coordinate rotation
                        TI_NN_G = il::dot(TI_NN, RT_S_t);

                        // Adding the block to the element-to-element
                        // influence sub-matrix
                        for (int j = 0; j < 3; ++j) {
                            for (int k = 0; k < 3; ++k) {
                                IM_H_L(3 * n_T + k, 3 * n_S + j) =
                                        TI_NN_G(k, j);
                            }
                        }
                    }
                }

                // Adding the element-to-element influence sub-matrix
                // to the global influence matrix
                IL_ASSERT(18 * (T_El + 1) <= Global_Matrix_H.size(0));
                IL_ASSERT(18 * (S_El + 1) <= Global_Matrix_H.size(1));
                for (il::int_t j1 = 0; j1 < 18; ++j1) {
                    for (il::int_t j0 = 0; j0 < 18; ++j0) {
                        Global_Matrix_H(18 * T_El + j0, 18 * S_El + j1) =
                                IM_H_L(j0, j1);
                    }
                }
            }
        }
        return Global_Matrix_H;
    };

// Stress at given points (MPt_Crd) vs DD at nodal points (Node_Crd)

    //template<typename C_array, typename N_array>
    il::Array2D<double> BEStressF_S
            (double Mu, double Nu, double beta,
             il::Array2D<il::int_t> &Conn_Mtr,
             il::Array2D<double> &Node_Crd,
             il::Array2D<double> &MPt_Crd) {
        // Stress at given points (MPt_Crd) vs DD at nodal points (Node_Crd)
        // from boundary mesh geometry data:
        // mesh connectivity (Conn_Mtr) and nodes' coordinates (Node_Crd)
        // Naive way: no parallelization, no ACA

        IL_ASSERT(Conn_Mtr.size(0) >= 3);
        IL_ASSERT(Conn_Mtr.size(1) >= 1); // at least 1 element
        IL_ASSERT(Node_Crd.size(0) >= 3);
        IL_ASSERT(Node_Crd.size(1) >= 3); // at least 3 nodes

        const il::int_t Num_El = Conn_Mtr.size(1);
        const il::int_t Num_DOF = 18 * Num_El;
        const il::int_t Num_MPt = MPt_Crd.size(1);

        //IL_ASSERT(Global_Matrix_H.size(0) == 18*Num_El);
        //IL_ASSERT(Global_Matrix_H.size(1) == 18*Num_El);

        il::Array2D<double> StressF(6 * Num_MPt, Num_DOF);
        //il::StaticArray2D<double, 6 * Num_MPt, Num_DOF> StressF;

        il::StaticArray2D<std::complex<double>, 6, 6> SFM{0.0};
        //il::StaticArray<double, 6> VW_S, VW_T;
        il::StaticArray<std::complex<double>, 3> tau;
        il::StaticArray2D<double, 3, 3> EV_S, EV_T;
        il::StaticArray2D<double, 3, 3> RT_S, RT_S_t, RT_T; //, RT_T_t;
        il::StaticArray2D<double, 3, 3> SI_NN, SI_NN_G; //, SI_NN_I;
        il::StaticArray<il::StaticArray<double, 3>, 6> CP_T;
        il::StaticArray<double, 3> N_CP, N_CP_L, M_Pt_C;
        il::Array2D<double> V_S(3, 1), V_T(3, 1);
        il::StaticArray2D<double, 6, 18> S_H_CP_L, S_H_CP_G;
        //il::StaticArray2D<double, 3, 18> T_H_CP_L, T_H_CP_G;
        il::StaticArray2D<double, 18, 18> IM_H_L;
        //il::StaticArray2D<double, 18, 18> IM_T_L;

        // Loop over elements
        for (il::int_t S_El = 0; S_El < Num_El; ++S_El) {
            // Vertices' coordinates
            for (il::int_t j = 0; j < 3; ++j) {
                il::int_t S_N = Conn_Mtr(j, S_El);
                for (il::int_t k = 0; k < 3; ++k) {
                    EV_S(k, j) = Node_Crd(k, S_N);
                }
                // get VW_S[j]
            }

            // Basis (shape) functions and rotation tensor (RT_S) of the el-t
            SFM = El_SFM_S(EV_S, il::io, RT_S);
            //SFM = El_SFM_N(RT, EV, VW_S);

            // Inverse (transposed) rotation tensor
            for (int j = 0; j < 3; ++j) {
                for (int k = 0; k < 3; ++k) {
                    RT_S_t(k, j) = RT_S(j, k);
                }
            }

            // Complex-valued positions of "source" element nodes
            tau = El_RT_Tr(EV_S, RT_S);

            // Loop over monitoring points
            for (il::int_t MPt = 0; MPt < Num_MPt; ++MPt) {
                // Monitoring points' coordinates
                for (il::int_t j = 0; j < 3; ++j) {
                    M_Pt_C[j] = MPt_Crd(j, MPt);
                }

                // Shifting to the monitoring point
                el_x_cr hz = El_X_CR(EV_S, M_Pt_C, RT_S);

                // Calculating DD-to stress influence
                // w.r. to the source element's local coordinate system
                S_H_CP_L = Local_IM(1, Mu, Nu, hz.h, hz.z, tau, SFM);

                // Rotating stress at MPt
                // to the reference ("global") coordinate system
                S_H_CP_G = SIM_P_R(RT_S, RT_S_t, S_H_CP_L);

                // Re-relating DD-to traction influence to DD
                // w.r. to the reference coordinate system
                for (int n_S = 0; n_S < 6; ++n_S) {
                    // taking a block (one node of the "source" element)
                    for (int j = 0; j < 6; ++j) {
                        for (int k = 0; k < 3; ++k) {
                            SI_NN(k, j) = S_H_CP_G(k, 3 * n_S + j);
                        }
                    }

                    // Coordinate rotation
                    SI_NN_G = il::dot(SI_NN, RT_S_t);

                    // Adding the block to the element-to-point
                    // influence sub-matrix
                    for (int j = 0; j < 6; ++j) {
                        for (int k = 0; k < 3; ++k) {
                            IM_H_L(k, 3 * n_S + j) = SI_NN_G(k, j);
                        }
                    }
                }

                // Adding the element-to-point influence sub-matrix
                // to the global stress matrix
                IL_ASSERT(6 * (MPt + 1) <= StressF.size(0));
                IL_ASSERT(18 * (S_El + 1) <= StressF.size(1));
                for (il::int_t j1 = 0; j1 < 18; ++j1) {
                    for (il::int_t j0 = 0; j0 < 6; ++j0) {
                        StressF(6 * MPt + j0, 18 * S_El + j1) = IM_H_L(j0, j1);
                    }
                }
            }
        }
        return StressF;
    };

// Element-to-point influence matrix (submatrix of the global one)

    il::StaticArray2D<double, 6, 18>
    Local_IM(const int Kernel,
             double mu, double nu, double h, std::complex<double> z,
             const il::StaticArray<std::complex<double>, 3> &tau,
             const il::StaticArray2D<std::complex<double>, 6, 6> &SFM) {
        // This function assembles a local "stiffness" sub-matrix
        // (influence of DD at the element nodes to stresses at the point z)
        // in terms of a triangular element's local coordinates
        //
        // tau (3) are coordinates of element's vertices and
        // the rows of SFM (6*6) are coefficients of shape functions
        // in terms of the element's own local coordinate system (tau-coordinates);
        // h and z define the position of the (collocation) point x
        // in the same coordinates

        il::StaticArray2D<double, 6, 18> LIM{0.0};

        const std::complex<double> I(0.0, 1.0);

        // scaling ("-" sign comes from traction Somigliana ID, H-term)
        double scale = -mu / (4.0 * M_PI * (1.0 - nu));
        // tolerance parameters
        const double HTol = 1.0E-16, DTol = 1.0E-8;
        // geometrical
        double an, am;
        std::complex<double> eixm, eixn, eipm, eipn, ntau2;

        // tz[m] and d[m] can be calculated here
        il::StaticArray<std::complex<double>, 3> tz, d, dtau;
        for (int j = 0; j < 3; ++j) {
            int q = (j + 1) % 3;
            tz[j] = tau[j] - z;
            dtau[j] = tau[q] - tau[j];
            ntau2 = dtau[j] / std::conj(dtau[j]);
            d[j] = 0.5 * (tz[j] - ntau2 * std::conj(tz[j]));
        }
        // also, "shifted" SFM from z, tau[m], and local SFM
        il::StaticArray2D<std::complex<double>, 6, 6> SftZ = El_Shift_SFM(z);
        il::StaticArray2D<std::complex<double>, 6, 6> SFMz = il::dot(SFM, SftZ);
        // constituents of the integrals
        il::StaticArray<std::complex<double>, 9> Fn, Fm;
        il::StaticArray3D<std::complex<double>, 6, 3, 9> Cn, Cm;
        il::StaticArray<std::complex<double>, 5> FnD, FmD;
        il::StaticArray3D<std::complex<double>, 6, 3, 5> CnD, CmD;
        // DD-to-stress influence
        // [(S11+S22)/2; (S11-S22)/2+i*S12; (S13+i*S23)/2; S33]
        // vs SF monomials (Sij_M) and nodal values (Sij_N)
        il::StaticArray2D<std::complex<double>, 6, 3>
                Sij_M_1{0.0}, Sij_N_1{0.0},
                Sij_M_2{0.0}, Sij_N_2{0.0},
                Sij_M_3{0.0}, Sij_N_3{0.0},
                Sij_M_4{0.0}, Sij_N_4{0.0};
        il::StaticArray3D<std::complex<double>, 6, 4, 3>
                SincLn{0.0}, SincLm{0.0};

        // searching for "degenerate" edges:
        // point x (collocation pt) projects onto an edge line or a vertex
        bool IsDegen = std::abs(d[0]) < HTol || std::abs(d[1]) < HTol ||
                       std::abs(d[2]) < HTol; // (d[0]*d[1]*d[2]==0);
        il::StaticArray2D<bool, 2, 3> IsClose{false};

        // calculating angles (phi, psi, chi)
        il::StaticArray<double, 3> phi{0.0}, psi{0.0};
        il::StaticArray2D<double, 2, 3> chi{0.0};
        for (int j = 0; j < 3; ++j) {
            phi[j] = std::arg(tz[j]);
            psi[j] = std::arg(d[j]);
        }
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 2; ++k) {
                int q = (j + k) % 3;
                chi(k, j) = phi[q] - psi[j];
                // make sure it's between -pi and pi (add or subtract 2*pi)
                if (chi(k, j) <= -M_PI)
                    while (chi(k, j) <= -M_PI)
                        chi(k, j) += 2.0 * M_PI;
                else if (chi(k, j) > M_PI)
                    while (chi(k, j) > M_PI)
                        chi(k, j) -= 2.0 * M_PI;
                // reprooving for "degenerate" edges
                // (chi angles too close to 90 degrees)
                if (fabs(M_PI_2 - std::fabs(chi(k, j))) < DTol) {
                    IsClose(k, j) = true;
                    IsDegen = true;
                }
            }
        }

        // summation over edges
        for (int m = 0; m < 3; ++m) {
            int n = (m + 1) % 3;
            if (std::abs(d[m]) >= HTol && ~IsClose(0, m) && ~IsClose(1, m)) {
                eixm = std::exp(I * chi(0, m));
                eixn = std::exp(I * chi(1, m));
                // limit case (point x on the element's plane)
                if (std::fabs(h) < HTol) {
                    SincLn = Sij_Int_Lim(Kernel, nu, eixn, -1.0, d[m]);
                    SincLm = Sij_Int_Lim(Kernel, nu, eixm, -1.0, d[m]);
                    for (int j = 0; j < 6; ++j) {
                        for (int k = 0; k < 3; ++k) {
                            Sij_M_1(j, k) += SincLn(j, 0, k) - SincLm(j, 0, k);
                            Sij_M_2(j, k) += SincLn(j, 1, k) - SincLm(j, 1, k);
                            Sij_M_3(j, k) += SincLn(j, 2, k) - SincLm(j, 2, k);
                            Sij_M_4(j, k) += SincLn(j, 3, k) - SincLm(j, 3, k);
                        }
                    }
                } else { // out-of-plane case
                    an = std::abs(tz[n] - d[m]);
                    an = (chi(1, m) < 0) ? -an : an;
                    am = std::abs(tz[m] - d[m]);
                    am = (chi(0, m) < 0) ? -am : am;
                    Fn = ICFns(h, d[m], an, chi(1, m), eixn);
                    Fm = ICFns(h, d[m], am, chi(0, m), eixm);
                    // S11+S22
                    Cn = Sij_Int_Gen(Kernel, 0, nu, eixn, h, d[m]);
                    Cm = Sij_Int_Gen(Kernel, 0, nu, eixm, h, d[m]);
                    il::blas(1.0, Cn, Fn, 1.0, il::io, Sij_M_1);
                    il::blas(-1.0, Cm, Fm, 1.0, il::io, Sij_M_1);
                    if (IsDegen) { // "degenerate" case
                        eipm = std::exp(I * phi[n]);
                        eipn = std::exp(I * phi[m]);
                        FnD = ICFns_red(h, d[m], an);
                        FmD = ICFns_red(h, d[m], am);
                        CnD = Sij_Int_Red(Kernel, 0, nu, eipn, h, d[m]);
                        CmD = Sij_Int_Red(Kernel, 0, nu, eipm, h, d[m]);
                        il::blas(1.0, CnD, FnD, 1.0, il::io, Sij_M_1);
                        il::blas(-1.0, CmD, FmD, 1.0, il::io, Sij_M_1);
                    }
                    // S11-S22+2*I*S12
                    Cn = Sij_Int_Gen(Kernel, 1, nu, eixn, h, d[m]);
                    Cm = Sij_Int_Gen(Kernel, 1, nu, eixm, h, d[m]);
                    il::blas(1.0, Cn, Fn, 1.0, il::io, Sij_M_2);
                    il::blas(-1.0, Cm, Fm, 1.0, il::io, Sij_M_2);
                    if (IsDegen) { // "degenerate" case
                        CnD = Sij_Int_Red(Kernel, 1, nu, eipn, h, d[m]);
                        CmD = Sij_Int_Red(Kernel, 1, nu, eipm, h, d[m]);
                        il::blas(1.0, CnD, FnD, 1.0, il::io, Sij_M_2);
                        il::blas(-1.0, CmD, FmD, 1.0, il::io, Sij_M_2);
                    }
                    // S13+I*S23
                    Cn = Sij_Int_Gen(Kernel, 2, nu, eixn, h, d[m]);
                    Cm = Sij_Int_Gen(Kernel, 2, nu, eixm, h, d[m]);
                    il::blas(1.0, Cn, Fn, 1.0, il::io, Sij_M_3);
                    il::blas(-1.0, Cm, Fm, 1.0, il::io, Sij_M_3);
                    if (IsDegen) { // "degenerate" case
                        CnD = Sij_Int_Red(Kernel, 2, nu, eipn, h, d[m]);
                        CmD = Sij_Int_Red(Kernel, 2, nu, eipm, h, d[m]);
                        il::blas(1.0, CnD, FnD, 1.0, il::io, Sij_M_3);
                        il::blas(-1.0, CmD, FmD, 1.0, il::io, Sij_M_3);
                    }
                    // S33
                    Cn = Sij_Int_Gen(Kernel, 3, nu, eixn, h, d[m]);
                    Cm = Sij_Int_Gen(Kernel, 3, nu, eixm, h, d[m]);
                    il::blas(1.0, Cn, Fn, 1.0, il::io, Sij_M_4);
                    il::blas(-1.0, Cm, Fm, 1.0, il::io, Sij_M_4);
                    if (IsDegen) { // "degenerate" case
                        CnD = Sij_Int_Red(Kernel, 3, nu, eipn, h, d[m]);
                        CmD = Sij_Int_Red(Kernel, 3, nu, eipm, h, d[m]);
                        il::blas(1.0, CnD, FnD, 1.0, il::io, Sij_M_4);
                        il::blas(-1.0, CmD, FmD, 1.0, il::io, Sij_M_4);
                    }
                }
            }
        }

        // here comes contraction with "shifted" SFM (left)
        Sij_N_1 = il::dot(SFMz, Sij_M_1);
        Sij_N_2 = il::dot(SFMz, Sij_M_2);
        Sij_N_3 = il::dot(SFMz, Sij_M_3);
        Sij_N_4 = il::dot(SFMz, Sij_M_4);

        // re-shaping of the resulting matrix
        // and scaling (comment out if not necessary)
        for (int j = 0; j < 6; ++j) {
            int q = j * 3;
            for (int k = 0; k < 3; ++k) {
                // [S11; S22; S33; S12; S13; S23] vs \delta{u}_k at j-th node
                LIM(0, q + k) = scale * (std::real(Sij_N_1(j, k)) +
                                         std::real(Sij_N_2(j, k))); // S11
                LIM(1, q + k) = scale * (std::real(Sij_N_1(j, k)) -
                                         std::real(Sij_N_2(j, k))); // S22
                LIM(2, q + k) = scale * std::real(Sij_N_4(j, k)); // S33
                LIM(3, q + k) = scale * std::imag(Sij_N_2(j, k)); // S12
                LIM(4, q + k) = scale * 2.0 * std::real(Sij_N_3(j, k)); // S13
                LIM(5, q + k) = scale * 2.0 * std::imag(Sij_N_3(j, k)); // S23
            }
        }
        return LIM;
    };

}