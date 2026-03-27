#pragma once

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPBasicCredentials.h>
#include <Poco/Net/NetException.h>
#include <Poco/Logger.h>
#include <Poco/Timestamp.h>
#include <Poco/JWT/Token.h>
#include <Poco/JWT/Signer.h>

#include "auth_config.h"
#include "auth_middleware.h"
#include "request_counter.h"
#include "../repository/user_repository.h"

#include <sstream>

namespace handlers {

class AuthHandler : public Poco::Net::HTTPRequestHandler {
private:
    repository::UserRepository& userRepo;
    
public:
    AuthHandler(repository::UserRepository& repo) : userRepo(repo) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override {
        Poco::Timestamp start;
        if (g_httpRequests) g_httpRequests->inc();

        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");

        try {
            Poco::Net::HTTPBasicCredentials creds(request);
            std::string username = creds.getUsername();
            std::string password = creds.getPassword();

            if (username.empty()) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                std::ostream& ostr = response.send();
                ostr << R"({"error":"username is required"})";
                if (g_httpErrors) g_httpErrors->inc();
                Poco::Timespan duration = Poco::Timestamp() - start;
                double seconds = static_cast<double>(duration.totalMicroseconds()) / 1000000.0;
                if (g_httpDuration) g_httpDuration->observe(seconds);
                Poco::Logger::get("Server").warning("400 POST /api/v1/auth from %s - username is required, %.2f ms",
                                                    request.clientAddress().toString(), seconds * 1000.0);
                return;
            }

            // Находим пользователя
            const model::User* user = userRepo.findByUsername(username);
            if (!user) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
                response.set("WWW-Authenticate", "Basic realm=\"api\"");
                std::ostream& ostr = response.send();
                ostr << R"({"error":"Invalid username or password"})";
                if (g_httpErrors) g_httpErrors->inc();
                Poco::Timespan duration = Poco::Timestamp() - start;
                double seconds = static_cast<double>(duration.totalMicroseconds()) / 1000000.0;
                if (g_httpDuration) g_httpDuration->observe(seconds);
                Poco::Logger::get("Server").warning("401 POST /api/v1/auth from %s - invalid username, %.2f ms",
                                                    request.clientAddress().toString(), seconds * 1000.0);
                return;
            }

            // Проверяем пароль
            if (!user->checkPassword(password)) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
                response.set("WWW-Authenticate", "Basic realm=\"api\"");
                std::ostream& ostr = response.send();
                ostr << R"({"error":"Invalid username or password"})";
                if (g_httpErrors) g_httpErrors->inc();
                Poco::Timespan duration = Poco::Timestamp() - start;
                double seconds = static_cast<double>(duration.totalMicroseconds()) / 1000000.0;
                if (g_httpDuration) g_httpDuration->observe(seconds);
                Poco::Logger::get("Server").warning("401 POST /api/v1/auth from %s - invalid password, %.2f ms",
                                                    request.clientAddress().toString(), seconds * 1000.0);
                return;
            }

            if (g_jwtSecret.empty()) {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                std::ostream& ostr = response.send();
                ostr << R"({"error":"JWT_SECRET not configured"})";
                if (g_httpErrors) g_httpErrors->inc();
                Poco::Timespan duration = Poco::Timestamp() - start;
                double seconds = static_cast<double>(duration.totalMicroseconds()) / 1000000.0;
                if (g_httpDuration) g_httpDuration->observe(seconds);
                Poco::Logger::get("Server").error("500 POST /api/v1/auth from %s - JWT_SECRET not configured, %.2f ms",
                                                  request.clientAddress().toString(), seconds * 1000.0);
                return;
            }

            // Создаем JWT токен с ролью пользователя
            std::string jwt = createJWTToken(username, user->role, g_jwtSecret);

            std::ostream& ostr = response.send();
            ostr << "{\"token\":\"" << jwt << "\","
                 << "\"username\":\"" << username << "\","
                 << "\"role\":\"" << model::roleToString(user->role) << "\"}";
                 
        } catch (const Poco::Net::NotAuthenticatedException& ex) {
            (void)ex;
            response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
            response.set("WWW-Authenticate", "Basic realm=\"api\"");
            std::ostream& ostr = response.send();
            ostr << R"({"error":"Basic authentication required"})";
            if (g_httpErrors) g_httpErrors->inc();
            Poco::Timespan duration = Poco::Timestamp() - start;
            double seconds = static_cast<double>(duration.totalMicroseconds()) / 1000000.0;
            if (g_httpDuration) g_httpDuration->observe(seconds);
            Poco::Logger::get("Server").warning("401 POST /api/v1/auth from %s - Basic authentication required, %.2f ms",
                                                request.clientAddress().toString(), seconds * 1000.0);
            return;
        } catch (const std::exception& e) {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            std::ostream& ostr = response.send();
            ostr << "{\"error\":\"internal error: " << e.what() << "\"}";
            if (g_httpErrors) g_httpErrors->inc();
            Poco::Timespan duration = Poco::Timestamp() - start;
            double seconds = static_cast<double>(duration.totalMicroseconds()) / 1000000.0;
            if (g_httpDuration) g_httpDuration->observe(seconds);
            Poco::Logger::get("Server").error("500 POST /api/v1/auth from %s - %s, %.2f ms",
                                              request.clientAddress().toString(), 
                                              std::string(e.what()).c_str(), 
                                              seconds * 1000.0);
            return;
        }

        Poco::Timespan duration = Poco::Timestamp() - start;
        double seconds = static_cast<double>(duration.totalMicroseconds()) / 1000000.0;
        if (g_httpDuration) g_httpDuration->observe(seconds);
        Poco::Logger::get("Server").information("200 POST /api/v1/auth from %s, %.2f ms",
                                                request.clientAddress().toString(), seconds * 1000.0);
    }
};

} // namespace handlers