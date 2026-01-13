#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <numeric>
#include <limits>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include <fstream>
#include <libpq-fe.h>

// ========== HELPER FUNCTIONS FOR WORKING WITH LIBPQ ==========

class PGResult {
private:
    PGresult* res;

public:
    PGResult(PGresult* r = nullptr) : res(r) {}
    ~PGResult() { if (res) PQclear(res); }

    PGresult* get() { return res; }
    PGresult* release() {
        PGresult* temp = res;
        res = nullptr;
        return temp;
    }

    operator bool() const { return res != nullptr; }
    ExecStatusType status() const {
        return res ? PQresultStatus(res) : PGRES_FATAL_ERROR;
    }

    int rowCount() const { return res ? PQntuples(res) : 0; }
    int colCount() const { return res ? PQnfields(res) : 0; }

    std::string getValue(int row, int col) const {
        if (!res || row >= rowCount() || col >= colCount())
            return "";
        const char* val = PQgetvalue(res, row, col);
        return val ? std::string(val) : "";
    }

    std::string getValue(int row, const char* colName) const {
        if (!res || row >= rowCount()) return "";
        int col = PQfnumber(res, colName);
        if (col == -1) return "";
        return getValue(row, col);
    }

    bool isNull(int row, int col) const {
        if (!res || row >= rowCount() || col >= colCount())
            return true;
        return PQgetisnull(res, row, col);
    }

    void print() const {
        if (!res) {
            std::cout << "Result is empty" << std::endl;
            return;
        }

        int rows = rowCount();
        int cols = colCount();

        std::cout << "Result (" << rows << " rows, " << cols << " columns):" << std::endl;

        // Headers
        for (int c = 0; c < cols; c++) {
            std::cout << std::setw(15) << std::left << PQfname(res, c) << " | ";
        }
        std::cout << std::endl << std::string(cols * 18, '-') << std::endl;

        // Data
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                std::cout << std::setw(15) << std::left << getValue(r, c) << " | ";
            }
            std::cout << std::endl;
        }
    }
};

// ========== DATABASE CLASSES ==========

// Template class for PostgreSQL connection via libpq-fe.h
template<typename T>
class DatabaseConnection {
private:
    PGconn* conn;
    bool inTransaction;
    std::string lastError;

    void setError(const std::string& msg) {
        lastError = msg;
        if (conn) {
            const char* pqError = PQerrorMessage(conn);
            if (pqError && strlen(pqError) > 0) {
                lastError += ": " + std::string(pqError);
            }
        }
    }

public:
    // Constructor (Requirement: 1.1.1)
    explicit DatabaseConnection(const std::string& connStr)
        : conn(nullptr), inTransaction(false) {

        conn = PQconnectdb(connStr.c_str());

        if (PQstatus(conn) != CONNECTION_OK) {
            setError("Failed to connect to database");
            PQfinish(conn);
            conn = nullptr;
        }
        else {
            std::cout << "Successfully connected to DB: " << PQdb(conn) << std::endl;
        }
    }

    ~DatabaseConnection() {
        if (inTransaction) {
            executeNonQuery("ROLLBACK");
        }
        if (conn) {
            PQfinish(conn);
        }
    }

    // Execute SQL query with result return (Requirement: 1.1.2)
    std::vector<std::vector<T>> executeQuery(const std::string& query) {
        std::vector<std::vector<T>> result;

        if (!conn) {
            setError("No database connection");
            return result;
        }

        PGResult res(PQexec(conn, query.c_str()));

        if (res.status() != PGRES_TUPLES_OK && res.status() != PGRES_COMMAND_OK) {
            setError("Query execution error: " + query);
            return result;
        }

        int rows = res.rowCount();
        int cols = res.colCount();

        for (int r = 0; r < rows; r++) {
            std::vector<T> row;
            for (int c = 0; c < cols; c++) {
                std::string val = res.getValue(r, c);
                if constexpr (std::is_same_v<T, std::string>) {
                    row.push_back(val);
                }
                else if constexpr (std::is_same_v<T, int>) {
                    row.push_back(val.empty() ? 0 : std::stoi(val));
                }
                else if constexpr (std::is_same_v<T, double>) {
                    row.push_back(val.empty() ? 0.0 : std::stod(val));
                }
            }
            result.push_back(row);
        }

        return result;
    }

    // Execute SQL queries without data return (Requirement: 1.1.3)
    int executeNonQuery(const std::string& query) {
        if (!conn) {
            setError("No database connection");
            return 0;
        }

        PGResult res(PQexec(conn, query.c_str()));

        if (res.status() != PGRES_COMMAND_OK && res.status() != PGRES_TUPLES_OK) {
            setError("Command execution error: " + query);
            return 0;
        }

        const char* affected = PQcmdTuples(res.get());
        return affected[0] ? std::stoi(affected) : 1;
    }

    // Start transaction (Requirement: 1.1.4)
    bool beginTransaction() {
        if (inTransaction) {
            setError("Transaction already started");
            return false;
        }

        if (executeNonQuery("BEGIN") > 0) {
            inTransaction = true;
            return true;
        }
        return false;
    }

    // Complete transaction (Requirement: 1.1.5)
    bool commitTransaction() {
        if (!inTransaction) {
            setError("No active transaction");
            return false;
        }

        if (executeNonQuery("COMMIT") > 0) {
            inTransaction = false;
            return true;
        }
        return false;
    }

    // Rollback transaction (Requirement: 1.1.6)
    bool rollbackTransaction() {
        if (!inTransaction) {
            setError("No active transaction to rollback");
            return false;
        }

        if (executeNonQuery("ROLLBACK") > 0) {
            inTransaction = false;
            return true;
        }
        return false;
    }

    // Create stored procedure or function (Requirement: 1.1.7)
    bool createFunction(const std::string& functionSql) {
        return executeNonQuery(functionSql) > 0;
    }

    // Create trigger (Requirement: 1.1.8)
    bool createTrigger(const std::string& triggerSql) {
        return executeNonQuery(triggerSql) > 0;
    }

    // Check current transaction status (Requirement: 1.1.9)
    std::string getTransactionStatus() const {
        if (!conn) return "No connection";
        if (inTransaction) return "Transaction active";
        return "No active transaction";
    }

    // Helper methods
    bool isConnected() const { return conn && PQstatus(conn) == CONNECTION_OK; }
    std::string getLastError() const { return lastError; }
    bool isInTransaction() const { return inTransaction; }

    // Execute stored procedure
    bool executeProcedure(const std::string& procName, const std::vector<std::string>& params) {
        std::stringstream query;
        query << "CALL " << procName << "(";

        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) query << ", ";
            // Check if quotes are needed (for strings)
            try {
                std::stod(params[i]); // If converts to number - no quotes
                query << params[i];
            }
            catch (...) {
                query << "'" << params[i] << "'";
            }
        }
        query << ")";

        return executeNonQuery(query.str()) > 0;
    }
};

