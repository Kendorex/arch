#pragma once

#include <string>
#include <vector>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/JWT/Token.h>
#include <Poco/JWT/Signer.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include "../model/user.h"

namespace handlers {

// Структура для хранения информации о текущем пользователе из токена
struct AuthInfo {
    std::string username;
    model::UserRole role;
    bool isValid;
    
    AuthInfo() : role(model::UserRole::GUEST), isValid(false) {}
    AuthInfo(const std::string& uname, model::UserRole r) 
        : username(uname), role(r), isValid(true) {}
};

// Класс для проверки аутентификации
class AuthMiddleware {
private:
    static std::string extractBearerToken(const Poco::Net::HTTPServerRequest& request) {
        std::string authHeader = request.get("Authorization", "");
        
        if (authHeader.empty() || authHeader.find("Bearer ") != 0) {
            return "";
        }
        
        return authHeader.substr(7);
    }
    
public:
    // Проверка и извлечение информации из токена
    static AuthInfo validateToken(const std::string& token, const std::string& jwtSecret) {
        if (token.empty() || jwtSecret.empty()) {
            return AuthInfo();
        }
        
        try {
            Poco::JWT::Signer signer(jwtSecret);
            Poco::JWT::Token jwtToken = signer.verify(token);
            
            std::string username = jwtToken.getSubject();
            
            // Исправление: getValue принимает только один аргумент
            std::string roleStr = "guest";
            if (jwtToken.payload().has("role")) {
                roleStr = jwtToken.payload().getValue<std::string>("role");
            }
            
            model::UserRole role = model::stringToRole(roleStr);
            
            return AuthInfo(username, role);
        } catch (const std::exception& e) {
            return AuthInfo();
        }
    }
    
    // Проверка аутентификации из запроса
    static AuthInfo authenticate(const Poco::Net::HTTPServerRequest& request, 
                                  const std::string& jwtSecret) {
        std::string token = extractBearerToken(request);
        return validateToken(token, jwtSecret);
    }
    
    // Отправка ошибки 401 Unauthorized
    static void sendUnauthorized(Poco::Net::HTTPServerResponse& response, 
                                 const std::string& message = "Unauthorized") {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        response.setContentType("application/json");
        std::ostream& ostr = response.send();
        ostr << "{\"error\":\"" << message << "\"}";
    }
    
    // Отправка ошибки 403 Forbidden
    static void sendForbidden(Poco::Net::HTTPServerResponse& response,
                              const std::string& message = "Forbidden") {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
        response.setContentType("application/json");
        std::ostream& ostr = response.send();
        ostr << "{\"error\":\"" << message << "\"}";
    }
    
    // Проверка роли пользователя
    static bool checkRole(const AuthInfo& auth, model::UserRole requiredRole) {
        if (!auth.isValid) return false;
        
        // Admin имеет все права
        if (auth.role == model::UserRole::ADMIN) return true;
        
        return auth.role == requiredRole;
    }
    
    // Проверка, что пользователь имеет одну из разрешенных ролей
    static bool checkRoles(const AuthInfo& auth, 
                          const std::vector<model::UserRole>& allowedRoles) {
        if (!auth.isValid) return false;
        
        // Admin имеет все права
        if (auth.role == model::UserRole::ADMIN) return true;
        
        for (const auto& role : allowedRoles) {
            if (auth.role == role) return true;
        }
        return false;
    }
};

// Утилита для создания JWT токена
inline std::string createJWTToken(const std::string& username, 
                                   model::UserRole role,
                                   const std::string& jwtSecret) {
    Poco::JWT::Token token;
    token.setType("JWT");
    token.setSubject(username);
    token.setIssuedAt(Poco::Timestamp());
    
    // Добавляем expiration (24 часа)
    Poco::Timestamp expTime;
    expTime += 24 * 60 * 60; // 24 часа в секундах
    token.setExpiration(expTime);
    
    token.payload().set("username", username);
    token.payload().set("role", model::roleToString(role));
    
    Poco::JWT::Signer signer(jwtSecret);
    return signer.sign(token, Poco::JWT::Signer::ALGO_HS256);
}

} // namespace handlers