#pragma once

#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include "../model/car.h"

namespace repository {

class CarRepository {
private:
    std::map<std::string, model::Car> cars;  // id -> Car
    mutable std::mutex mutex;
    
public:
    // Добавление автомобиля
    void addCar(const model::Car& car) {
        std::lock_guard<std::mutex> lock(mutex);
        cars[car.id] = car;
    }
    
    // Поиск автомобиля по ID
    const model::Car* findById(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = cars.find(id);
        if (it != cars.end()) {
            return &(it->second);
        }
        return nullptr;
    }
    
    // Обновление автомобиля (для изменения статуса)
    bool updateCar(const model::Car& car) {
        std::lock_guard<std::mutex> lock(mutex);
        
        if (cars.find(car.id) == cars.end()) {
            return false;
        }
        
        cars[car.id] = car;
        return true;
    }
    
    // Получение всех доступных автомобилей
    std::vector<model::Car> getAvailableCars() const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Car> result;
        for (const auto& pair : cars) {
            if (pair.second.isAvailable()) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    // Поиск автомобилей по классу
    std::vector<model::Car> findByClass(const std::string& carClass) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Car> result;
        for (const auto& pair : cars) {
            if (pair.second.matchesClass(carClass)) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    // Поиск автомобилей по маске модели
    std::vector<model::Car> findByModelMask(const std::string& mask) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Car> result;
        for (const auto& pair : cars) {
            if (pair.second.matchesModelMask(mask)) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    // Получение всех автомобилей
    std::vector<model::Car> getAllCars() const {
        std::lock_guard<std::mutex> lock(mutex);
        
        std::vector<model::Car> result;
        for (const auto& pair : cars) {
            result.push_back(pair.second);
        }
        return result;
    }
    
    // Резервирование автомобиля (блокировка)
    bool reserveCar(const std::string& carId) {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = cars.find(carId);
        if (it == cars.end()) return false;
        if (!it->second.isAvailable()) return false;
        
        model::Car carCopy = it->second;
        carCopy.reserve();
        cars[carId] = carCopy;
        return true;
    }
    
    // Освобождение автомобиля
    bool releaseCar(const std::string& carId) {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = cars.find(carId);
        if (it == cars.end()) return false;
        
        model::Car carCopy = it->second;
        carCopy.release();
        cars[carId] = carCopy;
        return true;
    }
};

} // namespace repository