// ========== PAYMENT STRATEGY CLASSES ==========

class PaymentStrategy {
public:
    virtual ~PaymentStrategy() = default;
    virtual bool pay(double amount) = 0;  // Pure virtual function
    virtual std::string getName() const = 0;
};

class CreditCardPayment : public PaymentStrategy {
private:
    std::string cardNumber;
    std::string expiryDate;
    std::string cvv;

public:
    CreditCardPayment(const std::string& card, const std::string& expiry, const std::string& cvv)
        : cardNumber(card), expiryDate(expiry), cvv(cvv) {
    }

    bool pay(double amount) override {
        std::cout << "Payment of " << amount << " RUB by credit card "
            << cardNumber.substr(cardNumber.length() - 4) << std::endl;
        return true;
    }

    std::string getName() const override {
        return "Credit Card";
    }
};

class EWalletPayment : public PaymentStrategy {
private:
    std::string walletId;
    std::string provider;

public:
    EWalletPayment(const std::string& id, const std::string& prov)
        : walletId(id), provider(prov) {
    }

    bool pay(double amount) override {
        std::cout << "Payment of " << amount << " RUB via " << provider
            << " wallet " << walletId << std::endl;
        return true;
    }

    std::string getName() const override {
        return "E-Wallet (" + provider + ")";
    }
};

class SBPPayment : public PaymentStrategy {
private:
    std::string phoneNumber;
    std::string bank;

public:
    SBPPayment(const std::string& phone, const std::string& bank)
        : phoneNumber(phone), bank(bank) {
    }

    bool pay(double amount) override {
        std::cout << "Payment of " << amount << " RUB via SBP (bank: "
            << bank << ", phone: " << phoneNumber << ")" << std::endl;
        return true;
    }

    std::string getName() const override {
        return "Fast Payment System";
    }
};

// ========== ORDER CLASSES ==========

class OrderItem {
private:
    int orderItemId;
    int productId;
    std::string productName;
    int quantity;
    double price;

public:
    OrderItem(int id, int prodId, const std::string& name, int qty, double pr)
        : orderItemId(id), productId(prodId), productName(name), quantity(qty), price(pr) {
    }

    double getTotal() const {
        return quantity * price;
    }

    void display() const {
        std::cout << "  " << productName << " x" << quantity << " - "
            << price << " RUB (total: " << getTotal() << " RUB)" << std::endl;
    }

    int getProductId() const { return productId; }
    int getQuantity() const { return quantity; }
    void setQuantity(int qty) { quantity = qty; }
};

class Payment {
private:
    int paymentId;
    double amount;
    std::string method;
    std::string status;
    std::time_t paymentDate;

public:
    Payment(int id, double amt, const std::string& meth)
        : paymentId(id), amount(amt), method(meth), status("waiting"),
        paymentDate(std::time(nullptr)) {
    }

    bool process(std::unique_ptr<PaymentStrategy> strategy) {
        if (strategy->pay(amount)) {
            status = "success";
            std::cout << "Payment successful!" << std::endl;
            return true;
        }
        status = "error";
        return false;
    }

    std::string getStatus() const { return status; }
};

class Order {
private:
    int orderId;
    int userId;
    std::string status;
    double totalPrice;
    std::time_t orderDate;
    std::vector<std::unique_ptr<OrderItem>> items;  // Composition
    std::unique_ptr<Payment> payment;               // Composition via unique_ptr

public:
    Order(int id, int uid, double total = 0.0)
        : orderId(id), userId(uid), status("waiting"), totalPrice(total),
        orderDate(std::time(nullptr)), payment(nullptr) {
    }

    // Add item to order
    bool addItem(int productId, const std::string& name, int quantity, double price) {
        auto item = std::make_unique<OrderItem>(items.size() + 1, productId, name, quantity, price);
        items.push_back(std::move(item));
        totalPrice += quantity * price;
        return true;
    }

    // Remove item from order
    bool removeItem(int productId) {
        auto it = std::remove_if(items.begin(), items.end(),
            [productId](const std::unique_ptr<OrderItem>& item) {
                return item->getProductId() == productId;
            });

        if (it != items.end()) {
            items.erase(it, items.end());
            return true;
        }
        return false;
    }

    // Cancel order
    bool cancel() {
        if (status == "waiting" || status == "processing") {
            status = "canceled";
            std::cout << "Order #" << orderId << " canceled." << std::endl;
            return true;
        }
        return false;
    }

    // Return order
    bool returnOrder() {
        if (status == "completed") {
            // Check: no more than 30 days have passed
            std::time_t now = std::time(nullptr);
            double days = difftime(now, orderDate) / (60 * 60 * 24);

            if (days <= 30) {
                status = "returned";
                std::cout << "Order #" << orderId << " returned. Refund amount: "
                    << totalPrice << " RUB." << std::endl;
                return true;
            }
            else {
                std::cout << "Return not possible: more than 30 days have passed." << std::endl;
            }
        }
        return false;
    }

    // Pay for order
    bool makePayment(std::unique_ptr<PaymentStrategy> strategy) {
        payment = std::make_unique<Payment>(orderId, totalPrice, strategy->getName());
        if (payment->process(std::move(strategy))) {
            if (status == "waiting") {
                status = "paid";
            }
            return true;
        }
        return false;
    }

    // Update status
    bool updateStatus(const std::string& newStatus) {
        std::vector<std::string> validStatuses = { "waiting", "processing", "paid",
                                                  "shipped", "completed", "canceled", "returned" };

        if (std::find(validStatuses.begin(), validStatuses.end(), newStatus) != validStatuses.end()) {
            status = newStatus;
            return true;
        }
        return false;
    }

    // Lambda function for filtering order items
    std::vector<OrderItem*> filterItems(std::function<bool(const OrderItem&)> predicate) {
        std::vector<OrderItem*> result;
        for (const auto& item : items) {
            if (predicate(*item)) {
                result.push_back(item.get());
            }
        }
        return result;
    }

    // Getters
    int getOrderId() const { return orderId; }
    int getUserId() const { return userId; }
    std::string getStatus() const { return status; }
    double getTotalPrice() const { return totalPrice; }
    std::time_t getOrderDate() const { return orderDate; }
    const std::vector<std::unique_ptr<OrderItem>>& getItems() const { return items; }

    void display() const {
        std::cout << "\n=== Order #" << orderId << " ===" << std::endl;
        std::cout << "Status: " << status << std::endl;
        std::cout << "Amount: " << totalPrice << " RUB" << std::endl;

        std::cout << "Products:" << std::endl;
        for (const auto& item : items) {
            item->display();
        }

        if (payment) {
            std::cout << "Payment: " << payment->getStatus() << std::endl;
        }
    }
};

