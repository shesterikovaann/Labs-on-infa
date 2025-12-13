#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <sstream>

class Vehicle {
protected:
	std::string brand; // данные скрыты от внешнего доступа
	std::string model;
	int year;
	std::vector<std::string> serviceRecords;
public:
	Vehicle(const std::string& b, const std::string& m, int y,
		const std::vector<std::string>& records)
		: brand(b), model(m), year(y), serviceRecords(records) {
	}
	virtual void display() const {
		std::cout << brand << " " << model << " (" << year << ")";
	}

	void saveToFile(std::ofstream& file) const {
		file << brand << ";" << model << ";" << year << ";";
		
	}

	void loadFromFile() {

	}

	// Геттеры
	std::string getBrand() const { return brand; }
	std::string getModel() const { return model; }
	int getYear() const { return year; }
	const std::vector<std::string>& getServiceRecords() const { return serviceRecords; }

	// Сеттеры
	void setBrand(const std::string& b) { brand = b; }
	void setModel(const std::string& m) { model = m; }
	void setYear(int y) { year = y; }
	void addServiceRecord(const std::string& record) { serviceRecords.push_back(record); }

};

class Car : public Vehicle {
public:
	std::string box_type;
	int doors;

	Car(const std::string& b, const std::string& m, int y,
		const std::string& box, int d, const std::vector<std::string>& records = {})
		: Vehicle(b, m, y, records), box_type(box), doors(d) {
	} 

	void display() const override {
		Vehicle::display();
		std::cout << " - Car, " << box_type << " transmission, " << doors << " doors\n";
		if (!serviceRecords.empty()) {
			std::cout << "  Service records: ";
			for (const auto& record : serviceRecords) {
				std::cout << record << "; ";
			}
			std::cout << "\n";
		}
	}

	void saveToFile(std::ofstream& file) const {
		file << "Car;";
		Vehicle::saveToFile(file);
		file << ";" << box_type << "-" << doors;
		for (size_t i = 0; i < serviceRecords.size(); ++i) {
			file << serviceRecords[i];
			if (i < serviceRecords.size() - 1) file << "|";
		}
	}
};

class Motorcycle : public Vehicle {
public:
	std::string moto_type;
	int power;

	Motorcycle(const std::string& b, const std::string& m, int y,
		const std::string& type, int p, const std::vector<std::string>& records = {})
		: Vehicle(b, m, y, records), moto_type(type), power(p) {
	}

	void display() const override {

	}

	void saveToFile(std::ofstream& file) const {
		file << "Motorcycle;";
		Vehicle::saveToFile(file);
		file << ";" << moto_type << "-" << power;
	}
};


void modifyVehicle(Vehicle* vehicle) {
	std::string newBrand, newModel;
	int newYear;
	std::string service;

	std::cout << "\n=== Modifying Vehicle ===\n";
	vehicle->display();
	std::cout << "\nEnter new brand: ";
	std::getline(std::cin, newBrand);
	std::cout << "Enter new model: ";
	std::getline(std::cin, newModel);
	std::cout << "Enter new year: ";
	std::cin >> newYear;
	std::cin.ignore(); // очистка буфера

	vehicle->setBrand(newBrand);
	vehicle->setModel(newModel);
	vehicle->setYear(newYear);

	std::cout << "Add service record: ";
	std::getline(std::cin, service);
	if (!service.empty()) {
		vehicle->addServiceRecord(service);
	}

	std::cout << "Vehicle modified successfully!\n";
}


void year_sorting() {
	std::ifstream out;          // поток для записи
	out.open("venicle.txt");      // открываем файл для записи
	
	std::vector<std::string> lines;
	std::vector<int> years;
	std::string line;
	while (std::getline(out, line)) {
		lines.push_back(line);
	}

	for (const std::string& str : lines) {

		size_t pos1 = str.find(';');
		size_t pos2 = str.find(';', pos1 + 1);
		size_t pos3 = str.find(';', pos2 + 1);
		size_t pos4 = str.find(';', pos3 + 1);


		// Выделяем подстроку между 3-й и 4-й точками с запятой
		std::string numberStr = str.substr(pos3 + 1, pos4 - pos3 - 1);
		int number = std::stoi(numberStr);
		years.push_back(number);
	}

	for (int num : years) {
		std::cout << num << " "; 
	}

	out.close();
	std::cout << "File has been written" << std::endl;
}


void auto_car() {
	std::ifstream file("venicle.txt");

	int automaticCarCount = 0;
	std::string line;

	while (std::getline(file, line)) {

		if (line.find("Car;") == 0) {
			// Ищем позицию пятого поля (параметр)
			size_t pos1 = line.find(';'); 
			pos1 = line.find(';', pos1 + 1); 
			pos1 = line.find(';', pos1 + 1);  
			pos1 = line.find(';', pos1 + 1); 
			size_t pos2 = line.find(';', pos1 + 1);

			std::string parameter = line.substr(pos1 + 1, pos2 - pos1 - 1);

				// Проверяем, содержит ли параметр "Automatic"
			if (parameter.find("Automatic") != std::string::npos) {
				automaticCarCount++;
				}
			
		}
	}

	file.close();

	std::cout << automaticCarCount << std::endl;
}



