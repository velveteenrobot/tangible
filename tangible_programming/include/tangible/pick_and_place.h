#ifndef TANGIBLE_PICK_AND_PLACE
#define TANGIBLE_PICK_AND_PLACE

#include <vector>

#include "tangible/operation.h"

// msg's and srv's
#include "tangible_msgs/Instruction.h"
#include "tangible_msgs/Scene.h"
#include "tangible_msgs/SceneObject.h"
#include "tangible_msgs/GetMatchingObjects.h"
#include "tangible_msgs/GetGrasps.h"
#include "tangible_msgs/GetReleases.h"
#include "moveit_msgs/Grasp.h"

namespace tangible
{

class PickAndPlace : public Operation
{
private:
	const static int PICK = 0;
	const static int PLACE = 1;

	bool once;

	bool attempt_pick();
	bool attempt_place();

	bool match_obj2criteria(tangible_msgs::SceneObject obj, tangible_msgs::Target trg);

	std::vector<moveit_msgs::Grasp> get_grasp(tangible_msgs::SceneObject obj, tangible_msgs::Scene scene);
	std::vector<moveit_msgs::Grasp> get_release(tangible_msgs::Target target, tangible_msgs::Scene scene);
	bool move(std::vector<moveit_msgs::Grasp> poses);

public:
	PickAndPlace(ros::NodeHandle& n, std::vector<tangible_msgs::Instruction> ins);
	~PickAndPlace();

	bool execute();
	void stop();
	void reset();

};

};

#endif