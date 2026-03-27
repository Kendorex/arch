#pragma once

#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include "../model/user.h"

namespace repository {

class UserRepository {
private:
    std::map<std::string, model::User> users;  // username -> User
    mutable std::mutex mutex;  // Для thread-safety
    
public:
    // Добавление пользователя
    bool addUser(const model::User& user) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Проверяем, существует ли уже такой username
        if (users.find(user.username) != users.end()) {
            return false;
        }
        
        users[user.username] = user;
        return true;
    }
    
    // Поиск пользователя по логину
    const model::User* findByUsername(const std::string& username) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = users.find(username);
        if (it != users.end()) {
            return &(it->second);
        }
        return nullptr;
    }
    
    // Поиск пользователей по маске имени и фамилии
    std::vector<model::User> findByMask(const std::string& nameMask, 
                                        const std::string& surnameMask) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::User> result;
        for (const auto& pair : users) {
            if (pair.second.matchesNameMask(nameMask, surnameMask)) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    // Получение всех пользователей (для админа)
    std::vector<model::User> getAllUsers() const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::User> result;
        for (const auto& pair : users) {
            result.push_back(pair.second);
        }
        return result;
    }
    
    // Проверка существования пользователя
    bool exists(const std::string& username) const {
        std::lock_guard<std::mutex> lock(mutex);
        return users.find(username) != users.end();
    }
    
    // Удаление пользователя (только для тестов)
    bool removeUser(const std::string& username) {
        std::lock_guard<std::mutex> lock(mutex);
        return users.erase(username) > 0;
    }
};

} // namespace repository