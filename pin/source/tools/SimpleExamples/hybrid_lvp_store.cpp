#include <iostream>
#include <fstream>
#include <unordered_map>
#include <cstdlib>
#include <queue>
#include <array>
#include <string>
#include "pin.H"

using std::cerr;
using std::cout;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;
using std::array;

#define PENALTY 2
#define COUNTER_THRESHOLD 3
#define CONTEXT_SIZE 4
#define MAX_CONTEXT_TABLE_SIZE 65536

UINT64 TABLE_SIZE = 1024;
string outputFileName;
ofstream OutFile;

class HybridLoadValuePredictor {
private:
    struct VHTEntry_context {
        array<UINT64, CONTEXT_SIZE> values;
        int nextIndex;
    };

    struct StrideEntry {
        UINT64 lastValue;
        INT64 stride;
        bool valid;
        StrideEntry() : lastValue(0), stride(0), valid(false) {}
    };

    std::unordered_map<ADDRINT, VHTEntry_context> valueHistoryTable_context;
    std::array<UINT64, MAX_CONTEXT_TABLE_SIZE> valuePredictTable;
    std::unordered_map<ADDRINT, UINT64> storeValueTable;
    std::unordered_map<ADDRINT, StrideEntry> strideTable;

    std::queue<ADDRINT> insertionQueue_storeToLoad;
    std::queue<ADDRINT> insertionQueue_stride;

    void replacementPolicy(UINT64 max_table_size, std::unordered_map<ADDRINT, UINT64> &table, std::queue<ADDRINT> &fifo_queue, ADDRINT hashIndex) {
        if (table.find(hashIndex) == table.end()) {
            if (table.size() >= max_table_size) {
                ADDRINT evictHashIndex = fifo_queue.front();
                fifo_queue.pop();
                table.erase(evictHashIndex);
            }
            fifo_queue.push(hashIndex);
        }
    }

    void replacementPolicy_stride(UINT64 max_table_size, std::unordered_map<ADDRINT, StrideEntry> &table, std::queue<ADDRINT> &fifo_queue, ADDRINT hashIndex) {
        if (table.find(hashIndex) == table.end()) {
            if (table.size() >= max_table_size) {
                ADDRINT evictHashIndex = fifo_queue.front();
                fifo_queue.pop();
                table.erase(evictHashIndex);
            }
            fifo_queue.push(hashIndex);
        }
    }

public:
    HybridLoadValuePredictor() {
        confidenceTable["context"] = 0;
        confidenceTable["stride"] = 0;
        confidenceTable["storeToLoad"] = 0;
        store_counter = 0;
        context_counter = 0;
        stride_counter = 0;
        memset(&valuePredictTable[0], 0, MAX_CONTEXT_TABLE_SIZE);
    }

    UINT64 store_counter;
    UINT64 context_counter;
    UINT64 stride_counter;

    std::unordered_map<string, int> confidenceTable;

    UINT16 fold(UINT16* val) {
        return val[0] ^ val[1] ^ val[2] ^ val[3];
    }

    UINT16 hashVPT(ADDRINT loadPC) {
        UINT16 hashFun = 0;
        array<UINT16, CONTEXT_SIZE> valueIDs;
        for (int i = 0; i < CONTEXT_SIZE; i++) {
            UINT64 rawLoadVal = valueHistoryTable_context[loadPC].values[i];
            valueIDs[i] = fold((UINT16*)&rawLoadVal);
        }
        int currIndex = valueHistoryTable_context[loadPC].nextIndex;
        for (int i = 0; i < CONTEXT_SIZE; i++) {
            UINT16 shiftedVal = valueIDs[currIndex] << (2 * (CONTEXT_SIZE - i - 1));
            hashFun ^= shiftedVal;
            currIndex = (currIndex + 1) % CONTEXT_SIZE;
        }
        return hashFun;
    }

    UINT64 getPredictionContext(ADDRINT loadPC) {
        if (valueHistoryTable_context.find(loadPC) != valueHistoryTable_context.end()) {
            UINT16 hashFun = hashVPT(loadPC);
            return valuePredictTable[hashFun];
        }
        return 0;
    }

    UINT64 getPredictionStoreToLoad(ADDRINT memoryAddr) {
        if (storeValueTable.find(memoryAddr) != storeValueTable.end())
            return storeValueTable[memoryAddr];
        return 0;
    }

    UINT64 getPredictionStride(ADDRINT loadPC) {
        if (strideTable.find(loadPC) != strideTable.end()) {
            StrideEntry &strideEntry = strideTable[loadPC];
            return strideEntry.lastValue + strideEntry.stride;
        }
        return 0;
    }

    void train(ADDRINT loadPC, UINT64 actualValue, ADDRINT memoryAddr) {
        if (valueHistoryTable_context.find(loadPC) != valueHistoryTable_context.end()) {
            VHTEntry_context& entry = valueHistoryTable_context[loadPC];
            entry.values[entry.nextIndex] = actualValue;
            entry.nextIndex = (entry.nextIndex + 1) % CONTEXT_SIZE;
            valuePredictTable[hashVPT(loadPC)] = actualValue;
        } else {
            valueHistoryTable_context[loadPC].values[0] = actualValue;
            for (int i = 1; i < CONTEXT_SIZE; i++)
                valueHistoryTable_context[loadPC].values[i] = 0;
            valueHistoryTable_context[loadPC].nextIndex = 1;
            valuePredictTable[hashVPT(loadPC)] = actualValue;
        }
        replacementPolicy(TABLE_SIZE, storeValueTable, insertionQueue_storeToLoad, memoryAddr);
        storeValueTable[memoryAddr] = actualValue;

        replacementPolicy_stride(TABLE_SIZE, strideTable, insertionQueue_stride, loadPC);
        StrideEntry strideEntry;
        strideEntry.lastValue = actualValue;
        strideEntry.valid = true;
        strideTable[loadPC] = strideEntry;
    }

