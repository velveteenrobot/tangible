#include "tangible/pick_and_place.h"

namespace tangible
{

PickAndPlace::PickAndPlace(ros::NodeHandle& n, std::vector<tangible_msgs::Instruction> ins) : Operation (n, ins) 
{
	// TO-DO error-checking
	// make sure there are exactly two instructions within the operation
	// the first instruction is a pick and the second one is a place

	// ROS_INFO("a pick and place operation created with the following instructions");
	// for(int i = 0; i < instructions.size(); i++)
	// 	switch(instructions[i].type)
	// 	{
	// 		case tangible_msgs::Instruction::PICK:
	// 			ROS_INFO("pick");
	// 			break;
	// 		case tangible_msgs::Instruction::PLACE:
	// 			ROS_INFO("place");
	// 			break;
	// 		case tangible_msgs::Instruction::DROP:
	// 			ROS_INFO("drop");
	// 			break;
	// 		default:
	// 			ROS_ERROR("invalid instruction");
	//			break;
	// 	}

	once = false; 
}

PickAndPlace::~PickAndPlace() {}

bool PickAndPlace::execute() 
{
	ROS_INFO("executing pick and place operation");

	int attempt = 0;
	
	while(!done[PLACE] && attempt <= OPERATION_MAX_ATTEMPTS)
	{
		
		ROS_INFO("attempt #%d", attempt+1);
		int pick_status = NO_PICK;

		if(!done[PICK])
			done[PICK] = attempt_pick(obj_of_op, pick_status);

		if(done[PICK])
			// NOTE: the content of obj_of_op is only used when it holds  
			// valid information so no need to clean it up from one attempt 
			// of operation to another or from one iteration over the entire 
			// program of operations to another.	
			done[PLACE] = attempt_place(obj_of_op);

		attempt++;

		if(done[PLACE] && !once)
			once = true;

		if(!(instructions[PICK].target.type == tangible_msgs::Target::POINT_LOCATION || 
		   instructions[PICK].target.type == tangible_msgs::Target::OBJECT_SELECTOR))
			// TO-DO better to clearly state the conditions for OBJECT_SELECTOR and OBJECTS_SELECTOR
		{
			if(done[PLACE])
			{
				done[PICK] = false;
				done[PLACE] = false;
				attempt = 0;
				ROS_INFO("moving to the next object of operation...");
			}
			else if(!done[PICK] && once && pick_status == NO_PICK)
			// TO-DO check whether the condition can be simplified. done[PICK] and pick_status seem redundant
			{
				done[PLACE] = true;
				ROS_INFO("no more objects for operation.");
			}
		}

	}

	if(done[PLACE])
		all_done = true;

	ROS_INFO("execution of operation was %s", all_done ? "SUCCESSFUL" : "UNSUCCESSFUL");

	return all_done;
	// TO-DO what to return is debatable
}

bool PickAndPlace::attempt_pick(tangible_msgs::SceneObject& obj_under_op, int& pick_status)
{
	ROS_INFO("   pick...");

	tangible_msgs::Scene scene = get_scene();
	//print_scene(scene);
	
	for(int i = 0; i < scene.objects.size(); i++)
	{
		ROS_INFO("   object %d bb(%f, %f, %f) at (%f, %f, %f):", i,
															  	 scene.objects[i].bounding_box.dimensions.x,
															  	 scene.objects[i].bounding_box.dimensions.y,
															  	 scene.objects[i].bounding_box.dimensions.z,
															  	 scene.objects[i].bounding_box.pose.pose.position.x,
															  	 scene.objects[i].bounding_box.pose.pose.position.y,
															  	 scene.objects[i].bounding_box.pose.pose.position.z);

		if(!match_obj2criteria(scene.objects[i], instructions[PICK].target))
			continue;
		ROS_INFO("      matched pick criteria");

		bool already_placed = false;

		if(instructions[PLACE].target.type == tangible_msgs::Target::REGION ||  
		   instructions[PLACE].target.type == tangible_msgs::Target::POINT_LOCATION)

			already_placed = match_obj2criteria(scene.objects[i], instructions[PLACE].target);
			
		else 
		// TO-DO better to clearly state the conditions for OBJECT_SELECTOR and OBJECTS_SELECTOR
		{

			for(int j = 0; j < scene.objects.size(); j++)
			{
				// NOTE: make an exclusive comparison (remove what you are comparing from candidate pool
				// as everything matches itself perfectly.)
				if(j == i)
					continue;

				if(!match_obj2criteria(scene.objects[j], instructions[PLACE].target))
					continue;
				
				tangible_msgs::Target place_target;
				place_target.type = tangible_msgs::Target::POINT_LOCATION;
				place_target.specified_point.point = scene.objects[j].bounding_box.pose.pose.position;

				if(match_obj2criteria(scene.objects[i], place_target))
				{
					already_placed = true;
					break;
				}
			}

		}

		if(already_placed)
			continue;
		ROS_INFO("      is not already at place target");

		int pick_attempt = 0;
		std::vector<moveit_msgs::Grasp> grasps;
		while(pick_attempt <= INSTRUCTION_MAX_ATTEMPTS)
		{
			grasps = get_grasp(scene.objects[i], scene);
			if(!grasps.empty())
				break;
			pick_attempt++;
		}

		pick_attempt = pick_attempt > INSTRUCTION_MAX_ATTEMPTS ? pick_attempt : 0;

		if(pick_attempt == 0)
			ROS_INFO("      has a grasp sequence.");
		else
			pick_status = FAILED_GRASP;

		while(pick_attempt <= INSTRUCTION_MAX_ATTEMPTS)
		{
			// if here, there has been a grasp
			if(move(grasps))
			{
				ROS_INFO("      is successfully picked.");
				obj_under_op = scene.objects[i];
				return true;
			}
			pick_attempt++; 
		}

		pick_status = FAILED_GRASP;

	}

	return false;
}

bool PickAndPlace::attempt_place(tangible_msgs::SceneObject obj_held)
{
	ROS_INFO("   place...");
	ROS_INFO("   object bb(%f, %f, %f) at (%f, %f, %f):", obj_held.bounding_box.dimensions.x,
														  obj_held.bounding_box.dimensions.y,
														  obj_held.bounding_box.dimensions.z,
														  obj_held.bounding_box.pose.pose.position.x,
														  obj_held.bounding_box.pose.pose.position.y,
														  obj_held.bounding_box.pose.pose.position.z);

	tangible_msgs::Scene scene = get_scene();

	std::vector<tangible_msgs::Target> place_target;

	if(instructions[PLACE].target.type == tangible_msgs::Target::REGION ||  
	   instructions[PLACE].target.type == tangible_msgs::Target::POINT_LOCATION)

		place_target.push_back(instructions[PLACE].target);

	else 
	// TO-DO better to clearly state the conditions for OBJECT_SELECTOR and OBJECTS_SELECTOR
	{
		
		for(int i = 0; i < scene.objects.size(); i++)
			if(match_obj2criteria(scene.objects[i], instructions[PLACE].target))
			{
				tangible_msgs::Target target;
				target.type = tangible_msgs::Target::OBJECT_SELECTOR;
				target.selected_object = scene.objects[i];
				place_target.push_back(target);
			}

	}

	ROS_INFO("number of valid placement targets: %d", (int) place_target.size());

	// TO-DO possibly rank the palcement targets before attempting at placing at them

	for(int i = 0; i < place_target.size(); i++)
	{
		
		std::vector<moveit_msgs::Grasp> releases;
		int place_attempt = 0;
		while(place_attempt <= INSTRUCTION_MAX_ATTEMPTS)
		{
			releases = get_release(place_target[i], obj_held, scene, instructions[PLACE].type);
			if(!releases.empty())
				break;
			place_attempt++;
		}

		place_attempt = place_attempt > INSTRUCTION_MAX_ATTEMPTS ? place_attempt : 0;

		if(place_attempt == 0)
			ROS_INFO("      has a release sequence");

		while(place_attempt <= INSTRUCTION_MAX_ATTEMPTS)
		{
			// if here, there has been a release
			if(move(releases))
			{
				ROS_INFO("      is successfully released.");
				return true;
			}
			place_attempt++;
		}

	}

	return false;
}

bool PickAndPlace::match_obj2criteria(tangible_msgs::SceneObject obj, tangible_msgs::Target trg)
{
	std::vector<tangible_msgs::SceneObject> objs_to_check;
	objs_to_check.push_back(obj);

	std::string ins_check_service = get_private_param("instruction_check_criteria_service");
	ros::ServiceClient ins_check_client = node_handle.serviceClient<tangible_msgs::GetMatchingObjects>(ins_check_service);

	tangible_msgs::GetMatchingObjects criteria_srv;
	criteria_srv.request.objects = objs_to_check;
	criteria_srv.request.target = trg;

	// ROS_INFO("      object (%f, %f, %f) at (%f, %f, %f) matched target %d?", obj.bounding_box.dimensions.x,
	// 					   												     obj.bounding_box.dimensions.y,
	// 																   		 obj.bounding_box.dimensions.z, 
	// 																   		 obj.bounding_box.pose.pose.position.x, 
	// 																   		 obj.bounding_box.pose.pose.position.y, 
	// 																   		 obj.bounding_box.pose.pose.position.z,
	// 																   		 trg.type);

	bool success = ins_check_client.call(criteria_srv);

	std::vector<tangible_msgs::SceneObject> objs_checked;
	if(success)
		objs_checked = criteria_srv.response.objects;
	else
	{
		ROS_ERROR("failed to call service %s to check instruction criteria.", ins_check_service.c_str());
		return false;
	}

	// if(objs_checked.empty())
	// 	ROS_INFO("      - NO");
	// else
	// 	ROS_INFO("      - YES");

	return !objs_checked.empty();
}

std::vector<moveit_msgs::Grasp> PickAndPlace::get_grasp(tangible_msgs::SceneObject obj, tangible_msgs::Scene scene)
{
	std::string grasp_service = get_private_param("grasp_acquisition_service");
	ros::ServiceClient grasp_acquisition_client = node_handle.serviceClient<tangible_msgs::GetGrasps>(grasp_service);

	tangible_msgs::GetGrasps grasp_srv;
	grasp_srv.request.object = obj;
	grasp_srv.request.scene = scene;

	bool success = grasp_acquisition_client.call(grasp_srv);

	std::vector<moveit_msgs::Grasp> grasps;
	if(success)
		grasps = grasp_srv.response.grasps;
	else
		ROS_ERROR("failed to call service %s to obtain grasp poses.", grasp_service.c_str());

	return grasps;
}

std::vector<moveit_msgs::Grasp> PickAndPlace::get_release(tangible_msgs::Target target, tangible_msgs::SceneObject obj, tangible_msgs::Scene scene, int release_type)
{
	std::string release_service = get_private_param("release_acquisition_service");
	ros::ServiceClient release_acquisition_client = node_handle.serviceClient<tangible_msgs::GetReleases>(release_service);

	tangible_msgs::GetReleases release_srv;
	// if(release_type == tangible_msgs::Instruction::PLACE)
	// 	release_srv.request.type = tangible_msgs::GetReleases::PLACE;
	// else if(release_type == tangible_msgs::Instruction::DROP)
	// 	release_srv.request.type = tangible_msgs::GetReleases::DROP;
	// TO-DO uncomment the above after pulling the latest version of GetReleases
	// TO-DO release_srv.request.num_orientations = 4;
	// TO-DO release_srv.request.region_sample_spacing = 0.3;
	release_srv.request.target = target;
	release_srv.request.object = obj;
	release_srv.request.scene = scene;

	bool success = release_acquisition_client.call(release_srv);

	std::vector<moveit_msgs::Grasp> releases;
	if(success)
		releases = release_srv.response.releases;
	else
		ROS_ERROR("failed to call service %s to obtain release poses.", release_service.c_str());

	return releases;
}

bool PickAndPlace::move(std::vector<moveit_msgs::Grasp> poses)
{
	std::string move_service = get_private_param("arm_movement_service");
	ros::ServiceClient move_client = node_handle.serviceClient<tangible_msgs::GetMovements>(move_service);

	tangible_msgs::GetMovements move_srv;
	move_srv.request.poses = poses;

	bool success = move_client.call(move_srv);
	// TO-DO arm_movement service has to try all generated grasps. If none was possible it should return false.
	// possible: there is a plan. 
	// there are other cases for failure, e.g. the movement is incomplete.

	bool movement_success = false;
	if(success)
		movement_success = move_srv.response.status;
	else
		ROS_ERROR("failed to call service %s to move the arm.", move_service.c_str());

	return movement_success;
}

void PickAndPlace::stop()
{
	ROS_INFO("stop Pick and Place operation");
	// TO-DO: make a call to the stop service 
}

void PickAndPlace::reset()
{
	once = false;

	Operation::reset();
	
	ROS_INFO("pick_and_place reset");
}

}