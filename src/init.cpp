// Copyright 2011 @ Bjoern Schenke, Sangyong Jeon, and Charles Gale
#include <iomanip>
#include <sstream>
#include <omp.h>
#include "./util.h"
#include "./cell.h"
#include "./grid.h"
#include "./init.h"
#include "./eos.h"
#include <complex>
#include <iterator>

#ifndef _OPENMP
  #define omp_get_thread_num() 0
#endif

using namespace std;

Init::Init(const EOS &eosIn, InitData &DATA_in, hydro_source &hydro_source_in) :
    DATA(DATA_in), eos(eosIn) , hydro_source_terms(hydro_source_in) {}

void Init::InitArena(SCGrid &arena_prev, SCGrid &arena_current,
                     SCGrid &arena_future) {
    music_message.info("initArena");
    if (DATA.Initial_profile == 0) {
        music_message << "Using Initial_profile=" << DATA.Initial_profile;
        music_message << "nx=" << DATA.nx << ", ny=" << DATA.ny;
        music_message << "dx=" << DATA.delta_x << ", dy=" << DATA.delta_y;
        music_message.flush("info");
    } else if (DATA.Initial_profile == 1) {
        music_message << "Using Initial_profile=" << DATA.Initial_profile;
        DATA.nx = 2;
        DATA.ny = 2;
        DATA.neta = 695;
        DATA.delta_x = 0.1;
        DATA.delta_y = 0.1;
        DATA.delta_eta = 0.02;
        music_message << "nx=" << DATA.nx << ", ny=" << DATA.ny;
        music_message << "dx=" << DATA.delta_x << ", dy=" << DATA.delta_y;
        music_message << "neta=" << DATA.neta << ", deta=" << DATA.delta_eta;
        music_message.flush("info");
    } else if (DATA.Initial_profile == 8) {
        music_message.info(DATA.initName);
        ifstream profile(DATA.initName.c_str());
        string dummy;
        int nx, ny, neta;
        double deta, dx, dy, dummy2;
        // read the first line with general info
        profile >> dummy >> dummy >> dummy2
                >> dummy >> neta >> dummy >> nx >> dummy >> ny
                >> dummy >> deta >> dummy >> dx >> dummy >> dy;
        profile.close();
        music_message << "Using Initial_profile=" << DATA.Initial_profile
                      << ". Overwriting lattice dimensions:";
        DATA.nx = nx;
        DATA.ny = ny;
        DATA.delta_x = dx;
        DATA.delta_y = dy;

        music_message << "neta=" << neta << ", nx=" << nx << ", ny=" << ny;
        music_message << "deta=" << DATA.delta_eta << ", dx=" << DATA.delta_x
                      << ", dy=" << DATA.delta_y;
        music_message.flush("info");
    } else if (   DATA.Initial_profile == 9 || DATA.Initial_profile == 91
               || DATA.Initial_profile == 92) {
        music_message.info(DATA.initName);
        ifstream profile(DATA.initName.c_str());
        string dummy;
        int nx, ny, neta;
        double deta, dx, dy, dummy2;
        // read the first line with general info
        profile >> dummy >> dummy >> dummy2
                >> dummy >> neta >> dummy >> nx >> dummy >> ny
                >> dummy >> deta >> dummy >> dx >> dummy >> dy;
        profile.close();
        music_message << "Using Initial_profile=" << DATA.Initial_profile
                      << ". Overwriting lattice dimensions:";
        DATA.nx = nx;
        DATA.ny = ny;
        DATA.neta = neta;
        DATA.delta_x = dx;
        DATA.delta_y = dy;
        DATA.delta_eta = 0.1;

        music_message << "neta=" << neta << ", nx=" << nx << ", ny=" << ny;
        music_message << "deta=" << DATA.delta_eta << ", dx=" << DATA.delta_x
                      << ", dy=" << DATA.delta_y;
        music_message.flush("info");
    } else if (DATA.Initial_profile == 13) {
        DATA.tau0 = hydro_source_terms.get_source_tau_min() - DATA.delta_tau;
        DATA.tau0 = std::max(0.1, DATA.tau0);
    } else if (DATA.Initial_profile == 30) {
        DATA.tau0 = hydro_source_terms.get_source_tau_min();
    } else if (DATA.Initial_profile == 101) {
        cout << "Using Initial_profile=" << DATA.Initial_profile << endl;
        cout << "nx=" << DATA.nx << ", ny=" << DATA.ny << endl;
        cout << "dx=" << DATA.delta_x << ", dy=" << DATA.delta_y << endl;
    }

    // initialize arena
    arena_prev    = SCGrid(DATA.nx, DATA.ny,DATA.neta);
    arena_current = SCGrid(DATA.nx, DATA.ny,DATA.neta);
    arena_future  = SCGrid(DATA.nx, DATA.ny,DATA.neta);
    music_message.info("Grid allocated.");

    InitTJb(arena_prev, arena_current);

    if (DATA.output_initial_density_profiles == 1) {
        output_initial_density_profiles(arena_current);
    }
}/* InitArena */

