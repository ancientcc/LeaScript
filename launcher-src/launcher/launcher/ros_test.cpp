#define GETTEXT_DOMAIN "launcher-lib"

#include <sstream>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <geometry_msgs/Twist.h>

#include <SDL_thread.h>
#include <SDL_log.h>
#include <SDL_timer.h>

// ========
#include <opencv2/opencv.hpp>
#include <Eigen/core>
#include <Eigen/Dense>
#include <ceres/ceres.h>

#include "rose_util.hpp"

// double 
struct CURVE_FITTING_COST 
{
    CURVE_FITTING_COST(double x, double y)
        : _x(x)
        , _y(y)
    {}

    template<typename T>
    bool operator()(const T* const abc, T* residual) const
    {
        // y - exp(ax^2 + bx + c)
        residual[0] = T(_y) - ceres::exp(abc[0] * T(_x) * T(_x) + abc[1] * T(_x) + abc[2]);

        // SDL_Log("_x: %.5f, _y: %.5f, abc(%.5f, %.5f, %.5f), residual:(%.5f)", 
        //    _x, _y, abc[0], abc[1], abc[2], residual[0]);
        return true;
    }

    double _x;
    double _y;
};

int test_slam()
{
    double ar = 1.0, br = 2.0, cr = 1.0;
    // double ar = 5, br = 19.987, cr = -12.32;
    double ae = 2.0, be = -1.0, ce = 5.0;
    int N = 100;
    double w_sigma = 1.0;
    double inv_sigma = 1.0 / w_sigma;
    cv::RNG rng;

    std::vector<double> x_data, y_data;
    for (int i = 0; i < N; i ++) {
        double x = i / 100.0;
        x_data.push_back(x);
        y_data.push_back(exp(ar * x * x + br * x + cr) + rng.gaussian(w_sigma * w_sigma));
    }

    double abc[3] = {ae, be, ce};

    ceres::Problem problem;
    for (int i = 0; i < N; i ++) {
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<CURVE_FITTING_COST, 1, 3>(
                new CURVE_FITTING_COST(x_data[i], y_data[i])),
            nullptr,
            abc
        );
    }

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_NORMAL_CHOLESKY;
    options.minimizer_progress_to_stdout = true;

    ceres::Solver::Summary summary;
    uint32_t t1 = SDL_GetTicks();
    ceres::Solve(options, &problem, &summary);
    uint32_t t2 = SDL_GetTicks();

    std::stringstream ss;
    ss << "message: " << summary.message.c_str() << "\n";
    ss << "solve time cost = " << (t2 - t1) << " ms. ";
    ss << "estimated a, b, c = ";
    for (auto a: abc) ss << a << " ";
    SDL_Log("%s", ss.str().c_str());
    return 1;
}


SDL_DPoint3 ceres_curve_fitting(const std::vector<SDL_DPoint>& xy_data)
{
    double ae = 2.0, be = -1.0, ce = 5.0;
    double abc[3] = {ae, be, ce};

    const int N = xy_data.size();

    const SDL_DPoint* xy_data_ptr = &xy_data[0];
    ceres::Problem problem;
    for (int i = 0; i < N; i ++) {
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<CURVE_FITTING_COST, 1, 3>(
                new CURVE_FITTING_COST(xy_data_ptr[i].x, xy_data_ptr[i].y)),
            nullptr,
            abc
        );
    }

    const bool verbose = false;

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_NORMAL_CHOLESKY;
    options.minimizer_progress_to_stdout = verbose;

    ceres::Solver::Summary summary;
    uint32_t t1 = SDL_GetTicks();
    ceres::Solve(options, &problem, &summary);
    uint32_t t2 = SDL_GetTicks();

    if (verbose) {
        std::stringstream ss;
        ss << "(1)message: " << summary.message.c_str() << "\n";
        ss << "solve time cost = " << (t2 - t1) << " ms. ";
        ss << "estimated a, b, c = ";
        for (auto a: abc) {
            ss << a << " ";
        }
        SDL_Log("%s", ss.str().c_str());
    }

    return SDL_DPoint3{abc[0], abc[1], abc[2]};
}

void test_ceres_curve_fitting()
{
    // double ar = 1.0, br = 2.0, cr = 1.0;
    double ar = 5, br = 19.987, cr = -12.32;
    int N = 100;
    double w_sigma = 1.0;
    double inv_sigma = 1.0 / w_sigma;
    cv::RNG rng;

    std::vector<SDL_DPoint> xy_data;
    for (int i = 0; i < N; i ++) {
        double x = i / 100.0;
        xy_data.push_back(SDL_DPoint{x, exp(ar * x * x + br * x + cr) + rng.gaussian(w_sigma * w_sigma)});
    }
    SDL_DPoint3 result = ceres_curve_fitting(xy_data);

    SDL_Log("{test_ceres_curve_fitting}real[a: %.6f b: %.6f c: %.6f] estimate[a: %.6f b: %.6f c: %.6f]", ar, br, cr, result.x, result.y, result.z);
}

#include <tf2_ros/transform_broadcaster.h>
#include <tf2/LinearMath/Quaternion.h>

#include <actionlib/client/simple_action_client.h>
#include <actionlib/server/simple_action_server.h>

static ros::CallbackQueue cbqueue;

