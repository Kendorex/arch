#pragma once

#include <string>
#include <vector>
#include <regex>
#include <stdexcept>

namespace model {

// Роли пользователей в системе
enum class UserRole {
    GUEST,          // Неавторизованный (только просмотр)
    CUSTOMER,       // Обычный клиент
    FLEET_MANAGER,  // Менеджер автопарка
    ADMIN           // Администратор (полные права)
};

// Преобразование роли в строку для JSON
inline std::string roleToString(UserRole role) {
    switch (role) {
        case UserRole::CUSTOMER: return "customer";
        case UserRole::FLEET_MANAGER: return "fleetManager";
        case UserRole::ADMIN: return "admin";
        default: return "guest";
    }
}

// Преобразование строки в роль
inline UserRole stringToRole(const std::string& roleStr) {
    if (roleStr == "customer") return UserRole::CUSTOMER;
    if (roleStr == "fleetManager") return UserRole::FLEET_MANAGER;
    if (roleStr == "admin") return UserRole::ADMIN;
    return UserRole::GUEST;
}

// Класс пользователя
class User {
public:
    std::string username;      // Уникальный логин
    std::string passwordHash;  // Хеш пароля (в реальном проекте)
    std::string firstName;     // Имя
    std::string lastName;      // Фамилия
    UserRole role;             // Роль
    std::string email;         // Email (опционально)
    
    // Конструктор для создания нового пользователя
    User(const std::string& uname, 
         const std::string& pass,
         const std::string& fname,
         const std::string& lname,
         const std::string& roleStr = "customer",
         const std::string& mail = "")
        : username(uname)
        , passwordHash(pass)  // В реальном проекте нужно хешировать!
        , firstName(fname)
        , lastName(lname)
        , role(stringToRole(roleStr))
        , email(mail)
    {
        // Валидация входных данных
        if (username.empty()) {
            throw std::invalid_argument("Username cannot be empty");
        }
        if (firstName.empty() || lastName.empty()) {
            throw std::invalid_argument("Name cannot be empty");
        }
    }
    
    // Конструктор по умолчанию
    User() : role(UserRole::GUEST) {}
    
    // Метод для проверки пароля (упрощенная версия)
    bool checkPassword(const std::string& password) const {
        // В реальном проекте используйте bcrypt или другую хеш-функцию
        return passwordHash == password;
    }
    
    // Получение полного имени
    std::string getFullName() const {
        return firstName + " " + lastName;
    }
    
    // Проверка, имеет ли пользователь доступ к операции
    bool hasRole(UserRole requiredRole) const {
        // Admin имеет все права
        if (role == UserRole::ADMIN) return true;
        
        // Проверяем по иерархии прав
        if (requiredRole == UserRole::CUSTOMER) {
            return role == UserRole::CUSTOMER || role == UserRole::FLEET_MANAGER;
        }
        if (requiredRole == UserRole::FLEET_MANAGER) {
            return role == UserRole::FLEET_MANAGER;
        }
        
        return role == requiredRole;
    }
    
    // Проверка соответствия маске (для поиска)
    bool matchesNameMask(const std::string& nameMask, const std::string& surnameMask) const {
        // Преобразуем маски в регулярные выражения
        // Маска может содержать * для любого количества символов
        auto buildRegex = [](const std::string& mask) -> std::regex {
            std::string pattern;
            for (char c : mask) {
                if (c == '*') {
                    pattern += ".*";
                } else if (c == '?') {
                    pattern += ".";
                } else {
                    // Экранируем специальные символы regex
                    if (c == '.' || c == '+' || c == '^' || c == '$' || 
                        c == '[' || c == ']' || c == '(' || c == ')' ||
                        c == '|' || c == '\\') {
                        pattern += '\\';
                    }
                    pattern += c;
                }
            }
            return std::regex(pattern, std::regex::icase);
        };
        
        bool nameMatch = nameMask.empty() || 
                         std::regex_match(firstName, buildRegex(nameMask));
        bool surnameMatch = surnameMask.empty() || 
                            std::regex_match(lastName, buildRegex(surnameMask));
        
        return nameMatch && surnameMatch;
    }
};

} // namespace model