//! This is a shell function to initial hydrodynamic fields
void Init::InitTJb(SCGrid &arena_prev, SCGrid &arena_current) {
    if (DATA.Initial_profile == 0) {
        // Gubser flow test
        music_message.info(" Perform Gubser flow test ... ");
        music_message.info(" ----- information on initial distribution -----");

        #pragma omp parallel for
        for (int ieta = 0; ieta < arena_current.nEta(); ieta++) {
            //cout << "[Info] Thread " << omp_get_thread_num()
            //     << " executes loop iteraction ieta = " << ieta << endl;
            initial_Gubser_XY(ieta, arena_prev, arena_current);
        }/* ieta */
    } else if (DATA.Initial_profile == 1) {
        // code test in 1+1 D vs Monnai's results
        music_message.info(" Perform 1+1D test vs Monnai's results... ");
        initial_1p1D_eta(arena_prev, arena_current);
    } else if (DATA.Initial_profile == 3) {
        music_message.info(" Deformed Gaussian initial condition");
	initial_distorted_Gaussian(arena_prev, arena_current);
    } else if (DATA.Initial_profile == 8) {
        // read in the profile from file
        // - IPGlasma initial conditions with initial flow
        music_message.info(" ----- information on initial distribution -----");
        music_message << "file name used: " << DATA.initName;
        music_message.flush("info");

        #pragma omp parallel for
        for (int ieta = 0; ieta < arena_current.nEta(); ieta++) {
            //cout << "[Info] Thread " << omp_get_thread_num()
            //     << " executes loop iteraction ieta = " << ieta << endl;
            initial_IPGlasma_XY(ieta, arena_prev, arena_current);
        } /* ieta */
    } else if (   DATA.Initial_profile == 9 || DATA.Initial_profile == 91
               || DATA.Initial_profile == 92) {
        // read in the profile from file
        // - IPGlasma initial conditions with initial flow
        // and initial shear viscous tensor
        music_message.info(" ----- information on initial distribution -----");
        music_message << "file name used: " << DATA.initName;
        music_message.flush("info");

        #pragma omp parallel for
        for (int ieta = 0; ieta < arena_current.nEta(); ieta++) {
            //cout << "[Info] Thread " << omp_get_thread_num()
            //     << " executes loop iteraction ieta = " << ieta << endl;
            initial_IPGlasma_XY_with_pi(ieta, arena_prev, arena_current);
        } /* ieta */
    } else if (DATA.Initial_profile == 11) {
        // read in the transverse profile from file with finite rho_B
        // the initial entropy and net baryon density profile are
        // constructed by nuclear thickness function TA and TB.
        // Along the longitudinal direction an asymmetric contribution from
        // target and projectile thickness function is allowed
        music_message.info(" ----- information on initial distribution -----");
        music_message << "file name used: " << DATA.initName_TA << " and "
                      << DATA.initName_TB;
        music_message.flush("info");

        #pragma omp parallel for
        for (int ieta = 0; ieta < arena_current.nEta(); ieta++) {
            //cout << "[Info] Thread " << omp_get_thread_num()
            //     << " executes loop iteraction ieta = " << ieta << endl;
            initial_MCGlb_with_rhob_XY(ieta, arena_prev, arena_current);
        } /* ix, iy, ieta */
    } else if (DATA.Initial_profile == 12) {
        // reads transverse profile generated by TRENTo
        // see: http://qcd.phy.duke.edu/trento/index.html
        music_message.info("Reading TRENTo initial conditions");
        #pragma omp parallel for
        for (int ieta = 0; ieta < arena_current.nEta(); ieta++){
            initial_trento_XY(ieta, arena_prev, arena_current);
        }
    } else if (DATA.Initial_profile == 13) {
        music_message.info("Initialize hydro with source terms");
        #pragma omp parallel for
        for (int ieta = 0; ieta < arena_current.nEta(); ieta++) {
            //cout << "[Info] Thread " << omp_get_thread_num()
            //     << " executes loop iteraction ieta = " << ieta << endl;
            initial_MCGlbLEXUS_with_rhob_XY(ieta, arena_prev, arena_current);
        } /* ix, iy, ieta */
    } else if (DATA.Initial_profile == 30) {
        #pragma omp parallel for
        for (int ieta = 0; ieta < arena_current.nEta(); ieta++) {
            //cout << "[Info] Thread " << omp_get_thread_num()
            //     << " executes loop iteraction ieta = " << ieta << endl;
            initial_AMPT_XY(ieta, arena_prev, arena_current);
        }
    } else if (DATA.Initial_profile == 101) {
        initial_UMN_with_rhob(arena_prev, arena_current);
    }
    output_2D_eccentricities(0, arena_current);
    music_message.info("initial distribution done.");
}

void Init::initial_Gubser_XY(int ieta, SCGrid &arena_prev,
                             SCGrid &arena_current) {
    string input_filename;
    string input_filename_prev;
    if (DATA.turn_on_shear == 1) {
        input_filename = "tests/Gubser_flow/Initial_Profile.dat";
    } else {
        input_filename = "tests/Gubser_flow/y=0_tau=1.00_ideal.dat";
        input_filename_prev = "tests/Gubser_flow/y=0_tau=0.98_ideal.dat";
    }

    ifstream profile(input_filename.c_str());
    if (!profile.good()) {
        music_message << "Init::InitTJb: "
                      << "Can not open the initial file: " << input_filename;
        music_message.flush("error");
        exit(1);
    }
    ifstream profile_prev;
    if (DATA.turn_on_shear == 0) {
        profile_prev.open(input_filename_prev.c_str());
        if (!profile_prev.good()) {
            music_message << "Init::InitTJb: "
                          << "Can not open the initial file: "
                          << input_filename_prev;
            music_message.flush("error");
            exit(1);
        }
    }

    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    double temp_profile_ed[nx][ny];
    double temp_profile_ux[nx][ny];
    double temp_profile_uy[nx][ny];
    double temp_profile_ed_prev[nx][ny];
    double temp_profile_rhob[nx][ny];
    double temp_profile_rhob_prev[nx][ny];
    double temp_profile_ux_prev[nx][ny];
    double temp_profile_uy_prev[nx][ny];
    double temp_profile_pixx[nx][ny];
    double temp_profile_piyy[nx][ny];
    double temp_profile_pixy[nx][ny];
    double temp_profile_pi00[nx][ny];
    double temp_profile_pi0x[nx][ny];
    double temp_profile_pi0y[nx][ny];
    double temp_profile_pi33[nx][ny];

    double dummy;
    for (int ix = 0; ix < nx; ix++) {
        for (int iy = 0; iy < ny; iy++) {
            if (DATA.turn_on_shear == 1) {
                profile >> dummy >> dummy >> temp_profile_ed[ix][iy]
                        >> temp_profile_ux[ix][iy] >> temp_profile_uy[ix][iy];
                profile >> temp_profile_pixx[ix][iy]
                        >> temp_profile_piyy[ix][iy]
                        >> temp_profile_pixy[ix][iy]
                        >> temp_profile_pi00[ix][iy]
                        >> temp_profile_pi0x[ix][iy]
                        >> temp_profile_pi0y[ix][iy]
                        >> temp_profile_pi33[ix][iy];
            } else {
                profile >> dummy >> dummy >> temp_profile_ed[ix][iy]
                        >> temp_profile_rhob[ix][iy]
                        >> temp_profile_ux[ix][iy] >> temp_profile_uy[ix][iy];
                profile_prev >> dummy >> dummy >> temp_profile_ed_prev[ix][iy]
                             >> temp_profile_rhob_prev[ix][iy]
                             >> temp_profile_ux_prev[ix][iy]
                             >> temp_profile_uy_prev[ix][iy];
            }
        }
    }
    profile.close();
    if (DATA.turn_on_shear == 0) {
        profile_prev.close();
    }

    for (int ix = 0; ix < nx; ix++) {
        for (int iy = 0; iy< ny; iy++) {
            double rhob = 0.0;
            if (DATA.turn_on_shear == 0 && DATA.turn_on_rhob == 1) {
                rhob = temp_profile_rhob[ix][iy];
            }

            double epsilon = temp_profile_ed[ix][iy];

            arena_current(ix, iy, ieta).epsilon = epsilon;
            arena_prev   (ix, iy, ieta).epsilon = epsilon;
            arena_current(ix, iy, ieta).rhob    = rhob;
            arena_prev   (ix, iy, ieta).rhob    = rhob;

            double utau_local = sqrt(1.
                          + temp_profile_ux[ix][iy]*temp_profile_ux[ix][iy]
                          + temp_profile_uy[ix][iy]*temp_profile_uy[ix][iy]);
            arena_current(ix, iy, ieta).u[0] = utau_local;
            arena_current(ix, iy, ieta).u[1] = temp_profile_ux[ix][iy];
            arena_current(ix, iy, ieta).u[2] = temp_profile_uy[ix][iy];
            arena_current(ix, iy, ieta).u[3] = 0.0;
            arena_prev(ix, iy, ieta).u = arena_current(ix, iy, ieta).u;

            if (DATA.turn_on_shear == 0) {
                double utau_prev = sqrt(1.
                    + temp_profile_ux_prev[ix][iy]*temp_profile_ux_prev[ix][iy]
                    + temp_profile_uy_prev[ix][iy]*temp_profile_uy_prev[ix][iy]
                );
                arena_prev(ix, iy, ieta).u[0] = utau_prev;
                arena_prev(ix, iy, ieta).u[1] = temp_profile_ux_prev[ix][iy];
                arena_prev(ix, iy, ieta).u[2] = temp_profile_uy_prev[ix][iy];
                arena_prev(ix, iy, ieta).u[3] = 0.0;
            }

            if (DATA.turn_on_shear == 1) {
                arena_current(ix,iy,ieta).Wmunu[0] = temp_profile_pi00[ix][iy];
                arena_current(ix,iy,ieta).Wmunu[1] = temp_profile_pi0x[ix][iy];
                arena_current(ix,iy,ieta).Wmunu[2] = temp_profile_pi0y[ix][iy];
                arena_current(ix,iy,ieta).Wmunu[3] = 0.0;
                arena_current(ix,iy,ieta).Wmunu[4] = temp_profile_pixx[ix][iy];
                arena_current(ix,iy,ieta).Wmunu[5] = temp_profile_pixy[ix][iy];
                arena_current(ix,iy,ieta).Wmunu[6] = 0.0;
                arena_current(ix,iy,ieta).Wmunu[7] = temp_profile_piyy[ix][iy];
                arena_current(ix,iy,ieta).Wmunu[8] = 0.0;
                arena_current(ix,iy,ieta).Wmunu[9] = temp_profile_pi33[ix][iy];
            }
            arena_prev(ix,iy,ieta).Wmunu = arena_current(ix,iy,ieta).Wmunu;
        }
    }
}

