/*
 * slam_toolbox
 * Copyright (c) 2008, Willow Garage, Inc.
 * Copyright Work Modifications (c) 2018, Simbe Robotics, Inc.
 * Copyright Work Modifications (c) 2019, Steve Macenski
 *
 * THE WORK (AS DEFINED BELOW) IS PROVIDED UNDER THE TERMS OF THIS CREATIVE
 * COMMONS PUBLIC LICENSE ("CCPL" OR "LICENSE"). THE WORK IS PROTECTED BY
 * COPYRIGHT AND/OR OTHER APPLICABLE LAW. ANY USE OF THE WORK OTHER THAN AS
 * AUTHORIZED UNDER THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
 *
 * BY EXERCISING ANY RIGHTS TO THE WORK PROVIDED HERE, YOU ACCEPT AND AGREE TO
 * BE BOUND BY THE TERMS OF THIS LICENSE. THE LICENSOR GRANTS YOU THE RIGHTS
 * CONTAINED HERE IN CONSIDERATION OF YOUR ACCEPTANCE OF SUCH TERMS AND
 * CONDITIONS.
 *
 */

#ifndef SLAM_TOOLBOX_SLAM_TOOLBOX_COMMON_H_
#define SLAM_TOOLBOX_SLAM_TOOLBOX_COMMON_H_

#include "ros/ros.h"
#include "message_filters/subscriber.h"
#include "tf2_ros/transform_broadcaster.h"
#include "tf2_ros/transform_listener.h"
#include "tf2_ros/message_filter.h"
#include "tf2/LinearMath/Matrix3x3.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.h"
#include "apriltag_ros/AprilTagDetectionArray.h"

#include "pluginlib/class_loader.h"

#include "slam_toolbox/toolbox_types.hpp"
#include "slam_toolbox/slam_mapper.hpp"
#include "slam_toolbox/snap_utils.hpp"
#include "slam_toolbox/laser_utils.hpp"
#include "slam_toolbox/get_pose_helper.hpp"
#include "slam_toolbox/map_saver.hpp"
#include "slam_toolbox/loop_closure_assistant.hpp"
#include "mtg_messages/agent_status.h"

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <cstdlib>
#include <fstream>
#include <boost/thread.hpp>
#include <sys/resource.h>
#include <assert.h>

namespace slam_toolbox
{

// dirty, dirty cheat I love
using namespace ::toolbox_types;

class SlamToolbox
{
public:
  SlamToolbox(ros::NodeHandle& nh);
  ~SlamToolbox();

protected:
  // threads
  void publishVisualizations();
  void publishTransformLoop(const double &transform_publish_period, const double &tag_publish_period);

  // setup
  void setParams(ros::NodeHandle& nh);
  void setSolver(ros::NodeHandle& private_nh_);
  void setROSInterfaces(ros::NodeHandle& node);

  // callbacks
  virtual void laserCallback(const sensor_msgs::LaserScan::ConstPtr& scan) = 0;
  virtual void apriltagCallback(const apriltag_ros::AprilTagDetectionArray::ConstPtr& apriltags) = 0;
  bool mapCallback(nav_msgs::GetMap::Request& req,
    nav_msgs::GetMap::Response& res);
  virtual bool serializePoseGraphCallback(slam_toolbox_msgs::SerializePoseGraph::Request& req,
    slam_toolbox_msgs::SerializePoseGraph::Response& resp);
  virtual bool deserializePoseGraphCallback(slam_toolbox_msgs::DeserializePoseGraph::Request& req,
    slam_toolbox_msgs::DeserializePoseGraph::Response& resp);
  void loadSerializedPoseGraph(std::unique_ptr<karto::Mapper>&, std::unique_ptr<karto::Dataset>&);
  void loadPoseGraphByParams(ros::NodeHandle& nh);

