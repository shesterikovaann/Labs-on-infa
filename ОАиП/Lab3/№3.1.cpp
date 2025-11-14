#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

void enter() {
    std::cout << "Введите строки, которые необходимо записать (EN):" << std::endl;
    std::ofstream out("tasks.txt", std::ios::app); 
    std::string data;
    std::getline(std::cin, data);
    while (data != "") {
        out << data << std::endl;
        std::getline(std::cin, data);
    }

    out.close();
    std::cout << "Данные записаны в файл" << std::endl;
}

void find_task() {
    std::cout << "Введите название задачи (EN):" << std::endl;
    std::string name;
    std::getline(std::cin, name);
    std::ifstream inFile("tasks.txt");
    std::string line;
    std::vector<std::string> words;
    std::string current_word;

    while (std::getline(inFile, line)) {
        for (char c : line) {
            if (c == ' ') {
                if (!current_word.empty()) {
                    words.push_back(current_word);
                    current_word.clear();
                }
            }
            else {
                current_word += c;
            }
        }
        // Добавить последнее слово, если оно есть
        if (!current_word.empty()) {
            words.push_back(current_word);
            current_word.clear();
        }
        if (words[0] == name) {
            std::cout << "Срок " << words[1] << " дней, Приоритет " << words[2] << std::endl;
            break;
        }
        words.clear();
    }
    inFile.close();
}


void sort() {
    std::cout << "Введите ключ сортировки date или priority:" << std::endl;
    std::string key;
    std::getline(std::cin, key);
    std::ifstream inFile("tasks.txt");
    std::string line;
    std::vector<std::string> words;
    std::vector<int> priors;
    std::vector<int> dates;
    std::string current_word;
    while (std::getline(inFile, line)) {
        for (char c : line) {
            if (c == ' ') {
                if (!current_word.empty()) {
                    words.push_back(current_word);
                    current_word.clear();
                }
            }
            else {
                current_word += c;
            }
        }
        // Добавить последнее слово, если оно есть
        if (!current_word.empty()) {
            words.push_back(current_word);
            current_word.clear();
        }
        if (key == "date") {
            int number = std::stoi(words[1]);
            dates.push_back(number);
        }
        if (key == "priority") {
            int number = std::stoi(words[2]);
            priors.push_back(number);
        }
        words.clear();
    }

    std::sort(priors.begin(), priors.end());
    std::sort(dates.begin(), dates.end());

    if (key == "date") {
        for (const auto& stre : dates) {
            std::cout << stre << std::endl;
        }
    }
    if (key == "priority") {
        for (const auto& stre : priors) {
            std::cout << stre << std::endl;
        }
    }
}


void find_priority() {
    std::cout << "Введите приоритет" << std::endl;
    int pri;
    std::cin >> pri;
    std::cout << "Задачи с меньшим или таким же приоритетом" << std::endl;
    std::ifstream inFile("tasks.txt");
    std::string line;
    std::vector<std::string> words;
    std::string current_word;
    while (std::getline(inFile, line)) {
        for (char c : line) {
            if (c == ' ') {
                if (!current_word.empty()) {
                    words.push_back(current_word);
                    current_word.clear();
                }
            }
            else {
                current_word += c;
            }

        }
        // Добавить последнее слово, если оно есть
        if (!current_word.empty()) {
            words.push_back(current_word);
            current_word.clear();
        }

        int cur_pri = std::stoi(words[2]);
        if (cur_pri <= pri) {
            std::cout << words[0] << ": приоритет " << cur_pri << std::endl;
        }
        words.clear();
    }
}

int main() {
    setlocale(LC_ALL, "Russian");
    while (true) {
        std::cout << "Введите команду: (add tasks, find task, sort, find priority)" << std::endl;
        std::string command;
        std::getline(std::cin, command);
        if (command == "add tasks") {
            enter();
            std::ofstream out("output.txt", std::ios::app);
            out << "add tasks" << std::endl;
            out.close();
        }
        if (command == "find task") {
            find_task();
            std::ofstream out("output.txt", std::ios::app);
            out << "find task" << std::endl;
            out.close();
        }
        if (command == "sort") {
            sort();
            std::ofstream out("output.txt", std::ios::app);
            out << "sort" << std::endl;
            out.close();
        }
        if (command == "find priority") {
            find_priority();
            std::ofstream out("output.txt", std::ios::app);
            out << "find priority" << std::endl;
            out.close();
        }
    }
}