void Init::initial_1p1D_eta(SCGrid &arena_prev, SCGrid &arena_current) {
    string input_ed_filename;
    string input_rhob_filename;
    input_ed_filename = "tests/test_1+1D_with_Akihiko/e_baryon_init.dat";
    input_rhob_filename = "tests/test_1+1D_with_Akihiko/rhoB_baryon_init.dat";

    ifstream profile_ed(input_ed_filename.c_str());
    if (!profile_ed.good()) {
        music_message << "Init::InitTJb: "
                      << "Can not open the initial file: "
                      << input_ed_filename;
        music_message.flush("error");
        exit(1);
    }
    ifstream profile_rhob;
    profile_rhob.open(input_rhob_filename.c_str());
    if (!profile_rhob.good()) {
        music_message << "Init::InitTJb: "
                      << "Can not open the initial file: "
                      << input_rhob_filename;
        music_message.flush("error");
        exit(1);
    }

    const int neta = arena_current.nEta();
    double temp_profile_ed[neta];
    double temp_profile_rhob[neta];

    double dummy;
    for (int ieta = 0; ieta < neta; ieta++) {
        profile_ed >> dummy >> temp_profile_ed[ieta];
        profile_rhob >> dummy >> temp_profile_rhob[ieta];
    }
    profile_ed.close();
    profile_rhob.close();

    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    for (int ieta = 0; ieta < neta; ieta++) {
        double rhob = temp_profile_rhob[ieta];
        double epsilon = temp_profile_ed[ieta]/hbarc;   // fm^-4
        for (int ix = 0; ix < nx; ix++) {
            for (int iy = 0; iy< ny; iy++) {
                // set all values in the grid element:
                arena_current(ix, iy, ieta).epsilon = epsilon;
                arena_current(ix, iy, ieta).rhob    = rhob;

                arena_current(ix, iy, ieta).u[0] = 1.0;
                arena_current(ix, iy, ieta).u[1] = 0.0;
                arena_current(ix, iy, ieta).u[2] = 0.0;
                arena_current(ix, iy, ieta).u[3] = 0.0;

                arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);
            }
        }
    }
}

void Init::initial_IPGlasma_XY(int ieta, SCGrid &arena_prev,
                               SCGrid &arena_current) {
    ifstream profile(DATA.initName.c_str());

    string dummy;
    // read the information line
    std::getline(profile, dummy);

    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    double temp_profile_ed[nx][ny];
    double temp_profile_utau[nx][ny];
    double temp_profile_ux[nx][ny];
    double temp_profile_uy[nx][ny];

    // read the one slice
    double density, dummy1, dummy2, dummy3;
    double ux, uy, utau;
    for (int ix = 0; ix < nx; ix++) {
        for (int iy = 0; iy < ny; iy++) {
            profile >> dummy1 >> dummy2 >> dummy3
                    >> density >> utau >> ux >> uy
                    >> dummy  >> dummy  >> dummy  >> dummy;
            temp_profile_ed[ix][iy] = density;
            temp_profile_ux[ix][iy] = ux;
            temp_profile_uy[ix][iy] = uy;
            temp_profile_utau[ix][iy] = sqrt(1. + ux*ux + uy*uy);
            if (ix == 0 && iy == 0) {
                DATA.x_size = -dummy2*2;
                DATA.y_size = -dummy3*2;
                if (omp_get_thread_num() == 0) {
                    music_message << "eta_size=" << DATA.eta_size
                                  << ", x_size=" << DATA.x_size
                                  << ", y_size=" << DATA.y_size;
                    music_message.flush("info");
                }
            }
        }
    }
    profile.close();

    double eta = (DATA.delta_eta)*ieta - (DATA.eta_size)/2.0;
    double eta_envelop_ed = eta_profile_normalisation(eta);
    int entropy_flag = DATA.initializeEntropy;
    for (int ix = 0; ix < nx; ix++) {
        for (int iy = 0; iy< ny; iy++) {
            double rhob = 0.0;
            double epsilon = 0.0;
            if (entropy_flag == 0) {
                epsilon = (temp_profile_ed[ix][iy]*eta_envelop_ed
                           *DATA.sFactor/hbarc);  // 1/fm^4
            } else {
                double local_sd = (temp_profile_ed[ix][iy]*DATA.sFactor
                                   *eta_envelop_ed);
                epsilon = eos.get_s2e(local_sd, rhob);
            }
            if (epsilon < 0.00000000001)
                epsilon = 0.00000000001;

            arena_current(ix, iy, ieta).epsilon = epsilon;
            arena_current(ix, iy, ieta).rhob = rhob;

            arena_current(ix, iy, ieta).u[0] = temp_profile_utau[ix][iy];
            arena_current(ix, iy, ieta).u[1] = temp_profile_ux[ix][iy];
            arena_current(ix, iy, ieta).u[2] = temp_profile_uy[ix][iy];
            arena_current(ix, iy, ieta).u[3] = 0.0;

            arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);
        }
    }
}

