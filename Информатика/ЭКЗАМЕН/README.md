Система управления онлайн-магазином
 Содержание
1.	Описание задачи
2.	Архитектура проекта
3.	Работа с базой данных
4.	Умные указатели и STL
5.	Логика ролей и прав доступа
6.	Аудит и история изменений
7.	Отчёт в формате CSV
8.	Сборка и запуск проекта
________________________________________
3.1 Описание задачи
Цель работы
Разработка системы управления онлайн-магазином с использованием программирования на C++ и PostgreSQL  с ролями и правами доступа к базе данных.
Краткое описание реализованной системы
Система представляет собой консольное приложение для управления интернет-магазином, включающее:
•	Управление товарами (добавление, редактирование, удаление)
•	Обработку заказов (создание, оплата, отслеживание статуса)
•	Управление пользователями с тремя ролями: администратор, менеджер, покупатель
•	Механизмы аудита и запись всех действий
•	Генерацию отчетов в формате CSV
•	Систему оплаты с поддержкой нескольких способов (карта, электронные кошельки, СБП)
Используемые технологии
•	C++ с использованием STL и умных указателей
•	PostgreSQL 18 с хранимыми процедурами, функциями и триггерами
•	libpq-fe.h для подключения БД к коду
________________________________________
3.2 Архитектура проекта
Описание классов и их взаимосвязей
1.	DatabaseConnection<T> (шаблонный класс)
o	Обеспечивает подключение к PostgreSQL
o	Реализует методы для выполнения запросов и управления транзакциями
2.	User (абстрактный базовый класс)
o	Admin (наследник) - полный доступ ко всем функциям
o	Manager (наследник) - управление заказами и складом
o	Customer (наследник) - создание и оплата заказов
3.	Order и OrderItem
o	Композиция: Order содержит вектор unique_ptr<OrderItem>
o	Реализует логику работы с заказами
4.	PaymentStrategy (абстрактный класс)
o	CreditCardPayment, EWalletPayment, SBPPayment (конкретные стратегии)
o	Реализует паттерн Strategy для различных способов оплаты
 Иерархия пользователей (Наследование)
User (базовый абстрактный класс)
    │
    ├── Admin (администратор)
    │
    ├── Manager (менеджер)
    │
    └── Customer (покупатель)
Связь: Классы Admin, Manager, Customer наследуются от User. Каждый переопределяет виртуальные методы:
•	displayMenu() - показывает свое меню
•	canPerformAction() - проверяет права доступа
•	processChoice() - обрабатывает выбор пользователя
•	
User (владелец)
    │
    └── содержит вектор ──→ Order
    │   (shared_ptr)       (может существовать без пользователя)
    │
    └── может создавать заказы через makeOrder()
--------------------------------------------------------------------
Order (владелец)
    │
    └── содержит вектор ──→ OrderItem
        (unique_ptr)       (не существует без заказа)
    │
    └── содержит ──→ Payment
        (unique_ptr) (не существует без заказа)
---------------------------------------------------------------------
Payment (использует)
    │
    └── использует ──→ PaymentStrategy
        (unique_ptr)   (стратегия может существовать отдельно)
-------------------------------------------------------------------------------
PaymentStrategy (абстрактный интерфейс)
    │
    ├── CreditCardPayment (оплата картой)
    │
    ├── EWalletPayment (электронный кошелек)
    │
    └── SBPPayment (система быстрых платежей)
-----------------------------------------------------------------------------
Все классы системы
    │
    └── зависят от ──→ DatabaseConnection<T>
        (используют для работы с БД)

Шаблонный класс DatabaseConnection<T>
cpp
template<typename T>
class DatabaseConnection {
    std::vector<std::vector<T>> executeQuery(const std::string& query);
    int executeNonQuery(const std::string& query);
    // ...  методы
};
Класс параметризован типом T, что позволяет возвращать данные в нужном формате (std::string, int, double).
________________________________________
3.3 Работа с базой данных
Структура базы данных
1.	users - пользователи системы
2.	products - товары магазина
3.	orders - заказы
4.	order_items - элементы заказов
5.	order_status_history - история изменений статусов заказов
6.	audit_log - журнал аудита всех действий
Хранимые процедуры
1. createOrder - создание заказа с проверкой наличия товара
2. updateOrderStatus - обновление статуса заказа
Функции PL/pgSQL
1.	getOrderStatus() - возвращает текущий статус заказа
2.	getUserOrderCount() - количество заказов по пользователям
3.	getTotalSpentByUser() - общая сумма покупок пользователя
4.	canReturnOrder() - проверка возможности возврата (не более 30 дней)
5.	getAuditLogByUser() - действия конкретного пользователя
Триггеры
1. Автоматическое логирование истории статусов
2. Аудит операций с товарами, заказами и пользователями
Механизм транзакций и отката
Пример использования при создании заказа:
cpp
if (!db.beginTransaction()) return;
try {
    // Проверка наличия товара
    // Создание заказа
    // Обновление остатков
    // Аудит операции
    
    if (!db.commitTransaction()) {
        throw std::runtime_error("Commit failed");
    }
} catch (...) {
    db.rollbackTransaction();
    throw;
}
________________________________________
3.4 Умные указатели и STL
Использование std::unique_ptr и std::shared_ptr
std::unique_ptr - для эксклюзивного владения
cpp
private:
    std::vector<std::unique_ptr<OrderItem>> items;  // Order владеет всеми OrderItem
    
