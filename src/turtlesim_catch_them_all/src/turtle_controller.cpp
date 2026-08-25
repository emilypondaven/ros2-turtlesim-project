#include "rclcpp/rclcpp.hpp"
#include "turtlesim/msg/pose.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "my_interfaces/msg/turtle.hpp"
#include "my_interfaces/msg/turtle_array.hpp"
#include "my_interfaces/srv/catch_turtle.hpp"

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

        catch_turtle_client_ = this->create_client<my_interfaces::srv::CatchTurtle>(
            "catch_turtle"
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
            callCatchTurtleService(turtle_to_catch_->name);
            turtle_to_catch_ = NULL;
        }

        cmd_vel_publisher_->publish(cmd);
    }

    void callbackAliveTurtles(const my_interfaces::msg::TurtleArray::SharedPtr msg)
    {
        if (!msg->turtles.empty()) {
            // Find closest turtle
            // Closest turtle is one to catch
            my_interfaces::msg::Turtle closest_turtle;
            double closest_turtle_distance = 0.0;
            bool found = false;

            for (const auto &turtle : msg->turtles) {
                double dist_x = turtle.x - pose_->x;
                double dist_y = turtle.y - pose_->y;
                double distance = std::sqrt(dist_x * dist_x + dist_y * dist_y);

                if (!found || distance < closest_turtle_distance) {
                    closest_turtle = turtle;
                    closest_turtle_distance = distance;
                    found = true;
                }
            }

            turtle_to_catch_ = std::make_shared<my_interfaces::msg::Turtle>(closest_turtle);
        }
    }

    void callCatchTurtleService(const std::string &turtle_name)
    {
        while (!catch_turtle_client_->wait_for_service(1s))
        {
            RCLCPP_WARN(this->get_logger(), "Waiting for catch turtle service...");
        }

        auto request = std::make_shared<my_interfaces::srv::CatchTurtle::Request>();
        request->name = turtle_name;

        // Use of lambda function
        catch_turtle_client_->async_send_request(
            request,
            [this, request](rclcpp::Client<my_interfaces::srv::CatchTurtle>::SharedFuture future) {
                callbackCallCatchTurtleService(future, request);
            });
    }

    void callbackCallCatchTurtleService(
        rclcpp::Client<my_interfaces::srv::CatchTurtle>::SharedFuture future,
        my_interfaces::srv::CatchTurtle::Request::SharedPtr request)
    {
        auto response = future.get();
        if (!response->success) {
            RCLCPP_ERROR(this->get_logger(), "Turtle %s could not be removed", request->name.c_str());
        }
    }

    turtlesim::msg::Pose::SharedPtr pose_;
    my_interfaces::msg::Turtle::SharedPtr turtle_to_catch_;

    rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr pose_subscriber_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_publisher_;
    rclcpp::Subscription<my_interfaces::msg::TurtleArray>::SharedPtr alive_turtles_subscriber_;
    rclcpp::Client<my_interfaces::srv::CatchTurtle>::SharedPtr catch_turtle_client_;
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