void Init::initial_IPGlasma_XY_with_pi(int ieta, SCGrid &arena_prev,
                                       SCGrid &arena_current) {
    // Initial_profile == 9 : full T^\mu\nu
    // Initial_profile == 91: e and u^\mu
    // Initial_profile == 92: e only
    double tau0 = DATA.tau0;
    ifstream profile(DATA.initName.c_str());

    string dummy;
    // read the information line
    std::getline(profile, dummy);

    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    std::vector<double> temp_profile_ed(nx*ny, 0.0);
    std::vector<double> temp_profile_utau(nx*ny, 0.0);
    std::vector<double> temp_profile_ux(nx*ny, 0.0);
    std::vector<double> temp_profile_uy(nx*ny, 0.0);
    std::vector<double> temp_profile_ueta(nx*ny, 0.0);
    std::vector<double> temp_profile_pitautau(nx*ny, 0.0);
    std::vector<double> temp_profile_pitaux(nx*ny, 0.0);
    std::vector<double> temp_profile_pitauy(nx*ny, 0.0);
    std::vector<double> temp_profile_pitaueta(nx*ny, 0.0);
    std::vector<double> temp_profile_pixx(nx*ny, 0.0);
    std::vector<double> temp_profile_pixy(nx*ny, 0.0);
    std::vector<double> temp_profile_pixeta(nx*ny, 0.0);
    std::vector<double> temp_profile_piyy(nx*ny, 0.0);
    std::vector<double> temp_profile_piyeta(nx*ny, 0.0);
    std::vector<double> temp_profile_pietaeta(nx*ny, 0.0);

    // read the one slice
    double density, dummy1, dummy2, dummy3;
    double ux, uy, utau, ueta;
    double pitautau, pitaux, pitauy, pitaueta;
    double pixx, pixy, pixeta, piyy, piyeta, pietaeta;
    for (int ix = 0; ix < nx; ix++) {
        for (int iy = 0; iy < ny; iy++) {
            int idx = iy + ix*ny;
            std::getline(profile, dummy);
            std::stringstream ss(dummy);
            ss >> dummy1 >> dummy2 >> dummy3
               >> density >> utau >> ux >> uy >> ueta
               >> pitautau >> pitaux >> pitauy >> pitaueta
               >> pixx >> pixy >> pixeta >> piyy >> piyeta >> pietaeta;
            temp_profile_ed    [idx] = density;
            temp_profile_ux    [idx] = ux;
            temp_profile_uy    [idx] = uy;
            temp_profile_ueta  [idx] = ueta*tau0;
            temp_profile_utau  [idx] = sqrt(1. + ux*ux + uy*uy + ueta*ueta);
            temp_profile_pixx  [idx] = pixx*DATA.sFactor;
            temp_profile_pixy  [idx] = pixy*DATA.sFactor;
            temp_profile_pixeta[idx] = pixeta*tau0*DATA.sFactor;
            temp_profile_piyy  [idx] = piyy*DATA.sFactor;
            temp_profile_piyeta[idx] = piyeta*tau0*DATA.sFactor;

            utau = temp_profile_utau[idx];
            ueta = ueta*tau0;
            temp_profile_pietaeta[idx] = (
                (2.*(  ux*uy*temp_profile_pixy[idx]
                     + ux*ueta*temp_profile_pixeta[idx]
                     + uy*ueta*temp_profile_piyeta[idx])
                 - (utau*utau - ux*ux)*temp_profile_pixx[idx]
                 - (utau*utau - uy*uy)*temp_profile_piyy[idx])
                /(utau*utau - ueta*ueta));
            temp_profile_pitaux  [idx] = (1./utau
                *(  temp_profile_pixx[idx]*ux
                  + temp_profile_pixy[idx]*uy
                  + temp_profile_pixeta[idx]*ueta));
            temp_profile_pitauy  [idx] = (1./utau
                *(  temp_profile_pixy[idx]*ux
                  + temp_profile_piyy[idx]*uy
                  + temp_profile_piyeta[idx]*ueta));
            temp_profile_pitaueta[idx] = (1./utau
                *(  temp_profile_pixeta[idx]*ux
                  + temp_profile_piyeta[idx]*uy
                  + temp_profile_pietaeta[idx]*ueta));
            temp_profile_pitautau[idx] = (1./utau
                *(  temp_profile_pitaux[idx]*ux
                  + temp_profile_pitauy[idx]*uy
                  + temp_profile_pitaueta[idx]*ueta));
            if (ix == 0 && iy == 0) {
                DATA.x_size = -dummy2*2;
                DATA.y_size = -dummy3*2;
                if (omp_get_thread_num() == 0) {
                    music_message << "eta_size=" << DATA.eta_size
                                  << ", x_size=" << DATA.x_size
                                  << ", y_size=" << DATA.y_size;
                    music_message.flush("info");
                }
            }
        }
    }
    profile.close();

    double eta = (DATA.delta_eta)*(ieta) - (DATA.eta_size)/2.0;
    double eta_envelop_ed = eta_profile_normalisation(eta);
    int entropy_flag = DATA.initializeEntropy;
    for (int ix = 0; ix < nx; ix++) {
        for (int iy = 0; iy< ny; iy++) {
            int idx = iy + ix*ny;
            double rhob = 0.0;
            double epsilon = 0.0;
            if (entropy_flag == 0) {
                epsilon = (temp_profile_ed[idx]*eta_envelop_ed
                           *DATA.sFactor/hbarc);  // 1/fm^4
            } else {
                double local_sd = (temp_profile_ed[idx]*DATA.sFactor
                                   *eta_envelop_ed);
                epsilon = eos.get_s2e(local_sd, rhob);
            }
            if (epsilon < 0.00000000001)
                epsilon = 0.00000000001;

            arena_current(ix, iy, ieta).epsilon = epsilon;
            arena_current(ix, iy, ieta).rhob = rhob;

            if (DATA.Initial_profile == 9 || DATA.Initial_profile == 91) {
                arena_current(ix, iy, ieta).u[0] = temp_profile_utau[idx];
                arena_current(ix, iy, ieta).u[1] = temp_profile_ux[idx];
                arena_current(ix, iy, ieta).u[2] = temp_profile_uy[idx];
                arena_current(ix, iy, ieta).u[3] = temp_profile_ueta[idx];
            } else {
                arena_current(ix, iy, ieta).u[0] = 1.0;
                arena_current(ix, iy, ieta).u[1] = 0.0;
                arena_current(ix, iy, ieta).u[2] = 0.0;
                arena_current(ix, iy, ieta).u[3] = 0.0;
            }

            if (DATA.Initial_profile == 9) {
                double pressure = eos.get_pressure(epsilon, rhob);
                arena_current(ix, iy, ieta).pi_b = epsilon/3. - pressure;

                arena_current(ix, iy, ieta).Wmunu[0] = temp_profile_pitautau[idx];
                arena_current(ix, iy, ieta).Wmunu[1] = temp_profile_pitaux[idx];
                arena_current(ix, iy, ieta).Wmunu[2] = temp_profile_pitauy[idx];
                arena_current(ix, iy, ieta).Wmunu[3] = temp_profile_pitaueta[idx];
                arena_current(ix, iy, ieta).Wmunu[4] = temp_profile_pixx[idx];
                arena_current(ix, iy, ieta).Wmunu[5] = temp_profile_pixy[idx];
                arena_current(ix, iy, ieta).Wmunu[6] = temp_profile_pixeta[idx];
                arena_current(ix, iy, ieta).Wmunu[7] = temp_profile_piyy[idx];
                arena_current(ix, iy, ieta).Wmunu[8] = temp_profile_piyeta[idx];
                arena_current(ix, iy, ieta).Wmunu[9] = temp_profile_pietaeta[idx];
            }

            arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);
        }
    }
}