public:
    bool addItem(int productId, const std::string& name, int quantity, double price) {
        auto item = std::make_unique<OrderItem>(
            items.size() + 1, productId, name, quantity, price
        );
        items.push_back(std::move(item));  // Перемещаем владение в вектор
        // ^^^ После этого item больше не существует, его владение передано вектору
    }
};
Только вектор items (а значит и Order) владеет этим элементом, когда заказ удаляется, все его элементы автоматически удаляются

std::shared_ptr - для совместного владения
cpp
// Агрегация - заказы могут существовать независимо от пользователя
std::vector<std::shared_ptr<Order>> orders;
orders.push_back(std::make_shared<Order>(orderId, userId, total));
Примеры использования STL алгоритмов
std::find_if - поиск заказа по ID
cpp
auto it = std::find_if(orders.begin(), orders.end(),
    [orderId](const std::shared_ptr<Order>& order) {
        return order->getOrderId() == orderId;
    });
std::copy_if - фильтрация заказов по статусу
cpp
std::vector<std::shared_ptr<Order>> filtered;
std::copy_if(orders.begin(), orders.end(), std::back_inserter(filtered),
    [statusFilter](const std::shared_ptr<Order>& order) {
        return order->getStatus() == statusFilter;
    });
std::accumulate - подсчет общей суммы
cpp
double totalSpent = std::accumulate(orders.begin(), orders.end(), 0.0,
    [](double sum, const std::shared_ptr<Order>& order) {
        return sum + order->getTotalPrice();
    });
