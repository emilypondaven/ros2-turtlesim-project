#include "rclcpp/rclcpp.hpp"
#include "turtlesim/msg/pose.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "my_interfaces/msg/turtle.hpp"
#include "my_interfaces/msg/turtle_array.hpp"

#include <cmath>

using namespace std::placeholders;
using namespace std::chrono_literals;

class TurtleControllerNode : public rclcpp::Node
{
public:
    TurtleControllerNode() : Node("turtle_controller")
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

        alive_turtles_subscriber_ = this->create_subscription<my_interfaces::msg::TurtleArray>(
            "alive_turtles", 10,
            std::bind(&TurtleControllerNode::callbackAliveTurtles, this, _1)
        );
    }
 
private:
    void callbackPose(const turtlesim::msg::Pose::SharedPtr msg) 
    {
        pose_ = msg;
    }

    void control_loop() 
    {
        if (!pose_ || !turtle_to_catch_) {
            return;
        }

        double dist_x = turtle_to_catch_->x - pose_->x;
        double dist_y = turtle_to_catch_->y - pose_->y;
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

    void callbackAliveTurtles(const my_interfaces::msg::TurtleArray::SharedPtr msg)
    {
        if (!msg->turtles.empty()) {
            turtle_to_catch_ = std::make_shared<my_interfaces::msg::Turtle>(msg->turtles[0]);
        }
    }

    turtlesim::msg::Pose::SharedPtr pose_;
    my_interfaces::msg::Turtle::SharedPtr turtle_to_catch_;

    rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr pose_subscriber_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_publisher_;
    rclcpp::Subscription<my_interfaces::msg::TurtleArray>::SharedPtr alive_turtles_subscriber_;
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