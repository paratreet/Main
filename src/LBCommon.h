#ifndef _LBCOMMON_H_
#define _LBCOMMON_H_
enum LBType {
  pt = 1111, // Partition
  cp = 2222 // TreeCanopy
};

struct LBUserData{
  int lb_type;
  int chare_idx;
  int particle_size;

  void pup(PUP::er &p){
    p | lb_type;
    p | chare_idx;
    p | particle_size;
  }
};

#endif //_LBCOMMON_H_
