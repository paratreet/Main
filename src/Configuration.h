#ifndef PARATREET_CONFIGURATION_H_
#define PARATREET_CONFIGURATION_H_

#include <functional>
#include <string>

#include "BoundingBox.h"

template<typename T>
class CProxy_Subtree;

namespace paratreet {
    struct Configuration {
        double decomp_tolerance;
        int max_particles_per_tp; // For OCT decomposition
        int max_particles_per_leaf; // For local tree build
        int decomp_type;
        int tree_type;
        int num_iterations;
        int num_share_nodes;
        int cache_share_depth;
        int flush_period;
        int flush_max_avg_ratio;
        Real timestep_size;
        std::string input_file;
        std::string output_file;
#ifdef __CHARMC__
#include "pup.h"
        void pup(PUP::er &p) {
            p | decomp_tolerance;
            p | max_particles_per_tp;
            p | max_particles_per_leaf;
            p | decomp_type;
            p | tree_type;
            p | num_iterations;
            p | num_share_nodes;
            p | cache_share_depth;
            p | flush_period;
            p | flush_max_avg_ratio;
            p | input_file;
            p | output_file;
            p | timestep_size;
        }
#endif //__CHARMC__
    };
}

#include "paratreet.decl.h"

#endif //PARATREET_CONFIGURATION_H_
