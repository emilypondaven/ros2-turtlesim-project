#include "rclcpp/rclcpp.hpp"
#include "turtlesim/srv/spawn.hpp"
#include "turtlesim/srv/kill.hpp"
#include "my_interfaces/msg/turtle.hpp"
#include "my_interfaces/msg/turtle_array.hpp"
#include "my_interfaces/srv/catch_turtle.hpp"

#include <string>
#include <cstdlib>
#include <random>
#include <vector>
 
using namespace std::chrono_literals;
using namespace std::placeholders;

class TurtleSpawnerNode : public rclcpp::Node
{
public:
    TurtleSpawnerNode() : Node("turtle_spawner"), turtle_counter_(2), alive_turtles_({})
    {
        this->declare_parameter("turtle_name_prefix", "turtle");
        this->declare_parameter("spawn_frequency", 1.0);
        turtle_name_prefix_ = this->get_parameter("turtle_name_prefix").as_string();
        spawn_frequency_ = this->get_parameter("spawn_frequency").as_double();

        alive_turtles_pub_ = this->create_publisher<my_interfaces::msg::TurtleArray>(
            "alive_turtles", 10
        );

        spawn_client_ = this->create_client<turtlesim::srv::Spawn>(
            "/spawn"
        );

        kill_client_ = this->create_client<turtlesim::srv::Kill>(
            "/kill"
        );

        catch_turtle_service_ = this->create_service<my_interfaces::srv::CatchTurtle>(
            "catch_turtle",
            std::bind(&TurtleSpawnerNode::callbackCatchTurtle, this, _1, _2)
        );

        // New turtle spawns every 2 seconds
        timer_ = this->create_wall_timer(
            std::chrono::duration<double>(spawn_frequency_),
            std::bind(&TurtleSpawnerNode::spawnNewTurtle, this));
    }

private:
    void callbackCatchTurtle(const my_interfaces::srv::CatchTurtle::Request::SharedPtr request,
                            const my_interfaces::srv::CatchTurtle::Response::SharedPtr response)
    {
        callKillService(request->name);
        response->success = true;
        // RETURN THE RESPONSE
    }

    void publishAliveTurtles()
    {
        auto msg = my_interfaces::msg::TurtleArray();
        msg.turtles = alive_turtles_;

        alive_turtles_pub_->publish(msg);
    }

    void callSpawnService(const std::string &turtle_name, double x, double y, double theta) 
    {
        while (!spawn_client_->wait_for_service(1s))
        {
            RCLCPP_WARN(this->get_logger(), "Waiting for spawn service...");
        }


        auto request = std::make_shared<turtlesim::srv::Spawn::Request>();
        request->x = x;
        request->y = y;
        request->theta = theta;
        request->name = turtle_name;

        // Use of lambda function
        spawn_client_->async_send_request(
            request,
            [this, request](rclcpp::Client<turtlesim::srv::Spawn>::SharedFuture future) {
                callbackCallSpawnService(future, request);
            });
    }

    void callbackCallSpawnService(
        rclcpp::Client<turtlesim::srv::Spawn>::SharedFuture future,
        turtlesim::srv::Spawn::Request::SharedPtr request)
    {
        auto response = future.get();
        if (response->name != "") {
            RCLCPP_INFO(this->get_logger(), "New alive turtle: %s", response->name.c_str());
            
            auto new_turtle = my_interfaces::msg::Turtle();
            new_turtle.name = response->name;
            new_turtle.x = request->x;
            new_turtle.y = request->y;
            new_turtle.theta = request->theta;

            alive_turtles_.push_back(new_turtle);
            publishAliveTurtles();
        }
    }

    void spawnNewTurtle()
    {
        turtle_counter_ += 1;
        std::string name = turtle_name_prefix_ + std::to_string(turtle_counter_);

        double x = randomDouble(1.0, 10.0);
        double y = randomDouble(1.0, 10.0);
        double theta = randomDouble(0.0, 2 * M_PI);

        callSpawnService(name, x, y, theta);
    }

    // Outsourced
    double randomDouble(double min, double max)
    {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<double> dist(min, max);
        return dist(gen);
    }

    void callKillService(const std::string &turtle_name)
    {
        while (!kill_client_->wait_for_service(1s))
        {
            RCLCPP_WARN(this->get_logger(), "Waiting for kill service...");
        }

        auto request = std::make_shared<turtlesim::srv::Kill::Request>();
        request->name = turtle_name;

        // Use of lambda function
        kill_client_->async_send_request(
            request,
            [this, request](rclcpp::Client<turtlesim::srv::Kill>::SharedFuture future) {
                callbackCallKillService(future, request);
            });
    }

    void callbackCallKillService(
        rclcpp::Client<turtlesim::srv::Kill>::SharedFuture future,
        turtlesim::srv::Kill::Request::SharedPtr request)
    {
        // Remove killed turtle from list
        for (size_t i = 0; i < alive_turtles_.size(); i++) {
            if (request->name == alive_turtles_[i].name) {
                alive_turtles_.erase(alive_turtles_.begin() + i);
                publishAliveTurtles();
                break;
            }
        }
    }

    double spawn_frequency_;
    std::string turtle_name_prefix_;
    int turtle_counter_;
    std::vector<my_interfaces::msg::Turtle> alive_turtles_;

    rclcpp::Publisher<my_interfaces::msg::TurtleArray>::SharedPtr alive_turtles_pub_;
    rclcpp::Client<turtlesim::srv::Spawn>::SharedPtr spawn_client_;
    rclcpp::Client<turtlesim::srv::Kill>::SharedPtr kill_client_;
    rclcpp::Service<my_interfaces::srv::CatchTurtle>::SharedPtr catch_turtle_service_;
    rclcpp::TimerBase::SharedPtr timer_;
};
 
int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TurtleSpawnerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}