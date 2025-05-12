#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include <string>
#include <vector>
#include <algorithm>

using std::cerr;
using std::endl;
using std::ofstream;
using std::string;

ofstream OutFile;

UINT64 totalPredictions = 0;
UINT64 correctPredictions = 0;
UINT64 TABLE_SIZE = 1024; // Default, overridden by runtime input
string outputFileName = "storetoload_output_default.txt";

class StoreToLoadValuePredictor {
private:
    std::unordered_map<ADDRINT, UINT64> storeValueTable;
    std::queue<ADDRINT> insertionQueue;

    void replacementPolicy(ADDRINT addr) {
        if (storeValueTable.find(addr) == storeValueTable.end()) {
            if (storeValueTable.size() >= TABLE_SIZE) {
                ADDRINT evictAddr = insertionQueue.front();
                insertionQueue.pop();
                storeValueTable.erase(evictAddr);
            }
            insertionQueue.push(addr);
        }
    }

public:
    StoreToLoadValuePredictor() {}

    VOID RecordStoreVal(ADDRINT addr, UINT64 val) {
        replacementPolicy(addr);
        storeValueTable[addr] = val;
    }

    UINT64 getPrediction(ADDRINT addr) {
        if (storeValueTable.find(addr) != storeValueTable.end()) {
            return storeValueTable[addr];
        }
        return 0;
    }

    void train(ADDRINT addr, UINT64 actualVal) {
        replacementPolicy(addr);
        storeValueTable[addr] = actualVal;
    }
};

StoreToLoadValuePredictor *predictor;

VOID AtLoadInstruction(ADDRINT pc, ADDRINT addr) {
    UINT64 actualValue = 0;

    if (PIN_SafeCopy(&actualValue, reinterpret_cast<void*>(addr), sizeof(UINT64)) != sizeof(UINT64)) {
        return;
    }

    UINT64 predictedValue = predictor->getPrediction(addr);

    if (predictedValue == actualValue) {
        correctPredictions++;
    } else {
        predictor->train(addr, actualValue);
    }
    
    totalPredictions++;
}

VOID AtStoreInstruction(ADDRINT addr, UINT64 value) {
    predictor->RecordStoreVal(addr, value);
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtLoadInstruction,
                       IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_END);
    }

    if (INS_IsMemoryWrite(ins)) {
        if (INS_OperandIsReg(ins, 1)) {
            REG reg = INS_OperandReg(ins, 1);
            if (REG_valid(reg) && REG_is_gr(reg)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtStoreInstruction,
                               IARG_MEMORYWRITE_EA, IARG_REG_VALUE, reg, IARG_END);
            }
        } else if (INS_OperandIsImmediate(ins, 1)) {
            ADDRINT immVal = INS_OperandImmediate(ins, 1);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtStoreInstruction,
                           IARG_MEMORYWRITE_EA, IARG_ADDRINT, immVal, IARG_END);
        }
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
    cerr << "This tool simulates a store-to-load value predictor for load instructions." << endl;
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
    outputFileName = "storetoload_output_" + std::to_string(TABLE_SIZE) + ".txt";
    OutFile.open(outputFileName);
    if (!OutFile.is_open()) {
        cerr << "Failed to open output file: " << outputFileName << endl;
        return -1;
    }
    cerr << "Output file: " << outputFileName << endl;

    predictor = new StoreToLoadValuePredictor();

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}
