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
#include <Poco/DateTime.h>
#include <Poco/DateTimeParser.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <sstream>
#include <ctime>

#include "auth_middleware.h"
#include "request_counter.h"
#include "auth_config.h"
#include "query_utils.h"
#include "../repository/rental_repository.h"
#include "../repository/car_repository.h"
#include "../repository/user_repository.h"
#include "../model/rental.h"
#include "../model/car.h"

namespace handlers {

// Утилита для преобразования строки в time_t
inline std::time_t parseDateTime(const std::string& dateStr) {
    int tzd;
    Poco::DateTime dt;
    Poco::DateTimeParser::parse(Poco::DateTimeFormat::ISO8601_FORMAT, dateStr, dt, tzd);
    return dt.timestamp().epochTime();
}

// Утилита для форматирования time_t в строку
inline std::string formatDateTime(std::time_t time) {
    if (time == 0) return "";
    Poco::Timestamp ts(time * 1000000); // секунды -> микросекунды
    Poco::DateTime dt(ts);
    return Poco::DateTimeFormatter::format(dt, Poco::DateTimeFormat::ISO8601_FORMAT);
}

// Создание новой аренды
class CreateRentalHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::RentalRepository& rentalRepo;
    repository::CarRepository& carRepo;
    repository::UserRepository& userRepo;
    
public:
    CreateRentalHandler(repository::RentalRepository& rentRepo,
                        repository::CarRepository& cRepo,
                        repository::UserRepository& uRepo)
        : rentalRepo(rentRepo), carRepo(cRepo), userRepo(uRepo) {}
    
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
            model::UserRole::CUSTOMER,
            model::UserRole::FLEET_MANAGER,
            model::UserRole::ADMIN
        };
        if (!AuthMiddleware::checkRoles(auth, allowedRoles)) {
            AuthMiddleware::sendForbidden(response, "Customer or higher role required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        try {
            std::istream& istr = request.stream();
            std::string body(std::istreambuf_iterator<char>(istr), {});
            
            Poco::JSON::Parser parser;
            Poco::Dynamic::Var result = parser.parse(body);
            Poco::JSON::Object::Ptr json = result.extract<Poco::JSON::Object::Ptr>();
            
            std::string carId = json->getValue<std::string>("carId");
            std::string startDateStr = json->getValue<std::string>("startDate");
            std::string endDateStr = json->getValue<std::string>("endDate");
            
            if (carId.empty() || startDateStr.empty() || endDateStr.empty()) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                response.send() << R"({"error":"Missing required fields"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            const model::Car* car = carRepo.findById(carId);
            if (!car) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
                response.send() << R"({"error":"Car not found"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            if (!car->isAvailable()) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_CONFLICT);
                response.send() << R"({"error":"Car is not available"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            std::time_t startDate = parseDateTime(startDateStr);
            std::time_t endDate = parseDateTime(endDateStr);
            std::time_t now = std::time(nullptr);
            
            if (startDate < now) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                response.send() << R"({"error":"Start date cannot be in the past"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            if (startDate >= endDate) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                response.send() << R"({"error":"End date must be after start date"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            model::Rental newRental(auth.username, carId, startDate, endDate);
            
            int days = newRental.getDaysCount();
            double totalPrice = days * car->pricePerDay;
            newRental.setPrice(totalPrice);
            
            if (!carRepo.reserveCar(carId)) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_CONFLICT);
                response.send() << R"({"error":"Failed to reserve car"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            rentalRepo.addRental(newRental);
            
            response.setStatus(Poco::Net::HTTPResponse::HTTP_CREATED);
            response.setContentType("application/json");
            
            Poco::JSON::Object jsonResp;
            jsonResp.set("id", newRental.id);
            jsonResp.set("userId", newRental.userId);
            jsonResp.set("carId", newRental.carId);
            jsonResp.set("carModel", car->model);
            jsonResp.set("startDate", startDateStr);
            jsonResp.set("endDate", endDateStr);
            jsonResp.set("days", days);
            jsonResp.set("pricePerDay", car->pricePerDay);
            jsonResp.set("totalPrice", totalPrice);
            jsonResp.set("status", model::statusToString(newRental.status));
            
            std::ostream& ostr = response.send();
            Poco::JSON::Stringifier::stringify(jsonResp, ostr);
            
        } catch (const std::exception& e) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.send() << "{\"error\":\"" << e.what() << "\"}";
            if (g_httpErrors) g_httpErrors->inc();
        }
        
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("%d POST /api/v1/rentals from %s (user: %s), %.2f ms",
                          response.getStatus(),
                          request.clientAddress().toString(),
                          auth.username.c_str(),
                          seconds * 1000.0);
    }
};