int test__talker(bool& exit)
{
    // ROS节点初始化
	int argc = 0;
    ros::init(argc, nullptr, "talker");

    // 创建节点句柄
    ros::NodeHandle n;
	n.setCallbackQueue(&cbqueue);

    // 创建一个Publisher，发布名为chatter的topic，消息类型为std_msgs::String
    ros::Publisher chatter_pub = n.advertise<std_msgs::String>("chatter", 1000);

	// ros::Publisher chatter_pub1 = n.advertise<std_msgs::String>("chatter", 1000);

    // 设置循环的频率
    ros::Rate loop_rate(10);

    int count = 0;
    while (!exit && ros::ok())
    {
        // 初始化std_msgs::String类型的消息
		boost::shared_ptr<std_msgs::String> m(new std_msgs::String);
		std_msgs::String& msg = *m.get();

        std::stringstream ss;
        ss << "hello world " << count;
        msg.data = ss.str();

        // 发布消息
        ROS_INFO("%s", msg.data.c_str());
        // chatter_pub.publish(msg);
		chatter_pub.publish(m);

        // 循环等待回调函数
		cbqueue.callAvailable(ros::WallDuration());
        // ros::spinOnce();

        // 按照循环频率延时
        loop_rate.sleep();
        ++count;
    }

    return 0;
}

// 接收到订阅的消息后，会进入消息回调函数
void chatterCallback(const std_msgs::String::ConstPtr& msg)
{
    // 将接收到的消息打印出来
    ROS_INFO("[chatterCallback]I heard: [%s]", msg->data.c_str());
}

void chatterCallback1(const std_msgs::String::ConstPtr& msg)
{
    // 将接收到的消息打印出来
    ROS_INFO("[chatterCallback1]I heard: [%s]", msg->data.c_str());
}

void chatterCallback2(const geometry_msgs::Twist::ConstPtr& msg)
{
    // 将接收到的消息打印出来
    ROS_INFO("[chatterCallback2]I heard: [%.5f]", msg->angular.x);
}

int test__listener(bool& exit)
{
    // 初始化ROS节点
	int argc = 0;
    ros::init(argc, nullptr, "listener");

    // 创建节点句柄
    ros::NodeHandle n;
	n.setCallbackQueue(&cbqueue);

    // 创建一个Subscriber，订阅名为chatter的topic，注册回调函数chatterCallback
    ros::Subscriber sub = n.subscribe("chatter", 1000, chatterCallback);

	// ros::Subscriber sub1 = n.subscribe("chatter", 1000, chatterCallback1);

	// ros::Subscriber sub2 = n.subscribe("chatter2", 1000, chatterCallback2);

    // 循环等待回调函数
	ros::Rate loop_rate(10);
	while (!exit && ros::ok()) {
		// ros::spinOnce();
		cbqueue.callAvailable(ros::WallDuration());
		loop_rate.sleep();
	}

    return 0;
}

int test__transform(bool& exit)
{
	int argc = 0;
	ros::init(argc, nullptr, "");

	ros::CallbackQueue cbqueue;
	tf2_ros::TransformBroadcaster br(&cbqueue);

    geometry_msgs::TransformStamped transform;
    transform.transform.translation.x = 0;
    transform.transform.translation.y = 0;
    transform.transform.translation.z = 1;

/*
    {
        transform.transform.translation.x = 0.298734;
        transform.transform.translation.y = -0.000288;
        transform.transform.translation.z = 0.293431;
    }
*/
    {
        transform.transform.translation.x = 0.175586;
        transform.transform.translation.y = -0.000289;
        transform.transform.translation.z = 0.295848;
    }

    tf2::Quaternion q;
    // q.setRPY(-M_PI/4, -M_PI/6, M_PI/2);
    // Eigen::Vector3d(2.34394, -3.13551, 1.57701)

    // double rad[3] = {2.34394, 0, 1.57701}; // 2.34394, -3.13551, 1.57701
    // double rad[3] = {1.76394, -3.13991, 1.57932}; // 2.34394, -3.13551, 1.57701

    // {p:(0.335955, -0.000289, 0.093811) M:(0.773975(44.34552), 3.135378(179.64402), 1.576868(90.34791))}


    // double rad[3] = {0.773975, 3.135378, 1.576868}; // 2.34394, -3.13551, 1.57701
    // double rad[3] = {2.343937, -3.135513, 1.577013}; // 2.34394, -3.13551, 1.57701
    double rad[3] = {1.773948, -3.139832, 1.579307}; // 2.34394, -3.13551, 1.57701
    double deg[3] = {RAD2DEG(rad[0]), RAD2DEG(rad[1]), RAD2DEG(rad[2])};

    SDL_Log("test__transform, deg[%.5f, %.5f, %.5f]", deg[0], deg[1], deg[2]);

    q.setRPY(rad[0], rad[1], rad[2]);
    transform.transform.rotation.x = q.x();
    transform.transform.rotation.y = q.y();
    transform.transform.rotation.z = q.z();
    transform.transform.rotation.w = q.w();

    transform.header.frame_id = "base_link"; // world
    transform.child_frame_id = "test";

	int k = 0;
	while (!exit && ros::ok()) {
        transform.header.stamp = ros::Time::now();
		br.sendTransform(transform);
		k ++;
		SDL_Delay(1000);
	}

	return 0;
}