Лямбда-выражения
Фильтрация с лямбдой
cpp
auto filterByStatus = [this](const std::string& status) -> 
    std::vector<std::shared_ptr<Order>> {
    std::vector<std::shared_ptr<Order>> filtered;
    std::copy_if(orders.begin(), orders.end(), 
                std::back_inserter(filtered),
                [status](const std::shared_ptr<Order>& order) {
                    return order->getStatus() == status;
                });
    return filtered;
};
Проверка прав доступа
cpp
std::function<bool(const std::string&)> createPermissionChecker() const {
    return [this](const std::string& action) -> bool {
        return this->canPerformAction(action);
    };
}
________________________________________
3.5 Логика ролей и прав доступа
Возможности администратора
1.	Полное управление товарами (добавление, редактирование, удаление)
2.	Просмотр всех заказов в системе
3.	Изменение статусов любых заказов
4.	Доступ к полному журналу аудита
5.	Генерация отчетов в CSV формате
6.	Управление пользователями
Возможности менеджера
1.	Просмотр заказов, ожидающих утверждения
2.	Утверждение/отклонение заказов
3.	Обновление информации о товарах (количество на складе)
4.	Просмотр истории изменений статусов заказов
Возможности покупателя
1.	Создание новых заказов
2.	Добавление/удаление товаров из заказов
3.	Просмотр истории своих заказов
4.	Оплата заказов различными способами
5.	Возврат заказов (если прошло не более 30 дней)
Ограничения доступа
Система реализует строгую модель разграничения прав доступа на основе ролей пользователей. 
Администратор обладает полными правами доступа ко всем функциям системы
Менеджер имеет ограниченный доступ, сосредоточенный на операционной деятельности
Покупатель имеет минимальные права, ограниченные личной активностью
Реализация проверки прав доступа
1.	Каждому типу пользователя отображается уникальное меню, содержащее только доступные ему функции. Например, меню администратора содержит 11 пунктов, менеджера - 8, покупателя - 9.
2.	При попытке выполнения действия система проверяет, имеет ли текущий пользователь право на его выполнение. Проверка реализована через виртуальный метод canPerformAction(), который по-разному реализован в каждом классе пользователя.
3.	Дополнительные проверки выполняются на стороне СУБД через ограничения (constraints) и условия в запросах, гарантируя, что пользователь может получать и изменять только разрешенные ему данные.
________________________________________
3.6 Аудит и история изменений
Таблица order_status_history
Записывает все изменения статусов заказов
Таблица audit_log
Система audit_log фиксирует все критически важные операции в базе данных, создавая полную историю изменений.
1) Добавление нового товара
2) Изменение информации о товаре
3) Удаление товара
4) Создание нового заказа
5) Изменение статуса заказа
6) Отмена/удаление заказа
7) Регистрация нового пользователя
8) Изменение данных пользователя
9) Удаление пользователя
Примеры записей аудита
text
1 | product | 101 | insert  | 1 | 2024-01-15 10:30:25
2 | order   | 502 | update  | 3 | 2024-01-15 11:45:12
3 | user    | 25  | insert  | 1 | 2024-01-15 14:20:33
Пример истории изменения статусов:
text
Order #502:
- 2024-01-15 10:00:00 | pending    -> processing  | Manager: Ivan
- 2024-01-15 12:30:00 | processing -> paid        | Customer: Peter
- 2024-01-15 14:00:00 | paid       -> completed   | Admin: System
________________________________________
3.7 Отчёт в формате CSV
Описание отчёта
Отчет «История изменений заказов и действий пользователей» содержит:
•	Информацию о заказах
•	Статус заказа
•	Количество изменений статуса
•	Последнюю операцию аудита
•	Время последнего изменения
SQL-функция формирования отчёта
sql
CREATE OR REPLACE FUNCTION generateOrderAuditReportCSV()
RETURNS TABLE(
    order_id INTEGER,
    customer_name VARCHAR,
    order_status VARCHAR,
    total_price DECIMAL,
    order_date TIMESTAMP,
    status_change_count BIGINT,
    last_audit_operation VARCHAR,
    last_audit_time TIMESTAMP
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        o.order_id,
        u.name AS customer_name,
        o.status AS order_status,
        o.total_price,
        o.order_date,
        COUNT(osh.history_id)::BIGINT AS status_change_count,
        MAX(al.operation) AS last_audit_operation,
        MAX(al.performed_at) AS last_audit_time
    FROM orders o
    JOIN users u ON o.user_id = u.user_id
    LEFT JOIN order_status_history osh ON o.order_id = osh.order_id
    LEFT JOIN audit_log al ON o.order_id = al.entity_id 
                           AND al.entity_type = 'order'
    GROUP BY o.order_id, u.name, o.status, o.total_price, o.order_date
    ORDER BY o.order_date DESC;
END;
$$ LANGUAGE plpgsql;
Пример содержимого CSV-файла
csv
OrderID,Customer,Status,TotalPrice,OrderDate,ItemsCount,StatusChanges,LastOperation,LastAuditTime
1001,Ivan Buyer,completed,48997.00,2024-01-15 10:30:00,3,2,update,2024-01-15 14:30:00
1002,Anna Buyer,pending,32999.00,2024-01-15 11:45:00,1,0,insert,2024-01-15 11:45:00
1003,Ivan Buyer,canceled,8999.00,2024-01-14 09:20:00,1,1,update,2024-01-14 10:15:00
________________________________________
3.8 Сборка и запуск проекта
Требования к окружению
•	Компилятор: GCC 9+ или Clang 10+ с поддержкой C++17
•	PostgreSQL: 18 с установленным сервером и клиентскими библиотеками
•	Библиотека: libpq-fe.h 
Инструкции по сборке
Файл csv с файлом cpp поместить в одну папку и запустить cpp. Перед этим соединить по инструкции из интернета PostgreSQL и Среду разработки через libpq-fe.h. В базу данных ввести большой запрос, который прикреплён к проекту, чтоб с ней можно было работать.
Пример запуска программы
----------------------------------------
MAIN MENU
----------------------------------------
Select role to login:
1. Administrator
2. Manager
3. Customer
4. Exit program
----------------------------------------
Your choice (1-4): 1
Примеры работы меню для разных ролей
Меню администратора:
text
==================================================
           ADMIN MENU
==================================================
1. Add new product
2. Update product information
3. Delete product
4. View all orders
5. View order details
6. Change order status
7. View order status history
8. View audit log
9. Generate report (CSV)
10. Manage users
11. Exit system
--------------------------------------------------
Your choice (1-11): 1
Меню покупателя:
text
==================================================
           CUSTOMER MENU
==================================================
1. Create new order
2. Add product to order
3. Remove product from order
4. View my orders
5. View order status
6. Pay for order
7. Return order
8. View order status history
9. Exit system
--------------------------------------------------
Your choice (1-9): 1
Примеры логов и истории изменений
1. Создание заказа (логи в БД):
text
[audit_log] Insert: entity_type='order', entity_id=1005, operation='insert', performed_by=3
[order_status_history] Order #1005: NULL -> 'pending', changed_by=3
2. Оплата заказа:
text
--- PAY FOR ORDER ---
Order ID: 1005

Select payment method:
1. Credit Card
2. E-Wallet
3. Fast Payment System (SBP)
Your choice (1-3): 1
Card number: 4111111111111111
Expiry date (MM/YY): 12/25
CVV: 123

Payment of 32999.00 RUB by credit card 1111
Payment successful!
3. Аудит операции:
text
--- AUDIT LOG ---
Last 20 actions:
2024-01-15 14:30:00 | Ivan Buyer | insert order #1005
2024-01-15 14:35:00 | Ivan Buyer | update order #1005
2024-01-15 14:40:00 | System Admin | update product #101
4. История статусов заказа:
text
--- ORDER STATUS HISTORY ---
Order ID: 1005

Ivan Buyer changed status from 'pending' to 'paid' at 2024-01-15 14:35:00
System Admin changed status from 'paid' to 'completed' at 2024-01-15 15:00:00
5. Генерация отчета:
text
--- GENERATE CSV REPORT ---
Report saved to file: orders_report.csv
Total records: 45