// ========== BASE USER CLASS ==========

class User {
protected:
    int userId;
    std::string name;
    std::string email;
    std::string role;
    std::string passwordHash;
    int loyaltyLevel;
    std::vector<std::shared_ptr<Order>> orders;  // Aggregation via shared_ptr

public:
    User(int id, const std::string& name, const std::string& email,
        const std::string& role, const std::string& pwdHash, int loyalty = 0)
        : userId(id), name(name), email(email), role(role),
        passwordHash(pwdHash), loyaltyLevel(loyalty) {
    }

    virtual ~User() = default;

    // Pure virtual functions (Requirement: OOP principles)
    virtual void displayMenu() = 0;
    virtual bool canPerformAction(const std::string& action) const = 0;
    virtual void processChoice(int choice, DatabaseConnection<std::string>& db) = 0;

    std::shared_ptr<Order> makeOrder(int orderId, double totalPrice = 0.0) {
        auto order = std::make_shared<Order>(orderId, userId, totalPrice);
        orders.push_back(order);
        return order;
    }

    // Create order (Requirement: 3.1.1 createOrder)
    std::shared_ptr<Order> createOrder(int orderId, double totalPrice = 0.0) {
        auto order = std::make_shared<Order>(orderId, userId, totalPrice);
        orders.push_back(order);
        return order;
    }

    // View order status (Requirement: 3.1.1 viewOrderStatus)
    std::string viewOrderStatus(int orderId) const {
        auto it = std::find_if(orders.begin(), orders.end(),
            [orderId](const std::shared_ptr<Order>& order) {
                return order->getOrderId() == orderId;
            });

        if (it != orders.end()) {
            return (*it)->getStatus();
        }
        return "Order not found";
    }

    // Cancel order (Requirement: 3.1.1 cancelOrder)
    bool cancelOrder(int orderId, DatabaseConnection<std::string>& db) {
        auto it = std::find_if(orders.begin(), orders.end(),
            [orderId](const std::shared_ptr<Order>& order) {
                return order->getOrderId() == orderId;
            });

        if (it != orders.end() && (*it)->cancel()) {
            // Logging to DB via procedure
            std::stringstream query;
            query << "CALL updateOrderStatus(" << orderId << ", 'canceled', " << userId << ")";
            db.executeNonQuery(query.str());
            return true;
        }
        return false;
    }

    // Add order (aggregation)
    void addOrder(std::shared_ptr<Order> order) { orders.push_back(order); }

    // Getters
    int getUserId() const { return userId; }
    std::string getName() const { return name; }
    std::string getEmail() const { return email; }
    std::string getRole() const { return role; }
    std::string getPasswordHash() const { return passwordHash; }
    int getLoyaltyLevel() const { return loyaltyLevel; }
    const std::vector<std::shared_ptr<Order>>& getOrders() const { return orders; }

    // Lambda for permission checking (Requirement: 5.2.3)
    std::function<bool(const std::string&)> createPermissionChecker() const {
        return [this](const std::string& action) -> bool {
            return this->canPerformAction(action);
            };
    }

    // Lambda for filtering orders by status (Requirement: 5.2.1)
    std::function<std::vector<std::shared_ptr<Order>>()> createOrderFilter(const std::string& statusFilter) {
        return [this, statusFilter]() {
            std::vector<std::shared_ptr<Order>> filtered;
            std::copy_if(orders.begin(), orders.end(), std::back_inserter(filtered),
                [statusFilter](const std::shared_ptr<Order>& order) {
                    return order->getStatus() == statusFilter;
                });
            return filtered;
            };
    }

    // Calculate total spent via std::accumulate (Requirement: 5.2.2)
    double calculateTotalSpent() const {
        return std::accumulate(orders.begin(), orders.end(), 0.0,
            [](double total, const std::shared_ptr<Order>& order) {
                return total + order->getTotalPrice();
            });
    }
};

// ========== ADMIN CLASS ==========

class Admin : public User {
public:
    Admin(int id, const std::string& name, const std::string& email, const std::string& pwdHash)
        : User(id, name, email, "admin", pwdHash, 1) {
    }

    void displayMenu() override {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "           ADMIN MENU" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        std::cout << "1. Add new product" << std::endl;
        std::cout << "2. Update product information" << std::endl;
        std::cout << "3. Delete product" << std::endl;
        std::cout << "4. View all orders" << std::endl;
        std::cout << "5. View order details" << std::endl;
        std::cout << "6. Change order status" << std::endl;
        std::cout << "7. View order status history" << std::endl;
        std::cout << "8. View audit log" << std::endl;
        std::cout << "9. Generate report (CSV)" << std::endl;
        std::cout << "10. Manage users" << std::endl;
        std::cout << "11. Exit system" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        std::cout << "Your choice (1-11): ";
    }

    bool canPerformAction(const std::string& action) const override {
        static const std::vector<std::string> allowedActions = {
            "add_product", "update_product", "delete_product", "view_all_orders",
            "view_order_details", "update_order_status", "view_order_history",
            "view_audit_log", "generate_report", "manage_users"
        };
        return std::find(allowedActions.begin(), allowedActions.end(), action) != allowedActions.end();
    }

    void processChoice(int choice, DatabaseConnection<std::string>& db) override {
        switch (choice) {
        case 1: addProduct(db); break;
        case 2: updateProduct(db); break;
        case 3: deleteProduct(db); break;
        case 4: viewAllOrders(db); break;
        case 5: viewOrderDetails(db); break;
        case 6: updateOrderStatus(db); break;
        case 7: viewOrderHistory(db); break;
        case 8: viewAuditLog(db); break;
        case 9: generateReport(db); break;
        case 10: manageUsers(db); break;
        case 11: std::cout << "Exiting system..." << std::endl; break;
        default: std::cout << "Invalid choice!" << std::endl;
        }
    }

private:
    // Admin methods (Requirement: 3.2.1)
    void addProduct(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- ADD NEW PRODUCT ---" << std::endl;

        std::string name, priceStr, quantityStr;
        std::cin.ignore();

        std::cout << "Product name: ";
        std::getline(std::cin, name);

        std::cout << "Price: ";
        std::getline(std::cin, priceStr);

        std::cout << "Stock quantity: ";
        std::getline(std::cin, quantityStr);

        try {
            if (!db.beginTransaction()) {
                std::cerr << "Transaction start error: " << db.getLastError() << std::endl;
                return;
            }

            std::stringstream query;
            query << "INSERT INTO products (name, price, stock_quantity) VALUES ('"
                << name << "', " << priceStr << ", " << quantityStr << ")";

            if (db.executeNonQuery(query.str()) <= 0) {
                throw std::runtime_error("Error adding product");
            }

            // Get ID of added product
            auto result = db.executeQuery("SELECT lastval()");
            std::string productId = !result.empty() ? result[0][0] : "0";

            // Audit operation
            std::stringstream auditQuery;
            auditQuery << "INSERT INTO audit_log (entity_type, entity_id, operation, performed_by) "
                << "VALUES ('product', " << productId << ", 'insert', "
                << userId << ")";
            db.executeNonQuery(auditQuery.str());

            if (!db.commitTransaction()) {
                throw std::runtime_error("Transaction commit error");
            }

            std::cout << "Product successfully added! ID: " << productId << std::endl;

        }
        catch (const std::exception& e) {
            db.rollbackTransaction();
            std::cerr << "Error: " << e.what() << std::endl;
            if (!db.getLastError().empty()) {
                std::cerr << "DB details: " << db.getLastError() << std::endl;
            }
        }
    }

