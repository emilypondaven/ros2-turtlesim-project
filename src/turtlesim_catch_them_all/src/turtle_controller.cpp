#include "rclcpp/rclcpp.hpp"
#include "turtlesim/msg/pose.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include <cmath>

using namespace std::placeholders;
using namespace std::chrono_literals;

class TurtleControllerNode : public rclcpp::Node
{
public:
    TurtleControllerNode() : Node("turtle_controller"), target_x_(8.0), target_y_(4.0)
    {
        pose_subscriber_ = this->create_subscription<turtlesim::msg::Pose>(
            "/turtle1/pose", 10,
            std::bind(&TurtleControllerNode::callbackPose, this, _1)
        );
        
        cmd_vel_publisher_ = create_publisher<geometry_msgs::msg::Twist>(
            "/turtle1/cmd_vel", 10
        );

        control_loop_timer_ = this->create_wall_timer(
            10ms, std::bind(&TurtleControllerNode::control_loop, this)
        );
    }
 
private:
    void callbackPose(const turtlesim::msg::Pose::SharedPtr msg) 
    {
        pose_ = msg;
    }

    void control_loop() 
    {
        if (!pose_) {
            return;
        }

        double dist_x = target_x_ - pose_->x;
        double dist_y = target_y_ - pose_->y;
        double distance = std::sqrt(dist_x * dist_x + dist_y * dist_y);

        auto cmd = geometry_msgs::msg::Twist();

        if (distance > 0.5) {
            // position
            cmd.linear.x = 2.0 * distance;

            //orientation
            double goal_theta = std::atan2(dist_y, dist_x);
            double diff = goal_theta - pose_->theta;

            if (diff > M_PI) {
                diff -= 2 * M_PI;
            } else if (diff < -M_PI) {
                diff += 2 * M_PI;
            }

            cmd.angular.z = 6.0 * diff;
        } else {
            // target reached
            cmd.linear.x = 0.0;
            cmd.angular.z = 0.0;
        }

        cmd_vel_publisher_->publish(cmd);
    }

    double target_x_;
    double target_y_;
    turtlesim::msg::Pose::SharedPtr pose_;
    rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr pose_subscriber_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_publisher_;
    rclcpp::TimerBase::SharedPtr control_loop_timer_;
};
 
int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TurtleControllerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}