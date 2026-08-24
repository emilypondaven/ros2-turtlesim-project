#include "rclcpp/rclcpp.hpp"
#include "turtlesim/srv/spawn.hpp"
#include <string>
#include <cstdlib>
#include <random>
 
using namespace std::chrono_literals;
using namespace std::placeholders;

class TurtleSpawnerNode : public rclcpp::Node
{
public:
    TurtleSpawnerNode() : Node("turtle_spawner"), turtle_name_prefix_("turtle"), turtle_counter_(0)
    {
        spawn_client_ = this->create_client<turtlesim::srv::Spawn>(
            "/spawn"
        );
        timer_ = this->create_wall_timer(2s, std::bind(&TurtleSpawnerNode::spawnNewTurtle, this));
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

        spawn_client_->async_send_request(
            request, std::bind(&TurtleSpawnerNode::callbackCallSpawnService, this, _1)
        );
    }

private:
    void callbackCallSpawnService(rclcpp::Client<turtlesim::srv::Spawn>::SharedFuture future)
    {
        auto response = future.get();
        if (response->name != "") {
            RCLCPP_INFO(this->get_logger(), "New alive turtle: %s", response->name.c_str());
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

    const std::string turtle_name_prefix_;
    int turtle_counter_;
    rclcpp::Client<turtlesim::srv::Spawn>::SharedPtr spawn_client_;
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