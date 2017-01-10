//
// This file is part of 3d_bem.
//
// Created by nikolski on 1/10/2017.
// Copyright (c) ECOLE POLYTECHNIQUE FEDERALE DE LAUSANNE, Switzerland,
// Geo-Energy Laboratory, 2016-2017.  All rights reserved.
// See the LICENSE.TXT file for more details. 
//

#include <il/StaticArray.h>
#include <il/StaticArray2D.h>
#include <il/linear_algebra/dense/blas/dot.h>
#include <complex>
//#include <Submatrix.h>

double VNorm(il::StaticArray<double, 3>);
il::StaticArray<double, 3> normalize(il::StaticArray<double, 3>);
il::StaticArray<double, 3> cross(il::StaticArray<double, 3>, il::StaticArray<double, 3>);

double VNorm(il::StaticArray<double, 3> a) {
    // L2 norm of a vector
    double N = 0.0;
    for (int k=0; k<int(a.size); ++k) {
        N += a[k]*a[k];
    }
    N = std::sqrt(N);
    return N;
};

il::StaticArray<double, 3> normalize(il::StaticArray<double, 3> a) {
    // normalized 3D vector
    il::StaticArray<double, 3> e;
    double N = VNorm(a);
    for (int k=0; k<int(a.size); ++k) {
        e[k] = a[k]/N;
    }
    return e;
};

il::StaticArray<double, 3> cross(il::StaticArray<double, 3> a, il::StaticArray<double, 3> b) {
    // cross product of two 3D vectors
    IL_ASSERT(int(a.size) == 3);
    IL_ASSERT(int(b.size) == 3);
    il::StaticArray<double, 3> c;
    c[0] = a[1]*b[2] - a[2]*b[1];
    c[1] = a[2]*b[0] - a[0]*b[2];
    c[2] = a[0]*b[1] - a[1]*b[0];
    return c;
};

il::StaticArray2D<double, 3, 3> El_LB_RT(il::StaticArray2D<double,3,3> EV) {
    // This function calculates the rotation tensor
    // of the element's local Cartesian coordinate system
    il::StaticArray<double, 3> a1{0.0}, a2{0.0}, a3{0.0}, e1{0.0}, e2{0.0}, e3{0.0};
    for (int n=0; n<=2; ++n) {
        a1[n] = EV(n,1)-EV(n,0);
        a2[n] = EV(n,2)-EV(n,0);
    }
    e1=normalize(a1);
    a3=cross(e1,a2);
    e3=normalize(a3);
    e2=cross(e3,e1);
    il::StaticArray2D<double, 3, 3> RM;
    for (int j=0; j<=3; ++j) {
        RM(j, 0) = e1[j];
        RM(j, 1) = e2[j];
        RM(j, 2) = e3[j];
    }
    return RM;
};

il::StaticArray2D<std::complex<double>, 2, 2> El_CT(il::StaticArray2D<double,3,3> EV) {
    // This function calculates the coordinate transform
    // from local Cartesian coordinates to "master element"
    // ([tau, conj(tau)] to [x,y])
    il::StaticArray<std::complex<double>, 2> z23{0.0};
    il::StaticArray<double, 3> xsi{0.0}, VV{0.0};
    std::complex<double> Dt;
    il::StaticArray2D<double, 3, 3> RM = El_LB_RT(EV);
    il::StaticArray2D<std::complex<double>, 2, 2> MI{0.0};
    for (int k=0; k<=1; ++k) {
        for (int n=0; n<=2; ++n) {
            VV[n] = EV(n,k+1)-EV(n,0);
        }
        xsi = il::dot(VV, RM); // or transposed RM dot VV
        z23[k] = std::complex(xsi[1], xsi[2]);
    }
    Dt=z23[0]*std::conj(z23[1])-z23[1]*std::conj(z23[0]);
    MI(0, 0) = std::conj(z23[1])/Dt; MI(0, 1) = -z23[1]/Dt;
    MI(1, 0) = -std::conj(z23[0])/Dt; MI(1, 1) = z23[0]/Dt;
    return MI;
};

il::StaticArray2D<std::complex<double>, 6, 6> El_SFM_S(il::StaticArray2D<double,3,3> EV) {
    // This function calculates the basis (shape) functions' coefficients
    // in terms of complex (tau, conj(tau)) representation of local element's coordinates
    // for trivial (middle) distribution of edge nodes
    // returns the same as El_SFM_N(EV, {1.0, 1.0, 1.0})

    // CT defines inverse coordinate transform [tau, conj(tau)] to [x,y]
    // common denominator (determinant)
    // CD=(z(3)-z(2)).*zc(1)+(z(1)-z(3)).*zc(2)+(z(2)-z(1)).*zc(3);
    // CT=[zc(3)-zc(1), z(1)-z(3);/ zc(1)-zc(2), z(2)-z(1)]./CD;
    il::StaticArray2D<std::complex<double>, 2, 2> CT = El_CT(EV);
    il::StaticArray2D<std::complex<double>, 3, 3> CQ{0.0};
    il::StaticArray2D<std::complex<double>, 6, 6> SFM{0.0}, SFM_M{0.0}, CTau{0.0};

    // coefficients of shape functions (rows) for master element (0<=x,y<=1); ~[1, x, y, x^2, y^2, x*y]
    SFM_M(0,0) = 1.0; SFM_M(0,1) = -3.0; SFM_M(0,2) = -3.0;
    SFM_M(0,3) = 2.0; SFM_M(0,4) = 2.0; SFM_M(0,5) = 4.0;
    SFM_M(1,1) = -1.0; SFM_M(1,3) = 2.0;
    SFM_M(2,2) = -1.0; SFM_M(2,4) = 2.0;
    SFM_M(3,5) = 4.0;
    SFM_M(4,2) = 4.0; SFM_M(4,4) = -4.0; SFM_M(4,5) = -4.0;
    SFM_M(5,1) = 4.0; SFM_M(5,3) = -4.0; SFM_M(5,5) = -4.0;

    // inverse coordinate transform [1, tau, tau_c, tau^2, tau_c^2, tau*tau_c] to [1, x, y, x^2, y^2, x*y]
    for (int j=0; j<=1; ++j) {
        for(int k=0; k<=1; ++k) {
            CQ(j,k) = CT(j,k)*CT(j,k);
        }
        CQ(j,2) = 2.0*CT(j,0)*CT(j,1);
        CQ(2,j) = CT(0,j)*CT(1,j);
    }
    CQ(2,2) = CT(0,0)*CT(1,1) + CT(1,0)*CT(0,1);
    CTau(0,0) = 1.0;
    for (int j=0; j<=1; ++j) {
        for(int k=0; k<=1; ++k) {
            CTau(j+1, k+1) = CT(j,k);
        }
    }
    for (int j=0; j<=2; ++j) {
        for(int k=0; k<=2; ++k) {
            CTau(j+3, k+3) = CQ(j,k);
        }
    }
    SFM=il::dot(SFM_M, CTau);
    return SFM;
};

