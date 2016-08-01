#include "ros/ros.h"
#include "tangible/frame_transformer.h"
#include "tangible/tag_extractor.h"
#include "tangible/scene_parser.h"
#include "tangible/program.h"
#include "tangible/visualizer.h"


int main (int argc, char** argv) {
	const static int MAX_BLOCKING_THREAD_NUM = 5;
	ros::init(argc, argv, "tangible_pbd");
	
	ros::NodeHandle node;
	ros::AsyncSpinner spinner(MAX_BLOCKING_THREAD_NUM);
	spinner.start();

	tangible::FrameTransformer trns(node, "base_footprint");
	tangible::TagExtractor tagext(node);
	tangible::SceneParser parser(node);
	tangible::Visualizer vis(node, "base_footprint");

	ros::Duration(5).sleep();
	//NOTE: wait is necessary here so tags are filled before the call to tagext.get_tags()
	//TO-DO change the whole architecture to avoid this race condition
	//      e.g. use ros::topic::waitForMessage<msg_type>("topic_name", timeOut);



	
	std::string err = "error";
	std::vector<tangible::Tag> tags;
	std::vector<rapid::perception::Object> objects;

	//do {		
		tags = tagext.get_tags();
		ROS_INFO("the following tags are detected:");
		for(int i = 0; i < tags.size(); i++)
			ROS_INFO("%s%s", tags[i].printID().c_str(), tags[i].printCenter().c_str());
		
		if(parser.isSuccessful()) {
		    objects = parser.getObjects();
		    vis.update(objects);
			tangible::Program program(tags, objects);
			err = program.error();
		}
	//} while(!err.empty());
	//ROS_INFO("\n%s", program.printInstructionTags().c_str());
	
	ros::Rate interval(3);
	while(ros::ok()) {
		//if(err.empty())
		//	vis.update(program);
		
		if(parser.isSuccessful())
			vis.update(parser.getObjects());
		interval.sleep();
	}

	
	//TO-DO later on there should be a mechanism for refreshing a program
	ros::waitForShutdown();

	return 0;
}