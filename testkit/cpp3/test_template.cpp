// Test code for bm2cpp3 generator using TEST_CLASS placeholder
//
// Usage: test_template <input file> <output file>
// This test reads an input file, decodes it using TEST_CLASS::decode(),
// then writes the output using TEST_CLASS::encode().

#include <fstream>
#include <iostream>
#include <vector>

#include "stub.cpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input file> <output file>" << std::endl;
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    // Read the entire input file into a vector.
    std::ifstream in(inputFile, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open input file: " << inputFile << std::endl;
        return 1;
    }
    std::vector<char> inputData((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());

    // Create an instance of TEST_CLASS.
    TEST_CLASS testObj;

    // Decode the input data.
    // Assume decode() takes a const std::vector<char>& and returns a bool indicating success.
    if (!testObj.decode(inputData)) {
        std::cerr << "Decoding failed." << std::endl;
        return 1;
    }

    // Encode the data.
    // Assume encode() returns a std::vector<char> containing the output data.
    std::vector<char> outputData = testObj.encode();

    // Write the output data to the output file.
    std::ofstream out(outputFile, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file: " << outputFile << std::endl;
        return 1;
    }
    out.write(outputData.data(), outputData.size());

    return 0;
}
