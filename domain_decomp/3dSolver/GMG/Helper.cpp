#include "Helper.h"
#include "AvgRstr.h"
#include "DrctIntp.h"
#include "FFTBlockJacobiSmoother.h"
#include "MatOp.h"
#include "MatrixHelper.h"
#include "VCycle.h"
#include <fstream>
using namespace std;
using namespace GMG;
using nlohmann::json;
Helper::Helper(int n, OctTree t, std::shared_ptr<DomainCollection> dc,
               std::shared_ptr<SchurHelper> sh, std::string config_file)
{
	ifstream config_stream(config_file);
	json     config_j;
	config_stream >> config_j;
	config_stream.close();
	int num_levels;
    try{
        num_levels = config_j.at("max_levels");
    }catch(nlohmann::detail::out_of_range oor){
        num_levels = 0;
    }
	if (num_levels <= 0 || num_levels > t.num_levels) { num_levels = t.num_levels; }
	// generate and balance domain collections
	vector<shared_ptr<DomainCollection>> dcs(num_levels);
	dcs[0] = dc;
	for (int i = 1; i < num_levels; i++) {
		dcs[i].reset(new DomainCollection(t, t.num_levels - i, n));
		dcs[i]->zoltanBalanceWithLower(*dcs[i - 1]);
	}

	// generate operators
	vector<shared_ptr<Operator>> ops(num_levels);
	for (int i = 0; i < num_levels; i++) {
		MatrixHelper mh(*dcs[i]);
		ops[i].reset(new MatOp(mh.formCRSMatrix()));
	}

	// generate smoothers
	vector<shared_ptr<Smoother>> smoothers(num_levels);
	smoothers[0].reset(new FFTBlockJacobiSmoother(sh));
	for (int i = 1; i < num_levels; i++) {
		shared_ptr<SchurHelper> sh2(
		new SchurHelper(*dcs[i], sh->getSolver(), sh->getOp(), sh->getInterpolator()));
		smoothers[i].reset(new FFTBlockJacobiSmoother(sh2));
	}

	// generate inter-level comms, restrictors, interpolators
	vector<shared_ptr<InterLevelComm>> comms(num_levels - 1);
	vector<shared_ptr<Restrictor>>     restrictors(num_levels - 1);
	vector<shared_ptr<Interpolator>>   interpolators(num_levels - 1);
	for (int i = 0; i < num_levels - 1; i++) {
		comms[i].reset(new InterLevelComm(dcs[i + 1], dcs[i]));
		restrictors[i].reset(new AvgRstr(dcs[i + 1], dcs[i], comms[i]));
		interpolators[i].reset(new DrctIntp(dcs[i + 1], dcs[i], comms[i]));
	}

	// create  level objects
	vector<shared_ptr<Level>> levels(num_levels);
	for (int i = 0; i < num_levels; i++) {
		levels[i].reset(new Level(dcs[i]));
		levels[i]->setOperator(ops[i]);
		levels[i]->setSmoother(smoothers[i]);
	}

	// set restrictors and interpolators
	for (int i = 0; i < num_levels - 1; i++) {
		levels[i]->setRestrictor(restrictors[i]);
		levels[i + 1]->setInterpolator(interpolators[i]);
	}

	// link levels to each other
	levels[0]->setCoarser(levels[1]);
	for (int i = 1; i < num_levels - 1; i++) {
		levels[i]->setCoarser(levels[i + 1]);
		levels[i]->setFiner(levels[i - 1]);
	}
	levels[num_levels - 1]->setFiner(levels[num_levels - 2]);

	cycle.reset(new VCycle(levels[0],config_j));
}
void Helper::apply(Vec f, Vec u)
{
	cycle->apply(f, u);
}