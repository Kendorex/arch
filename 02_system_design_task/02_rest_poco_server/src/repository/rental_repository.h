#pragma once

#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <ctime>
#include "../model/rental.h"

namespace repository {

class RentalRepository {
private:
    std::map<std::string, model::Rental> rentals;  // id -> Rental
    mutable std::mutex mutex;
    
public:
    // Добавление аренды
    void addRental(const model::Rental& rental) {
        std::lock_guard<std::mutex> lock(mutex);
        rentals[rental.id] = rental;
    }
    
    // Поиск аренды по ID
    const model::Rental* findById(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = rentals.find(id);
        if (it != rentals.end()) {
            return &(it->second);
        }
        return nullptr;
    }
    
    // Обновление аренды
    bool updateRental(const model::Rental& rental) {
        std::lock_guard<std::mutex> lock(mutex);
        
        if (rentals.find(rental.id) == rentals.end()) {
            return false;
        }
        
        rentals[rental.id] = rental;
        return true;
    }
    
    // Получение активных аренд пользователя
    std::vector<model::Rental> getActiveRentalsByUser(const std::string& userId) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Rental> result;
        for (const auto& pair : rentals) {
            if (pair.second.userId == userId && pair.second.isActive()) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    // Получение истории аренд пользователя (все завершенные)
    std::vector<model::Rental> getRentalHistoryByUser(const std::string& userId) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Rental> result;
        for (const auto& pair : rentals) {
            if (pair.second.userId == userId && pair.second.isCompleted()) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    // Получение всех аренд пользователя
    std::vector<model::Rental> getAllRentalsByUser(const std::string& userId) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Rental> result;
        for (const auto& pair : rentals) {
            if (pair.second.userId == userId) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    // Получение всех аренд (для администратора)
    std::vector<model::Rental> getAllRentals() const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Rental> result;
        for (const auto& pair : rentals) {
            result.push_back(pair.second);
        }
        return result;
    }
    
    // Поиск аренд, которые нужно автоматически завершить
    std::vector<model::Rental> findRentalsToAutoComplete(std::time_t now) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Rental> result;
        for (const auto& pair : rentals) {
            if (pair.second.shouldAutoComplete(now)) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
};

} // namespace repository