void max_moto() {
	std::ifstream file("venicle.txt");

	std::string line;
	int maxPower = 0;
	std::string maxPowerMotorcycleInfo;
	int motorcycleCount = 0;

	while (std::getline(file, line)) {

		if (line.find("Motorcycle;") == 0) {
	    	size_t pos1 = line.find(';');  
			size_t pos2 = line.find(';', pos1 + 1); 
			size_t pos3 = line.find(';', pos2 + 1); 
			size_t pos4 = line.find(';', pos3 + 1); 
			size_t pos5 = line.find(';', pos4 + 1); 


			std::string brand = line.substr(pos1 + 1, pos2 - pos1 - 1);
			std::string model = line.substr(pos2 + 1, pos3 - pos2 - 1);
			std::string yearStr = line.substr(pos3 + 1, pos4 - pos3 - 1);
			std::string parameter = line.substr(pos4 + 1, pos5 - pos4 - 1);

			size_t dashPos = parameter.find('-');
			if (dashPos != std::string::npos) {
				std::string powerStr = parameter.substr(dashPos + 1);
				int power = std::stoi(powerStr);

				// Обновляем максимум
				if (power > maxPower) {
					maxPower = power;
					maxPowerMotorcycleInfo = brand + " " + model;
				}
			}
		}
	}

	file.close();

	std::cout << "\n=== Результаты ===\n";
	std::cout << "Найдено мотоциклов: " << motorcycleCount << std::endl;

	if (motorcycleCount > 0) {
		std::cout << maxPower << " - " << maxPowerMotorcycleInfo;

	}
}


void splitVehiclesToFiles() {
	std::ifstream inFile("vehicle.txt");

	std::ofstream carsOut("cars.txt");
	std::ofstream motorcyclesOut("motorcycles.txt");

	std::string line;
	int carCount = 0, motorcycleCount = 0;

	while (std::getline(inFile, line)) {
		if (line.empty()) continue;

		if (line.rfind("Car;", 0) == 0) { // строка начинается с "Car;"
			carsOut << line << '\n';
			carCount++;
		}
		else if (line.rfind("Motorcycle;", 0) == 0) { // строка начинается с "Motorcycle;"
			motorcyclesOut << line << '\n';
			motorcycleCount++;
		}
	}
}

double getAverageCarYear() {
	std::ifstream file("vehicle.txt");
	if (!file) return 0.0;

	std::string line;
	int sum = 0, count = 0;

	while (std::getline(file, line)) {
		if (line.find("Car;") != 0) continue;

		std::stringstream ss(line);
		std::string token;
		// Пропускаем первые 3 поля
		for (int i = 0; i < 3; i++) std::getline(ss, token, ';');

		if (std::getline(ss, token, ';')) {
			try {
				sum += std::stoi(token);
				count++;
			}
			catch (...) {}
		}
	}

	return (count > 0) ? static_cast<double>(sum) / count : 0.0;
}

void findEngineServices() {
	std::ifstream file("vehicle.txt");
	if (!file) {
		std::cout << "Файл не найден!" << std::endl;
		return;
	}

	std::string line;
	bool found = false;

	std::cout << "Транспорт с сервисом 'engine':\n";
	std::cout << "-----------------------------\n";

	while (std::getline(file, line)) {
		if (line.empty()) continue;

		// Находим список сервисов (после 5-го ';')
		int semicolonCount = 0;
		size_t pos = 0;

		while (semicolonCount < 5 && (pos = line.find(';', pos)) != std::string::npos) {
			semicolonCount++;
			pos++;
		}

		if (pos != std::string::npos && pos < line.length()) {
			std::string services = line.substr(pos);

			// Приводим к нижнему регистру для поиска
			std::string servicesLower = services;
			std::transform(servicesLower.begin(), servicesLower.end(),
				servicesLower.begin(), ::tolower);

			if (servicesLower.find("engine") != std::string::npos) {
				found = true;

				// Парсим строку для вывода
				std::stringstream ss(line);
				std::string type, brand, model, year;

				std::getline(ss, type, ';');
				std::getline(ss, brand, ';');
				std::getline(ss, model, ';');
				std::getline(ss, year, ';');

				std::cout << type << " | " << brand << " " << model
					<< " (" << year << ")\n";
				std::cout << "Сервисы: " << services << "\n\n";
			}
		}
	}

	if (!found) {
		std::cout << "Не найдено транспортных средств с сервисом 'engine'.\n";
	}

	file.close();
}


int main() {
    std::ofstream file("venicle.txt"); 

	std::cout << "=== МЕНЮ ===\n";
	std::cout << "1. Средний год выпуска машин\n";
	std::cout << "2. Транспорт с сервисом 'engine'\n";
	std::cout << "3. Разделить по типам в файлы\n";
	std::cout << "4. Машины с автоматической коробкой\n";
	std::cout << "5. Мотоцикл с максимальной мощностью\n";
	std::cout << "0. Выход\n\n";

	int choice;

	while (true) {
		std::cout << "Выберите действие (0-5): ";
		std::cin.ignore(); // Очищаем буфер

		switch (choice) {
		case 0:
			return 0;

		case 1:
			std::cout << "\n--- Средний год выпуска ---\n";
			getAverageCarYear();
			break;

		case 2:
			std::cout << "\n--- Поиск 'engine' сервисов ---\n";
			findEngineServices();
			break;

		case 3:
			std::cout << "\n--- Разделение по типам ---\n";
			splitVehiclesToFiles();
			break;

		case 4:
			std::cout << "\n--- Машины с автоматом ---\n";
			auto_car();
			break;

		case 5:
			std::cout << "\n--- Максимальная мощность ---\n";
			max_moto();
			break;

		default:
			std::cout << "Неверный выбор. Введите число от 0 до 5.\n";
			break;
		}

		std::cout << std::endl;
	}

	return 0;
	return 0;
}