void Init::initial_MCGlb_with_rhob_XY(int ieta, SCGrid &arena_prev,
                                      SCGrid &arena_current) {
    // first load in the transverse profile
    ifstream profile_TA(DATA.initName_TA.c_str());
    ifstream profile_TB(DATA.initName_TB.c_str());
    ifstream profile_rhob_TA(DATA.initName_rhob_TA.c_str());
    ifstream profile_rhob_TB(DATA.initName_rhob_TB.c_str());

    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    double temp_profile_TA[nx][ny];
    double temp_profile_TB[nx][ny];
    double temp_profile_rhob_TA[nx][ny];
    double temp_profile_rhob_TB[nx][ny];
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            profile_TA >> temp_profile_TA[i][j];
            profile_TB >> temp_profile_TB[i][j];
            profile_rhob_TA >> temp_profile_rhob_TA[i][j];
            profile_rhob_TB >> temp_profile_rhob_TB[i][j];
        }
    }
    profile_TA.close();
    profile_TB.close();
    profile_rhob_TA.close();
    profile_rhob_TB.close();

    double eta = (DATA.delta_eta)*ieta - (DATA.eta_size)/2.0;
    double eta_envelop_left  = eta_profile_left_factor(eta);
    double eta_envelop_right = eta_profile_right_factor(eta);
    double eta_rhob_left     = eta_rhob_left_factor(eta);
    double eta_rhob_right    = eta_rhob_right_factor(eta);

    int entropy_flag = DATA.initializeEntropy;
    for (int ix = 0; ix < nx; ix++) {
        for (int iy = 0; iy< ny; iy++) {
            double rhob = 0.0;
            double epsilon = 0.0;
            if (DATA.turn_on_rhob == 1) {
                rhob = (
                    (temp_profile_rhob_TA[ix][iy]*eta_rhob_left
                     + temp_profile_rhob_TB[ix][iy]*eta_rhob_right));
            } else {
                rhob = 0.0;
            }
            if (entropy_flag == 0) {
                epsilon = (
                    (temp_profile_TA[ix][iy]*eta_envelop_left
                     + temp_profile_TB[ix][iy]*eta_envelop_right)
                    *DATA.sFactor/hbarc);   // 1/fm^4
            } else {
                double local_sd = (
                    (temp_profile_TA[ix][iy]*eta_envelop_left
                     + temp_profile_TB[ix][iy]*eta_envelop_right)
                    *DATA.sFactor);         // 1/fm^3
                epsilon = eos.get_s2e(local_sd, rhob);
            }
            if (epsilon < 0.00000000001)
                epsilon = 0.00000000001;

            arena_current(ix, iy, ieta).epsilon = epsilon;
            arena_current(ix, iy, ieta).rhob = rhob;

            arena_current(ix, iy, ieta).u[0] = 1.0;
            arena_current(ix, iy, ieta).u[1] = 0.0;
            arena_current(ix, iy, ieta).u[2] = 0.0;
            arena_current(ix, iy, ieta).u[3] = 0.0;

            arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);
        }
    }
}

void Init::initial_trento_XY(int ieta, SCGrid &arena_prev, SCGrid &arena_current) {
    // initial condition is a 2D profile generated by Trento
    const size_t nx = arena_current.nX();
    const size_t ny = arena_current.nY();

    ifstream surfaceFile(DATA.initName.c_str());

    size_t iy = 0;
    string surfaceLine;

    while (getline(surfaceFile, surfaceLine)) {
        if (surfaceLine[0] != '#') { // ignore TRENTo header lines
            istringstream lineStream(surfaceLine);
            vector<double> lineVector((std::istream_iterator<double>(lineStream)), std::istream_iterator<double>());

            // checks if number of columns in initial condition file matches grid nx size in MUSIC
            if (lineVector.size() != nx) {
                music_message.error("nx size on initial condition file does not match MUSIC nx !");
                exit(1);
            }

            for (size_t ix = 0; ix < nx; ix++) {
                double s = lineVector[ix];
                s /= hbarc;//

                double epsilon = eos.get_s2e(s, 0.0);
                epsilon = max(epsilon, 1e-11);

                double rhob = 0.;

                // set all values in the grid element:
                // cout << "epsilon = " << (*arena)[ix][iy][0].epsilon << endl;
                arena_current(ix, iy, ieta).epsilon = epsilon;
                arena_current(ix, iy, ieta).rhob = rhob;

                /* for HIC */
                arena_current(ix, iy, ieta).u[0] = 1.0;
                arena_current(ix, iy, ieta).u[1] = 0.0;
                arena_current(ix, iy, ieta).u[2] = 0.0;
                arena_current(ix, iy, ieta).u[3] = 0.0;

                arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);

            } /* x */
            iy++;
        }
    } // end of initial condition file
    if (iy != ny) {
        music_message.error("ny size on initial condition file does not match MUSIC grid ny size. Aborting!");
        exit(1);
    }
}

