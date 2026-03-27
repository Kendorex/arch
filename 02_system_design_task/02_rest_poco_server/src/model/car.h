#pragma once

#include <string>
#include <regex>
#include <stdexcept>
#include <chrono>

namespace model {

// Класс автомобиля
class Car {
public:
    std::string id;              // Уникальный ID (госномер или сгенерированный)
    std::string model;           // Модель
    std::string licensePlate;    // Госномер
    std::string carClass;        // Класс: compact, midsize, luxury, suv, etc.
    bool available;              // Доступен ли для аренды
    double pricePerDay;          // Цена за день в USD
    int year;                    // Год выпуска
    std::string color;           // Цвет
    
    // Конструктор
    Car(const std::string& mdl,
        const std::string& plate,
        const std::string& cls,
        double price = 50.0,
        int yr = 2020,
        const std::string& clr = "White")
        : model(mdl)
        , licensePlate(plate)
        , carClass(cls)
        , available(true)
        , pricePerDay(price)
        , year(yr)
        , color(clr)
    {
        // Генерируем ID из госномера + случайное число
        id = licensePlate + "_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count() % 10000
        );
        
        if (model.empty() || licensePlate.empty()) {
            throw std::invalid_argument("Model and license plate are required");
        }
        
        if (pricePerDay <= 0) {
            throw std::invalid_argument("Price must be positive");
        }
    }
    
    // Конструктор по умолчанию
    Car() : available(true), pricePerDay(0), year(0) {}
    
    // Метод для проверки доступности
    bool isAvailable() const {
        return available;
    }
    
    // Метод для бронирования автомобиля
    void reserve() {
        if (!available) {
            throw std::runtime_error("Car is already reserved");
        }
        available = false;
    }
    
    // Метод для освобождения автомобиля
    void release() {
        available = true;
    }
    
    // Проверка соответствия классу
    bool matchesClass(const std::string& className) const {
        if (className.empty()) return true;
        return carClass == className;
    }
    
    // Проверка соответствия маске модели
    bool matchesModelMask(const std::string& mask) const {
        if (mask.empty()) return true;
        
        // Простое сравнение с учетом * и ?
        auto buildRegex = [](const std::string& m) -> std::regex {
            std::string pattern;
            for (char c : m) {
                if (c == '*') pattern += ".*";
                else if (c == '?') pattern += ".";
                else pattern += c;
            }
            return std::regex(pattern, std::regex::icase);
        };
        
        return std::regex_match(model, buildRegex(mask));
    }
    
    // Получение информации для JSON
    std::string toJson() const {
        return "{"
            "\"id\":\"" + id + "\","
            "\"model\":\"" + model + "\","
            "\"licensePlate\":\"" + licensePlate + "\","
            "\"class\":\"" + carClass + "\","
            "\"available\":" + (available ? "true" : "false") + ","
            "\"pricePerDay\":" + std::to_string(pricePerDay) + ","
            "\"year\":" + std::to_string(year) + ","
            "\"color\":\"" + color + "\""
        "}";
    }
};

} // namespace model