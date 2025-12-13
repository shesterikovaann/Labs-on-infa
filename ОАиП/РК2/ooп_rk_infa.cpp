#include <iostream>
#include <string>
#include <iomanip>

using namespace std;

class BankAccount {
protected:
    string accountNumber; // Номер счёта
    string ownerName;     // Имя владельца
    double balance;       // Текущий баланс

public:
    // Конструктор с параметрами для инициализации счёта
    BankAccount(string accNum, string name, double initialBalance)
        : accountNumber(accNum), ownerName(name), balance(initialBalance) {
    }

    // Метод для пополнения счёта
    void deposit(double amount) {
        if (amount > 0) {
            balance += amount;
            cout << "Внесено: " << amount << " руб.\n";
        }
    }

    // Метод для снятия средств
    void withdraw(double amount) {
        balance -= amount;
        cout << "Со счёта #" << accountNumber << " снято: " << amount
            << " руб. Остаток: " << balance << " руб.\n";
        return; // Операция успешна
    }


    // Геттер для баланса (может пригодиться для производного класса)
    double getBalance() const {
        return balance;
    }
};


class SavingsAccount : public BankAccount {
private:
    double interestRate; // Процентная ставка (в процентах)

public:
    SavingsAccount(string accNum, string name, double initialBalance, double rate)
        : BankAccount(accNum, name, initialBalance), interestRate(rate) {
    }

    // Метод для начисления процентов на текущий баланс
    void applyInterest() {
        double interest = balance * (interestRate / 100.0); // Рассчитываем проценты
        balance += interest; // Добавляем проценты к балансу
    }
};

int main() {
    BankAccount ordinaryAcc("440011234567", "Иванов Иван", 5000.0);

    ordinaryAcc.deposit(3000.0);   // Пополнение
    ordinaryAcc.withdraw(2000.0);  // Cнятие

    // Создаём сберегательный счёт с процентной ставкой 4.5%
    SavingsAccount savingsAcc("550022345678", "Петрова Мария", 10000.0, 4.5);

    savingsAcc.deposit(5000.0);    // Пополнение
    savingsAcc.withdraw(3000.0);   // Снятие
    savingsAcc.applyInterest();    // Начисление процентов

    return 0;
}