void Init::initial_distorted_Gaussian(SCGrid &arena_prev,
                                           SCGrid &arena_current) {
	// initial condition is a 2D Gaussian in the transverse plane, deformed to obtain an arbitrary anisotropy
    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    double u[4] = {1.0, 0.0, 0.0, 0.0};
    for (int ix = 0; ix < nx; ix++) {
	double x = DATA.delta_x*(ix*2.0 - nx)/2.0;
        for (int iy = 0; iy < ny; iy++) {
	    double y = DATA.delta_y*(iy*2.0 - ny)/2.0;
            for (int ieta = 0; ieta < arena_current.nEta(); ieta++) {
//    		double eta = (DATA.delta_eta)*ieta - (DATA.eta_size)/2.0;

		double phi = atan2(y,x);
		double Rgauss = 3.0; //in fm
		int nharmonics = 7; //number of harmonics to include in deformation
		double ecc[nharmonics] = {0,0.6,0,0,0,0,0};
		double psi[nharmonics] = {0,0,0,0,0,0,0};
		double r2 = x*x+y*y;
		double stretch = 1.0;
		for(int n = 1; n <= nharmonics; n++) {
			stretch += ecc[n-1]*cos(n*phi - n*psi[n-1]);
		}
		double epsilon = 50*exp(-r2*stretch/(2*Rgauss*Rgauss));
		epsilon = max(epsilon, 1e-11);
            	arena_current(ix, iy, ieta).epsilon = epsilon;
//            	arena_current(ix, iy, ieta).rhob = 0.0;
//            	Can also include an initial transverse flow.  Choose the flow vector to always be in the radial direction, but with a magnitude obtained from a derivative of the deformed Gaussian above.

		double eccU[nharmonics] = {0,0.0,0,0,0,0,0};
		double psiU[nharmonics] = {0,0,0,0,0,0,0};
		double stretchU = 1.0;
		for(int n = 0; n < nharmonics; n++) {
			stretchU += eccU[n]*cos(n*phi - n*psiU[n]);
		}
            	u[3] = 0.0;  // boost invariant flow profile
//		u[1] = 0.2*0.2*stretchU*2*x/(2*Rgauss*Rgauss)*DATA.tau0;
//		u[2] = 0.2*0.2*stretchU*2*y/(2*Rgauss*Rgauss)*DATA.tau0;
		//u[1] = 0.04*0.03*4/3/epsilon*exp(-(x*x+y*y)*stretchU/(2*Rgauss*Rgauss))*cos(phi);
		//u[2] = 0.04*0.03*4/3/epsilon*exp(-(x*x+y*y)*stretchU/(2*Rgauss*Rgauss))*sin(phi);
//		u[0] = sqrt(1.0-u[1]*u[1]-u[2]*u[2]);
		u[0] = 1.0;
		u[1] = 0.0;
		u[2] = 0.0;
            	arena_current(ix, iy, ieta).u[0] = u[0];
            	arena_current(ix, iy, ieta).u[1] = u[1];
            	arena_current(ix, iy, ieta).u[2] = u[2];
            	arena_current(ix, iy, ieta).u[3] = u[3];

            	arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);
	    }
        }
    }
}

void Init::initial_MCGlbLEXUS_with_rhob_XY(int ieta, SCGrid &arena_prev,
                                           SCGrid &arena_current) {
    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    double u[4] = {1.0, 0.0, 0.0, 0.0};
    for (int ix = 0; ix < nx; ix++) {
        for (int iy = 0; iy < ny; iy++) {
            double rhob = 0.0;
            double epsilon = 1e-12;

            arena_current(ix, iy, ieta).epsilon = epsilon;
            arena_current(ix, iy, ieta).rhob = rhob;

            arena_current(ix, iy, ieta).u[0] = u[0];
            arena_current(ix, iy, ieta).u[1] = u[1];
            arena_current(ix, iy, ieta).u[2] = u[2];
            arena_current(ix, iy, ieta).u[3] = u[3];

            arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);
        }
    }
}

void Init::initial_UMN_with_rhob(SCGrid &arena_prev, SCGrid &arena_current) {
    // first load in the transverse profile
    ifstream profile(DATA.initName.c_str());
    ifstream profile_TA(DATA.initName_TA.c_str());
    ifstream profile_TB(DATA.initName_TB.c_str());

    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    double temp_profile_TA[nx][ny];
    double temp_profile_TB[nx][ny];
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            profile_TA >> temp_profile_TA[i][j];
            profile_TB >> temp_profile_TB[i][j];
        }
    }
    profile_TA.close();
    profile_TB.close();

    double dummy;
    double ed_local, rhob_local;
    for (int ieta = 0; ieta < DATA.neta; ieta++) {
        double eta = (DATA.delta_eta)*ieta - (DATA.eta_size)/2.0;
        double eta_envelop_left = eta_profile_left_factor(eta);
        double eta_envelop_right = eta_profile_right_factor(eta);
        for (int ix = 0; ix < nx; ix++) {
            for (int iy = 0; iy< ny; iy++) {
                double rhob = 0.0;
                double epsilon = 0.0;
                profile >> dummy >> dummy >> dummy >> ed_local >> rhob_local;
                rhob = rhob_local;
                double sd_bg = (
                      temp_profile_TA[ix][iy]*eta_envelop_left
                    + temp_profile_TB[ix][iy]*eta_envelop_right)*DATA.sFactor;
                double ed_bg = eos.get_s2e(sd_bg, 0.0);
                epsilon = ed_bg + ed_local/hbarc;    // 1/fm^4
                if (epsilon < 0.00000000001) {
                    epsilon = 0.00000000001;
                }

                arena_current(ix, iy, ieta).epsilon = epsilon;
                arena_current(ix, iy, ieta).rhob = rhob;

                arena_current(ix, iy, ieta).u[0] = 1.0;
                arena_current(ix, iy, ieta).u[1] = 0.0;
                arena_current(ix, iy, ieta).u[2] = 0.0;
                arena_current(ix, iy, ieta).u[3] = 0.0;

                arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);
            }
        }
    }
    profile.close();
}

void Init::initial_AMPT_XY(int ieta, SCGrid &arena_prev,
                           SCGrid &arena_current) {
    double u[4] = {1.0, 0.0, 0.0, 0.0};
    double j_mu[4] = {0.0, 0.0, 0.0, 0.0};

    double eta = (DATA.delta_eta)*ieta - (DATA.eta_size)/2.0;
    double tau0 = DATA.tau0;
    const int nx = arena_current.nX();
    const int ny = arena_current.nY();
    for (int ix = 0; ix < nx; ix++) {
        double x_local = - DATA.x_size/2. + ix*DATA.delta_x;
        for (int iy = 0; iy < ny; iy++) {
            double y_local = - DATA.y_size/2. + iy*DATA.delta_y;
            double rhob = 0.0;
            double epsilon = 0.0;
            if (DATA.turn_on_rhob == 1) {
                rhob = hydro_source_terms.get_hydro_rhob_source_before_tau(
                                                tau0, x_local, y_local, eta);
            } else {
                rhob = 0.0;
            }

            hydro_source_terms.get_hydro_energy_source_before_tau(
                                    tau0, x_local, y_local, eta, j_mu);

            epsilon = j_mu[0];           // 1/fm^4

            if (epsilon < 0.00000000001)
                epsilon = 0.00000000001;

            arena_current(ix, iy, ieta).epsilon = epsilon;
            arena_current(ix, iy, ieta).rhob = rhob;

            arena_current(ix, iy, ieta).u[0] = u[0];
            arena_current(ix, iy, ieta).u[1] = u[1];
            arena_current(ix, iy, ieta).u[2] = u[2];
            arena_current(ix, iy, ieta).u[3] = u[3];

            arena_prev(ix, iy, ieta) = arena_current(ix, iy, ieta);
        }
    }
}