    void updateProduct(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- UPDATE PRODUCT ---" << std::endl;
        std::string id, field, value;

        std::cout << "Product ID: ";
        std::cin >> id;

        std::cout << "Field to update (name/price/stock_quantity): ";
        std::cin >> field;

        std::cout << "New value: ";
        std::cin >> value;

        try {
            if (!db.beginTransaction()) {
                std::cerr << "Transaction start error" << std::endl;
                return;
            }

            std::stringstream query;
            query << "UPDATE products SET " << field << " = ";

            if (field == "name") {
                query << "'" << value << "'";
            }
            else {
                query << value;
            }
            query << " WHERE product_id = " << id;

            if (db.executeNonQuery(query.str()) <= 0) {
                throw std::runtime_error("Product not found or not updated");
            }

            // Audit
            std::stringstream auditQuery;
            auditQuery << "INSERT INTO audit_log (entity_type, entity_id, operation, performed_by) "
                << "VALUES ('product', " << id << ", 'update', " << userId << ")";
            db.executeNonQuery(auditQuery.str());

            if (!db.commitTransaction()) {
                throw std::runtime_error("Transaction commit error");
            }

            std::cout << "Product updated!" << std::endl;

        }
        catch (const std::exception& e) {
            db.rollbackTransaction();
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void deleteProduct(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- DELETE PRODUCT ---" << std::endl;
        std::string id;

        std::cout << "Product ID to delete: ";
        std::cin >> id;

        try {
            if (!db.beginTransaction()) {
                std::cerr << "Transaction start error" << std::endl;
                return;
            }

            std::stringstream query;
            query << "DELETE FROM products WHERE product_id = " << id;

            if (db.executeNonQuery(query.str()) <= 0) {
                throw std::runtime_error("Product not found or not deleted");
            }

            // Audit
            std::stringstream auditQuery;
            auditQuery << "INSERT INTO audit_log (entity_type, entity_id, operation, performed_by) "
                << "VALUES ('product', " << id << ", 'delete', " << userId << ")";
            db.executeNonQuery(auditQuery.str());

            if (!db.commitTransaction()) {
                throw std::runtime_error("Transaction commit error");
            }

            std::cout << "Product deleted!" << std::endl;

        }
        catch (const std::exception& e) {
            db.rollbackTransaction();
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void viewAllOrders(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- ALL ORDERS IN SYSTEM ---" << std::endl;

        try {
            auto result = db.executeQuery(
                "SELECT o.order_id, u.name, o.status, o.total_price, "
                "to_char(o.order_date, 'YYYY-MM-DD HH24:MI:SS') "
                "FROM orders o JOIN users u ON o.user_id = u.user_id "
                "ORDER BY o.order_date DESC LIMIT 20");

            if (result.empty()) {
                std::cout << "No orders." << std::endl;
                return;
            }

            // Using STL algorithms for output formatting
            std::for_each(result.begin(), result.end(),
                [](const std::vector<std::string>& row) {
                    std::cout << "Order #" << std::setw(6) << std::left << row[0]
                        << " | Customer: " << std::setw(20) << std::left << row[1]
                        << " | Status: " << std::setw(12) << std::left << row[2]
                        << " | Amount: " << std::setw(10) << std::left << row[3] << " RUB"
                        << " | Date: " << row[4] << std::endl;
                });

            // Calculate statistics via std::accumulate
            double totalRevenue = std::accumulate(result.begin(), result.end(), 0.0,
                [](double sum, const std::vector<std::string>& row) {
                    try {
                        return sum + std::stod(row[3]);
                    }
                    catch (...) {
                        return sum;
                    }
                });

            std::cout << "\nTotal revenue: " << totalRevenue << " RUB" << std::endl;
            std::cout << "Total orders: " << result.size() << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void viewOrderDetails(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- ORDER DETAILS ---" << std::endl;
        std::string orderId;

        std::cout << "Order ID: ";
        std::cin >> orderId;

        try {
            // Get order information
            auto orderInfo = db.executeQuery(
                "SELECT o.order_id, u.name, o.status, o.total_price, "
                "to_char(o.order_date, 'YYYY-MM-DD HH24:MI:SS') "
                "FROM orders o JOIN users u ON o.user_id = u.user_id "
                "WHERE o.order_id = " + orderId);

            if (orderInfo.empty()) {
                std::cout << "Order not found!" << std::endl;
                return;
            }

            std::cout << "\nOrder #" << orderInfo[0][0] << std::endl;
            std::cout << "Customer: " << orderInfo[0][1] << std::endl;
            std::cout << "Status: " << orderInfo[0][2] << std::endl;
            std::cout << "Amount: " << orderInfo[0][3] << " RUB" << std::endl;
            std::cout << "Date: " << orderInfo[0][4] << std::endl;

            // Get order items
            auto items = db.executeQuery(
                "SELECT p.name, oi.quantity, oi.price "
                "FROM order_items oi JOIN products p ON oi.product_id = p.product_id "
                "WHERE oi.order_id = " + orderId);

            std::cout << "\nProducts in order:" << std::endl;
            for (const auto& item : items) {
                std::cout << "  " << item[0] << " x" << item[1]
                    << " - " << item[2] << " RUB" << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void updateOrderStatus(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- CHANGE ORDER STATUS ---" << std::endl;
        std::string orderId, newStatus;

        std::cout << "Order ID: ";
        std::cin >> orderId;

        std::cout << "New status (pending/completed/canceled/returned): ";
        std::cin >> newStatus;

        try {
            // Using stored procedure for status update
            std::vector<std::string> params = { orderId, newStatus, std::to_string(userId) };
            if (!db.executeProcedure("updateOrderStatus", params)) {
                throw std::runtime_error("Error updating status: " + db.getLastError());
            }

            std::cout << "Order status updated!" << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void viewOrderHistory(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- ORDER STATUS HISTORY ---" << std::endl;
        std::string orderId;

        std::cout << "Order ID: ";
        std::cin >> orderId;

        try {
            auto history = db.executeQuery(
                "SELECT old_status, new_status, "
                "to_char(changed_at, 'YYYY-MM-DD HH24:MI:SS'), u.name "
                "FROM order_status_history h JOIN users u ON h.changed_by = u.user_id "
                "WHERE h.order_id = " + orderId + " ORDER BY changed_at DESC");

            if (history.empty()) {
                std::cout << "No status change history found." << std::endl;
                return;
            }

            std::cout << "\nStatus change history for order #" << orderId << ":" << std::endl;
            for (const auto& record : history) {
                std::cout << record[3] << " changed status from '" << record[0]
                    << "' to '" << record[1] << "' at " << record[2] << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void viewAuditLog(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- AUDIT LOG ---" << std::endl;

        try {
            auto logs = db.executeQuery(
                "SELECT entity_type, entity_id, operation, u.name, "
                "to_char(performed_at, 'YYYY-MM-DD HH24:MI:SS') "
                "FROM audit_log a JOIN users u ON a.performed_by = u.user_id "
                "ORDER BY performed_at DESC LIMIT 20");

            if (logs.empty()) {
                std::cout << "No audit log entries." << std::endl;
                return;
            }

            std::cout << "Last 20 actions:" << std::endl;
            for (const auto& log : logs) {
                std::cout << log[4] << " | " << log[3] << " | " << log[2]
                    << " " << log[0] << " #" << log[1] << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void generateReport(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- GENERATE CSV REPORT ---" << std::endl;

        try {
            auto report = db.executeQuery(
                "SELECT o.order_id, u.name, o.status, "
                "o.total_price, to_char(o.order_date, 'YYYY-MM-DD HH24:MI:SS'), "
                "(SELECT COUNT(*) FROM order_items WHERE order_id = o.order_id) "
                "FROM orders o JOIN users u ON o.user_id = u.user_id "
                "WHERE o.order_date >= CURRENT_DATE - INTERVAL '30 days' "
                "ORDER BY o.order_date DESC");

            if (report.empty()) {
                std::cout << "No data for report." << std::endl;
                return;
            }

            // Save to CSV file
            std::ofstream csvFile("orders_report.csv");
            csvFile << "OrderID,Customer,Status,TotalPrice,OrderDate,ItemsCount\n";

            for (const auto& row : report) {
                for (size_t i = 0; i < row.size(); ++i) {
                    // Escape commas in strings
                    if (row[i].find(',') != std::string::npos) {
                        csvFile << "\"" << row[i] << "\"";
                    }
                    else {
                        csvFile << row[i];
                    }
                    if (i < row.size() - 1) csvFile << ",";
                }
                csvFile << "\n";
            }

            csvFile.close();
            std::cout << "Report saved to file: orders_report.csv" << std::endl;
            std::cout << "Total records: " << report.size() << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void manageUsers(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- USER MANAGEMENT ---" << std::endl;

        try {
            auto users = db.executeQuery(
                "SELECT user_id, name, email, role, loyalty_level FROM users ORDER BY user_id");

            if (users.empty()) {
                std::cout << "No users." << std::endl;
                return;
            }

            std::cout << "Total users: " << users.size() << std::endl;
            for (const auto& user : users) {
                std::cout << "ID: " << user[0] << " | Name: " << user[1]
                    << " | Email: " << user[2] << " | Role: " << user[3]
                    << " | Loyalty level: " << user[4] << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
};

// ========== MANAGER CLASS ==========

class Manager : public User {
private:
    std::vector<int> approvedOrders;

public:
    Manager(int id, const std::string& name, const std::string& email, const std::string& pwdHash)
        : User(id, name, email, "manager", pwdHash, 1) {
    }

    void displayMenu() override {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "            MANAGER MENU" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        std::cout << "1. View pending orders" << std::endl;
        std::cout << "2. Approve order" << std::endl;
        std::cout << "3. Update product stock quantity" << std::endl;
        std::cout << "4. View order details" << std::endl;
        std::cout << "5. Change order status" << std::endl;
        std::cout << "6. View approved orders history" << std::endl;
        std::cout << "7. View order status history" << std::endl;
        std::cout << "8. Exit system" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        std::cout << "Your choice (1-8): ";
    }

    bool canPerformAction(const std::string& action) const override {
        static const std::vector<std::string> allowedActions = {
            "approve_order", "update_stock", "view_pending_orders",
            "view_order_details", "update_order_status_limited",
            "view_approved_history", "view_order_history"
        };
        return std::find(allowedActions.begin(), allowedActions.end(), action) != allowedActions.end();
    }

    void processChoice(int choice, DatabaseConnection<std::string>& db) override {
        switch (choice) {
        case 1: viewPendingOrders(db); break;
        case 2: approveOrder(db); break;
        case 3: updateStock(db); break;
        case 4: viewOrderDetails(db); break;
        case 5: updateOrderStatusLimited(db); break;
        case 6: viewApprovedHistory(); break;
        case 7: viewOrderHistory(db); break;
        case 8: std::cout << "Exiting system..." << std::endl; break;
        default: std::cout << "Invalid choice!" << std::endl;
        }
    }

private:
    void viewPendingOrders(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- PENDING ORDERS FOR APPROVAL ---" << std::endl;

        try {
            auto orders = db.executeQuery(
                "SELECT o.order_id, u.name, o.total_price, "
                "to_char(o.order_date, 'YYYY-MM-DD HH24:MI:SS') "
                "FROM orders o JOIN users u ON o.user_id = u.user_id "
                "WHERE o.status = 'pending' ORDER BY o.order_date");

            if (orders.empty()) {
                std::cout << "No orders pending approval." << std::endl;
                return;
            }

            for (const auto& order : orders) {
                std::cout << "Order #" << order[0] << " | Customer: " << order[1]
                    << " | Amount: " << order[2] << " RUB"
                    << " | Date: " << order[3] << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void approveOrder(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- APPROVE ORDER ---" << std::endl;
        std::string orderId;

        std::cout << "Order ID to approve: ";
        std::cin >> orderId;

        try {
            if (!db.beginTransaction()) {
                std::cerr << "Transaction start error" << std::endl;
                return;
            }

            std::stringstream query;
            query << "UPDATE orders SET status = 'completed' WHERE order_id = " << orderId;

            if (db.executeNonQuery(query.str()) <= 0) {
                throw std::runtime_error("Order not found or not updated");
            }

            // Audit
            std::stringstream auditQuery;
            auditQuery << "INSERT INTO audit_log (entity_type, entity_id, operation, performed_by) "
                << "VALUES ('order', " << orderId << ", 'update', " << userId << ")";
            db.executeNonQuery(auditQuery.str());

            if (!db.commitTransaction()) {
                throw std::runtime_error("Transaction commit error");
            }

            approvedOrders.push_back(std::stoi(orderId));
            std::cout << "Order #" << orderId << " approved!" << std::endl;

        }
        catch (const std::exception& e) {
            db.rollbackTransaction();
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void updateStock(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- UPDATE STOCK ---" << std::endl;
        std::string productId, newQuantity;

        std::cout << "Product ID: ";
        std::cin >> productId;

        std::cout << "New quantity: ";
        std::cin >> newQuantity;

        try {
            if (!db.beginTransaction()) {
                std::cerr << "Transaction start error" << std::endl;
                return;
            }

            std::stringstream query;
            query << "UPDATE products SET stock_quantity = " << newQuantity
                << " WHERE product_id = " << productId;

            if (db.executeNonQuery(query.str()) <= 0) {
                throw std::runtime_error("Product not found or not updated");
            }

            // Audit
            std::stringstream auditQuery;
            auditQuery << "INSERT INTO audit_log (entity_type, entity_id, operation, performed_by) "
                << "VALUES ('product', " << productId << ", 'update', " << userId << ")";
            db.executeNonQuery(auditQuery.str());

            if (!db.commitTransaction()) {
                throw std::runtime_error("Transaction commit error");
            }

            std::cout << "Stock updated!" << std::endl;

        }
        catch (const std::exception& e) {
            db.rollbackTransaction();
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void viewOrderDetails(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- ORDER DETAILS ---" << std::endl;
        std::string orderId;

        std::cout << "Order ID: ";
        std::cin >> orderId;

        try {
            auto orderInfo = db.executeQuery(
                "SELECT o.order_id, u.name, o.status, o.total_price "
                "FROM orders o JOIN users u ON o.user_id = u.user_id "
                "WHERE o.order_id = " + orderId);

            if (orderInfo.empty()) {
                std::cout << "Order not found!" << std::endl;
                return;
            }

            std::cout << "Order #" << orderInfo[0][0] << std::endl;
            std::cout << "Customer: " << orderInfo[0][1] << std::endl;
            std::cout << "Status: " << orderInfo[0][2] << std::endl;
            std::cout << "Amount: " << orderInfo[0][3] << " RUB" << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void updateOrderStatusLimited(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- CHANGE ORDER STATUS ---" << std::endl;
        std::string orderId, newStatus;

        std::cout << "Order ID: ";
        std::cin >> orderId;

        std::cout << "New status (completed/processing): ";
        std::cin >> newStatus;

        try {
            std::vector<std::string> params = { orderId, newStatus, std::to_string(userId) };
            if (!db.executeProcedure("updateOrderStatus", params)) {
                throw std::runtime_error("Error updating status");
            }

            std::cout << "Order status updated!" << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void viewApprovedHistory() {
        std::cout << "\n--- APPROVED ORDERS HISTORY ---" << std::endl;

        if (approvedOrders.empty()) {
            std::cout << "You haven't approved any orders yet." << std::endl;
            return;
        }

        std::cout << "You have approved " << approvedOrders.size() << " orders:" << std::endl;
        for (int orderId : approvedOrders) {
            std::cout << "Order #" << orderId << std::endl;
        }
    }

    void viewOrderHistory(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- ORDER STATUS HISTORY ---" << std::endl;

        try {
            auto history = db.executeQuery(
                "SELECT DISTINCT o.order_id, o.status, u.name "
                "FROM orders o JOIN users u ON o.user_id = u.user_id "
                "WHERE o.status != 'pending' "
                "ORDER BY o.order_id");

            if (history.empty()) {
                std::cout << "History not found." << std::endl;
                return;
            }

            for (const auto& record : history) {
                std::cout << "Order #" << record[0] << " | Customer: " << record[2]
                    << " | Current status: " << record[1] << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
};

// ========== CUSTOMER CLASS ==========

class Customer : public User {
public:
    Customer(int id, const std::string& name, const std::string& email, const std::string& pwdHash, int loyalty = 0)
        : User(id, name, email, "customer", pwdHash, loyalty) {
    }

    void displayMenu() override {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "           CUSTOMER MENU" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        std::cout << "1. Create new order" << std::endl;
        std::cout << "2. Add product to order" << std::endl;
        std::cout << "3. Remove product from order" << std::endl;
        std::cout << "4. View my orders" << std::endl;
        std::cout << "5. View order status" << std::endl;
        std::cout << "6. Pay for order" << std::endl;
        std::cout << "7. Return order" << std::endl;
        std::cout << "8. View order status history" << std::endl;
        std::cout << "9. Exit system" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        std::cout << "Your choice (1-9): ";
    }

    bool canPerformAction(const std::string& action) const override {
        static const std::vector<std::string> allowedActions = {
            "create_order", "add_to_order", "remove_from_order",
            "view_own_orders", "view_order_status", "make_payment",
            "return_order", "view_own_order_history"
        };
        return std::find(allowedActions.begin(), allowedActions.end(), action) != allowedActions.end();
    }

    void processChoice(int choice, DatabaseConnection<std::string>& db) override {
        switch (choice) {
        case 1: createOrder(db); break;
        case 2: addToOrder(db); break;
        case 3: removeFromOrder(db); break;
        case 4: viewMyOrders(db); break;
        case 5: viewOrderStatus(db); break;
        case 6: makePayment(db); break;
        case 7: returnOrder(db); break;
        case 8: viewMyOrderHistory(db); break;
        case 9: std::cout << "Exiting system..." << std::endl; break;
        default: std::cout << "Invalid choice!" << std::endl;
        }
    }

private:
    void createOrder(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- CREATE NEW ORDER ---" << std::endl;

        std::string productId, quantity;

        std::cout << "Product ID: ";
        std::cin >> productId;

        std::cout << "Quantity: ";
        std::cin >> quantity;

        try {
            // Get product price
            auto priceResult = db.executeQuery(
                "SELECT price, name FROM products WHERE product_id = " + productId);

            if (priceResult.empty()) {
                std::cout << "Product not found!" << std::endl;
                return;
            }

            double price = std::stod(priceResult[0][0]);
            std::string productName = priceResult[0][1];
            double totalPrice = price * std::stoi(quantity);

            // Using stored procedure to create order in DB
            std::vector<std::string> params = {
                std::to_string(userId), productId, quantity, std::to_string(totalPrice)
            };

            if (!db.executeProcedure("createOrder", params)) {
                throw std::runtime_error("Error creating order: " + db.getLastError());
            }

            // Get ID of created order
            auto newOrderIdResult = db.executeQuery("SELECT lastval()");
            int newOrderId = !newOrderIdResult.empty() ? std::stoi(newOrderIdResult[0][0]) : orders.size() + 1000;

            // CREATE ORDER IN MEMORY
            auto order = std::make_shared<Order>(newOrderId, userId, totalPrice);
            order->addItem(std::stoi(productId), productName, std::stoi(quantity), price);

            // Add order to user's vector
            orders.push_back(order);

            std::cout << "Order #" << newOrderId << " created successfully!" << std::endl;
            order->display();

        }
        catch (const std::exception& e) {
            std::cerr << "Error creating order: " << e.what() << std::endl;
        }
    }

    void addToOrder(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- ADD PRODUCT TO ORDER ---" << std::endl;

        if (orders.empty()) {
            std::cout << "You have no active orders. Create an order first." << std::endl;
            return;
        }

        std::string orderIdStr, productIdStr, quantityStr;

        std::cout << "Order ID: ";
        std::cin >> orderIdStr;

        std::cout << "Product ID: ";
        std::cin >> productIdStr;

        std::cout << "Quantity: ";
        std::cin >> quantityStr;

        try {
            int orderId = std::stoi(orderIdStr);
            int productId = std::stoi(productIdStr);
            int quantity = std::stoi(quantityStr);

            // Find order
            auto it = std::find_if(orders.begin(), orders.end(),
                [orderId](const std::shared_ptr<Order>& order) {
                    return order->getOrderId() == orderId;
                });

            if (it != orders.end()) {
                // Get product information from DB
                auto productInfo = db.executeQuery(
                    "SELECT name, price FROM products WHERE product_id = " + productIdStr);

                if (productInfo.empty()) {
                    std::cout << "Product not found!" << std::endl;
                    return;
                }

                std::string productName = productInfo[0][0];
                double price = std::stod(productInfo[0][1]);

                (*it)->addItem(productId, productName, quantity, price);

                // Update total amount in DB
                double newTotal = (*it)->getTotalPrice();
                std::stringstream updateQuery;
                updateQuery << "UPDATE orders SET total_price = " << newTotal
                    << " WHERE order_id = " << orderId;
                db.executeNonQuery(updateQuery.str());

                std::cout << "Product added to order!" << std::endl;
                (*it)->display();
            }
            else {
                std::cout << "Order not found!" << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void removeFromOrder(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- REMOVE PRODUCT FROM ORDER ---" << std::endl;

        std::string orderIdStr, productIdStr;

        std::cout << "Order ID: ";
        std::cin >> orderIdStr;

        std::cout << "Product ID: ";
        std::cin >> productIdStr;

        try {
            int orderId = std::stoi(orderIdStr);
            int productId = std::stoi(productIdStr);

            auto it = std::find_if(orders.begin(), orders.end(),
                [orderId](const std::shared_ptr<Order>& order) {
                    return order->getOrderId() == orderId;
                });

            if (it != orders.end() && (*it)->removeItem(productId)) {
                // Update total amount in DB
                double newTotal = (*it)->getTotalPrice();
                std::stringstream updateQuery;
                updateQuery << "UPDATE orders SET total_price = " << newTotal
                    << " WHERE order_id = " << orderId;
                db.executeNonQuery(updateQuery.str());

                std::cout << "Product removed from order!" << std::endl;
                (*it)->display();
            }
            else {
                std::cout << "Failed to remove product!" << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void viewMyOrders(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- MY ORDERS ---" << std::endl;

        if (orders.empty()) {
            std::cout << "You have no orders yet." << std::endl;
            return;
        }

        // Lambda function for filtering by status
        auto filterByStatus = [this](const std::string& status) -> std::vector<std::shared_ptr<Order>> {
            std::vector<std::shared_ptr<Order>> filtered;
            std::copy_if(orders.begin(), orders.end(), std::back_inserter(filtered),
                [status](const std::shared_ptr<Order>& order) {
                    return order->getStatus() == status;
                });
            return filtered;
            };

        std::cout << "Total orders: " << orders.size() << std::endl;
        std::cout << "Total spent: " << calculateTotalSpent() << " RUB" << std::endl;

        // Count orders by status via std::accumulate
        std::vector<std::string> statuses = { "waiting", "paid", "processing", "completed", "canceled" };
        for (const auto& status : statuses) {
            int count = std::accumulate(orders.begin(), orders.end(), 0,
                [status](int total, const std::shared_ptr<Order>& order) {
                    return total + (order->getStatus() == status ? 1 : 0);
                });

            if (count > 0) {
                std::cout << status << ": " << count << " order(s)" << std::endl;
            }
        }

        std::cout << "\nOrder details:" << std::endl;
        for (const auto& order : orders) {
            order->display();
        }
    }

    void viewOrderStatus(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- ORDER STATUS ---" << std::endl;
        std::string orderIdStr;

        std::cout << "Order ID: ";
        std::cin >> orderIdStr;

        try {
            // Use DB function getOrderStatus
            auto result = db.executeQuery("SELECT getOrderStatus(" + orderIdStr + ")");

            if (!result.empty()) {
                std::cout << "Status of order #" << orderIdStr << ": " << result[0][0] << std::endl;
            }
            else {
                std::cout << "Failed to get order status" << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void makePayment(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- PAY FOR ORDER ---" << std::endl;

        if (orders.empty()) {
            std::cout << "You have no orders to pay for." << std::endl;
            return;
        }

        std::string orderIdStr;
        int paymentMethod;

        std::cout << "Order ID: ";
        std::cin >> orderIdStr;

        std::cout << "\nSelect payment method:" << std::endl;
        std::cout << "1. Credit Card" << std::endl;
        std::cout << "2. E-Wallet" << std::endl;
        std::cout << "3. Fast Payment System (SBP)" << std::endl;
        std::cout << "Your choice (1-3): ";
        std::cin >> paymentMethod;

        try {
            int orderId = std::stoi(orderIdStr);

            // Find order
            auto it = std::find_if(orders.begin(), orders.end(),
                [orderId](const std::shared_ptr<Order>& order) {
                    return order->getOrderId() == orderId;
                });

            if (it == orders.end()) {
                std::cout << "Order not found!" << std::endl;
                return;
            }

            std::unique_ptr<PaymentStrategy> strategy;

            switch (paymentMethod) {
            case 1: {
                std::string card, expiry, cvv;
                std::cout << "Card number: ";
                std::cin >> card;
                std::cout << "Expiry date (MM/YY): ";
                std::cin >> expiry;
                std::cout << "CVV: ";
                std::cin >> cvv;
                strategy = std::make_unique<CreditCardPayment>(card, expiry, cvv);
                break;
            }
            case 2: {
                std::string walletId, provider;
                std::cout << "Wallet ID: ";
                std::cin >> walletId;
                std::cout << "Provider (YooMoney/WebMoney/Qiwi): ";
                std::cin >> provider;
                strategy = std::make_unique<EWalletPayment>(walletId, provider);
                break;
            }
            case 3: {
                std::string phone, bank;
                std::cout << "Phone number: ";
                std::cin >> phone;
                std::cout << "Bank: ";
                std::cin >> bank;
                strategy = std::make_unique<SBPPayment>(phone, bank);
                break;
            }
            default:
                std::cout << "Invalid payment method choice!" << std::endl;
                return;
            }

            if ((*it)->makePayment(std::move(strategy))) {
                // Update status in DB
                std::vector<std::string> params = { orderIdStr, "paid", std::to_string(userId) };
                db.executeProcedure("updateOrderStatus", params);

                std::cout << "Payment successful!" << std::endl;
            }
            else {
                std::cout << "Payment error!" << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void returnOrder(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- RETURN ORDER ---" << std::endl;

        std::string orderIdStr;
        std::cout << "Order ID to return: ";
        std::cin >> orderIdStr;

        try {
            // Check via DB function can_return_order
            auto canReturn = db.executeQuery(
                "SELECT canReturnOrder(" + orderIdStr + ")");

            if (!canReturn.empty() && canReturn[0][0] == "t") {
                auto it = std::find_if(orders.begin(), orders.end(),
                    [orderIdStr](const std::shared_ptr<Order>& order) {
                        return order->getOrderId() == std::stoi(orderIdStr);
                    });

                if (it != orders.end() && (*it)->returnOrder()) {
                    // Update status in DB
                    std::vector<std::string> params = { orderIdStr, "returned", std::to_string(userId) };
                    db.executeProcedure("updateOrderStatus", params);

                    std::cout << "Return request submitted!" << std::endl;
                }
            }
            else {
                std::cout << "Return not possible!" << std::endl;
                std::cout << "Return conditions:" << std::endl;
                std::cout << "1. Order must be completed" << std::endl;
                std::cout << "2. No more than 30 days since order date" << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    void viewMyOrderHistory(DatabaseConnection<std::string>& db) {
        std::cout << "\n--- MY ORDER HISTORY ---" << std::endl;

        try {
            auto history = db.executeQuery(
                "SELECT o.order_id, o.status, o.total_price, "
                "to_char(o.order_date, 'YYYY-MM-DD HH24:MI:SS') "
                "FROM orders o WHERE o.user_id = " + std::to_string(userId) +
                " ORDER BY o.order_date DESC");

            if (history.empty()) {
                std::cout << "You have no orders yet." << std::endl;
                return;
            }

            std::cout << "Total orders: " << history.size() << std::endl;

            // Using std::accumulate to calculate total amount
            double totalSpent = std::accumulate(history.begin(), history.end(), 0.0,
                [](double sum, const std::vector<std::string>& row) {
                    try {
                        return sum + std::stod(row[2]);
                    }
                    catch (...) {
                        return sum;
                    }
                });

            std::cout << "Total spent: " << totalSpent << " RUB" << std::endl;

            for (const auto& order : history) {
                std::cout << "Order #" << order[0] << " | Status: " << order[1]
                    << " | Amount: " << order[2] << " RUB"
                    << " | Date: " << order[3] << std::endl;
            }

        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
};

// ========== MAIN FUNCTION ==========

int main() {
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "       ONLINE STORE SYSTEM" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Database connection
    std::string connStr = "dbname=online_store_db user=postgres password=Anna1875po hostaddr=127.0.0.1 port=5432";
    DatabaseConnection<std::string> db(connStr);

    if (!db.isConnected()) {
        std::cerr << "Database connection error: " << db.getLastError() << std::endl;
        return 1;
    }

    // Test data (in real system - authentication)
    std::map<int, std::shared_ptr<User>> users;

    // Create test users
    users[1] = std::make_shared<Admin>(1, "Administrator", "admin@store.com", "admin_hash");
    users[2] = std::make_shared<Manager>(2, "Manager Ivan", "manager@store.com", "manager_hash");
    users[3] = std::make_shared<Customer>(3, "Customer Peter", "customer@mail.com", "customer_hash", 1);

    // Add test orders for customer
    auto customer = std::dynamic_pointer_cast<Customer>(users[3]);
    if (customer) {
        std::shared_ptr<Order> order1 = customer->makeOrder(1001, 48997.0);
        order1->addItem(1, "Smartphone", 2, 19999);
        order1->addItem(3, "Headphones", 1, 8999);
        order1->updateStatus("completed");

        std::shared_ptr<Order> order2 = customer->makeOrder(1002, 32999.0);
        order2->addItem(4, "Tablet", 1, 32999);
        order2->updateStatus("paid");
    }

    // Main program loop
    while (true) {
        std::cout << "\n" << std::string(40, '-') << std::endl;
        std::cout << "MAIN MENU" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Select role to login:" << std::endl;
        std::cout << "1. Administrator" << std::endl;
        std::cout << "2. Manager" << std::endl;
        std::cout << "3. Customer" << std::endl;
        std::cout << "4. Exit program" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Your choice (1-4): ";

        int roleChoice;
        std::cin >> roleChoice;

        if (roleChoice == 4) {
            std::cout << "\nProgram termination..." << std::endl;
            break;
        }

        if (roleChoice < 1 || roleChoice > 3) {
            std::cout << "Invalid choice! Try again." << std::endl;
            continue;
        }

        // Get user by selected role
        auto userIt = users.find(roleChoice);
        if (userIt == users.end()) {
            std::cout << "Error: user not found!" << std::endl;
            continue;
        }

        std::shared_ptr<User> currentUser = userIt->second;
        std::cout << "\nWelcome, " << currentUser->getName() << "!" << std::endl;

        // Selected role menu loop
        bool exitRole = false;
        while (!exitRole) {
            currentUser->displayMenu();

            int choice;
            std::cin >> choice;

            if (choice == 0 ||
                (currentUser->getRole() == "admin" && choice == 11) ||
                (currentUser->getRole() == "manager" && choice == 8) ||
                (currentUser->getRole() == "customer" && choice == 9)) {
                std::cout << "Exiting menu..." << std::endl;
                exitRole = true;
            }
            else {
                currentUser->processChoice(choice, db);

                // Pause to view results
                std::cout << "\nPress Enter to continue...";
                std::cin.ignore();
                std::cin.get();
            }
        }
    }

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Program completed. Thank you for using!" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    return 0;
}