  // functional bits
  karto::LaserRangeFinder* getLaser(const sensor_msgs::LaserScan::ConstPtr& scan);
  virtual karto::LocalizedRangeScan* addScan(karto::LaserRangeFinder* laser, const sensor_msgs::LaserScan::ConstPtr& scan,
    karto::Pose2& karto_pose);
  karto::LocalizedRangeScan* addScan(karto::LaserRangeFinder* laser, PosedScan& scanWPose);
  void addTag(apriltag_ros::AprilTagDetectionArray::ConstPtr& apriltag, karto::LocalizedRangeScan* scan);
  bool updateMap();
  tf2::Stamped<tf2::Transform> setTransformFromPoses(const karto::Pose2& pose,
    const karto::Pose2& karto_pose, const std_msgs::Header& header, const bool& update_reprocessing_transform);
  tf2::Stamped<tf2::Transform> publishTagTransform(int tag_id, const karto::Name& sensor_name);
  karto::LocalizedRangeScan* getLocalizedRangeScan(karto::LaserRangeFinder* laser,
    const sensor_msgs::LaserScan::ConstPtr& scan,
    karto::Pose2& karto_pose);
  bool shouldStartWithPoseGraph(std::string& filename, geometry_msgs::Pose2D& pose, bool& start_at_dock);
  bool shouldProcessScan(const sensor_msgs::LaserScan::ConstPtr& scan, const karto::Pose2& pose);
  std::set<std::string> getFleetStatusInfo();

  // pausing bits
  bool isPaused(const PausedApplication& app);
  bool pauseNewMeasurementsCallback(slam_toolbox_msgs::Pause::Request& req,
    slam_toolbox_msgs::Pause::Response& resp);

  // ROS-y-ness
  ros::NodeHandle nh_;
  std::unique_ptr<tf2_ros::Buffer> tf_;
  std::unique_ptr<tf2_ros::TransformListener> tfL_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tfB_;
  std::vector<std::unique_ptr<message_filters::Subscriber<sensor_msgs::LaserScan> > > scan_filter_subs_;
  std::vector<std::unique_ptr<tf2_ros::MessageFilter<sensor_msgs::LaserScan> > > scan_filters_;
  std::vector<std::unique_ptr<message_filters::Subscriber<apriltag_ros::AprilTagDetectionArray> > > apriltag_subs_;
  std::vector<std::unique_ptr<apriltag_ros::AprilTagDetectionArray> > apriltags_;
  ros::Publisher sst_, sstm_, tag_pub_;
  ros::ServiceServer ssMap_, ssPauseMeasurements_, ssSerialize_, ssDesserialize_;
  ros::ServiceClient status_client_;

  // Storage for ROS parameters
  std::string map_frame_, map_name_;
  std::vector<std::string> odom_frames_, base_frames_, laser_topics_, apriltag_topics_;
  ros::Duration transform_timeout_, tf_buffer_dur_, minimum_time_interval_;
  int throttle_scans_;

  double resolution_;
  bool first_measurement_, enable_interactive_mode_;

  // Book keeping
  std::unique_ptr<mapper_utils::SMapper> smapper_;
  std::unique_ptr<karto::Dataset> dataset_;
  std::map<std::string, laser_utils::LaserMetadata> lasers_;
  std::map<std::string, tf2::Transform> m_map_to_odoms_;
  std::map<int, tf2::Transform> m_map_to_tags_;
  std::map<int, std::pair<geometry_msgs::PoseWithCovarianceStamped, karto::LocalizedRangeScan*>> m_apriltag_to_scan_;

  // helpers
  std::map<std::string,std::unique_ptr<laser_utils::LaserAssistant>> laser_assistants_;
  std::vector<std::unique_ptr<pose_utils::GetPoseHelper>> pose_helpers_;
  std::unique_ptr<map_saver::MapSaver> map_saver_;
  std::unique_ptr<loop_closure_assistant::LoopClosureAssistant> closure_assistant_;
  std::unique_ptr<laser_utils::ScanHolder> scan_holder_;

  // Internal state
  std::vector<std::unique_ptr<boost::thread> > threads_;
  tf2::Transform map_to_odom_;
  std::string map_to_odom_child_frame_id_;
  boost::mutex map_to_odom_mutex_, smapper_mutex_, pose_mutex_, apriltag_mutex_, map_to_tags_mutex_;
  PausedState state_;
  nav_msgs::GetMap::Response map_;
  ProcessType processor_type_;
  std::unique_ptr<karto::Pose2> process_near_pose_;
  tf2::Transform reprocessing_transform_;
  std::set<std::string> fleet_info_;

  // pluginlib
  pluginlib::ClassLoader<karto::ScanSolver> solver_loader_;
  boost::shared_ptr<karto::ScanSolver> solver_;
};

} // end namespace

#endif //SLAM_TOOLBOX_SLAM_TOOLBOX_COMMON_H_