double Init::eta_profile_normalisation(double eta) {
    // this function return the eta envelope profile for energy density
    double res;
    // Hirano's plateau + Gaussian fall-off
    if (DATA.initial_eta_profile == 1) {
        double exparg1 = (fabs(eta) - DATA.eta_flat/2.0)/DATA.eta_fall_off;
        double exparg = exparg1*exparg1/2.0;
        res = exp(-exparg*theta(exparg1));
    } else if (DATA.initial_eta_profile == 2) {
        // Woods-Saxon
        // The radius is set to be half of DATA.eta_flat
        // The diffusiveness is set to DATA.eta_fall_off
        double ws_R = DATA.eta_flat/2.0;
        double ws_a = DATA.eta_fall_off;
        res = (1.0 + exp(-ws_R/ws_a))/(1.0 + exp((abs(eta) - ws_R)/ws_a));
    } else {
        music_message.error("initial_eta_profile out of range.");
        exit(1);
    }
    return res;
}

double Init::eta_profile_left_factor(double eta) {
    // this function return the eta envelope for projectile
    double res = eta_profile_normalisation(eta);
    if (fabs(eta) < DATA.beam_rapidity) {
        res = (1. - eta/DATA.beam_rapidity)*res;
    } else {
        res = 0.0;
    }
    return(res);
}

double Init::eta_profile_right_factor(double eta) {
    // this function return the eta envelope for target
    double res = eta_profile_normalisation(eta);
    if (fabs(eta) < DATA.beam_rapidity) {
        res = (1. + eta/DATA.beam_rapidity)*res;
    } else {
        res = 0.0;
    }
    return(res);
}

double Init::eta_rhob_profile_normalisation(double eta) {
    // this function return the eta envelope profile for net baryon density
    double res;
    int profile_flag = DATA.initial_eta_rhob_profile;
    double eta_0 = DATA.eta_rhob_0;
    double tau0 = DATA.tau0;
    if (profile_flag == 1) {
        const double eta_width = DATA.eta_rhob_width;
        const double norm      = 1./(2.*sqrt(2*M_PI)*eta_width*tau0);
        const double exparg1   = (eta - eta_0)/eta_width;
        const double exparg2   = (eta + eta_0)/eta_width;
        res = norm*(exp(-exparg1*exparg1/2.0) + exp(-exparg2*exparg2/2.0));
    } else if (profile_flag == 2) {
        double eta_abs     = fabs(eta);
        double delta_eta_1 = DATA.eta_rhob_width_1;
        double delta_eta_2 = DATA.eta_rhob_width_2;
        double A           = DATA.eta_rhob_plateau_height;
        double exparg1     = (eta_abs - eta_0)/delta_eta_1;
        double exparg2     = (eta_abs - eta_0)/delta_eta_2;
        double theta;
        double norm = 1./(tau0*(sqrt(2.*M_PI)*delta_eta_1
                          + (1. - A)*sqrt(2.*M_PI)*delta_eta_2 + 2.*A*eta_0));
        if (eta_abs > eta_0)
            theta = 1.0;
        else
            theta = 0.0;
        res = norm*(theta*exp(-exparg1*exparg1/2.)
                    + (1. - theta)*(A + (1. - A)*exp(-exparg2*exparg2/2.)));
    } else {
        music_message << "initial_eta_rhob_profile = " << profile_flag
                      << " out of range.";
        music_message.flush("error");
        exit(1);
    }
    return res;
}

double Init::eta_rhob_left_factor(double eta) {
    double eta_0       = -fabs(DATA.eta_rhob_0);
    double tau0        = DATA.tau0;
    double delta_eta_1 = DATA.eta_rhob_width_1;
    double delta_eta_2 = DATA.eta_rhob_width_2;
    double norm        = 2./(sqrt(M_PI)*tau0*(delta_eta_1 + delta_eta_2));
    double exp_arg     = 0.0;
    if (eta < eta_0) {
        exp_arg = (eta - eta_0)/delta_eta_1;
    } else {
        exp_arg = (eta - eta_0)/delta_eta_2;
    }
    double res = norm*exp(-exp_arg*exp_arg);
    return(res);
}

double Init::eta_rhob_right_factor(double eta) {
    double eta_0       = fabs(DATA.eta_rhob_0);
    double tau0        = DATA.tau0;
    double delta_eta_1 = DATA.eta_rhob_width_1;
    double delta_eta_2 = DATA.eta_rhob_width_2;
    double norm        = 2./(sqrt(M_PI)*tau0*(delta_eta_1 + delta_eta_2));
    double exp_arg     = 0.0;
    if (eta < eta_0) {
        exp_arg = (eta - eta_0)/delta_eta_2;
    } else {
        exp_arg = (eta - eta_0)/delta_eta_1;
    }
    double res = norm*exp(-exp_arg*exp_arg);
    return(res);
}

void Init::output_initial_density_profiles(SCGrid &arena) {
    // this function outputs the 3d initial energy density profile
    // and net baryon density profile (if turn_on_rhob == 1)
    // for checking purpose
    music_message.info("output initial density profiles into a file... ");
    ofstream of("check_initial_density_profiles.dat");
    of << "# x(fm)  y(fm)  eta  ed(GeV/fm^3)";
    if (DATA.turn_on_rhob == 1)
        of << "  rhob(1/fm^3)";
    of << endl;
    for (int ieta = 0; ieta < arena.nEta(); ieta++) {
        double eta_local = (DATA.delta_eta)*ieta - (DATA.eta_size)/2.0;
        for(int ix = 0; ix < arena.nX(); ix++) {
            double x_local = -DATA.x_size/2. + ix*DATA.delta_x;
            for(int iy = 0; iy < arena.nY(); iy++) {
                double y_local = -DATA.y_size/2. + iy*DATA.delta_y;
                of << scientific << setw(18) << std::setprecision(8)
                   << x_local << "   " << y_local << "   "
                   << eta_local << "   " << arena(ix,iy,ieta).epsilon*hbarc;
                if (DATA.turn_on_rhob == 1) {
                    of << "   " << arena(ix,iy,ieta).rhob;
                }
                of << endl;
            }
        }
    }
    music_message.info("done!");
}