il::StaticArray2D<std::complex<double>, 6, 6> El_SFM_N(il::StaticArray2D<double,3,3> EV, il::StaticArray<double,3> VW) {
    // This function calculates the basis (shape) functions' coefficients
    // in terms of complex (tau, conj(tau)) representation of local element's coordinates
    // for non-trivial distribution of edge nodes defined by "weights" VW

    double P12=VW[0]/VW[1], P13=VW[0]/VW[2], P23=VW[1]/VW[2],
    C122=P12+1.0,     // (VW[0]+w(2))/VW[1];
    C121=1.0/P12+1.0,	// (VW[0]+w(2))/VW[0];
    C12q=C121+C122,
    C233=P23+1.0,     // (VW[1]+w(3))/VW[2];
    C232=1.0/P23+1.0,	// (VW[1]+w(3))/VW[1];
    C23q=C232+C233,
    C133=P13+1.0,     // (VW[0]+w(3))/VW[2];
    C131=1.0/P13+1.0,	// (VW[0]+w(3))/VW[0];
    C13q=C131+C133;

    // CT defines inverse coordinate transform [tau, conj(tau)] to [x,y]
    // common denominator (determinant)
    // CD=(z(3)-z(2)).*zc(1)+(z(1)-z(3)).*zc(2)+(z(2)-z(1)).*zc(3);
    // CT=[zc(3)-zc(1), z(1)-z(3);/ zc(1)-zc(2), z(2)-z(1)]./CD;
    il::StaticArray2D<std::complex<double>, 2, 2> CT = El_CT(EV);
    il::StaticArray2D<std::complex<double>, 3, 3> CQ{0.0};
    il::StaticArray2D<std::complex<double>, 6, 6> SFM{0.0}, SFM_M{0.0}, CTau{0.0};

    // coefficients of shape functions (rows) for master element (0<=x,y<=1); ~[1, x, y, x^2, y^2, x*y]
    SFM_M(0,0) = 1.0; SFM_M(0,1) = -P12-2.0; SFM_M(0,2) = -P13-2.0;
    SFM_M(0,3) = C122; SFM_M(0,4) = C133; SFM_M(0,5) = P13+P12+2.0;
    SFM_M(1,1) = -1.0/P12; SFM_M(1,3) = C121; SFM_M(1,5) = 1.0/P12-P23;
    SFM_M(2,2) = -1.0/P13; SFM_M(2,4) = C131; SFM_M(2,5) = 1.0/P13-1.0/P23;
    SFM_M(3,5) = C23q;
    SFM_M(4,2) = C13q; SFM_M(4,4) = -C13q; SFM_M(4,5) = -C13q;
    SFM_M(5,1) = C12q; SFM_M(5,3) = -C12q; SFM_M(5,5) = -C12q;

    // inverse coordinate transform [1, tau, tau_c, tau^2, tau_c^2, tau*tau_c] to [1, x, y, x^2, y^2, x*y]
    for (int j=0; j<=1; ++j) {
        for(int k=0; k<=1; ++k) {
            CQ(j,k) = CT(j,k)*CT(j,k);
        }
        CQ(j,2) = 2.0*CT(j,0)*CT(j,1);
        CQ(2,j) = CT(0,j)*CT(1,j);
    }
    CQ(2,2) = CT(0,0)*CT(1,1) + CT(1,0)*CT(0,1);
    CTau(0,0) = 1.0;
    for (int j=0; j<=1; ++j) {
        for(int k=0; k<=1; ++k) {
            CTau(j+1, k+1) = CT(j,k);
        }
    }
    for (int j=0; j<=2; ++j) {
        for(int k=0; k<=2; ++k) {
            CTau(j+3, k+3) = CQ(j,k);
        }
    }
    SFM=il::dot(SFM_M, CTau);
    return SFM;
};

//il::StaticArray2D<std::complex<double>, 6, 6> El_SFM_C(il::StaticArray2D<double,3,3> EV, il::StaticArray<double,3> VW, double beta) {
// This function calculates the basis (shape) functions' coefficients
// in terms of complex (tau, conj(tau)) representation of local element's coordinates
// for nodes' offset (e.g. at collocation points) defined by beta
// and non-trivial distribution of edge nodes
//}
