#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <cstdint>

using namespace std;

uint32_t* readFileContents(char* filepath) {
    ifstream machineFile(filepath, ios::in|ios::binary|ios::ate);
    if (machineFile.is_open()) {

        // read file into memory
        streampos fileSize = machineFile.tellg();
        char* buffer = new char[fileSize];
        machineFile.seekg(0, ios::beg);
        machineFile.read(buffer, fileSize);
        machineFile.close();

        // cout << "File size: " << fileSize << endl;
        cout << "File Hex Dump:" << endl;
        for (int i = 0; i < fileSize; i++) {
            cout << setw(2) << setfill('0') << hex  << (0x000000FF & (unsigned int) buffer[i]) << " "; 
        }
        cout << endl;

        int programLength = fileSize / 4;
        uint32_t* program = new uint32_t[programLength];

        // rearrange into instruction format
        for(int i = 0; i < programLength; i++) {
            program[i] = ((0x000000FF & buffer[i * 4]) | 
               (0x000000FF & buffer[i * 4 + 1]) << 8 | 
               (0x000000FF & buffer[i * 4 + 2]) << 16 | 
               (0x000000FF & buffer[i * 4 + 3]) << 24);
            // cout << setw(8) << setfill('0') << hex << program[i] << endl;
        }

        delete[] buffer;

        return program;
    } else {
        throw invalid_argument("file could not be opened");
    }
}

void console() {
    string input;
    while(input != "q") {
        cout << ">> ";
        cin >> input;

        // TODO: implement some commands here
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Please specify a machine code file." << endl;
        return 0;
    }

    try {
        uint32_t* fileContents = readFileContents(argv[1]);

        console();

        delete[] fileContents;
    } catch (invalid_argument& e) {
        cout << "The specified file could not be opened";
        return 0;
    }

}