// Получение активных аренд пользователя
class GetActiveRentalsHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::RentalRepository& rentalRepo;
    repository::CarRepository& carRepo;
    
public:
    GetActiveRentalsHandler(repository::RentalRepository& rentRepo,
                            repository::CarRepository& cRepo)
        : rentalRepo(rentRepo), carRepo(cRepo) {}
    
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
        
        AuthInfo auth = AuthMiddleware::authenticate(request, g_jwtSecret);
        if (!auth.isValid) {
            AuthMiddleware::sendUnauthorized(response, "Authentication required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        Poco::URI uri(request.getURI());
        std::string targetUserId = getQueryParameter(uri, "userId");
        
        if (targetUserId.empty()) {
            targetUserId = auth.username;
        } else {
            if (targetUserId != auth.username && 
                !AuthMiddleware::checkRole(auth, model::UserRole::ADMIN)) {
                AuthMiddleware::sendForbidden(response, "Admin access required to view other users' rentals");
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
        }
        
        std::vector<model::Rental> rentals = rentalRepo.getActiveRentalsByUser(targetUserId);
        
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");
        
        Poco::JSON::Array jsonArray;
        for (const auto& rental : rentals) {
            Poco::JSON::Object jsonRental;
            jsonRental.set("id", rental.id);
            jsonRental.set("userId", rental.userId);
            jsonRental.set("carId", rental.carId);
            
            const model::Car* car = carRepo.findById(rental.carId);
            if (car) {
                jsonRental.set("carModel", car->model);
                jsonRental.set("carClass", car->carClass);
                jsonRental.set("pricePerDay", car->pricePerDay);
            }
            
            jsonRental.set("startDate", formatDateTime(rental.startDate));
            jsonRental.set("endDate", formatDateTime(rental.endDate));
            jsonRental.set("days", rental.getDaysCount());
            jsonRental.set("totalPrice", rental.totalPrice);
            jsonRental.set("status", model::statusToString(rental.status));
            
            jsonArray.add(jsonRental);
        }
        
        std::ostream& ostr = response.send();
        Poco::JSON::Stringifier::stringify(jsonArray, ostr);
        
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("200 GET /api/v1/rentals/active from %s (user: %s, target: %s), %.2f ms, found %zu rentals",
                          request.clientAddress().toString(),
                          auth.username.c_str(),
                          targetUserId.c_str(),
                          seconds * 1000.0,
                          rentals.size());
    }
};

// Завершение аренды (досрочно)
class CompleteRentalHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::RentalRepository& rentalRepo;
    repository::CarRepository& carRepo;
    
public:
    CompleteRentalHandler(repository::RentalRepository& rentRepo,
                          repository::CarRepository& cRepo)
        : rentalRepo(rentRepo), carRepo(cRepo) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override {
        Poco::Timestamp start;
        if (g_httpRequests) g_httpRequests->inc();
        
        if (request.getMethod() != "PUT") {
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
        
        std::string uri = request.getURI();
        std::string rentalId;
        
        size_t startPos = uri.find("/api/v1/rentals/");
        if (startPos != std::string::npos) {
            startPos += 16;
            size_t endPos = uri.find("/complete", startPos);
            if (endPos != std::string::npos) {
                rentalId = uri.substr(startPos, endPos - startPos);
            }
        }
        
        if (rentalId.empty()) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.send() << R"({"error":"Invalid rental ID in URL"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        const model::Rental* rental = rentalRepo.findById(rentalId);
        if (!rental) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
            response.send() << R"({"error":"Rental not found"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        if (rental->userId != auth.username && 
            !AuthMiddleware::checkRole(auth, model::UserRole::ADMIN)) {
            AuthMiddleware::sendForbidden(response, "You can only complete your own rentals");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        if (!rental->isActive()) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.send() << R"({"error":"Rental is not active"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        try {
            model::Rental updatedRental = *rental;
            std::time_t now = std::time(nullptr);
            updatedRental.completeEarly(now);
            
            int actualDays = updatedRental.getActualDaysCount();
            const model::Car* car = carRepo.findById(rental->carId);
            if (car && actualDays > 0) {
                double newTotal = actualDays * car->pricePerDay;
                updatedRental.setPrice(newTotal);
            }
            
            if (!rentalRepo.updateRental(updatedRental)) {
                throw std::runtime_error("Failed to update rental");
            }
            
            carRepo.releaseCar(rental->carId);
            
            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            response.setContentType("application/json");
            
            Poco::JSON::Object jsonResp;
            jsonResp.set("id", updatedRental.id);
            jsonResp.set("status", model::statusToString(updatedRental.status));
            jsonResp.set("actualEndDate", formatDateTime(updatedRental.actualEndDate));
            jsonResp.set("totalPrice", updatedRental.totalPrice);
            jsonResp.set("message", "Rental completed successfully");
            
            std::ostream& ostr = response.send();
            Poco::JSON::Stringifier::stringify(jsonResp, ostr);
            
        } catch (const std::exception& e) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            response.send() << "{\"error\":\"" << e.what() << "\"}";
            if (g_httpErrors) g_httpErrors->inc();
        }
        
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("%d PUT /api/v1/rentals/%s/complete from %s (user: %s), %.2f ms",
                          response.getStatus(),
                          rentalId.c_str(),
                          request.clientAddress().toString(),
                          auth.username.c_str(),
                          seconds * 1000.0);
    }
};

