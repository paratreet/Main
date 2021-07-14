#include "Main.h"

#include "GravityVisitor.h"
#include "DensityVisitor.h"
#include "DensityVisitorDen.h"
//#include "PressureVisitor.h"
#include "CountVisitor.h"
#include "CollisionVisitor.h"

PARATREET_REGISTER_MAIN(ExMain);

/* readonly */ bool verify;
/* readonly */ bool dual_tree;
/* readonly */ bool periodic;
/* readonly */ int peanoKey;
/* readonly */ int iter_start_collision;
/* readonly */ CProxy_CountManager count_manager;
/* readonly */ CProxy_NeighborListCollector neighbor_list_collector;
/* readonly */ CProxy_CollisionTracker collision_tracker;

  static void initialize() {
    BoundingBox::registerReducer();
  }

  void ExMain::main(CkArgMsg* m) {
    // mainProxy = thisProxy;
    int cur_iteration;
    double total_start_time;
    double start_time;

    // Initialize readonly variables
    conf.input_file = "";
    conf.output_file = "";
    conf.min_n_subtrees = CkNumPes() * 8; // default from ChaNGa
    conf.min_n_partitions = CkNumPes() * 8;
    conf.max_particles_per_leaf = 12; // default from ChaNGa
    conf.decomp_type = paratreet::DecompType::eBinaryOct;
    conf.tree_type = paratreet::TreeType::eBinaryOct;
    conf.num_iterations = 3;
    conf.num_share_nodes = 0; // 3;
    conf.cache_share_depth= 3;
    conf.flush_period = 0;
    conf.flush_max_avg_ratio = 10.;
    conf.lb_period = 5;

    verify = false;
    dual_tree = false;
    periodic = false;
    peanoKey = 3;
    iter_start_collision = 0;

    // Initialize member variables
    cur_iteration = 0;

    // Process command line arguments
    int c;
    std::string input_str;
    while ((c = getopt(m->argc, m->argv, "f:n:p:l:d:t:i:s:u:r:b:v:amec:")) != -1) {
      switch (c) {
        case 'f':
          conf.input_file = optarg;
          break;
        case 'n':
          conf.min_n_subtrees = atoi(optarg);
          break;
        case 'p':
          conf.min_n_partitions = atoi(optarg);
          break;
        case 'l':
          conf.max_particles_per_leaf = atoi(optarg);
          break;
        case 'd':
          input_str = optarg;
          if (input_str.compare("oct") == 0) {
            conf.decomp_type = paratreet::DecompType::eOct;
          }
          else if (input_str.compare("binoct") == 0) {
            conf.decomp_type = paratreet::DecompType::eBinaryOct;
          }
          else if (input_str.compare("sfc") == 0) {
            conf.decomp_type = paratreet::DecompType::eSfc;
          }
          else if (input_str.compare("kd") == 0) {
            conf.decomp_type = paratreet::DecompType::eKd;
          }
          else if (input_str.compare("longest") == 0) {
            conf.decomp_type = paratreet::DecompType::eLongest;
          }
          break;
        case 't':
          input_str = optarg;
          if (input_str.compare("oct") == 0) {
            conf.tree_type = paratreet::TreeType::eOct;
          }
          else if (input_str.compare("binoct") == 0) {
            conf.tree_type = paratreet::TreeType::eBinaryOct;
          }
          else if (input_str.compare("kd") == 0) {
            conf.tree_type = paratreet::TreeType::eKd;
          }
          else if (input_str.compare("longest") == 0) {
            conf.tree_type = paratreet::TreeType::eLongest;
          }
          break;
        case 'i':
          conf.num_iterations = atoi(optarg);
          break;
        case 's':
          conf.num_share_nodes = atoi(optarg);
          break;
        case 'u':
          conf.flush_period = atoi(optarg);
          break;
        case 'r':
          conf.flush_max_avg_ratio = atoi(optarg);
          break;
        case 'b':
          conf.lb_period = atoi(optarg);
          break;
        case 'v':
          verify = true;
          conf.output_file = optarg;
          if (conf.output_file.empty()) CkAbort("output file unspecified");
          break;
	case 'm':
	  peanoKey = 0; // morton
	  break;
        case 'e':
          dual_tree = true;
          CkPrintf("You are doing a dual-tree traversal. Make sure you have matching decomps.\n");
          break;
        case 'c':
          iter_start_collision = atoi(optarg);
          break;
        default:
          CkPrintf("Usage: %s\n", m->argv[0]);
          CkPrintf("\t-f [input file]\n");
          CkPrintf("\t-n [number of treepieces]\n");
          CkPrintf("\t-p [maximum number of particles per treepiece]\n");
          CkPrintf("\t-l [maximum number of particles per leaf]\n");
          CkPrintf("\t-d [decomposition type: oct, sfc, kd]\n");
          CkPrintf("\t-t [tree type: oct, bin, kd]\n");
          CkPrintf("\t-i [number of iterations]\n");
          CkPrintf("\t-s [number of shared tree levels]\n");
          CkPrintf("\t-u [flush period]\n");
          CkPrintf("\t-r [flush threshold for Subtree max_average ratio]\n");
          CkPrintf("\t-b [load balancing period]\n");
          CkPrintf("\t-v [filename prefix]\n");
          CkExit();
      }
    }
    delete m;

    // Print configuration
    CkPrintf("\n[PARATREET]\n");
    if (conf.input_file.empty()) CkAbort("Input file unspecified");
    CkPrintf("Input file: %s\n", conf.input_file.c_str());
    CkPrintf("Decomposition type: %s\n", paratreet::asString(conf.decomp_type).c_str());
    CkPrintf("Tree type: %s\n", paratreet::asString(conf.tree_type).c_str());
    CkPrintf("Minimum number of subtrees: %d\n", conf.min_n_subtrees);
    CkPrintf("Minimum number of partitions: %d\n", conf.min_n_partitions);
    CkPrintf("Maximum number of particles per leaf: %d\n\n", conf.max_particles_per_leaf);

    count_manager = CProxy_CountManager::ckNew(0.00001, 10000, 5);
    neighbor_list_collector = CProxy_NeighborListCollector::ckNew();
    collision_tracker = CProxy_CollisionTracker::ckNew();

    // Delegate to Driver
    // CkCallback runCB(CkIndex_Main::run(), thisProxy);
    // driver = paratreet::initialize<CentroidData>(conf, runCB);
  }

  void ExMain::run() {
    driver.run(CkCallbackResumeThread());

    CkExit();
  }

  /* void checkParticlesChangedDone(bool result) {
    if (result) {
      CkPrintf("[Main, iter %d] Particles are the same.\n", cur_iteration);
    }
    else {
      CkPrintf("[Main, iter %d] Particles are changed.\n", cur_iteration);
    }
  } */

// #include "paratreet.def.h"
#include "templates.h"

#include "NeighborListCollectorDen.h"
#include "Main.def.h"

