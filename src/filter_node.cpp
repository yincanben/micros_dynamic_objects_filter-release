/*************************************************************************
	> File Name: fliter_node.cpp
	> Author: yincanben
	> Mail: yincanben@163.com
	> Created Time: Thu 05 Feb 2015 09:15:56 PM CST
 ************************************************************************/

#include <iostream>

#include <ros/ros.h>

#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>

#include <image_transport/image_transport.h>
#include <image_transport/subscriber_filter.h>

#include <image_geometry/stereo_camera_model.h>

#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
#include <cv_bridge/cv_bridge.h>

#include "moving_object_filter.h"

#include <nav_msgs/Odometry.h>

//using namespace std;
class DynamicObjectFilter: public MovingObjectFilter{
    public:
        DynamicObjectFilter(int argc, char**argv):
        MovingObjectFilter( argc, argv ),
        sync_(0),
        rate_(1.0),
        time_(ros::Time::now())
        {
            ros::NodeHandle nh ;
            ros::NodeHandle pnh("~") ;
            int queueSize = 5 ;
            ros::NodeHandle rgb_nh(nh, "rgb") ;
            ros::NodeHandle depth_nh(nh, "depth") ;
            ros::NodeHandle rgb_pnh(pnh, "rgb") ;
            ros::NodeHandle depth_pnh(pnh, "depth" ) ;
        
            image_transport::ImageTransport rgb_it(rgb_nh) ;
            image_transport::ImageTransport depth_it(depth_nh) ;

            image_transport::TransportHints hintsRgb("raw", ros::TransportHints(), rgb_pnh) ;
            image_transport::TransportHints hintsDepth("raw", ros::TransportHints(), depth_pnh) ;

            // subscribe rgb image
            image_mono_sub_.subscribe(rgb_it, rgb_nh.resolveName("image"), 1, hintsRgb) ;
            // subscribe depth image
            image_depth_sub_.subscribe(depth_it, depth_nh.resolveName("image"), 1, hintsDepth) ;
            // subscribe camera_info
            info_sub_.subscribe(rgb_nh, "camera_info", 1) ;
            sync_ = new message_filters::Synchronizer<MySyncPolicy>(MySyncPolicy(queueSize), image_mono_sub_, image_depth_sub_, info_sub_) ;
            sync_->registerCallback(boost::bind(&DynamicObjectFilter::callback, this, _1, _2, _3)) ;

        }
        ~DynamicObjectFilter(){
            delete sync_ ;
        }

        // inspired from RTABMap  rtabmap_ros/src/RGBDOdometryNode.cpp
        void callback( const sensor_msgs::ImageConstPtr& image,
                    const sensor_msgs::ImageConstPtr& depth,
                    const sensor_msgs::CameraInfoConstPtr& cameraInfo){

            //std::cout << "Entering the callback" << std::endl ;
            if(!(image->encoding.compare(sensor_msgs::image_encodings::MONO8) ==0 ||
                 image->encoding.compare(sensor_msgs::image_encodings::MONO16) ==0 ||
                 image->encoding.compare(sensor_msgs::image_encodings::BGR8) == 0 ||
                 image->encoding.compare(sensor_msgs::image_encodings::RGB8) == 0) ||
                 !(depth->encoding.compare(sensor_msgs::image_encodings::TYPE_16UC1)==0 ||
                 depth->encoding.compare(sensor_msgs::image_encodings::TYPE_32FC1)==0)){
                    ROS_ERROR("Input type must be image=mono8,mono16,rgb8,bgr8 (mono8 recommended) and image_depth=16UC1");
                            return;
            }else if(depth->encoding.compare(sensor_msgs::image_encodings::TYPE_32FC1)==0){
                static bool warned = false;
                if(!warned){
					ROS_WARN("Input depth type is 32FC1, please use type 16UC1 for depth. The depth images "
							 "will be processed anyway but with a conversion. This warning is only be printed once...") ;
					warned = true ;
                }// for if
            }// for else

            //Process data every second
            if(rate_>0.0f)
            {
                std::cout << "time_ = " << time_ << std::endl ;
                if(ros::Time::now() - time_ < ros::Duration(1.0f/rate_))
                {
                    std::cout << "Don't process Data" <<  std::endl ;
                    return ;
                }
            }
            time_ = ros::Time::now();
            //std::cout << "rgb frame_id" << image->header.frame_id << std::endl ;
            if( image->data.size() && depth->data.size() && cameraInfo->K[4] != 0 ){
                // obtain the camera's parameters
                image_geometry::PinholeCameraModel model ;
                model.fromCameraInfo(*cameraInfo) ;
                float cx = model.cx() ;
                float cy = model.cy() ;
                float fx = model.fx() ;
                float fy = model.fy() ;

                // convert the ros message to Mat in OpenCV
                cv_bridge::CvImageConstPtr ptrImage = cv_bridge::toCvShare(image, "bgr8");  //mono8
                cv_bridge::CvImageConstPtr ptrDepth = cv_bridge::toCvShare(depth);
                // process the data
                std::cout << "Process the data" << std::endl ;
                setFrameId(image->header.frame_id, depth->header.frame_id);
                this->processData( ptrImage->image, ptrDepth->image, cx, cy, fx, fy);
            }

//            if(rate_ < 10){
//               rate_ ++ ;
//            }else{
//                if( image->data.size() && depth->data.size() && cameraInfo->K[4] != 0 ){
//                    // obtain the camera's parameters
//                    image_geometry::PinholeCameraModel model ;
//                    model.fromCameraInfo(*cameraInfo) ;
//                    float cx = model.cx() ;
//                    float cy = model.cy() ;
//                    float fx = model.fx() ;
//                    float fy = model.fy() ;

//                    // convert the ros message to Mat in OpenCV
//                    cv_bridge::CvImageConstPtr ptrImage = cv_bridge::toCvShare(image, "bgr8");  //mono8
//                    cv_bridge::CvImageConstPtr ptrDepth = cv_bridge::toCvShare(depth);
//                    // process the data
//                    std::cout << "Process the data" << std::endl ;
//                    this->processData( ptrImage->image, ptrDepth->image, cx, cy, fx, fy);
//               }
//               rate_ = 0 ;
//            }
        }

    private:
        
        image_transport::SubscriberFilter image_mono_sub_ ;
        image_transport::SubscriberFilter image_depth_sub_ ;
        message_filters::Subscriber<sensor_msgs::CameraInfo> info_sub_ ;
        typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::Image, sensor_msgs::Image, sensor_msgs::CameraInfo> MySyncPolicy;
        message_filters::Synchronizer<MySyncPolicy> * sync_;

        float rate_ ;
        ros::Time time_ ;

        
    };
int main( int argc, char**argv ){
    ros::init( argc,argv,"filter_node");

    DynamicObjectFilter filter( argc, argv ) ;


    ros::spin() ;

    return 0 ;
}
