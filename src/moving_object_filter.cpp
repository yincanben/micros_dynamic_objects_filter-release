/*************************************************************************
	> File Name: moving_object_filter.cpp
	> Author: yincanben
	> Mail: yincanben@163.com
	> Created Time: 2015年01月09日 星期五 08时05分54秒
 ************************************************************************/
 #include "moving_object_filter.h"
 #include "umath.h"
 #include "optical_flow.h"
 //#define  DEBUG
 //#define  TIME
// #define  VIEW
 #define  VIEW_RESULT
// #define  DEBUG_LOOP
 //#define  DEBUG_WRITE
using namespace Eigen;
 
 MovingObjectFilter::MovingObjectFilter( int argc, char**argv ):rgb_frame_id(""),depth_frame_id(""){//,result_viewer("Result"){//cloud_viewer("Moving Object Viewer"),viewer("Viewer")

    ros::NodeHandle nh ;
    rgbPub_ = nh.advertise<sensor_msgs::Image>( "/filter/rgb" , 10 ) ;
    depthPub_ = nh.advertise<sensor_msgs::Image>( "/filter/depth", 10 ) ;
    cloudPub_ = nh.advertise<sensor_msgs::PointCloud2>("/filter/pointcloud2",10) ;
    num1 = 0 ;


}
 void MovingObjectFilter::setFrameId(std::string rgb_frame, std::string depth_frame){
     rgb_frame_id = rgb_frame ;
     depth_frame_id = depth_frame ;
 }

 void MovingObjectFilter::processData( const cv::Mat & image, const cv::Mat &depth ,
                                       float cx,float cy,
                                       float fx,float fy){

     cv::Mat imageMono ;
     //convert to grayscale

     if(image.channels() > 1){
        cv::cvtColor(image, imageMono, cv::COLOR_BGR2GRAY) ;
     }else{
        imageMono = image ;
     }

     timeval start , end ;
     long long time ;

     gettimeofday( &start, NULL ) ;

     //compute the Homography
     this->computeHomography(imageMono);

     gettimeofday( &end, NULL ) ;
     time = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)  ;

     #ifdef TIME
     cout << "findhomography Time =  " << time << endl;
     #endif TIME

     //OpticalFlow of ;

     //of.process( imageMono );

     //cv::imshow("current View", imageMono) ;


     //ROS_INFO( "Process data" ) ;

     pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>) ;
     gettimeofday( &start, NULL ) ;
     //compute PointCloud
     cloud = this->cloudFromDepthRGB( image, depth, cx, cy, fx, fy, 1.0 ) ;
     gettimeofday( &end, NULL ) ;
     time = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)  ;

     #ifdef TIME
     cout << "calculateCloud Time =  " << time << endl;
     #endif TIME

     //if(!result_viewer.wasStopped()){
     //   result_viewer.showCloud(cloud) ;
     //}



     gettimeofday( &start, NULL ) ;

     // make image difference
     this->image_diff( imageMono, cloud ) ;

     gettimeofday( &end, NULL ) ;
     time = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)  ;

     #ifdef TIME
     cout << "Diffetrence Time =  " << time << endl;
     #endif TIME

     pcl::PointCloud<pcl::PointXYZRGB>::Ptr restCloud;
     cv::Mat restDepth ;

     gettimeofday( &start, NULL ) ;

     //
     restDepth = this->pcl_segmentation(cloud, image, depth, cx, cy, fx, fy) ;

     gettimeofday( &end, NULL ) ;
     time = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)  ;

     #ifdef TIME
     cout << "CloudCluster Time =  " << time << endl;
     #endif TIME

     //Publish depth image
     //std::cout << "rgb_frame_id" << rgb_frame_id << std::endl ;
     cv_bridge::CvImage cv_image ;
     cv_image.image = restDepth ;
     cv_image.encoding = "16UC1" ;
     sensor_msgs::Image depth_image ;
     cv_image.toImageMsg( depth_image ) ;
     depth_image.header.stamp = ros::Time::now() ;
     depth_image.header.frame_id = depth_frame_id ;
     depthPub_.publish(depth_image) ;

     //Publish RGB image
     cv_bridge::CvImage cv_rgbImage ;
     cv_rgbImage.image = image ;
     cv_rgbImage.encoding = "bgr8" ;
     sensor_msgs::Image rgbImage ;
     cv_rgbImage.toImageMsg( rgbImage ) ;
     rgbImage.header.stamp = ros::Time::now() ;
     rgbImage.header.frame_id = rgb_frame_id ;
     rgbPub_.publish( rgbImage ) ;

     //previous_coordinate.clear() ;
     //current_coordinate.clear() ;

     restCloud = this->cloudFromDepthRGB(image, restDepth, cx, cy, fx, fy, 1.0) ;

     #ifdef DEBUG_WRITE
     pcl::PCDWriter writer ;
     //static int num1=0 ;
     std::stringstream ss1;
     ss1 << "restcloud_" << num1  << ".pcd" ;
     writer.write<pcl::PointXYZRGB>(ss1.str(), *restCloud,false) ;
     //num1++ ;
     #endif DEBUG_WRITE

     #ifdef View
     //cv::namedWindow("Image") ;
     //cv::imshow("Image", image) ;
     //cv::waitKey(1);

     //if(!result_viewer.wasStopped()){
     //   result_viewer.showCloud(restCloud->makeShared()) ;
     //}

     #endif
     
     sensor_msgs::PointCloud2::Ptr cloudMsg(new sensor_msgs::PointCloud2) ;
     pcl::toROSMsg(*restCloud, *cloudMsg) ;
     cloudMsg->header.stamp = ros::Time::now() ;
     cloudMsg->header.frame_id = "camera_link" ;
     cloudPub_.publish(cloudMsg) ;




     /*
     //cv::namedWindow( "Display window", CV_WINDOW_AUTOSIZE );// Create a window for display.
     //cv::imshow( "Display window", image );                   // Show our image inside it.
     //cv::waitKey(0);
     //rgbPub_.publish(binary_image) ;
     */
     //pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>) ;
     //cloud = this->cloudFromDepthRGB( image, depth, cx, cy, fx, fy, 1 ) ;
     //this->image_separate(cloud);


}

 void MovingObjectFilter::computeHomography(cv::Mat &grayImage){

     //Step 1: Detect the keypoints using ORB Detector
     cv::ORB orb ;
     std::vector<cv::KeyPoint> lastKeypoints ;
     std::vector<cv::KeyPoint> keypoints ;

     #ifdef DEBUG_WRITE
     //static int num1 = 0 ;
     //cout << "num = " << num1 << endl ;
     std::stringstream ss;
     ss << "currentImage" << num1 << ".jpg";
     cv::imwrite( ss.str(), grayImage ) ;
     //num1++ ;
     #endif DEBUG_WRITE
     
     //featureDetector_ ->detect( grayImage, keypoints ) ;
     //ROS_INFO( "The size of keypoints: %d", keypoints.size() ) ;
     //if( keypoints.size() == 0 )
     //    return ;
     //Step 2: Calculate descriptors (features vectors)
     cv::Mat lastDescriptors ;
     cv::Mat descriptors ;

     if(!lastImage.empty()){
         orb( lastImage, cv::Mat(), lastKeypoints, lastDescriptors) ;
         orb( grayImage, cv::Mat(), keypoints, descriptors );
     }

     cv::BFMatcher matcher(cv::NORM_L2) ;
     //cv::BruteForceMatcher<cv::HammingLUT> matcher ;
     std::vector<cv::DMatch> matches ;
     //matcher.match( lastDescriptors, descriptors, matches );
     std::vector<cv::DMatch> goodMatches ;
     double minDist = 1000.0 , maxDist = 0.0 ;
     cv::Mat img_matches ;
     std::vector<cv::Point2f> lastPoint ;
     std::vector<cv::Point2f> currentPoint ;
     cv::Mat shft ;
     if(!lastDescriptors.empty()){
         //cout << "************" << endl ;
         matcher.match( lastDescriptors, descriptors, matches );
         for(int i=0; i<lastDescriptors.rows; i++){
             double dist = matches[i].distance ;
             if(dist < minDist)
                 minDist = dist ;
             if(dist > maxDist)
                 maxDist = dist ;    
         }

         for(int i=0; i<lastDescriptors.rows; i++){
             if( matches[i].distance < 0.6*maxDist ){
                 goodMatches.push_back(matches[i]);
             }
         }

         //draw matches


         //cv::namedWindow("matches") ;
         cv::drawMatches( lastImage, lastKeypoints, grayImage, keypoints, goodMatches,img_matches, cv::Scalar::all(-1), cv::Scalar::all(-1),std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
         //imshow("matches", img_matches) ;

        #ifdef DEBUG_WRITE
        //static int num = 1 ;
        std::stringstream ss1;
        ss1 << "matchesImage" << num1 << ".jpg";
        cv::imwrite( ss1.str(), img_matches ) ;
        #endif DEBUG_WRITE

        #ifdef DEBUG_WRITE
        std::stringstream ss2;
        ss2 << "lastImage" << num1 << ".jpg";
        cv::imwrite( ss2.str(), lastImage ) ;
        #endif DEBUG_WRITE



         if(cv::waitKey(1) > 0){
             exit(0);
         }
         cout << "match size = " << goodMatches.size() << endl ;
         if(goodMatches.size() > 4){
             for( int i=0; i<goodMatches.size();i++ ){
                 lastPoint.push_back( lastKeypoints[goodMatches[i].queryIdx].pt );
                 currentPoint.push_back( keypoints[goodMatches[i].trainIdx].pt );
             }
             double m[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
             Homography = cv::Mat(3, 3, CV_64F, m );
             Homography = cv::findHomography( lastPoint, currentPoint, CV_RANSAC ) ;
             shft = ( cv::Mat_<double>(3,3)<< 1.0, 0, lastImage.cols, 0, 1.0, 0, 0, 0, 1.0) ;
         }

         //warp the image


         /*cv::Mat dst ;
         //cv::warpPerspective( lastImage, dst, Homography, cv::Size(lastImage.cols + grayImage.cols + lastImage.cols, lastImage.rows));
         cv::warpPerspective( lastImage, dst, shft*Homography, cv::Size(lastImage.cols + grayImage.cols + lastImage.cols, lastImage.rows));

         cv::Mat rightImage = grayImage ;
         rightImage.copyTo(cv::Mat( dst, cv::Rect( lastImage.cols, 0, grayImage.cols, grayImage.rows  ) )) ;
         //cv::namedWindow("warpImage") ;
         //cv::imshow("warpImage", dst) ;
         */

        #ifdef DEBUG_WRITE
        std::stringstream ss3;
        ss3 << "warpImage" << num1 << ".jpg";
        cv::imwrite( ss3.str(), dst ) ;
        //num++ ;
        #endif DEBUG_WRITE
     }

     lastImage = grayImage ;

 }



void MovingObjectFilter::image_diff(const cv::Mat &currentImage, cloud_type::ConstPtr cloud){
    cv::Mat BlurImage1, BlurImage2 ;
    if(lastBlurImage.empty()){
        cv::GaussianBlur( currentImage, BlurImage1, cv::Size( 11, 11 ), 0, 0 );
        lastBlurImage = BlurImage1 ;
        //lastDepth = depth ;
        lastCloud = *cloud ;
    }else{
        cv::GaussianBlur( currentImage, BlurImage2, cv::Size( 11, 11 ), 0, 0 );

        //cv::absdiff( BlurImage2, lastImage, diff_image ) ;

        //Calculate the difference of image
        cv::Mat last_frame( 480,640, CV_8UC1, cv::Scalar(0) );
        cv::Mat current_frame( 480,640, CV_8UC1, cv::Scalar(0) );
        cv::Mat diffFrame( 480,640, CV_8UC1, cv::Scalar(0) );
        //cv::Mat last = lastBlurImage ;
        //cv::Mat current = BlurImage2 ;
        lastFrame = last_frame ;
        currentFrame = current_frame ;
        threshod = 40 ;
        //cv::namedWindow("lastFrame") ;
        //cv::namedWindow("currentFrame") ;
        //cv::namedWindow("diffFrame") ;

        #ifdef VIEW
        cv::namedWindow("currentImage");
        cv::imshow("currentImage", currentImage) ;
        cv::waitKey(1) ;
        #endif VIEW

        ros::Time timebegin = ros::Time::now();
        cv::Mat dst ;
        cv::absdiff(lastImage, BlurImage2, dst) ;
        ros::Time timeend = ros::Time::now() ;
        ros::Duration time1 = timeend - timebegin;
        double timecost = time1.toSec();
        //cout << "difference  Time1 : " << 1000*timecost << endl;

        //cv::namedWindow("dst") ;
        //cv::imshow("dst", dst) ;
        //cv::waitKey(1) ;


        //cv::Mat srcMat = cv::Mat::zeros(3,1,CV_64FC1);
        //cv::Mat warpMat ;
        cv::Point warpPt ;

        //MatrixXf hom(3,3) ;
        double hom[4][4] ;

        for( int rows=0; rows < Homography.rows; rows++ ){
            const double* homPtr  = Homography.ptr<double>(rows) ;
            for(int cols=0; cols< Homography.cols; cols++){
                //cout << "homVal = " << homPtr[cols]  << endl ;
                hom[rows][cols] = homPtr[cols] ;
            }
        }
        //MatrixXf srcMatrix(3,1) ;
        //MatrixXf resMatrix(3,1) ;
        double srcMatrix[4][2];
        double resMatrix[4][2];
        //float warpx  = 0.0 , warpy = 0.0 , warpz = 0.0 ;
        for( int rows=0; rows < lastImage.rows; rows++ ){
            for(int cols=0; cols< lastImage.cols; cols++){

                //srcMat.at<double>(0,0) = cols ;
                //srcMat.at<double>(1,0) = rows;
                //srcMat.at<double>(2,0) = 1.0 ;
                //warpMat = Homography * srcMat ;
                //warpPt.x = cvRound( warpMat.at<double>(0,0) / warpMat.at<double>(2,0) ) ;
                //warpPt.y = cvRound( warpMat.at<double>(1,0) / warpMat.at<double>(2,0) ) ;
                //cout << "before, " << "warpPt.x= " << warpPt.x << endl;
                //cout << "before, " << "warpPt.y= " << warpPt.y << endl ;
                srcMatrix[0][0] = cols ;
                srcMatrix[1][0] = rows ;
                srcMatrix[2][0] = 1.0 ;
                resMatrix[0][0] = hom[0][0]*srcMatrix[0][0] + hom[0][1]*srcMatrix[1][0] + hom[0][2]*srcMatrix[2][0] ;
                resMatrix[1][0] = hom[1][0]*srcMatrix[0][0] + hom[1][1]*srcMatrix[1][0] + hom[1][2]*srcMatrix[2][0] ;
                resMatrix[2][0] = hom[2][0]*srcMatrix[0][0] + hom[2][1]*srcMatrix[1][0] + hom[2][2]*srcMatrix[2][0] ;

                warpPt.x = cvRound( resMatrix[0][0] / resMatrix[2][0] ) ;
                warpPt.y = cvRound( resMatrix[1][0] / resMatrix[2][0] ) ;
                //cout << "after, " << "warpPt.x= " << warpPt.x << endl;
                //cout << "after, " << "warpPt.y= " << warpPt.y << endl ;
                //float lastDepthValue = (float)lastDepth.at<uint16_t>( rows, cols )*0.001f ;
                //cout << "The rows of depth:" << rows << " ,The cols of depth: " << cols << endl ;
                const uchar* lastBlurImagePtr ;
                const uchar* currentBlurImagePtr ;

                if( warpPt.x>0 && warpPt.x<640  &&  warpPt.y>0 && warpPt.y< 480){
                    lastBlurImagePtr = lastBlurImage.ptr<uchar>( rows );
                    currentBlurImagePtr = BlurImage2.ptr<uchar>( warpPt.y );
                    const uchar& lastCol  = lastBlurImagePtr[cols] ;
                    const uchar& currentCol  = currentBlurImagePtr[warpPt.x] ;
                    double imageDiff = abs(lastCol - currentCol) ;

                    //double imageDiff = abs( lastBlurImage.at<unsigned char>(rows, cols) - BlurImage2.at<unsigned char>(warpPt.y ,warpPt.x));

                    double lastDepthValue = 0.0 ;
                    double depthValue = 0.0 ;
                    //Eigen::Vector3f v1 ;
                    //Eigen::Vector3f v2 ;

                    //cout << "After  abs" << endl;
                    //ROS_INFO("depth rows:%d ; cols:%d", depth.rows, depth.cols
                    uchar* lastFramePtr = lastFrame.ptr<uchar>(rows) ;
                    uchar* lastFrameCol = &lastFramePtr[cols] ;

                    uchar* currentFramePtr = currentFrame.ptr<uchar>(warpPt.y) ;
                    uchar* currentFrameCol = &currentFramePtr[warpPt.x] ;

                    //const uint16_t* lastDepthPtr = lastCloud.ptr<uint16_t>(cols);
                    //const uint16_t* currentDepthPtr = cloud
                    if( imageDiff > threshod ){
                        diffFrame.at<unsigned char>(warpPt.y ,warpPt.x) = 255 ;

                        //lastDepthValue = isnan( lastCloud.at( cols,rows).z) ? 20 : lastCloud.at(cols,rows).z ;
                        //depthValue = isnan( cloud->at(warpPt.x, warpPt.y).z) ? 20 : cloud->at(warpPt.x, warpPt.y).z ;
                        lastDepthValue = isnan( lastCloud.at(rows*lastCloud.width + cols).z) ? 20 : lastCloud.at(rows*lastCloud.width + cols).z ;
                        depthValue = isnan( cloud->at(warpPt.y*cloud->width + warpPt.x).z) ? 20 : cloud->at(warpPt.y*cloud->width + warpPt.x).z ;

                        if( lastDepthValue - depthValue > 0.2 && lastDepthValue<20 ){
                            *currentFrameCol = 255 ;
                            //currentFrame.at<unsigned char>(warpPt.y ,warpPt.x) = 255 ;
                            //current.at<unsigned char>(warpPt.y ,warpPt.x) = 255 ;
                            //v1 << cloud->at(warpPt.x, warpPt.y).x , cloud->at(warpPt.x, warpPt.y).y ,cloud->at(warpPt.x, warpPt.y).z ;
                            //current_coordinate.push_back(v1) ;

                        }else if( depthValue -lastDepthValue > 0.2 && depthValue <20 ){
                            *lastFrameCol = 255 ;
                            //lastFrame.at<unsigned char>(rows, cols) = 255 ;
                            //last.at<unsigned char>(rows, cols) = 255 ;
                            //v2 << lastCloud.at( cols,rows).x , lastCloud.at( cols,rows).y , lastCloud.at( cols,rows).z ;
                            //previous_coordinate.push_back(v2) ;

                        }else{
                            continue ;
                        }

                    }else{
                        continue ;
                    }
                }else{
                    continue ;
                }
            }
        }
        ros::Time timeend1 = ros::Time::now();
        ros::Duration time2 = timeend1 - timeend;
        double timecost1 = time2.toSec();

       // static int num = 1 ;




        #ifdef DEBUG_WRITE
        std::stringstream ss1;
        ss1 << "differenceImage" << num1 << ".jpg";
        cv::imwrite( ss1.str(), diffFrame ) ;
        #endif DEBUG_WRITE

        #ifdef DEBUG_WRITE
        std::stringstream ss2;
        ss2 << "lastFrame" << num1 << ".jpg";
        cv::imwrite( ss2.str(), lastFrame ) ;
        #endif DEBUG_WRITE

        #ifdef DEBUG_WRITE
        std::stringstream ss3;
        ss3 << "currentFrame" << num1 << ".jpg";
        cv::imwrite( ss3.str(),currentFrame ) ;
        #endif DEBUG_WRITE

        #ifdef DEBUG_WRITE
        pcl::PCDWriter writer ;
        std::stringstream ss4;
        ss4 << "cloud_filtered_" << num1  << ".pcd" ;
        writer.write<pcl::PointXYZRGB>( ss4.str(), *cloud, false ) ;
        #endif DEBUG_WRITE




        //num++ ;

        //cout << "difference  Time2 : " << 1000000*timecost1 << endl;


        //cv::imshow("lastFrame",lastFrame);
        //cv::waitKey(5);
        #ifdef VIEW
        cv::imshow("currentFrame",currentFrame) ;
        cv::waitKey(5);
        #endif VIEW
        //cv::imshow("diffFrame", diffFrame) ;
        //cv::waitKey(5);

        //if(cv::waitKey(1) > 0){
        //    exit(0);
        //}

        lastBlurImage = BlurImage2 ;
        lastCloud = *cloud ;
        //lastDepth = depth ;
        //cv::threshold( diff_image, binary_image, threshod_binary, 255, cv::THRESH_BINARY );
    }
}

pcl::PointXYZ MovingObjectFilter::projectDepthTo3D(
        const cv::Mat & depthImage,
        float x, float y,
        float cx, float cy,
        float fx, float fy,
        bool smoothing,
        float maxZError)
{
    //UASSERT(depthImage.type() == CV_16UC1 || depthImage.type() == CV_32FC1);

    pcl::PointXYZ pt;
    float bad_point = std::numeric_limits<float>::quiet_NaN ();

    int u = int(x+0.5f);
    int v = int(y+0.5f);

    if(!(u >=0 && u<depthImage.cols && v >=0 && v<depthImage.rows))
    {
        //UERROR("!(x >=0 && x<depthImage.cols && y >=0 && y<depthImage.rows) cond failed! returning bad point. (x=%f (u=%d), y=%f (v=%d), cols=%d, rows=%d)",
        //		x,u,y,v,depthImage.cols, depthImage.rows);
        pt.x = pt.y = pt.z = bad_point;
        return pt;
    }

    bool isInMM = depthImage.type() == CV_16UC1; // is in mm?


    // Inspired from RGBDFrame::getGaussianMixtureDistribution() method from
    // https://github.com/ccny-ros-pkg/rgbdtools/blob/master/src/rgbd_frame.cpp
    // Window weights:
    //  | 1 | 2 | 1 |
    //  | 2 | 4 | 2 |
    //  | 1 | 2 | 1 |
    int u_start = std::max(u-1, 0);
    int v_start = std::max(v-1, 0);
    int u_end = std::min(u+1, depthImage.cols-1);
    int v_end = std::min(v+1, depthImage.rows-1);
     const uint16_t *depthPtr = depthImage.ptr<uint16_t>( v ) ;
     const uint16_t &dv = depthPtr[u] ;

    //float depth = isInMM?(float)depthImage.at<uint16_t>(v,u)*0.001f:depthImage.at<float>(v,u);
    float depth = isInMM?(float) dv*0.001f : dv ;
    if(depth!=0.0f && uIsFinite(depth))
    {
        if(smoothing)
        {
            float sumWeights = 0.0f;
            float sumDepths = 0.0f;
            for(int uu = u_start; uu <= u_end; ++uu)
            {
                for(int vv = v_start; vv <= v_end; ++vv)
                {
                    if(!(uu == u && vv == v))
                    {
                        //float d = isInMM?(float)depthImage.at<uint16_t>(vv,uu)*0.001f:depthImage.at<float>(vv,uu);
                        float d = isInMM ? (float)dv*0.001f : dv ;
                        // ignore if not valid or depth difference is too high
                        if(d != 0.0f && uIsFinite(d) && fabs(d - depth) < maxZError)
                        {
                            if(uu == u || vv == v)
                            {
                                sumWeights+=2.0f;
                                d*=2.0f;
                            }
                            else
                            {
                                sumWeights+=1.0f;
                            }
                            sumDepths += d;
                        }
                    }
                }
            }
            // set window weight to center point
            depth *= 4.0f;
            sumWeights += 4.0f;

            // mean
            depth = (depth+sumDepths)/sumWeights;
        }

        // Use correct principal point from calibration
        cx = cx > 0.0f ? cx : float(depthImage.cols/2) - 0.5f; //cameraInfo.K.at(2)
        cy = cy > 0.0f ? cy : float(depthImage.rows/2) - 0.5f; //cameraInfo.K.at(5)

        // Fill in XYZ
        pt.x = (x - cx) * depth / fx ;
        pt.y = (y - cy) * depth / fy ;
        pt.z = depth;
    }
    else
    {
        pt.x = pt.y = pt.z = bad_point;
    }
    return pt;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr MovingObjectFilter::cloudFromDepthRGB(
                const cv::Mat & imageRgb,
                const cv::Mat & imageDepth,
                float cx, float cy,
                float fx, float fy,
                int decimation)
{
    //UASSERT(imageRgb.rows == imageDepth.rows && imageRgb.cols == imageDepth.cols);
    //UASSERT(!imageDepth.empty() && (imageDepth.type() == CV_16UC1 || imageDepth.type() == CV_32FC1));
    assert(imageDepth.type() == CV_16UC1);
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    if( decimation < 1 ){
        return cloud;
    }

    bool mono;
    if(imageRgb.channels() == 3){// BGR
        mono = false;
    }else if(imageRgb.channels() == 1){ // Mono
        mono = true;
    }else{
        return cloud;
    }

    //cloud.header = cameraInfo.header;
    cloud->height = imageDepth.rows/decimation;
    cloud->width  = imageDepth.cols/decimation;
    cloud->is_dense = false;
    cloud->resize(cloud->height * cloud->width);
    pcl::PointCloud<pcl::PointXYZRGB>::iterator pc_iter = cloud->begin() ;
    //timeval start , end ;
    //gettimeofday( &start, NULL ) ;

    for(int h = 0; h < imageDepth.rows && h/decimation < (int)cloud->height; h+=decimation){
        const cv::Vec3b *rgbPtr = imageRgb.ptr<cv::Vec3b>( h ) ;
        for(int w = 0; w < imageDepth.cols && w/decimation < (int)cloud->width; w+=decimation){
            pcl::PointXYZRGB &pt = *pc_iter++ ;
            const cv::Vec3b& col = rgbPtr[w];
            //pcl::PointXYZRGB & pt = cloud->at((h/decimation)*cloud->width + (w/decimation));
            if(!mono){
                pt.b = col[0] ;//imageRgb.at<cv::Vec3b>(h,w)[0];
                pt.g = col[1] ;//imageRgb.at<cv::Vec3b>(h,w)[1];
                pt.r = col[2] ;//imageRgb.at<cv::Vec3b>(h,w)[2];
            }else{
                //unsigned char v = imageRgb.at<unsigned char>(h,w);
                pt.b = col[0] ;
                pt.g = col[1] ;
                pt.r = col[2] ;
            }

            pcl::PointXYZ ptXYZ = this->projectDepthTo3D(imageDepth, w, h, cx, cy, fx, fy, false);
            pt.x = ptXYZ.x;
            pt.y = ptXYZ.y;
            pt.z = ptXYZ.z;
        }
    }
//    gettimeofday( &end, NULL ) ;
//    long long time ;
//    time = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)  ;
//    cout << "The time of calculating PointCloud = " << time << endl;
    //if(!result_viewer.wasStopped()){
    //result_viewer.showCloud(cloud) ;
    //}
    return cloud;
}
cv::Mat MovingObjectFilter::pcl_segmentation( cloud_type::ConstPtr cloud , const cv::Mat &image , const cv::Mat &depthImage, float cx, float cy, float fx, float fy ){

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_f (new pcl::PointCloud<pcl::PointXYZRGB>);

    // Step 1: Filter out NaNs from data
    //ros::Time last = ros::Time::now() ;
    /*
    pcl::PassThrough<pcl::PointXYZRGB> pass ;
    pass.setInputCloud (cloud);
    pass.setFilterFieldName ("z");
    pass.setFilterLimits (0.5, 6.0);
    pass.filter (*cloud_filtered);
    */

    //
    //pass.setInputCloud (cloud_filtered);
    //pass.setFilterFieldName ("x");
    //pass.setFilterLimits (-4.0, 4.0);
    //pass.setFilterLimitsNegative (true);
    //pass.filter (*cloud_filtered);

    //pass.setInputCloud (cloud_filtered);
    //pass.setFilterFieldName ("y");
    //pass.setFilterLimits (-4.0, 4.0);
    //pass.filter (*cloud_filtered);
    //

    // Step 2: Filter out statistical outliers*****************influence the speed

    //pcl::StatisticalOutlierRemoval<pcl::PointXYZRGB> sor ;
    //sor.setInputCloud(cloud_filtered) ;
    //sor.setMeanK(50) ;
    //sor.setStddevMulThresh(1.0) ;
    //sor.filter(*cloud_filtered) ;

    //Step 3: Downsample the point cloud (to save time in the next step)
    timeval start , end, start1, end1, start2, end2 ;
    long long time , time1, time2;

    gettimeofday( &start, NULL ) ;

    //spend time:50ms
    pcl::PCDWriter writer ;
    pcl::VoxelGrid<pcl::PointXYZRGB> downSampler;
    downSampler.setInputCloud (cloud->makeShared());
    downSampler.setLeafSize (0.02f, 0.02f, 0.02f);
    downSampler.filter (*cloud_filtered);

    //if(!result_viewer.wasStopped()){
    //    result_viewer.showCloud(cloud_filtered->makeShared()) ;
    //}

    gettimeofday( &end, NULL ) ;
    time = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec)  ;
    #ifdef  TIME
    cout << "Downsampler Time =  " << time << endl;
    #endif  TIME

    #ifdef DEBUG
    static int num1=0 ;
    std::stringstream ss1;
    ss1 << "cloud_" << num1  << ".pcd" ;
    writer.write<pcl::PointXYZRGB>(ss1.str(), *cloud_filtered,false) ;
    
    #endif DEBUG

    gettimeofday( &start1, NULL ) ;
    // Step 4: Remove the ground plane using RANSAC
    // Spend time : 10ms(max)
    pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
    pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
    pcl::SACSegmentation<pcl::PointXYZRGB> seg;
    //seg.setOptimizeCoefficients (true); // Optional

    seg.setModelType (pcl::SACMODEL_PERPENDICULAR_PLANE);//SACMODEL_PERPENDICULAR_PLANE
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setDistanceThreshold (0.1); //10cm
    seg.setMaxIterations(300);//300
    seg.setAxis(Eigen::Vector3f(0.0, 1.0, 0.0));
    seg.setEpsAngle (pcl::deg2rad (8.0));//8 deg

    //seg.setModelType (pcl::SACMODEL_PLANE);
    //seg.setMethodType (pcl::SAC_RANSAC);
    //seg.setMaxIterations(1000);
    //seg.setDistanceThreshold (0.08);
    /*
    seg.setMethodType (pcl::SACMODEL_NORMAL_PARALLEL_PLANE) ;
    seg.setAxis(Eigen::Vector3f(0,0,1));
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setEpsAngle (0);
    seg.setDistanceThreshold (0.01); //1cm
    */
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_plane (new pcl::PointCloud<pcl::PointXYZRGB> ()) ;
    pcl::PointCloud<pcl::PointXYZRGB> plane  ;
    pcl::ExtractIndices<pcl::PointXYZRGB> extract;
    int  nr_points = (int) cloud_filtered->points.size ();
    while(cloud_filtered->points.size () > 0.4 * nr_points){
        seg.setInputCloud (cloud_filtered->makeShared());
        seg.segment (*inliers, *coefficients);
        if (inliers->indices.size () == 0){
            std::cout << "Could not estimate a planar model for the given dataset." << std::endl;
            break;
        }
        cout << "a = " << coefficients->values.at(0) << " ;b = " << coefficients->values.at(1) << " ;c = " << coefficients->values.at(2)<< " ;d = " <<  coefficients->values.at(3) << endl ;

        extract.setInputCloud (cloud_filtered->makeShared());
        // Step 4.1: Extract the points that lie in the ground plane
        extract.setIndices (inliers) ;
        extract.setNegative (false) ;
        extract.filter (*cloud_plane) ;
        plane += *cloud_plane ;

        // Step 4.2: Extract the points that are objects(i.e. are not in the ground plane)
        extract.setNegative (true) ;
        extract.filter ( *cloud_f ) ;
        *cloud_filtered = *cloud_f ;
        break ;
    }
    //if(!result_viewer.wasStopped()){
    //    result_viewer.showCloud(cloud_filtered->makeShared()) ;
    //}


    #ifdef DEBUG
    std::stringstream ss2;
    ss2 << "cloud_filtered_" << num1  << ".pcd" ;
    writer.write<pcl::PointXYZRGB>(ss2.str(), *cloud_filtered,false) ;
    #endif DEBUG

    //#ifdef DEBUG
    //std::stringstream ss3;
    //ss3 << "plane_" << num1  << ".pcd" ;
    //writer.write<pcl::PointXYZRGB>(ss3.str(), plane,false) ;
    //num1++ ;
    //#endif DEBUG

    gettimeofday( &end1, NULL ) ;
    time1 = 1000000*(end1.tv_sec - start1.tv_sec) + (end1.tv_usec - start1.tv_usec)  ;

    #ifdef TIME
    cout << "Removing Planar Time =  " << time1 << endl;
    #endif TIME

    gettimeofday( &start2, NULL ) ;
    // Step 5: EuclideanCluster Extract the moving objects
    pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZRGB>);
    tree->setInputCloud (cloud_filtered->makeShared());

    std::vector<pcl::PointIndices> cluster_indices ;
    pcl::EuclideanClusterExtraction<pcl::PointXYZRGB> ec ;
    ec.setClusterTolerance (0.06) ; // 4cm
    ec.setMinClusterSize (100) ;
    ec.setMaxClusterSize (50000) ;
    ec.setSearchMethod (tree) ;
    ec.setInputCloud (cloud_filtered->makeShared()) ;
    ec.extract (cluster_indices) ;
    //ros::Time now = ros::Time::now() ;
    //ros::Duration time = now - last ;
    //cout << "Time : " << time.nsec << endl;

    //cout << "The size of cluster_indices : " << cluster_indices.size() << endl;

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_cluster (new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr dynamic_object (new pcl::PointCloud<pcl::PointXYZRGB>);
    //double minX(0.0), minY(0.0), minZ(0.0), maxX(0.0), maxY(0.0), maxZ(0.0) ;
    //point_type cluster_point ;
    //pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_cluster (new pcl::PointCloud<pcl::PointXYZRGB>);
    static int num = 0 ;
    int j = 0 ;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr static_object(new pcl::PointCloud<pcl::PointXYZRGB>)  ;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr object(new pcl::PointCloud<pcl::PointXYZRGB>)  ;
    *static_object = plane ;

    //for view clusters
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cluster(new pcl::PointCloud<pcl::PointXYZRGB>) ;

    int num_dy_object = 0;
    cv::Mat imageMono ;
    //convert to grayscale

    if(image.channels() > 1){
       cv::cvtColor(image, imageMono, cv::COLOR_BGR2GRAY) ;
    }else{
       imageMono = image ;
    }
    dynamicImage = imageMono ;

    for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin (); it != cluster_indices.end (); ++it){
        for (std::vector<int>::const_iterator pit = it->indices.begin (); pit != it->indices.end (); pit++)
            cloud_cluster->push_back( cloud_filtered->points[*pit]) ;

        cloud_cluster->width = cloud_cluster->points.size () ;
        cloud_cluster->height = 1 ;
        cloud_cluster->is_dense = true ;
        cloud_cluster->sensor_origin_ = cloud_filtered->sensor_origin_ ;
        cloud_cluster->sensor_orientation_ = cloud_filtered->sensor_orientation_ ;
        *cluster += *cloud_cluster ;

        #ifdef DEBUG
        std::stringstream ss;
        ss << "cluster_cloud_" << num << "_" << j << ".pcd" ;
        writer.write<pcl::PointXYZRGB>(ss.str(), *cloud_cluster,false) ;
        #endif DEBUG
        //Extract the objects

        if(image_extract_cluster( cloud_cluster->makeShared(), cloud->makeShared(), image, cx, cy, fx, fy, num , j)) {
            *object = objectFromOriginalCloud( cloud_cluster,cloud ) ;
            //cout << "The size of object: " << object->size() << endl ;
            *dynamic_object += *object;

            #ifdef DEBUG
            std::stringstream ss;
            ss << "cluster_dynamic_" << num << "_" << j << ".pcd" ;
            writer.write<pcl::PointXYZRGB>(ss.str(), *cloud_cluster,false) ;
            #endif DEBUG

            //object->clear();
            //
            /*
            if(!cloud_viewer.wasStopped()){
                cloud_viewer.showCloud(cloud_cluster);
                num++ ;
                cout << "num = "  << num << endl;
            }*/
            num_dy_object ++;
        }else{
            //*static_object += *cloud_cluster ;
            //continue ;
        }
        j++ ;
        num++ ;
        #ifdef DEBUG
        cout << "j = " << j << endl ;
        #endif DEBUG

        cloud_cluster->clear();
        //std::cout << "PointCloud representing the Cluster: " << cloud_cluster->points.size () << " data points." << std::endl;
    }

    cout << "dynamic objects:  " << num_dy_object << endl;

    //if(!result_viewer.wasStopped()){
    //    result_viewer.showCloud(cluster->makeShared()) ;
    //}

    #ifdef VIEW_RESULT
    cv::namedWindow("dynamicObjectDetection") ;
    cv::imshow("dynamicObjectDetection",  dynamicImage) ;
    cv::waitKey(5) ;
    #endif VIEW_RESULT

    gettimeofday( &end2, NULL ) ;
    time2 = 1000000*(end2.tv_sec - start2.tv_sec) + (end2.tv_usec - start2.tv_usec)  ;

    #ifdef TIME
    cout << "cluster Time =  " << time2 << endl;
    #endif TIME


    dynamic_object->width = dynamic_object->points.size () ;
    dynamic_object->height = 1 ;
    dynamic_object->is_dense = true ;
    dynamic_object->resize(dynamic_object->height * dynamic_object->width);


    //cout << "The size of object =  " << object->size() << endl ;
//    if(!result_viewer.wasStopped()){
//      result_viewer.showCloud(dynamic_object) ;
//    }
    cv::Mat rest ;
    rest = getDepth( dynamic_object, depthImage, cx, cy, fx, fy );
    this->getImage( dynamic_object, image, cx, cy, fx, fy );



    #ifdef DEBUG
    num++ ;
    #endif DEBUG

    return rest ;

    //cv::Mat resultImage;
    //resultImage = this->bgrFromCloud( *static_object,false) ;
    //cv::namedWindow("resultImage");
    //cv::imshow("resultImage", resultImage);
    //cv::waitKey(1);

    //ros::Duration next = ros::Time::now() - now ;
    //cout << "next = " << next.nsec << endl ;
    //viewer.showCloud(cloud_cluster);
    //cloud_viewer->showCloud(cloud_plane) ;
}
bool MovingObjectFilter::image_extract_cluster( cloud_type::ConstPtr cluster_cloud, cloud_type::ConstPtr cloud, const cv::Mat &image ,  float cx, float cy, float fx, float fy , int num , int j){
    //pcl::PointCloud<pcl::PointXYZ>  pt ;
    cv::Mat imageMono ;
    //convert to grayscale

    if(image.channels() > 1){
       cv::cvtColor(image, imageMono, cv::COLOR_BGR2GRAY) ;
    }else{
       imageMono = image ;
    }
    double minX(1000.0), minY(1000.0), minZ(1000.0), maxX(0.0), maxY(0.0) , maxZ(0.0) ;
    double averageZ = 0.0 ;
    cv::Mat cluster( 480,640, CV_8UC1, cv::Scalar(0) );
    float x0 = 0.0 , y0 = 0.0 , z = 0.0 ;
    int x = 0 , y = 0 ;
    int count = 0 ;
    cv::Mat result = imageMono ;
    for( int i = 0 ; i < cluster_cloud->points.size(); i++){
        z = cluster_cloud->points[i].z ;
        x0 = (cluster_cloud->points[i].x * fx)/z + cx ;
        y0 = (cluster_cloud->points[i].y * fy)/z + cy ;
        x = cvRound(x0) ;
        y = cvRound(y0) ;
        cluster.at<unsigned char>(y, x) = 255 ;
        //result.at<unsigned char>(y,x) = 255;

        if( x < minX )  minX = x ;
        if( y < minY )  minY = y ;
        if( z < minZ )  minZ = z ;

        if( x > maxX )  maxX = x ;
        if( y > maxY )  maxY = y ;
        if( z > maxZ )  maxZ = z ;

        averageZ += z ;
    }

    averageZ = averageZ / cluster_cloud->points.size() ;

    //cv::namedWindow("cluster") ;
    //cv::imshow("cluster", cluster) ;
    //cv::waitKey(1) ;
    //averageZ = averageZ / cloud->points.size()

    const uchar * currentImagePtr ;
    for(int row = 0 ; row < currentFrame.rows; row++){
        currentImagePtr = currentFrame.ptr<uchar>(row) ;
        for(int col= 0; col < currentFrame.cols; col++){
            const uchar &currentCol = currentImagePtr[col] ;
            if( currentCol == 255){
            //if( currentFrame.at<unsigned char>(row,col) == 255){
                if(row > minY && row<maxY && col>minX && col<maxX){
                    if(cloud->at(row*cloud->width + col).z > minZ && cloud->at(row*cloud->width + col).z<maxZ ){
                        //result.at<unsigned char>(row,col) = 255;
                        dynamicImage.at<unsigned char>(row,col) = 255;
                        count ++ ;
                    }
                }
            }
        }
    }


    //std::stringstream ss;
    //ss << "clusterImage_" << num << "_" << j << ".jpg";
    //cv::imwrite( ss.str(), result ) ;


    cout << "count= " << count << endl ;


    
    
    if(count > 3000){
        cv::line( dynamicImage, cvPoint(maxX, maxY),cvPoint(minX, maxY),CV_RGB(255,0,0),2,CV_AA,0) ;
        cv::line( dynamicImage, cvPoint(minX, maxY),cvPoint(minX, minY),CV_RGB(255,0,0),2,CV_AA,0) ;
        cv::line( dynamicImage, cvPoint(minX, minY),cvPoint(maxX, minY),CV_RGB(255,0,0),2,CV_AA,0) ;
        cv::line( dynamicImage, cvPoint(maxX, minY),cvPoint(maxX, maxY),CV_RGB(255,0,0),2,CV_AA,0) ;
        double density = count/((maxY-minY)*(maxX-minX)) ;
        cout << "density = " << density << endl ;
        //#ifdef VIEW
        //cv::namedWindow("result") ;
        //cv::imshow("result", result) ;
        //cv::waitKey(5) ;
        //#endif VIEW
        cout << "The count of dynamic objects =  " << count << endl ;
        //if( density > 0.07){
        return true ;
        //}else{
        //  return false ;
        //}

    }else{
        return false ;
    }

    /*
    count ++ ;
    cout << "count= " << count << endl ;
    std::stringstream ss;
    ss << "Resultimage" << count << ".jpg";
    cv::namedWindow("cluster") ;
    cv::imshow("cluster", cluster) ;
    imwrite( ss.str(),cluster );
    cv::Mat new_cluster( 480,640, CV_8UC1, cv::Scalar(0) );
    cluster = new_cluster ;
    cv::waitKey(1) ;
    */

}
pcl::PointCloud<pcl::PointXYZRGB> MovingObjectFilter::objectFromOriginalCloud(cloud_type::ConstPtr clusterCloud, cloud_type::ConstPtr cloud){
    double average_z = 0.0 ;
    for(int i = 0; i < clusterCloud->size(); i++){
        average_z += clusterCloud->points[i].z ;
    }
    average_z = average_z / clusterCloud->size() ;
    cloud_type::Ptr cluster (new cloud_type)  ;
    cluster->points.resize(clusterCloud->size());

    for(int i = 0; i < clusterCloud->size();i++ ){
        cluster->points[i].x = clusterCloud->points[i].x ;
        cluster->points[i].y = clusterCloud->points[i].y ;
        cluster->points[i].z = average_z ;
        cluster->points[i].b = clusterCloud->points[i].b ;
        cluster->points[i].g = clusterCloud->points[i].g ;
        cluster->points[i].r = clusterCloud->points[i].r ;
    }
    cluster->width = cluster->points.size ();
    cluster->height = 1;
    cluster->is_dense = true;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr hull_points (new pcl::PointCloud<pcl::PointXYZRGB> ());
    pcl::PointIndices::Ptr cloud_indices (new pcl::PointIndices);
    pcl::ConcaveHull<pcl::PointXYZRGB> hull ;
    hull.setInputCloud (cluster);
    hull.setAlpha(0.2);
    hull.reconstruct (*hull_points);
    double z_min = -0.2, z_max = 0.2; // we want the points above the plane, no farther than 5 cm from the surface
    pcl::ExtractIndices<pcl::PointXYZRGB> extract;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr objects(new pcl::PointCloud<pcl::PointXYZRGB>);

    if (hull.getDimension () == 2){
        pcl::ExtractPolygonalPrismData<pcl::PointXYZRGB> prism;
        prism.setInputCloud (cloud);
        prism.setInputPlanarHull (hull_points);
        prism.setHeightLimits (z_min, z_max);
        prism.segment (*cloud_indices);
        if(cloud_indices->indices.size() == 0){
            cout << "Cloud not find objects" << endl ;

        }else{
            cout << "indices.size() = " << cloud_indices->indices.size() << endl ;
        }
        extract.setInputCloud(cloud);
        extract.setIndices(cloud_indices);
        extract.filter(*objects);

        objects->width  = objects->points.size() ;
        objects->height = 1 ;
        objects->is_dense = true ;
        objects->resize(objects->height * objects->width);

        //  if(!result_viewer.wasStopped()){
        //      result_viewer.showCloud(objects) ;
        //  }

    }else{
        PCL_ERROR ("The input cloud does not represent a planar surface.\n");
    }
    return *objects ;

}

cv::Mat  MovingObjectFilter::getDepth(cloud_type::ConstPtr cloud , const cv::Mat &depthImage, float cx, float cy, float fx, float fy){
    //convert to grayscale
    /*
    cv::Mat imageMono ;
    if(image.channels() > 1){
       cv::cvtColor(image, imageMono, cv::COLOR_BGR2GRAY) ;
    }else{
       imageMono = image ;
    }
    */
    cout << "Enter calculating depthImage" << endl ;
    cv::Mat restDepth = depthImage ;
    float x0 = 0.0 , y0 = 0.0 , z = 0.0 ;
    int x = 0 , y = 0 ;
    //cv::Mat rest( 480,640, CV_8UC1, cv::Scalar(0) );
    for( int i = 0 ; i < cloud->points.size(); i++){
        z = cloud->points[i].z ;
        x0 = (cloud->points[i].x * fx)/z + cx ;
        y0 = (cloud->points[i].y * fy)/z + cy ;
        x = cvRound(x0) ;
        y = cvRound(y0) ;
        //uint16_t *restPtr = restImage.ptr<uint16_t>(y) ;
        //uint16_t & restCol = restPtr[x] ;
        //restCol = 0 ;
        restDepth.at<uint16_t>(y, x) = 0 ;
        //result.at<unsigned char>(y,x) = 255;
    }
    /*
    for(int row = 0 ; row < rest.rows; row++){
        for(int col= 0; col < rest.cols; col++){
            if(rest.at<unsigned char>(y, x) == 255 )
                restImage.at<unsigned char>(row,col) = 255;
        }
    }*/
    //cv::namedWindow("restCloud") ;
    //cv::imshow("resCloud", restDepth) ;
    //cv::waitKey(1) ;
    return restDepth ;

}
void  MovingObjectFilter::getImage(cloud_type::ConstPtr cloud , const cv::Mat &image, float cx, float cy, float fx, float fy){
    //convert to grayscale

    cv::Mat imageMono ;
    if(image.channels() > 1){
       cv::cvtColor(image, imageMono, cv::COLOR_BGR2GRAY) ;
    }else{
       imageMono = image ;
    }

    cv::Mat restImage = imageMono ;
    //static int num = 0 ;

    #ifdef DEBUG_LOOP
    std::stringstream ss;
    ss << "currentImage" << num1 << ".jpg";
    cv::imwrite( ss.str(),restImage ) ;
    #endif DEBUG_LOOP

    float x0 = 0.0 , y0 = 0.0 , z = 0.0 ;
    int x = 0 , y = 0 ;
    //cv::Mat rest( 480,640, CV_8UC1, cv::Scalar(0) );
    for( int i = 0 ; i < cloud->points.size(); i++){
        z = cloud->points[i].z ;
        x0 = (cloud->points[i].x * fx)/z + cx ;
        y0 = (cloud->points[i].y * fy)/z + cy ;
        x = cvRound(x0) ;
        y = cvRound(y0) ; 
        restImage.at<unsigned char>(y, x) = 255 ;
        //result.at<unsigned char>(y,x) = 255;
    }
    /*
    for(int row = 0 ; row < rest.rows; row++){
        for(int col= 0; col < rest.cols; col++){
            if(rest.at<unsigned char>(y, x) == 255 )
                restImage.at<unsigned char>(row,col) = 255;
        }
    }*/

    #ifdef DEBUG_LOOP
    std::stringstream ss1;
    ss1 << "restImage" << num1 << ".jpg";
    cv::imwrite( ss1.str(),restImage ) ;
    #endif DEBUG_LOOP

    #ifdef VIEW_RESULT
    cv::namedWindow("restImage") ;
    cv::imshow("restImage", restImage) ;
    cv::waitKey(5) ;
    #endif VIEW_RESULT

    #ifdef DEBUG
    static int num = 0 ;
    std::stringstream ss4;
    ss4 << "rest" << num << ".jpg";
    cv::imwrite( ss4.str(), restImage ) ;
    num++ ;
    #endif
    num1++ ;


    //return restImage ;

}


cv::Mat MovingObjectFilter::bgrFromCloud(const pcl::PointCloud<pcl::PointXYZRGB> & cloud, bool bgrOrder)
{
    cv::Mat frameBGR = cv::Mat(cloud.height,cloud.width,CV_8UC3);

    for(unsigned int h = 0; h < cloud.height; h++)
    {
        for(unsigned int w = 0; w < cloud.width; w++)
        {
            if(bgrOrder)
            {
                frameBGR.at<cv::Vec3b>(h,w)[0] = cloud.at(h*cloud.width + w).b;
                frameBGR.at<cv::Vec3b>(h,w)[1] = cloud.at(h*cloud.width + w).g;
                frameBGR.at<cv::Vec3b>(h,w)[2] = cloud.at(h*cloud.width + w).r;
            }
            else
            {
                frameBGR.at<cv::Vec3b>(h,w)[0] = cloud.at(h*cloud.width + w).r;
                frameBGR.at<cv::Vec3b>(h,w)[1] = cloud.at(h*cloud.width + w).g;
                frameBGR.at<cv::Vec3b>(h,w)[2] = cloud.at(h*cloud.width + w).b;
            }
        }
    }
    return frameBGR;
}



// return float image in meter
cv::Mat MovingObjectFilter::depthFromCloud(
        const pcl::PointCloud<pcl::PointXYZRGB> & cloud,
        float & fx,
        float & fy,
        bool depth16U)
{
    cv::Mat frameDepth = cv::Mat(cloud.height,cloud.width,depth16U?CV_16UC1:CV_32FC1);
    fx = 0.0f; // needed to reconstruct the cloud
    fy = 0.0f; // needed to reconstruct the cloud
    for(unsigned int h = 0; h < cloud.height; h++)
    {
        for(unsigned int w = 0; w < cloud.width; w++)
        {
            float depth = cloud.at(h*cloud.width + w).z;
            if(depth16U)
            {
                depth *= 1000.0f;
                unsigned short depthMM = 0;
                if(depth <= (float)USHRT_MAX)
                {
                    depthMM = (unsigned short)depth;
                }
                frameDepth.at<unsigned short>(h,w) = depthMM;
            }
            else
            {
                frameDepth.at<float>(h,w) = depth;
            }

            // update constants
            if(fx == 0.0f &&
               uIsFinite(cloud.at(h*cloud.width + w).x) &&
               uIsFinite(depth) &&
               w != cloud.width/2 &&
               depth > 0)
            {
                fx = cloud.at(h*cloud.width + w).x / ((float(w) - float(cloud.width)/2.0f) * depth);
                if(depth16U)
                {
                    fx*=1000.0f;
                }
            }
            if(fy == 0.0f &&
               uIsFinite(cloud.at(h*cloud.width + w).y) &&
               uIsFinite(depth) &&
               h != cloud.height/2 &&
               depth > 0)
            {
                fy = cloud.at(h*cloud.width + w).y / ((float(h) - float(cloud.height)/2.0f) * depth);
                if(depth16U)
                {
                    fy*=1000.0f;
                }
            }
        }
    }
    return frameDepth;
}

