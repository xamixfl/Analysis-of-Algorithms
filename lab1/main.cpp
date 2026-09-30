#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

// 1. Линейный поиск (несортированный массив)
int linearSearch(const std::vector<int>& arr, int x, long long& comparisonCount) {
    comparisonCount = 0;
    for (size_t i = 0; i < arr.size(); ++i) {
        comparisonCount++;
        if (arr[i] == x) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Вспомогательная функция для рекурсивного бинарного поиска
int binarySearchRecursiveHelper(const std::vector<int>& arr, int left, int right, int x, long long& comparisonCount) {
    comparisonCount++;
    if (left > right) {
        return -1;
    }
    int mid = left + (right - left) / 2;
    
    comparisonCount++;
    if (arr[mid] == x) {
        return mid;
    }
    
    comparisonCount++;
    if (arr[mid] > x) {
        return binarySearchRecursiveHelper(arr, left, mid - 1, x, comparisonCount);
    }
    return binarySearchRecursiveHelper(arr, mid + 1, right, x, comparisonCount);
}

// 2. Рекурсивный бинарный поиск (сортированный массив)
int binarySearchRecursive(const std::vector<int>& arr, int x, long long& comparisonCount) {
    comparisonCount = 0;
    if (arr.empty()) return -1;
    return binarySearchRecursiveHelper(arr, 0, static_cast<int>(arr.size()) - 1, x, comparisonCount);
}

// 3. Итеративный бинарный поиск 1 (сортированный массив)
int binarySearchIterative1(const std::vector<int>& arr, int x, long long& comparisonCount) {
    comparisonCount = 0;
    int left = 0;
    int right = static_cast<int>(arr.size()) - 1;

    while (left <= right) {
        comparisonCount++;
        int mid = left + (right - left) / 2;
        
        comparisonCount++;
        if (arr[mid] == x) {
            return mid;
        }
        
        comparisonCount++;
        if (arr[mid] < x) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    comparisonCount++;
    return -1;
}

// 4. Итеративный бинарный поиск 2 (сортированный массив)
int binarySearchIterative2(const std::vector<int>& arr, int x, long long& comparisonCount) {
    comparisonCount = 0;
    int left = 0;
    int right = static_cast<int>(arr.size()) - 1;

    while (left <= right) {
        comparisonCount++;
        int mid = left + (right - left) / 2;
        
        comparisonCount++;
        if (arr[mid] < x) {
            left = mid + 1;
        } 
        else {
            comparisonCount++;
            if (arr[mid] > x) {
                right = mid - 1;
            } 
            else {
                return mid;
            }
        }
    }
    comparisonCount++;
    return -1;
}

// Функция для генерации массивов
void generateArrays(int n, std::vector<int>& originalArr, std::vector<int>& sortedArr) {
    originalArr.resize(n);
    for (int i = 0; i < n; ++i) {
        originalArr[i] = i * 2; // заполняем четными числами
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(originalArr.begin(), originalArr.end(), g);

    sortedArr = originalArr;
    std::sort(sortedArr.begin(), sortedArr.end());
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    int n = 1000; // дефолтный размер
    std::vector<int> originalArr;
    std::vector<int> sortedArr;

    generateArrays(n, originalArr, sortedArr);

    int choice;
    do {
        std::cout << "\n================ MENU (N = " << n << ") ================\n";
        std::cout << "1. Change array size N (and regenerate data)\n";
        std::cout << "2. Linear Search (unsorted array)\n";
        std::cout << "3. Recursive Binary Search (sorted array)\n";
        std::cout << "4. Iterative Binary Search 1 (sorted array)\n";
        std::cout << "5. Iterative Binary Search 2 (sorted array)\n";
        std::cout << "0. Exit\n";
        std::cout << "Choose option (0-5): " << std::flush;
        
        if (!(std::cin >> choice)) {
            break;
        }

        if (choice == 0) {
            std::cout << "Exiting program.\n";
            break;
        }

        if (choice == 1) {
            std::cout << "Enter new array size N: " << std::flush;
            if (std::cin >> n && n > 0) {
                generateArrays(n, originalArr, sortedArr);
                std::cout << "Array regenerated successfully with size N = " << n << ".\n";
            } else {
                std::cout << "Invalid size.\n";
            }
            continue;
        }

        if (choice < 2 || choice > 5) {
            std::cout << "Wrong choice. Try again.\n";
            continue;
        }

        int x;
        std::cout << "Enter value x to search: " << std::flush;
        std::cin >> x;

        int resultIndex = -1;
        long long comparisonCount = 0;

        auto start = std::chrono::high_resolution_clock::now();

        switch (choice) {
            case 2:
                resultIndex = linearSearch(originalArr, x, comparisonCount);
                break;
            case 3:
                resultIndex = binarySearchRecursive(sortedArr, x, comparisonCount);
                break;
            case 4:
                resultIndex = binarySearchIterative1(sortedArr, x, comparisonCount);
                break;
            case 5:
                resultIndex = binarySearchIterative2(sortedArr, x, comparisonCount);
                break;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        if (resultIndex != -1) {
            std::cout << "Element found! Index: " << resultIndex << "\n";
        } else {
            std::cout << "Element not found (index: -1).\n";
        }
        std::cout << "Number of comparisons: " << comparisonCount << "\n";
        std::cout << "Execution time: " << duration << " ns\n";

    } while (true);

    return 0;
}