    void confidenceUpdate(UINT64 actualValue, UINT64 predictedValue, const string& predictor) {
        if (predictedValue == actualValue) {
            if (confidenceTable[predictor] < 15) confidenceTable[predictor]++;
        } else {
            if (confidenceTable[predictor] > PENALTY)
                confidenceTable[predictor] -= PENALTY;
            else
                confidenceTable[predictor] = 0;
        }
    }

    VOID RecordStoreVal(ADDRINT memoryAddr, UINT64 storeVal) {
        replacementPolicy(TABLE_SIZE, storeValueTable, insertionQueue_storeToLoad, memoryAddr);
        storeValueTable[memoryAddr] = storeVal;
    }

    UINT64 finalPredictor(UINT64 predictedValue_context, UINT64 predictedValue_storeToLoad, UINT64 predictedValue_stride) {
        if ((confidenceTable["context"] > confidenceTable["storeToLoad"]) &&
            (confidenceTable["context"] > confidenceTable["stride"]) &&
            (confidenceTable["context"] > COUNTER_THRESHOLD)) {
            context_counter++;
            return predictedValue_context;
        } else if ((confidenceTable["stride"] >= confidenceTable["context"]) &&
                   (confidenceTable["stride"] > confidenceTable["storeToLoad"]) &&
                   (confidenceTable["stride"] > COUNTER_THRESHOLD)) {
            stride_counter++;
            return predictedValue_stride;
        } else {
            store_counter++;
            return predictedValue_storeToLoad;
        }
    }
};

HybridLoadValuePredictor *hybridLoadValuePredictor;

static UINT64 correctPredictionCount = 0;
static UINT64 totalPredictions = 0;

VOID AtLoadInstruction(ADDRINT loadPC, ADDRINT memoryAddr) {
    UINT64 actualValue = 0;
    if (PIN_SafeCopy(&actualValue, reinterpret_cast<void*>(memoryAddr), sizeof(UINT64)) != sizeof(UINT64))
        return;

    UINT64 predictedValue_context = hybridLoadValuePredictor->getPredictionContext(loadPC);
    UINT64 predictedValue_storeToLoad = hybridLoadValuePredictor->getPredictionStoreToLoad(memoryAddr);
    UINT64 predictedValue_stride = hybridLoadValuePredictor->getPredictionStride(loadPC);

    UINT64 finalPrediction = hybridLoadValuePredictor->finalPredictor(predictedValue_context, predictedValue_storeToLoad, predictedValue_stride);

    if (finalPrediction == actualValue) correctPredictionCount++;

    hybridLoadValuePredictor->confidenceUpdate(actualValue, predictedValue_context, "context");
    hybridLoadValuePredictor->confidenceUpdate(actualValue, predictedValue_storeToLoad, "storeToLoad");
    hybridLoadValuePredictor->confidenceUpdate(actualValue, predictedValue_stride, "stride");

    hybridLoadValuePredictor->train(loadPC, actualValue, memoryAddr);

    totalPredictions++;
}

VOID AtStoreInstruction(ADDRINT memoryAddr, UINT64 storeVal) {
    hybridLoadValuePredictor->RecordStoreVal(memoryAddr, storeVal);
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins)) {
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

VOID Fini(int, VOID *) {
    OutFile << "\n--- Simulation Summary ---" << endl;
    OutFile << "Prediction Accuracy: "
            << ((totalPredictions > 0) ? (100.0 * correctPredictionCount / totalPredictions) : 0) << "%" << endl;
    OutFile << "STORE predictor usage: " << hybridLoadValuePredictor->store_counter << endl;
    OutFile << "CONTEXT predictor usage: " << hybridLoadValuePredictor->context_counter << endl;
    OutFile << "STRIDE predictor usage: " << hybridLoadValuePredictor->stride_counter << endl;
    OutFile.close();

    cerr << "\n--- Simulation Summary ---" << endl;
    cerr << "Prediction Accuracy: "
         << ((totalPredictions > 0) ? (100.0 * correctPredictionCount / totalPredictions) : 0) << "%" << endl;
    cerr << "STORE predictor usage: " << hybridLoadValuePredictor->store_counter << endl;
    cerr << "CONTEXT predictor usage: " << hybridLoadValuePredictor->context_counter << endl;
    cerr << "STRIDE predictor usage: " << hybridLoadValuePredictor->stride_counter << endl;
}

INT32 Usage() {
    cerr << "This tool simulates a hybrid load value predictor." << endl;
    return -1;
}

int main(int argc, char *argv[]) {
    std::vector<char*> pin_args;
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
    char** new_argv = &pin_args[0];

    if (PIN_Init(new_argc, new_argv)) return Usage();

    outputFileName = "hybrid_lvp_store_" + std::to_string(TABLE_SIZE) + ".txt";
    OutFile.open(outputFileName);
    if (!OutFile.is_open()) {
        cerr << "Failed to open output file: " << outputFileName << endl;
        return -1;
    }
    cerr << "Output file: " << outputFileName << endl;

    hybridLoadValuePredictor = new HybridLoadValuePredictor();

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    return 0;
}