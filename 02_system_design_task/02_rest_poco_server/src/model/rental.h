#pragma once

#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace model {

// Статусы аренды
enum class RentalStatus {
    ACTIVE,      // Активна
    COMPLETED,   // Завершена (по расписанию)
    EARLY_COMPLETED, // Завершена досрочно
    CANCELLED    // Отменена
};

inline std::string statusToString(RentalStatus status) {
    switch (status) {
        case RentalStatus::ACTIVE: return "active";
        case RentalStatus::COMPLETED: return "completed";
        case RentalStatus::EARLY_COMPLETED: return "early_completed";
        case RentalStatus::CANCELLED: return "cancelled";
        default: return "unknown";
    }
}

// Класс аренды
class Rental {
public:
    std::string id;                  // Уникальный ID аренды
    std::string userId;              // ID пользователя
    std::string carId;               // ID автомобиля
    std::time_t startDate;           // Дата начала
    std::time_t endDate;             // Дата окончания (плановая)
    std::time_t actualEndDate;       // Фактическая дата окончания (0 если не завершена)
    RentalStatus status;             // Статус
    double totalPrice;               // Итоговая стоимость
    std::string paymentId;           // ID платежа
    
    // Конструктор
    Rental(const std::string& uId,
           const std::string& cId,
           std::time_t start,
           std::time_t end)
        : userId(uId)
        , carId(cId)
        , startDate(start)
        , endDate(end)
        , actualEndDate(0)
        , status(RentalStatus::ACTIVE)
        , totalPrice(0)
    {
        // Генерируем ID
        id = "RENT_" + std::to_string(start) + "_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count() % 1000
        );
        
        if (userId.empty() || carId.empty()) {
            throw std::invalid_argument("User ID and Car ID are required");
        }
        
        if (start >= end) {
            throw std::invalid_argument("End date must be after start date");
        }
    }
    
    // Конструктор по умолчанию
    Rental() : startDate(0), endDate(0), actualEndDate(0), 
               status(RentalStatus::ACTIVE), totalPrice(0) {}
    
    // Расчет количества дней аренды
    int getDaysCount() const {
        const int SECONDS_IN_DAY = 24 * 60 * 60;
        return static_cast<int>((endDate - startDate) / SECONDS_IN_DAY);
    }
    
    // Расчет фактических дней (для досрочного завершения)
    int getActualDaysCount() const {
        if (actualEndDate == 0) return getDaysCount();
        const int SECONDS_IN_DAY = 24 * 60 * 60;
        return static_cast<int>((actualEndDate - startDate) / SECONDS_IN_DAY);
    }
    
    // Проверка, активна ли аренда
    bool isActive() const {
        return status == RentalStatus::ACTIVE;
    }
    
    // Проверка, завершена ли аренда
    bool isCompleted() const {
        return status == RentalStatus::COMPLETED || 
               status == RentalStatus::EARLY_COMPLETED;
    }
    
    // Завершение аренды (досрочно)
    void completeEarly(std::time_t actualEnd) {
        if (status != RentalStatus::ACTIVE) {
            throw std::runtime_error("Rental is not active");
        }
        actualEndDate = actualEnd;
        status = RentalStatus::EARLY_COMPLETED;
    }
    
    // Завершение аренды по расписанию
    void complete() {
        if (status != RentalStatus::ACTIVE) {
            throw std::runtime_error("Rental is not active");
        }
        actualEndDate = endDate;
        status = RentalStatus::COMPLETED;
    }
    
    // Установка цены
    void setPrice(double price) {
        totalPrice = price;
    }
    
    // Проверка, нужно ли завершить аренду автоматически
    bool shouldAutoComplete(std::time_t now) const {
        return status == RentalStatus::ACTIVE && now >= endDate;
    }
    
    // Форматирование даты для JSON
    static std::string formatTime(std::time_t time) {
        if (time == 0) return "null";
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S");
        return "\"" + ss.str() + "\"";
    }
    
    // Получение JSON представления
    std::string toJson() const {
        std::stringstream ss;
        ss << "{"
           << "\"id\":\"" << id << "\","
           << "\"userId\":\"" << userId << "\","
           << "\"carId\":\"" << carId << "\","
           << "\"startDate\":" << formatTime(startDate) << ","
           << "\"endDate\":" << formatTime(endDate) << ","
           << "\"actualEndDate\":" << formatTime(actualEndDate) << ","
           << "\"status\":\"" << statusToString(status) << "\","
           << "\"totalPrice\":" << totalPrice
           << "}";
        return ss.str();
    }
};

} // namespace model