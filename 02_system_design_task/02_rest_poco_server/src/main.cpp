#include <map>
#include <memory>
#include <string>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Environment.h>
#include <Poco/Logger.h>
#include <Poco/NumberParser.h>
#include <Poco/Prometheus/Registry.h>
#include <Poco/Prometheus/Counter.h>
#include <Poco/Prometheus/Histogram.h>

#include "handler/router_factory.h"
#include "handler/request_counter.h"
#include "handler/auth_config.h"
#include "repository/user_repository.h"
#include "repository/car_repository.h"
#include "repository/rental_repository.h"
#include "model/user.h"
#include "model/car.h"
#include "model/rental.h"

// Global repositories
repository::UserRepository g_userRepo;
repository::CarRepository g_carRepo;
repository::RentalRepository g_rentalRepo;

class CarRentalApp : public Poco::Util::ServerApplication {
protected:
    void initialize(Application& self) override {
        loadConfiguration();
        ServerApplication::initialize(self);
        
        // Initialize sample data
        initSampleData();
    }
    
    void initSampleData() {
        // Create admin user
        model::User admin("admin", "admin123", "Admin", "User", "admin");
        g_userRepo.addUser(admin);
        
        // Create fleet manager
        model::User manager("manager", "manager123", "John", "Doe", "fleetManager");
        g_userRepo.addUser(manager);
        
        // Create regular customer
        model::User customer("customer", "customer123", "Jane", "Smith", "customer");
        g_userRepo.addUser(customer);
        
        // Create another customer for testing
        model::User alice("alice", "alice123", "Alice", "Johnson", "customer", "alice@example.com");
        g_userRepo.addUser(alice);
        
        // Add sample cars
        model::Car car1("Toyota Camry", "ABC123", "midsize", 65.0, 2022, "Silver");
        model::Car car2("Honda Civic", "XYZ789", "compact", 45.0, 2023, "Red");
        model::Car car3("BMW X5", "BMW001", "luxury", 120.0, 2024, "Black");
        model::Car car4("Ford Explorer", "FORD01", "suv", 85.0, 2022, "Blue");
        model::Car car5("Tesla Model 3", "TSLA01", "electric", 95.0, 2023, "White");
        
        g_carRepo.addCar(car1);
        g_carRepo.addCar(car2);
        g_carRepo.addCar(car3);
        g_carRepo.addCar(car4);
        g_carRepo.addCar(car5);
        
        // Add some sample rentals for history
        std::time_t now = std::time(nullptr);
        std::time_t pastStart = now - 30 * 24 * 3600; // 30 days ago
        std::time_t pastEnd = now - 15 * 24 * 3600;   // 15 days ago
        
        model::Rental pastRental("customer", car1.id, pastStart, pastEnd);
        pastRental.complete(); // Already completed
        pastRental.setPrice(15 * 65.0); // 15 days * price
        g_rentalRepo.addRental(pastRental);
        
        auto& logger = Poco::Logger::get("Server");
        logger.information("Sample data initialized with %zu users, %zu cars, %zu rentals",
                          g_userRepo.getAllUsers().size(),
                          g_carRepo.getAllCars().size(),
                          g_rentalRepo.getAllRentals().size());
    }
    
    int main(const std::vector<std::string>& args) override {
        configureLogging();
        
        // Initialize Prometheus metrics (assign to global pointers)
        static Poco::Prometheus::Counter httpRequestsCounter("http_requests_total");
        httpRequestsCounter.help("Total number of HTTP requests");
        handlers::g_httpRequests = &httpRequestsCounter;
        
        static Poco::Prometheus::Counter httpErrorsCounter("http_errors_total");
        httpErrorsCounter.help("Total number of HTTP errors");
        handlers::g_httpErrors = &httpErrorsCounter;
        
        static Poco::Prometheus::Histogram httpDurationHistogram("http_request_duration_seconds");
        httpDurationHistogram.help("HTTP request duration in seconds");
        httpDurationHistogram.buckets({0.0001, 0.0005, 0.001, 0.0025, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0});
        handlers::g_httpDuration = &httpDurationHistogram;
        
        // Initialize JWT secret
        handlers::g_jwtSecret = Poco::Environment::get("JWT_SECRET", "default_secret_key_change_in_production");
        
        // Configure server
        unsigned short port = 8080;
        if (Poco::Environment::has("PORT")) {
            port = static_cast<unsigned short>(Poco::NumberParser::parse(Poco::Environment::get("PORT")));
        }
        
        Poco::Net::ServerSocket socket(port);
        Poco::Net::HTTPServer server(new handlers::RouterFactory(g_userRepo, g_carRepo, g_rentalRepo), 
                                      socket, 
                                      new Poco::Net::HTTPServerParams);
        
        server.start();
        auto& logger = Poco::Logger::get("Server");
        logger.information("Car Rental API Server started on port %hu", port);
        logger.information("Available endpoints:");
        logger.information("  POST   /api/v1/auth                - Get JWT token");
        logger.information("  POST   /api/v1/users/register     - Register new user");
        logger.information("  GET    /api/v1/users/search?login - Search user by login");
        logger.information("  GET    /api/v1/users/search?name&surname - Search by name mask");
        logger.information("  POST   /api/v1/cars               - Add car (fleetManager/admin)");
        logger.information("  GET    /api/v1/cars               - List available cars");
        logger.information("  GET    /api/v1/cars/search?class  - Search cars by class");
        logger.information("  POST   /api/v1/rentals            - Create rental");
        logger.information("  GET    /api/v1/rentals/active     - Get active rentals");
        logger.information("  GET    /api/v1/rentals/history    - Get rental history");
        logger.information("  PUT    /api/v1/rentals/{id}/complete - Complete rental early");
        logger.information("  GET    /swagger.yaml              - OpenAPI spec");
        
        waitForTerminationRequest();
        server.stop();
        
        return Application::EXIT_OK;
    }
    
    void configureLogging() {
        std::string level = Poco::Environment::get("LOG_LEVEL", "information");
        int prio = Poco::Logger::parseLevel(level);
        Poco::Logger::root().setLevel(prio);
    }
};

POCO_SERVER_MAIN(CarRentalApp)