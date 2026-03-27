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
#include <vector>
#include <utility>

#include "auth_middleware.h"
#include "request_counter.h"
#include "auth_config.h"
#include "query_utils.h"
#include "../repository/user_repository.h"
#include "../model/user.h"

namespace handlers {

// Регистрация нового пользователя
class RegisterUserHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::UserRepository& userRepo;
    
public:
    RegisterUserHandler(repository::UserRepository& repo) : userRepo(repo) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override {
        Poco::Timestamp start;
        if (g_httpRequests) g_httpRequests->inc();
        
        // Только POST
        if (request.getMethod() != "POST") {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
            response.send() << R"({"error":"Method not allowed"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        try {
            // Читаем тело запроса
            std::istream& istr = request.stream();
            std::string body(std::istreambuf_iterator<char>(istr), {});
            
            // Парсим JSON
            Poco::JSON::Parser parser;
            Poco::Dynamic::Var result = parser.parse(body);
            Poco::JSON::Object::Ptr json = result.extract<Poco::JSON::Object::Ptr>();
            
            // Извлекаем поля
            std::string username = json->getValue<std::string>("username");
            std::string password = json->getValue<std::string>("password");
            std::string firstName = json->getValue<std::string>("firstName");
            std::string lastName = json->getValue<std::string>("lastName");
            std::string role = json->optValue<std::string>("role", "customer");
            std::string email = json->optValue<std::string>("email", "");
            
            // Валидация
            if (username.empty() || password.empty() || 
                firstName.empty() || lastName.empty()) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                response.send() << R"({"error":"Missing required fields"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            // Проверяем, существует ли пользователь
            if (userRepo.exists(username)) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_CONFLICT);
                response.send() << R"({"error":"Username already exists"})";
                if (g_httpErrors) g_httpErrors->inc();
                return;
            }
            
            // Создаем пользователя
            model::User newUser(username, password, firstName, lastName, role, email);
            
            if (userRepo.addUser(newUser)) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_CREATED);
                response.setContentType("application/json");
                
                Poco::JSON::Object jsonResp;
                jsonResp.set("username", username);
                jsonResp.set("firstName", firstName);
                jsonResp.set("lastName", lastName);
                jsonResp.set("role", role);
                
                std::ostream& ostr = response.send();
                Poco::JSON::Stringifier::stringify(jsonResp, ostr);
            } else {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                response.send() << R"({"error":"Failed to create user"})";
                if (g_httpErrors) g_httpErrors->inc();
            }
            
        } catch (const std::exception& e) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.send() << "{\"error\":\"" << e.what() << "\"}";
            if (g_httpErrors) g_httpErrors->inc();
        }
        
        // Логирование
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("%d POST /api/v1/users/register from %s, %.2f ms",
                          response.getStatus(),
                          request.clientAddress().toString(), seconds * 1000.0);
    }
};

// Поиск пользователя по логину
class SearchUserByLoginHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::UserRepository& userRepo;
    
public:
    SearchUserByLoginHandler(repository::UserRepository& repo) : userRepo(repo) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override {
        Poco::Timestamp start;
        if (g_httpRequests) g_httpRequests->inc();
        
        // Проверяем аутентификацию
        AuthInfo auth = AuthMiddleware::authenticate(request, g_jwtSecret);
        if (!auth.isValid) {
            AuthMiddleware::sendUnauthorized(response, "Authentication required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        // Проверяем права (только админ может искать пользователей)
        if (!AuthMiddleware::checkRole(auth, model::UserRole::ADMIN)) {
            AuthMiddleware::sendForbidden(response, "Admin access required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        // Извлекаем параметр login из URL
        Poco::URI uri(request.getURI());
        std::string login = getQueryParameter(uri, "login");
        
        if (login.empty()) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.send() << R"({"error":"login parameter required"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        const model::User* user = userRepo.findByUsername(login);
        
        if (!user) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
            response.send() << R"({"error":"User not found"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        // Формируем ответ
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");
        
        Poco::JSON::Object jsonResp;
        jsonResp.set("username", user->username);
        jsonResp.set("firstName", user->firstName);
        jsonResp.set("lastName", user->lastName);
        jsonResp.set("role", model::roleToString(user->role));
        jsonResp.set("email", user->email);
        
        std::ostream& ostr = response.send();
        Poco::JSON::Stringifier::stringify(jsonResp, ostr);
        
        // Логирование
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("200 GET /api/v1/users/search?login=%s from %s, %.2f ms",
                          login.c_str(),
                          request.clientAddress().toString(), seconds * 1000.0);
    }
};

// Поиск пользователей по маске имени и фамилии
class SearchUserByMaskHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::UserRepository& userRepo;
    
public:
    SearchUserByMaskHandler(repository::UserRepository& repo) : userRepo(repo) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override {
        Poco::Timestamp start;
        if (g_httpRequests) g_httpRequests->inc();
        
        // Проверяем аутентификацию
        AuthInfo auth = AuthMiddleware::authenticate(request, g_jwtSecret);
        if (!auth.isValid) {
            AuthMiddleware::sendUnauthorized(response, "Authentication required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        // Проверяем права (только админ может искать пользователей)
        if (!AuthMiddleware::checkRole(auth, model::UserRole::ADMIN)) {
            AuthMiddleware::sendForbidden(response, "Admin access required");
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        // Извлекаем параметры
        Poco::URI uri(request.getURI());
        std::string nameMask = getQueryParameter(uri, "name");
        std::string surnameMask = getQueryParameter(uri, "surname");
        
        if (nameMask.empty() && surnameMask.empty()) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.send() << R"({"error":"At least one mask (name or surname) required"})";
            if (g_httpErrors) g_httpErrors->inc();
            return;
        }
        
        // Поиск
        std::vector<model::User> users = userRepo.findByMask(nameMask, surnameMask);
        
        // Формируем ответ
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");
        
        Poco::JSON::Array jsonArray;
        for (const auto& user : users) {
            Poco::JSON::Object jsonUser;
            jsonUser.set("username", user.username);
            jsonUser.set("firstName", user.firstName);
            jsonUser.set("lastName", user.lastName);
            jsonUser.set("role", model::roleToString(user.role));
            jsonUser.set("email", user.email);
            jsonArray.add(jsonUser);
        }
        
        std::ostream& ostr = response.send();
        Poco::JSON::Stringifier::stringify(jsonArray, ostr);
        
        // Логирование
        Poco::Timespan elapsed = Poco::Timestamp() - start;
        double seconds = static_cast<double>(elapsed.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        auto& logger = Poco::Logger::get("Server");
        logger.information("200 GET /api/v1/users/search?name=%s&surname=%s from %s, %.2f ms, found %zu users",
                          nameMask.c_str(), surnameMask.c_str(),
                          request.clientAddress().toString(), seconds * 1000.0, users.size());
    }
};

} // namespace handlers