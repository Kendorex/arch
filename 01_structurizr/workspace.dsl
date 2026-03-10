workspace "Car Rental Management System" "ДЗ 01 – Документирование архитектуры в Structurizr (Вариант 21)" {

  model {
    // Роли пользователей
    guest = person "Гость" "Неавторизованный пользователь. Может просматривать доступные автомобили и регистрироваться."
    customer = person "Клиент" "Зарегистрированный пользователь. Арендует автомобили, управляет активными арендами и просматривает историю."
    fleetManager = person "Менеджер автопарка" "Сотрудник, управляющий автопарком. Может добавлять новые автомобили в парк."
    admin = person "Администратор" "Пользователь с полными правами. Может выполнять ВСЕ операции: создавать пользователей, искать пользователей по логину, искать пользователей по маске имени/фамилии, просматривать доступные автомобили, искать автомобили по классу, создавать аренды, просматривать активные аренды любого пользователя, завершать аренды, просматривать историю аренд любого пользователя."

    // Внешние системы
    paymentGateway = softwareSystem "Платежный шлюз" "Внешняя система для обработки платежей по кредитным картам и блокировки средств." "External"
    smsService = softwareSystem "SMS-сервис" "Внешний сервис для отправки SMS-уведомлений (напоминания, подтверждения)." "External"

    // Основная система
    carRentalSystem = softwareSystem "Система управления арендой автомобилей" "Позволяет клиентам арендовать автомобили, а менеджерам — управлять автопарком." {

      // Клиент
      webApp = container "Веб-приложение" "Браузерный интерфейс для всех ролей." "Single Page Application"

      // Точка входа
      apiGateway = container "API Gateway" "Единая точка входа: маршрутизация запросов, аутентификация и ограничение скорости." "Reverse proxy / API Gateway"

      // Сервисы
      userService = container "Сервис пользователей" "Управляет учетными записями: регистрация, поиск по логину, поиск по маске имени/фамилии." "Backend service"
      fleetService = container "Сервис автопарка" "Управляет автопарком: добавление автомобилей, поиск по классу, получение доступных автомобилей." "Backend service"
      bookingService = container "Сервис бронирования" "Управляет арендами: создание, завершение, история, получение активных аренд. Содержит планировщик для автоматического завершения." "Backend service"
      paymentService = container "Платежный сервис" "Интегрируется с внешним платежным шлюзом для обработки транзакций." "Backend service"
      notificationService = container "Сервис уведомлений" "Отправляет SMS-уведомления через внешнего провайдера." "Backend service"

      // Хранилища
      userDb = container "База данных пользователей" "Хранит информацию о пользователях: логин, хеш пароля, имя, фамилия, роль." "Relational Database"
      fleetDb = container "База данных автопарка" "Хранит данные об автомобилях: модель, класс, госномер, статус доступности." "Relational Database"
      bookingDb = container "База данных бронирований" "Хранит записи об арендах: ID пользователя, ID автомобиля, даты, статус, стоимость." "Relational Database"
      paymentDb = container "База данных платежей" "Хранит логи транзакций для отчетности и сверки." "Relational Database"

      // Взаимодействие пользователей с клиентом
      guest -> webApp "Открывает сайт, просматривает доступные автомобили, регистрируется" "HTTPS"
      customer -> webApp "Управляет аккаунтом, создает аренды, просматривает историю, досрочно завершает аренды" "HTTPS"
      fleetManager -> webApp "Добавляет новые автомобили в парк" "HTTPS"
      admin -> webApp "Выполняет все операции (управление пользователями, просмотр автомобилей, управление арендами)" "HTTPS"

      // Клиент -> шлюз
      webApp -> apiGateway "Вызывает API" "HTTPS/REST"

      // Шлюз -> сервисы
      apiGateway -> userService "User API (/api/users/*)" "HTTPS/REST"
      apiGateway -> fleetService "Fleet API (/api/cars/*)" "HTTPS/REST"
      apiGateway -> bookingService "Booking API (/api/rentals/*)" "HTTPS/REST"
      apiGateway -> paymentService "Payment API (/api/payments/*)" "HTTPS/REST"

      // Сервисы -> Базы данных
      userService -> userDb "Данные пользователей" "Database protocol"
      fleetService -> fleetDb "Данные об автомобилях" "Database protocol"
      bookingService -> bookingDb "Данные об арендах" "Database protocol"
      paymentService -> paymentDb "Данные о транзакциях" "Database protocol"

      // Взаимодействие между сервисами
      bookingService -> userService "Проверяет существование и данные пользователя" "HTTPS/REST"
      bookingService -> fleetService "Проверяет доступность и резервирует автомобиль" "HTTPS/REST"
      bookingService -> paymentService "Запрашивает авторизацию/списание платежа" "HTTPS/REST"

      // Асинхронные уведомления
      bookingService -> notificationService "Отправляет команду на отправку SMS" "Message Queue"
      
      // Взаимодействие с внешними системами
      paymentService -> paymentGateway "Пересылает платежные запросы" "HTTPS/REST (API Key)"
      notificationService -> smsService "Отправляет SMS-уведомления" "HTTPS/REST (API Key)"
    }
  }

  views {
    systemContext carRentalSystem "C1-SystemContext" {
      include guest
      include customer
      include fleetManager
      include admin
      include carRentalSystem
      include paymentGateway
      include smsService
      autolayout lr
      title "C1 – Системный контекст: Система управления арендой автомобилей"
      description "Гость, клиент, менеджер автопарка и администратор взаимодействуют с системой через веб-приложение. Система интегрируется с внешним платежным шлюзом и SMS-сервисом."
    }

    container carRentalSystem "C2-Containers" {
      include *
      autolayout lr
      title "C2 – Диаграмма контейнеров: Система управления арендой автомобилей"
      description "Контейнеры представляют ключевые подсистемы: управление пользователями, управление автопарком, бронирование, платежи и уведомления. Каждый сервис имеет собственную базу данных. Сервис бронирования содержит планировщик для автоматического завершения аренд."
    }

    dynamic carRentalSystem "D1-CreateRental" {
      title "D1 – Динамическая: Создание новой аренды"
      description "Клиент создает аренду через веб-приложение. Сервис бронирования проверяет пользователя, доступность автомобиля, обрабатывает платеж, сохраняет аренду и отправляет SMS-подтверждение."

      customer -> webApp "1. Выбирает автомобиль и даты, нажимает 'Арендовать'"
      webApp -> apiGateway "2. POST /api/rentals {carId, startDate, endDate}" "HTTPS/REST"
      apiGateway -> bookingService "3. Направляет запрос в Сервис бронирования" "HTTPS/REST"
      bookingService -> userService "4. Проверяет существование и данные пользователя" "HTTPS/REST"
      userService -> bookingService "5. Возвращает данные пользователя" "HTTPS/REST"
      bookingService -> fleetService "6. Проверяет доступность и резервирует автомобиль" "HTTPS/REST"
      fleetService -> bookingService "7. Возвращает подтверждение резервирования" "HTTPS/REST"
      bookingService -> paymentService "8. Запрашивает авторизацию платежа" "HTTPS/REST"
      paymentService -> paymentGateway "9. Пересылает платежный запрос" "HTTPS/REST (API Key)"
      paymentGateway -> paymentService "10. Возвращает подтверждение платежа" "HTTPS/REST (API Key)"
      paymentService -> bookingService "11. Возвращает подтверждение платежа" "HTTPS/REST"
      bookingService -> bookingDb "12. Сохраняет запись об аренде со статусом 'Активна'" "Database protocol"
      bookingService -> notificationService "13. Отправляет команду на отправку SMS-подтверждения" "Message Queue"
      notificationService -> smsService "14. Отправляет SMS с деталями аренды" "HTTPS/REST (API Key)"
      bookingService -> apiGateway "15. Возвращает 201 Created с данными аренды" "HTTPS/REST"
      apiGateway -> webApp "16. Возвращает 201 Created с данными аренды" "HTTPS/REST"

      autolayout lr
    }

    dynamic carRentalSystem "D2-AutoCompleteRental" {
      title "D2 – Динамическая: Автоматическое завершение аренды по расписанию"
      description "Когда наступает дата окончания аренды, планировщик в Сервисе бронирования автоматически завершает аренду: обновляет статус, обрабатывает финальный платеж и отправляет SMS-чек."

      bookingService -> bookingDb "1. Планировщик запрашивает активные аренды с датой окончания <= текущему времени" "Database protocol"
      bookingDb -> bookingService "2. Возвращает список аренд для завершения" "Database protocol"
      bookingService -> bookingDb "3. Обновляет статус аренды с 'Активна' на 'Завершена'" "Database protocol"
      bookingDb -> bookingService "4. Подтверждает обновление статуса" "Database protocol"
      bookingService -> paymentService "5. Запрашивает финальное списание средств" "HTTPS/REST"
      paymentService -> paymentGateway "6. Пересылает запрос на списание" "HTTPS/REST (API Key)"
      paymentGateway -> paymentService "7. Возвращает подтверждение списания" "HTTPS/REST (API Key)"
      paymentService -> bookingService "8. Возвращает подтверждение платежа" "HTTPS/REST"
      bookingService -> notificationService "9. Отправляет команду на отправку SMS-чека" "Message Queue"
      notificationService -> smsService "10. Отправляет SMS с итогами аренды и благодарностью" "HTTPS/REST (API Key)"

      autolayout lr
    }

    dynamic carRentalSystem "D3-EarlyCompleteRental" {
      title "D3 – Динамическая: Досрочное завершение аренды по запросу клиента"
      description "Клиент возвращает автомобиль раньше срока и вручную завершает аренду через веб-приложение. Система пересчитывает платеж и отправляет SMS-чек."

      customer -> webApp "1. Нажимает 'Завершить аренду досрочно' для активной аренды"
      webApp -> apiGateway "2. PUT /api/rentals/{id}/early-complete" "HTTPS/REST"
      apiGateway -> bookingService "3. Направляет запрос в Сервис бронирования" "HTTPS/REST"
      bookingService -> bookingDb "4. Проверяет возможность досрочного завершения аренды" "Database protocol"
      bookingDb -> bookingService "5. Возвращает данные аренды" "Database protocol"
      bookingService -> bookingDb "6. Обновляет статус аренды с 'Активна' на 'Завершена досрочно'" "Database protocol"
      bookingDb -> bookingService "7. Подтверждает обновление статуса" "Database protocol"
      bookingService -> paymentService "8. Запрашивает финальное списание с перерасчетом" "HTTPS/REST"
      paymentService -> paymentGateway "9. Пересылает запрос на списание" "HTTPS/REST (API Key)"
      paymentGateway -> paymentService "10. Возвращает подтверждение списания" "HTTPS/REST (API Key)"
      paymentService -> bookingService "11. Возвращает подтверждение платежа с скорректированной суммой" "HTTPS/REST"
      bookingService -> notificationService "12. Отправляет команду на отправку SMS-чека" "Message Queue"
      notificationService -> smsService "13. Отправляет SMS с итогами досрочного завершения и финальной суммой" "HTTPS/REST (API Key)"
      bookingService -> apiGateway "14. Возвращает 200 OK с итогами аренды" "HTTPS/REST"
      apiGateway -> webApp "15. Возвращает 200 OK с итогами аренды" "HTTPS/REST"

      autolayout lr
    }
  }
}