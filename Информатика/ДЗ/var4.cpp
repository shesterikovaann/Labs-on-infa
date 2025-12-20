#include <iostream>
#include <fstream>
#include <string>
#include <limits>
// Шестерикова Анна ИУ1-12Б Вариант 4 Умный дом

// Класс управления освещением
class Light {
private:
    bool isOn;

public:
    Light() : isOn(false) {}

    void turnOn() { isOn = true; }
    void turnOff() { isOn = false; }

    std::string getStatus() const { return isOn ? "On" : "Off"; }
    bool getState() const { return isOn; }
    void setState(bool state) { isOn = state; }
};

// Класс управления термостатом
class Thermostat {
private:
    int temperature;

public:
    Thermostat() : temperature(20) {}

    void setTemperature(int t) { temperature = t; }
    int getTemperature() const { return temperature; }
};

// Класс управления системой безопасности
class SecuritySystem {
private:
    bool isArmed;

public:
    SecuritySystem() : isArmed(false) {}

    void arm() { isArmed = true; }
    void disarm() { isArmed = false; }

    std::string getStatus() const { return isArmed ? "Armed" : "Disarmed"; }
    bool getState() const { return isArmed; }
    void setState(bool state) { isArmed = state; }
};

// Класс управления шторами
class Curtains {
private:
    bool isUp;

public:
    Curtains() : isUp(false) {}

    void raise() { isUp = true; }
    void lower() { isUp = false; }

    std::string getStatus() const { return isUp ? "Raised" : "Lowered"; }
    bool getState() const { return isUp; }
    void setState(bool state) { isUp = state; }
};

// Класс управления кондиционером
class AirConditioner {
private:
    bool isOn;

public:
    AirConditioner() : isOn(false) {}

    void turnOn() { isOn = true; }
    void turnOff() { isOn = false; }

    std::string getStatus() const { return isOn ? "On" : "Off"; }
    bool getState() const { return isOn; }
    void setState(bool state) { isOn = state; }
};

// Основной класс умного дома
class SmartHome {
private:
    Light light;
    Thermostat thermostat;
    SecuritySystem securitySystem;
    Curtains curtains;
    AirConditioner airConditioner;
    std::string stateFile;

public:
    SmartHome() : stateFile("home_state.txt") {}

    void controlLight(bool state) { light.setState(state); }
    void setTemperature(int temp) { thermostat.setTemperature(temp); }
    void controlSecurity(bool state) { securitySystem.setState(state); }
    void controlCurtains(bool state) { curtains.setState(state); }
    void controlAirConditioner(bool state) { airConditioner.setState(state); }

    void displayStatus() const {
        std::cout << "\nCurrent State:\n";
        std::cout << "Light: " << light.getStatus() << "\n";
        std::cout << "Temperature: " << thermostat.getTemperature() << "°C\n";
        std::cout << "Security System: " << securitySystem.getStatus() << "\n";
        std::cout << "Curtains: " << curtains.getStatus() << "\n";
        std::cout << "Air Conditioner: " << airConditioner.getStatus() << "\n";
    }

    void saveState() const {
        std::ofstream file(stateFile);
        if (file.is_open()) {
            file << light.getState() << "\n";
            file << thermostat.getTemperature() << "\n";
            file << securitySystem.getState() << "\n";
            file << curtains.getState() << "\n";
            file << airConditioner.getState() << "\n";
            file.close();
        }
    }

    void loadState() {
        std::ifstream file(stateFile);
        if (file.is_open()) {
            int lightState, temp, securityState, curtainsState, acState;

            if (file >> lightState >> temp >> securityState >> curtainsState >> acState) {
                light.setState(lightState);
                thermostat.setTemperature(temp);
                securitySystem.setState(securityState);
                curtains.setState(curtainsState);
                airConditioner.setState(acState);
                std::cout << "State loaded from file.\n";
            }
            else {
                std::cout << "State file corrupted. Using default settings.\n";
            }
            file.close();
        }
        else {
            std::cout << "No previous state file found. Using default settings.\n";
        }
    }

    void run() {
        loadState();

        int choice;
        bool running = true;

        while (running) {
            displayStatus();

            std::cout << "\nChoose an action:\n";
            std::cout << "1. Turn on light\n";
            std::cout << "2. Turn off light\n";
            std::cout << "3. Set temperature\n";
            std::cout << "4. Arm security system\n";
            std::cout << "5. Disarm security system\n";
            std::cout << "6. Raise curtains\n";
            std::cout << "7. Lower curtains\n";
            std::cout << "8. Turn on air conditioner\n";
            std::cout << "9. Turn off air conditioner\n";
            std::cout << "10. Exit\n";
            std::cout << "Enter your choice: ";

            std::cin >> choice;

            switch (choice) {
            case 1:
                light.turnOn();
                std::cout << "Light turned on.\n";
                break;
            case 2:
                light.turnOff();
                std::cout << "Light turned off.\n";
                break;
            case 3: {
                int temp;
                std::cout << "Enter temperature: ";
                std::cin >> temp;
                thermostat.setTemperature(temp);
                std::cout << "Temperature set to " << temp << "°C.\n";
                break;
            }
            case 4:
                securitySystem.arm();
                std::cout << "Security system armed.\n";
                break;
            case 5:
                securitySystem.disarm();
                std::cout << "Security system disarmed.\n";
                break;
            case 6:
                curtains.raise();
                std::cout << "Curtains raised.\n";
                break;
            case 7:
                curtains.lower();
                std::cout << "Curtains lowered.\n";
                break;
            case 8:
                airConditioner.turnOn();
                std::cout << "Air conditioner turned on.\n";
                break;
            case 9:
                airConditioner.turnOff();
                std::cout << "Air conditioner turned off.\n";
                break;
            case 10:
                running = false;
                std::cout << "\nUpdated State:\n";
                displayStatus();
                saveState();
                std::cout << "State saved to " << stateFile << ". Exiting...\n";
                break;
            default:
                std::cout << "Invalid choice. Please try again.\n";
                break;
            }

            // Очистка буфера ввода
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
};

int main() {
    SmartHome smartHome;
    smartHome.run();

    return 0;
}