// Получение истории аренд пользователя
class GetRentalHistoryHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::RentalRepository& rentalRepo;
    repository::CarRepository& carRepo;
    
public:
    GetRentalHistoryHandler(repository::RentalRepository& rentRepo,
                            repository::CarRepository& cRepo)
        : rentalRepo(rentRepo), carRepo(cRepo) {}
    
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
        
        AuthInfo auth = AuthMiddleware::authenticate(request, g_jwtSecret);
        if (!auth.isValid) {
            AuthMiddleware::sendUnauthorized(response, "Authentication required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        Poco::URI uri(request.getURI());
        std::string targetUserId = getQueryParameter(uri, "userId");
        
        if (targetUserId.empty()) {
            targetUserId = auth.username;
        } else {
            if (targetUserId != auth.username && 
                !AuthMiddleware::checkRole(auth, model::UserRole::ADMIN)) {
                AuthMiddleware::sendForbidden(response, "Admin access required to view other users' history");
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
        }
        
        std::vector<model::Rental> rentals = rentalRepo.getRentalHistoryByUser(targetUserId);
        
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");
        
        Poco::JSON::Array jsonArray;
        for (const auto& rental : rentals) {
            Poco::JSON::Object jsonRental;
            jsonRental.set("id", rental.id);
            jsonRental.set("userId", rental.userId);
            jsonRental.set("carId", rental.carId);
            
            const model::Car* car = carRepo.findById(rental.carId);
            if (car) {
                jsonRental.set("carModel", car->model);
                jsonRental.set("carClass", car->carClass);
            }
            
            jsonRental.set("startDate", formatDateTime(rental.startDate));
            jsonRental.set("endDate", formatDateTime(rental.endDate));
            jsonRental.set("actualEndDate", formatDateTime(rental.actualEndDate));
            jsonRental.set("status", model::statusToString(rental.status));
            jsonRental.set("totalPrice", rental.totalPrice);
            
            jsonArray.add(jsonRental);
        }
        
        std::ostream& ostr = response.send();
        Poco::JSON::Stringifier::stringify(jsonArray, ostr);
        
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("200 GET /api/v1/rentals/history from %s (user: %s, target: %s), %.2f ms, found %zu rentals",
                          request.clientAddress().toString(),
                          auth.username.c_str(),
                          targetUserId.c_str(),
                          seconds * 1000.0,
                          rentals.size());
    }
};

} // namespace handlers