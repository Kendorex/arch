#pragma once

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/URI.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Logger.h>
#include <Poco/Timestamp.h>
#include <sstream>

#include "auth_middleware.h"
#include "request_counter.h"
#include "auth_config.h"
#include "query_utils.h"
#include "../repository/car_repository.h"
#include "../model/car.h"

namespace handlers {

// Добавление автомобиля в парк
class AddCarHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::CarRepository& carRepo;
    
public:
    AddCarHandler(repository::CarRepository& repo) : carRepo(repo) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override {
        Poco::Timestamp start;
        if (g_httpRequests) g_httpRequests->inc();
        
        if (request.getMethod() != "POST") {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
            response.send() << R"({"error":"Method not allowed"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        AuthInfo auth = AuthMiddleware::authenticate(request, g_jwtSecret);
        if (!auth.isValid) {
            AuthMiddleware::sendUnauthorized(response, "Authentication required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        std::vector<model::UserRole> allowedRoles = {
            model::UserRole::FLEET_MANAGER,
            model::UserRole::ADMIN
        };
        if (!AuthMiddleware::checkRoles(auth, allowedRoles)) {
            AuthMiddleware::sendForbidden(response, "Fleet manager or admin access required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        try {
            std::istream& istr = request.stream();
            std::string body(std::istreambuf_iterator<char>(istr), {});
            
            Poco::JSON::Parser parser;
            Poco::Dynamic::Var result = parser.parse(body);
            Poco::JSON::Object::Ptr json = result.extract<Poco::JSON::Object::Ptr>();
            
            std::string model = json->getValue<std::string>("model");
            std::string licensePlate = json->getValue<std::string>("licensePlate");
            std::string carClass = json->getValue<std::string>("class");
            double pricePerDay = json->optValue<double>("pricePerDay", 50.0);
            int year = json->optValue<int>("year", 2020);
            std::string color = json->optValue<std::string>("color", "White");
            
            if (model.empty() || licensePlate.empty() || carClass.empty()) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                response.send() << R"({"error":"Missing required fields"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            model::Car newCar(model, licensePlate, carClass, pricePerDay, year, color);
            carRepo.addCar(newCar);
            
            response.setStatus(Poco::Net::HTTPResponse::HTTP_CREATED);
            response.setContentType("application/json");
            std::ostream& ostr = response.send();
            ostr << newCar.toJson();
            
        } catch (const std::exception& e) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.send() << "{\"error\":\"" << e.what() << "\"}";
            if (g_httpErrors) g_httpErrors->inc();
        }
        
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("%d POST /api/v1/cars from %s (user: %s), %.2f ms",
                          response.getStatus(),
                          request.clientAddress().toString(),
                          auth.username.c_str(),
                          seconds * 1000.0);
    }
};

// Получение списка доступных автомобилей
class GetAvailableCarsHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::CarRepository& carRepo;
    
public:
    GetAvailableCarsHandler(repository::CarRepository& repo) : carRepo(repo) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override {
        Poco::Timestamp start;
        if (g_httpRequests) g_httpRequests->inc();
        
        if (request.getMethod() != "GET") {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
            response.send() << R"({"error":"Method not allowed"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        std::vector<model::Car> cars = carRepo.getAvailableCars();
        
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");
        
        Poco::JSON::Array jsonArray;
        for (const auto& car : cars) {
            Poco::JSON::Object jsonCar;
            jsonCar.set("id", car.id);
            jsonCar.set("model", car.model);
            jsonCar.set("licensePlate", car.licensePlate);
            jsonCar.set("class", car.carClass);
            jsonCar.set("available", car.available);
            jsonCar.set("pricePerDay", car.pricePerDay);
            jsonCar.set("year", car.year);
            jsonCar.set("color", car.color);
            jsonArray.add(jsonCar);
        }
        
        std::ostream& ostr = response.send();
        Poco::JSON::Stringifier::stringify(jsonArray, ostr);
        
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("200 GET /api/v1/cars from %s, %.2f ms, found %zu cars",
                          request.clientAddress().toString(), seconds * 1000.0, cars.size());
    }
};

// Поиск автомобилей по классу
class SearchCarsByClassHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::CarRepository& carRepo;
    
public:
    SearchCarsByClassHandler(repository::CarRepository& repo) : carRepo(repo) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override {
        Poco::Timestamp start;
        if (g_httpRequests) g_httpRequests->inc();
        
        if (request.getMethod() != "GET") {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
            response.send() << R"({"error":"Method not allowed"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        Poco::URI uri(request.getURI());
        std::string carClass = getQueryParameter(uri, "class");
        
        if (carClass.empty()) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.send() << R"({"error":"class parameter required"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        std::vector<model::Car> cars = carRepo.findByClass(carClass);
        
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");
        
        Poco::JSON::Array jsonArray;
        for (const auto& car : cars) {
            Poco::JSON::Object jsonCar;
            jsonCar.set("id", car.id);
            jsonCar.set("model", car.model);
            jsonCar.set("licensePlate", car.licensePlate);
            jsonCar.set("class", car.carClass);
            jsonCar.set("available", car.available);
            jsonCar.set("pricePerDay", car.pricePerDay);
            jsonCar.set("year", car.year);
            jsonCar.set("color", car.color);
            jsonArray.add(jsonCar);
        }
        
        std::ostream& ostr = response.send();
        Poco::JSON::Stringifier::stringify(jsonArray, ostr);
        
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("200 GET /api/v1/cars/search?class=%s from %s, %.2f ms, found %zu cars",
                          carClass.c_str(),
                          request.clientAddress().toString(), seconds * 1000.0, cars.size());
    }
};

} // namespace handlers