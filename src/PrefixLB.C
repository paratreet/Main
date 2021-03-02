/**
 * \addtogroup CkLdb
*/
/*@{*/

#include "elements.h"
#include "ckheap.h"
#include "PrefixLB.h"
#include "LBCommon.h"

extern int quietModeRequested;
CkpvExtern(int, _lb_obj_index);

CreateLBFunc_Def(PrefixLB, "Move jobs base on prefix sum.")

PrefixLB::PrefixLB(const CkLBOptions &opt): CBase_PrefixLB(opt)
{
  lbname = (char *)"PrefixLB";
  if (CkMyPe() == 0 && !quietModeRequested)
    CkPrintf("CharmLB> PrefixLB created.\n");
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) == -1)
    CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(LBUserData));
  #endif
}

//typedef struct {
//  int index;
//  LDObjData data;
//  int from_proc;
//  int to_proc;
//  inline void pup(PUP::er &p);
//} LDObjStats;
bool compareLDObjStats(const LDObjStats & a, const LDObjStats & b)
{
  // first compare PE
  int a_pe = a.from_proc, b_pe = b.from_proc;
  if (a_pe != b_pe) return a_pe < b_pe;

  // then compare chare array index
  // to_proc is assigned with chare_idx from user data
  return a.to_proc < b.to_proc;
}

void PrefixLB::work(LDStats* stats)
{
  int obj;
  int n_pes = stats->nprocs();

  // calculate total wallTime of all objects
  double total_load = 0.0, total_nonmig_load = 0.0;
  int migratable_obj_ct = 0, nonmig_obj_ct = 0;
  std::vector<LDObjStats> objMap;

  double total_patition_load = 0.0;
  int total_partical = 0;
  std::vector<int> size_map (stats->objData.size(), 0);

  for (int obj_idx = 0; obj_idx < stats->objData.size(); obj_idx++){
    LDObjData &oData = stats->objData[obj_idx];
    int pe = stats->from_proc[obj_idx];

    #if CMK_LB_USER_DATA
    int idx = CkpvAccess(_lb_obj_index);
    LBUserData usr_data = *(LBUserData *)oData.getUserData(CkpvAccess(_lb_obj_index));
    if (usr_data.lb_type == pt){
      // Assign chare_idx at the position of to_proc
      objMap.push_back(LDObjStats{obj_idx, oData, pe, usr_data.chare_idx});
      size_map[obj_idx] = usr_data.particle_size;
      total_patition_load += oData.wallTime * stats->procs[pe].pe_speed;
      total_partical += usr_data.particle_size;
    }else{
      // For non Parition objects, keep the PE
      stats->assign(obj_idx, pe);
    }
    #endif

    //if(!oData.migratable){
    //  nonmig_obj_ct ++;
    //  total_nonmig_load += oData.wallTime * stats->procs[pe].pe_speed;
    //}else{
    //  migratable_obj_ct ++;
    //  total_load += oData.wallTime * stats->procs[pe].pe_speed;
    //}
  }

  #if CMK_LB_USER_DATA
  std::sort(objMap.begin(), objMap.end(), compareLDObjStats);

  double avg_load = total_patition_load / n_pes;
  int avg_particals = total_partical / n_pes;
  double prefixed_load = 0.0;
  int prefix_particle_size = 0;
  int migrate_ct = 0;
  for (LDObjStats & oStat : objMap){
    int pe = oStat.from_proc;
    prefixed_load += oStat.data.wallTime * stats -> procs[pe].pe_speed;
    prefix_particle_size += size_map[oStat.index];
    int to_pei_load = std::floor(prefixed_load / avg_load);
    int to_pe_size = std::floor(prefix_particle_size / avg_particals);
    int to_pe = std::min(to_pe_size, n_pes-1);
    stats->assign(oStat.index, to_pe);
    if (pe != to_pe){
      migrate_ct ++;
      //CkPrintf("Move obj[%d] from PE[%d] to PE[%d]\n", oStat.index, pe, to_pe);
    }
  }

  //CkPrintf("[%d] PrefixLB strategy moved %d objs\n n_pes = %d; n_objs = %d; n_migrateobjs = %d\n total migratable ct= %d load =  %f; total nonmig ct = %d load = %f\n",CkMyPe(),  migrate_ct, n_pes, stats->objData.size(), stats->n_migrateobjs, migratable_obj_ct, total_load, nonmig_obj_ct, total_nonmig_load);
  CkPrintf("CharmLB> PrefixLB: PE [%d] moved %d objects.\n", CkMyPe(), migrate_ct);
  //CkPrintf("Partition load =  %f, else = %f\n", total_patition_load, total_load - total_patition_load);
  #endif
}

#include "PrefixLB.def.h"

/*@}*/
