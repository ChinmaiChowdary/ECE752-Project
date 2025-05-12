#include <iostream>
#include <fstream>
#include <sstream>

#define MAX_SIZE 1024

int main() {
    std::ifstream infile("/home/tmaddineni/pin/testbenches/testdata.txt");
    if (!infile.is_open()) {
        perror("fopen"); // prints system error
        std::cerr << "Error: Could not open file.\n";
        return 1;
    }

    int array1[MAX_SIZE], array2[MAX_SIZE], result[MAX_SIZE];
    int size1 = 0, size2 = 0;
    int res;
    std::string line;

    // Read first array
    if (std::getline(infile, line)) {
        std::stringstream ss(line);
        while (ss >> array1[size1]) {
            size1++;
        }
    }

    // Read second array
    if (std::getline(infile, line)) {
        std::stringstream ss(line);
        while (ss >> array2[size2]) {
            size2++;
        }
    }

    infile.close();

    // Check size match
    if (size1 != size2) {
        std::cerr << "Error: Arrays must be of the same size.\n";
        return 1;
    }

    // Multiply
    for (int i = 0; i < size1; ++i) {
        result[i] = array1[i] * array2[i];
    }

    // Output result
    std::cout << "Result of element-wise multiplication:\n";
    for (int i = 0; i < size1; ++i) {
        std::cout << result[i] << " ";
    }

    // Accumulate
    for (int j = 0; j < size1; ++j) {
        res = result[j] + result[j+1]; 
    }
    // Output result
    std::cout << "Op: MAC\n";
    std::cout << res;
    std::cout << std::endl;

    return 0;
}
