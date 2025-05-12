#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <cstdlib>
#include <array>
#include <queue>

using std::cerr;
using std::endl;
using std::ofstream;
using std::string;
using std::array;

#define CONTEXT_SIZE 4
#define MAX_TABLE_SIZE 65536

class ContextLoadValuePredictor {
private:
    struct VHTEntry {
        array<UINT64, CONTEXT_SIZE> values;
        int nextIndex;
    };

    std::unordered_map<ADDRINT, VHTEntry> valueHistoryTable;
    std::array<UINT64, MAX_TABLE_SIZE> valuePredictTable;

public:
    ContextLoadValuePredictor() {
        for (int i = 0; i < MAX_TABLE_SIZE; i++) {
            valuePredictTable[i] = 0;
        }
    }

    UINT16 fold(UINT16* val) {
        return val[0] ^ val[1] ^ val[2] ^ val[3];
    }

    UINT16 hashVPT(ADDRINT loadPC) {
        UINT16 hashFun = 0;
        array<UINT16, CONTEXT_SIZE> valueIDs;
        for (int i = 0; i < CONTEXT_SIZE; i++) {
            UINT64 rawVal = valueHistoryTable[loadPC].values[i];
            valueIDs[i] = fold((UINT16*)&rawVal);
        }

        int currIndex = valueHistoryTable[loadPC].nextIndex;

        for (int i = 0; i < CONTEXT_SIZE; i++) {
            UINT16 shiftedVal = valueIDs[currIndex] << (2 * (CONTEXT_SIZE - i - 1));
            hashFun ^= shiftedVal;
            currIndex = (currIndex + 1) % CONTEXT_SIZE;
        }
        return hashFun;
    }

    UINT64 getPrediction(ADDRINT loadPC) {
        if (valueHistoryTable.find(loadPC) != valueHistoryTable.end()) {
            UINT16 hashFun = hashVPT(loadPC);
            return valuePredictTable[hashFun];
        }
        return 0;
    }

    void train(ADDRINT loadPC, UINT64 actualValue) {
        if (valueHistoryTable.find(loadPC) != valueHistoryTable.end()) {
            VHTEntry &entry = valueHistoryTable[loadPC];
            entry.values[entry.nextIndex] = actualValue;
            entry.nextIndex = (entry.nextIndex + 1) % CONTEXT_SIZE;
            valuePredictTable[hashVPT(loadPC)] = actualValue;
        } else {
            valueHistoryTable[loadPC] = {{actualValue, 0, 0, 0}, 1};
            valuePredictTable[hashVPT(loadPC)] = actualValue;
        }
    }
};

ContextLoadValuePredictor *contextLoadValuePredictor;

ofstream OutFile;

static UINT64 correctPredictionCount = 0;
static UINT64 totalPredictions = 0;

VOID AtLoadInstruction(ADDRINT loadPC, ADDRINT memoryAddr) {
    UINT64 actualValue = 0;

    if (PIN_SafeCopy(&actualValue, reinterpret_cast<void*>(memoryAddr), sizeof(UINT64)) != sizeof(UINT64)) {
        return;
    }

    UINT64 predictedValue = contextLoadValuePredictor->getPrediction(loadPC);

    if (predictedValue == actualValue) {
        correctPredictionCount++;
    }

    contextLoadValuePredictor->train(loadPC, actualValue);

    totalPredictions++;
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtLoadInstruction,
                       IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v) {
    OutFile << "\n--- Summary ---\n";
    OutFile << "Total Predictions: " << totalPredictions << "\n";
    OutFile << "Correct Predictions: " << correctPredictionCount << "\n";
    OutFile << "Prediction Accuracy: "
            << (totalPredictions ? (100.0 * correctPredictionCount / totalPredictions) : 0.0)
            << " %" << endl;

    cerr << "\n--- Summary ---\n";
    cerr << "Total Predictions: " << totalPredictions << "\n";
    cerr << "Correct Predictions: " << correctPredictionCount << "\n";
    cerr << "Prediction Accuracy: "
         << (totalPredictions ? (100.0 * correctPredictionCount / totalPredictions) : 0.0)
         << " %" << endl;

    OutFile.close();
}

INT32 Usage() {
    cerr << "This tool simulates a context-based load value predictor." << endl;
    return -1;
}

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) return Usage();

    contextLoadValuePredictor = new ContextLoadValuePredictor();

    OutFile.open("context_lvp_output.txt");
    if (!OutFile.is_open()) {
        cerr << "Failed to open output file." << endl;
        return -1;
    }

    cerr << "Running context load value predictor simulation..." << endl;

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();

    return 0;
}