void Init::output_2D_eccentricities(int ieta, SCGrid &arena) {
    // this function outputs a set of eccentricities (cumulants) to a file
    music_message.info("output initial eccentricities into a file... ");
    ofstream of("ecc.dat");
    of << "#No recentering correction has been made! Must use full expression for cumulants!\n";
    of << "#i\tj\t<z^i zbar^j>_eps\t<z^i zbar^j>_U\t<z^i zbar^j>_Ubar\t<z^i zbar^j>_s\n";
    int zmax = 12;
    complex<double> eps[zmax][zmax] = {{0}}; // moment <z^j z*^k> =  <r^(j+k) e^{i(j-k) phi}>
    complex<double> epsU[zmax][zmax] = {{0}}; // same but using momentum density as weight U = T^0x + i T^0y
    complex<double> epsUbar[zmax][zmax] = {{0}}; //
    complex<double> epsS[zmax][zmax] = {{0}}; // using entropy density as weight
//    if (DATA.nx != arena.nX()) cout << "DATA.nx = " << DATA.nx << ", arena.nX = " << arena.nX() << endl;
	for(int ix = 0; ix < arena.nX(); ix++) {
	    double x = DATA.delta_x*(ix*2.0 - DATA.nx)/2.0;
//	    double x = -DATA.x_size/2. + ix*DATA.delta_x;
	    for(int iy = 0; iy < arena.nY(); iy++) {
		double y = DATA.delta_y*(iy*2.0 - DATA.ny)/2.0;
//		double y = -DATA.y_size/2. + iy*DATA.delta_y;
		std::complex<double> z (x,y);
		std::complex<double> zbar = conj(z);
		double e = arena(ix,iy,ieta).epsilon;
		double u[4];
		for (int i = 0; i<4; i++)
		    u[i] = arena(ix,iy,ieta).u[i];
		double rhob = arena(ix,iy,ieta).rhob;
		double p = eos.get_pressure(e,rhob);
		double pi00 = arena(ix, iy, ieta).Wmunu[0];
		double T00 = (e+p)*u[0]*u[0] - p + pi00;// T^{tau tau}
		double pi0x = arena(ix,iy,ieta).Wmunu[1];
		double pi0y = arena(ix,iy,ieta).Wmunu[2];
		double T0x = (e+p)*u[0]*u[1] + pi0x;// T^{tau x}
		double T0y = (e+p)*u[0]*u[2] + pi0y;
//		std::complex<double> U (arena(ix,iy,ieta).u[1],arena(ix,iy,ieta).u[2]);
		std::complex<double> U (T0x,T0y);
		double s = eos.get_entropy(e, rhob);
		for(int j=0; j < zmax; j++) {
		    for(int k=0; k < zmax; k++) {
			complex<double> powz, powzbar;
			if(abs(z) == 0.0) // pow() doesn't work nicely with a vanishing complex number 
			{
			   powz = pow(0,j);
			   powzbar = pow(0,k);
			}
			else 
			{
			   powz = pow(z,j);
			   powzbar = pow(zbar,k);
			}
			eps[j][k] += T00*powz*powzbar;
			epsU[j][k] += U*powz*powzbar;
			epsUbar[j][k] += conj(U)*powz*powzbar;
			epsS[j][k] += s*powz*powzbar;
		    }
		}
	    }
	}
	// normalize by total energy to obtain <z^j z*^k> and output to file
	for(int j=0; j < zmax; j++) {
	    for(int k=0; k < zmax; k++) {
		if(!(j==0 && k==0)) {
		    eps[j][k] /= eps[0][0];
		    epsS[j][k] /= epsS[0][0];
		}
		epsU[j][k] /= eps[0][0];
		epsUbar[j][k] /= eps[0][0];
		of << j << "\t" << k << "\t" << eps[j][k] << "\t" 
		    << epsU[j][k] << "\t" << epsUbar[j][k] << "\t" << epsS[j][k] << endl;
	    }
	}
	of.close();
	// Define the cumulants by hand
	complex<double> W11 = eps[1][0];
	complex<double> W02 = eps[1][1] - eps[1][0]*eps[0][1];
	complex<double> W22 = eps[2][0] - W11*W11;
	complex<double> W13 = eps[2][1] - eps[2][0]*eps[0][1]
	    - 2.0*eps[1][1]*eps[1][0] + 2.0*eps[1][0]*eps[1][0]*eps[0][1];
	complex<double> W33 = eps[3][0] + eps[1][0]*(3.0*eps[2][0] - 2.0*eps[1][0]*eps[1][0]);
	complex<double> SW11 = epsS[1][0];
	complex<double> SW02 = epsS[1][1] - epsS[1][0]*eps[0][1];
	complex<double> SW22 = epsS[2][0] - SW11*SW11;
	complex<double> SW13 = epsS[2][1] - epsS[2][0]*epsS[0][1]
	    - 2.0*epsS[1][1]*epsS[1][0] + 2.0*epsS[1][0]*epsS[1][0]*epsS[0][1];
	complex<double> SW33 = epsS[3][0] + epsS[1][0]*(3.0*epsS[2][0] - 2.0*epsS[1][0]*epsS[1][0]);
	cout << "W11 = " << W11 << endl;
	cout << "eps2 = " << -W22/W02 << endl;
	cout << "eps3 = " << -W33/pow(W02,1.5) << endl;
	cout << "eps1 = " << -W13/pow(W02,1.5) << endl;
	cout << "Using entropy as weight instead of energy density:\n";
	cout << "eps2S = " << -SW22/SW02 << endl;
	cout << "eps3S = " << -SW33/pow(SW02,1.5) << endl;
	cout << "eps1S = " << -SW13/pow(SW02,1.5) << endl;
// calculate <r^3> and <r^5> in centered coordinates
// for comparison to common convention for normalization of eccentricities
	double den2 = 0.0; // <r^2>, should be equal to W02
	double den3 = 0.0; // <r^3>, not an analytic function of cumulants
	double den4 = 0.0; // <r^4> = W04 + 2*W02^2 + |W22|^2
	double den5 = 0.0; // <r^5>
	for(int ix = 0; ix < arena.nX(); ix++) {
	    double x = DATA.delta_x*(ix*2.0 - DATA.nx)/2.0;
	    // recenter coordinates
	    x += -W11.real();
//	    double x = -DATA.x_size/2. + ix*DATA.delta_x;
	    for(int iy = 0; iy < arena.nY(); iy++) {
		double y = DATA.delta_y*(iy*2.0 - DATA.ny)/2.0;
	    // recenter coordinates
		y += -W11.imag();
		double r2 = x*x+y*y;
		double r3 = pow(r2,1.5);
		double r4 = r2*r2;
		double r5 = r3*r2;
		den2 += r2;
		den3 += r3;
		den4 += r4;
		den5 += r5;
	    }
	}
	cout << "<r^2> = " << den2 << endl;
	cout << "<r^3> = " << den3 << endl;
	cout << "<r^4> = " << den4 << endl;
	cout << "<r^5> = " << den5 << endl;
	cout << "Alternate eps3 = " << -W33/den3 << endl;
//	cout << "Alternate eps5 = " << -W55/den5 << endl;
    }
