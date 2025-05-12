#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

using std::cerr;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;

ofstream OutFile;

UINT64 totalInstructions = 0;
UINT64 totalPredictions = 0;
UINT64 correctPredictions = 0;
UINT64 TABLE_SIZE = 1024; // Default size, can be overridden with -table_size
string outputFileName = "lvp_output_default.txt";

class LoadValuePredictor {
private:
    std::unordered_map<ADDRINT, UINT64> lastValueTable;

public:
    LoadValuePredictor() {}

    UINT64 getPrediction(ADDRINT loadPC) {
        auto it = lastValueTable.find(loadPC);
        if (it != lastValueTable.end()) {
            return it->second;
        }
        return 0;
    }

    void train(ADDRINT loadPC, UINT64 actualValue) {
        if (lastValueTable.size() >= TABLE_SIZE && lastValueTable.find(loadPC) == lastValueTable.end()) {
            lastValueTable.erase(lastValueTable.begin()); // FIFO eviction
        }
        lastValueTable[loadPC] = actualValue;
    }
};

LoadValuePredictor *lvp;

VOID AtLoadInstruction(ADDRINT pc, ADDRINT addr) {
    UINT64 actualValue = 0;

    if (PIN_SafeCopy(&actualValue, reinterpret_cast<void*>(addr), sizeof(UINT64)) != sizeof(UINT64)) {
        return;
    }

    UINT64 predictedValue = lvp->getPrediction(pc);

    if (predictedValue == actualValue) {
        correctPredictions++;
    }
    totalPredictions++;
    lvp->train(pc, actualValue);
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtLoadInstruction,
                       IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v) {
    OutFile << std::dec;
    OutFile << "\n--- Summary ---\n";
    OutFile << "Total Predictions: " << totalPredictions << "\n";
    OutFile << "Correct Predictions: " << correctPredictions << "\n";
    OutFile << "Prediction Accuracy: " 
            << (totalPredictions ? (100.0 * correctPredictions / totalPredictions) : 0.0) 
            << " %" << endl;

    cerr << "\n--- Summary ---\n";
    cerr << "Total Predictions: " << totalPredictions << "\n";
    cerr << "Correct Predictions: " << correctPredictions << "\n";
    cerr << "Prediction Accuracy: " 
         << (totalPredictions ? (100.0 * correctPredictions / totalPredictions) : 0.0)
         << " %" << endl;

    OutFile.close();
}

INT32 Usage() {
    cerr << "This tool simulates a simple last-value predictor for load instructions." << endl;
    return -1;
}

int main(int argc, char *argv[]) {
    std::vector<char *> pin_args;
    pin_args.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-table_size" && i + 1 < argc) {
            TABLE_SIZE = std::stoull(argv[i + 1]);
            cerr << "TABLE_SIZE set to: " << TABLE_SIZE << endl;
            ++i;
        } else {
            pin_args.push_back(argv[i]);
        }
    }

    int new_argc = pin_args.size();
    char **new_argv = &pin_args[0];

    if (PIN_Init(new_argc, new_argv)) {
        return Usage();
    }

    // Dynamic output file based on TABLE_SIZE
    outputFileName = "lvp_output_" + std::to_string(TABLE_SIZE) + ".txt";
    OutFile.open(outputFileName);
    if (!OutFile.is_open()) {
        cerr << "Failed to open output file: " << outputFileName << endl;
        return -1;
    }
    cerr << "Output file: " << outputFileName << endl;

    lvp = new LoadValuePredictor();

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}
