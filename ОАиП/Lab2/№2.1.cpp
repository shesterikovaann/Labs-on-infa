#include <iostream>

int main() {
	setlocale(LC_ALL, "rus");
	char a[100];
	std::cin.getline(a, 100);
	int count = 0;
	int length = strlen(a);
	// поиск слов, оканчивающихся на b
	for (int i = 0; i < length; i++) { 
		if (i != 0 and a[i] == ' ') {
			if (a[i - 1] == 'b') {
				count += 1; 
			}
		 }
	}
	if (a[length - 1] == 'b') {
		count += 1;
	}
	std::cout << count << "\n";

	// поиск длины cамого длинного слова
	int max_len = 0;
	int l = 0;
	for (int i = 0; i < length; i++) {
		if (a[i] == ' ') {
			if (l > max_len) {
				max_len = l;
				l = 0;
			}
		}
		else {
			l += 1;
		}
	}
	std::cout << max_len << "\n";

	// поиск количества букв d в последнем слове строки
	int c = 0;
	for (int i = length - 1; i > 0; i--) {
		if (a[i] == ' ') {
			break;
		}
		if (a[i] == 'd') {
			c += 1;
		}
	}
	std::cout << c << "\n";

	// замена всех строчных букв в заглавные
	for (int i = 0; i < length; i++) {
		a[i] = toupper(a[i]);
	}
	std::cout << a << "\n";

	// поиск слов с совпадающими вторым и предпоследним символами
	int index_word = 0;
	char sim = ' ';
	int co = 0;
 	for (int i = 0; i < length; i++) {
		if (a[i] != ' ') {
			index_word += 1;
		}
		if (a[i] != ' ' and index_word == 2) {
			sim = a[i];
		}
		if (a[i] == ' ') {
			if (index_word > 1 and a[i - 2] == sim) {
				co += 1;
			}
		    index_word = 0;
		}
	}
	std::cout << co << "\n";
}
