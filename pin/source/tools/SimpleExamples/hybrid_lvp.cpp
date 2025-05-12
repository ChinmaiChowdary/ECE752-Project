#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <array>
#include <queue>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

using std::cerr;
using std::endl;
using std::ofstream;
using std::string;
using std::array;
using std::queue;

ofstream OutFile;

UINT64 totalPredictions = 0;
UINT64 correctPredictions = 0;
UINT64 TABLE_SIZE = 1024;
string outputFileName = "hybrid_lvp_output_default.txt";

#define CONTEXT_SIZE 4
#define MAX_CONTEXT_TABLE_SIZE 65536
#define COUNTER_THRESHOLD 3
#define PENALTY 2

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
    array<UINT64, MAX_CONTEXT_TABLE_SIZE> valuePredictTable;
    std::unordered_map<ADDRINT, UINT64> storeValueTable;
    std::unordered_map<ADDRINT, StrideEntry> strideTable;
    queue<ADDRINT> insertionQueue_storeToLoad;
    queue<ADDRINT> insertionQueue_stride;

    void replacementPolicy(UINT64 max_table_size, std::unordered_map<ADDRINT, UINT64> &table, queue<ADDRINT> &fifo_queue, ADDRINT addr) {
        if (table.find(addr) == table.end()) {
            if (table.size() >= max_table_size) {
                ADDRINT evictAddr = fifo_queue.front();
                fifo_queue.pop();
                table.erase(evictAddr);
            }
            fifo_queue.push(addr);
        }
    }

    void replacementPolicy_stride(UINT64 max_table_size, std::unordered_map<ADDRINT, StrideEntry> &table, queue<ADDRINT> &fifo_queue, ADDRINT addr) {
        if (table.find(addr) == table.end()) {
            if (table.size() >= max_table_size) {
                ADDRINT evictAddr = fifo_queue.front();
                fifo_queue.pop();
                table.erase(evictAddr);
            }
            fifo_queue.push(addr);
        }
    }

public:
    UINT64 store_counter, context_counter, stride_counter;
    std::unordered_map<string, int> confidenceTable;

    HybridLoadValuePredictor() {
        confidenceTable["context"] = 0;
        confidenceTable["storeToLoad"] = 0;
        confidenceTable["stride"] = 0;
        store_counter = context_counter = stride_counter = 0;
        memset(&valuePredictTable[0], 0, MAX_CONTEXT_TABLE_SIZE * sizeof(UINT64));
    }

    UINT16 fold(UINT16 *val) {
        return val[0] ^ val[1] ^ val[2] ^ val[3];
    }

    UINT16 hashVPT(ADDRINT pc) {
        UINT16 hashVal = 0;
        array<UINT16, CONTEXT_SIZE> valueIDs;
        auto it = valueHistoryTable_context.find(pc);

        if (it != valueHistoryTable_context.end()) {
            for (int i = 0; i < CONTEXT_SIZE; i++) {
                UINT64 rawVal = it->second.values[i];
                valueIDs[i] = fold((UINT16*)&rawVal);
            }
            int currIndex = it->second.nextIndex;
            for (int i = 0; i < CONTEXT_SIZE; i++) {
                UINT16 shiftedVal = valueIDs[currIndex] << (2 * (CONTEXT_SIZE - i - 1));
                hashVal ^= shiftedVal;
                currIndex = (currIndex + 1) % CONTEXT_SIZE;
            }
        }
        return hashVal;
    }

    UINT64 getPredictionContext(ADDRINT pc) {
        auto it = valueHistoryTable_context.find(pc);
        if (it != valueHistoryTable_context.end()) {
            UINT16 hash = hashVPT(pc);
            return valuePredictTable[hash];
        }
        return 0;
    }

    UINT64 getPredictionStoreToLoad(ADDRINT addr) {
        if (storeValueTable.find(addr) != storeValueTable.end()) {
            return storeValueTable[addr];
        }
        return 0;
    }

    UINT64 getPredictionStride(ADDRINT pc) {
        if (strideTable.find(pc) != strideTable.end()) {
            StrideEntry &entry = strideTable[pc];
            return entry.lastValue + entry.stride;
        }
        return 0;
    }

    void train(ADDRINT pc, UINT64 actualVal, ADDRINT addr) {
        auto it = valueHistoryTable_context.find(pc);
        if (it != valueHistoryTable_context.end()) {
            VHTEntry_context &entry = it->second;
            entry.values[entry.nextIndex] = actualVal;
            entry.nextIndex = (entry.nextIndex + 1) % CONTEXT_SIZE;
            valuePredictTable[hashVPT(pc)] = actualVal;
        } else {
            VHTEntry_context newEntry;
            newEntry.values[0] = actualVal;
            for (int i = 1; i < CONTEXT_SIZE; i++) {
                newEntry.values[i] = 0;
            }
            newEntry.nextIndex = 1;
            valueHistoryTable_context.insert(std::make_pair(pc, newEntry));
            valuePredictTable[hashVPT(pc)] = actualVal;
        }

        replacementPolicy(TABLE_SIZE, storeValueTable, insertionQueue_storeToLoad, addr);
        storeValueTable[addr] = actualVal;

        replacementPolicy_stride(TABLE_SIZE, strideTable, insertionQueue_stride, pc);
        StrideEntry strideEntry;
        strideEntry.lastValue = actualVal;
        strideEntry.stride = 0;
        strideEntry.valid = true;
        strideTable[pc] = strideEntry;
    }

    void confidenceUpdate(UINT64 actualVal, UINT64 predictedVal, const string &predictor) {
        if (predictedVal == actualVal) {
            if (confidenceTable[predictor] < 15) confidenceTable[predictor]++;
        } else {
            confidenceTable[predictor] = (confidenceTable[predictor] > PENALTY) ? confidenceTable[predictor] - PENALTY : 0;
        }
    }

    VOID RecordStoreVal(ADDRINT addr, UINT64 val) {
        replacementPolicy(TABLE_SIZE, storeValueTable, insertionQueue_storeToLoad, addr);
        storeValueTable[addr] = val;
    }

    UINT64 finalPredictor(UINT64 val_context, UINT64 val_storeToLoad, UINT64 val_stride) {
        if (confidenceTable["context"] > confidenceTable["storeToLoad"] && confidenceTable["context"] > confidenceTable["stride"] && confidenceTable["context"] > COUNTER_THRESHOLD) {
            context_counter++;
            return val_context;
        } else if (confidenceTable["storeToLoad"] >= confidenceTable["context"] && confidenceTable["storeToLoad"] > confidenceTable["stride"] && confidenceTable["storeToLoad"] > COUNTER_THRESHOLD) {
            store_counter++;
            return val_storeToLoad;
        } else {
            stride_counter++;
            return val_stride;
        }
    }
};

HybridLoadValuePredictor *predictor;

VOID AtLoadInstruction(ADDRINT pc, ADDRINT addr) {
    UINT64 actualVal = 0;
    if (PIN_SafeCopy(&actualVal, reinterpret_cast<void*>(addr), sizeof(UINT64)) != sizeof(UINT64)) {
        return;
    }
    UINT64 val_context = predictor->getPredictionContext(pc);
    UINT64 val_storeToLoad = predictor->getPredictionStoreToLoad(addr);
    UINT64 val_stride = predictor->getPredictionStride(pc);

    UINT64 finalPrediction = predictor->finalPredictor(val_context, val_storeToLoad, val_stride);

    if (finalPrediction == actualVal) correctPredictions++;
    predictor->confidenceUpdate(actualVal, val_context, "context");
    predictor->confidenceUpdate(actualVal, val_storeToLoad, "storeToLoad");
    predictor->confidenceUpdate(actualVal, val_stride, "stride");
    predictor->train(pc, actualVal, addr);
    totalPredictions++;
}

VOID AtStoreInstruction(ADDRINT addr, UINT64 val) {
    predictor->RecordStoreVal(addr, val);
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

VOID Fini(INT32 code, VOID *v) {
    OutFile << "\n--- Summary ---\n";
    OutFile << "Total Predictions: " << totalPredictions << "\n";
    OutFile << "Correct Predictions: " << correctPredictions << "\n";
    OutFile << "Prediction Accuracy: "
            << (totalPredictions ? (100.0 * correctPredictions / totalPredictions) : 0.0)
            << " %" << endl;
    OutFile.close();

    cerr << "\n--- Summary ---\n";
    cerr << "Total Predictions: " << totalPredictions << "\n";
    cerr << "Correct Predictions: " << correctPredictions << "\n";
    cerr << "Prediction Accuracy: "
         << (totalPredictions ? (100.0 * correctPredictions / totalPredictions) : 0.0)
         << " %" << endl;
}

INT32 Usage() {
    cerr << "This tool simulates a hybrid load value predictor." << endl;
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

    outputFileName = "hybrid_lvp_output_" + std::to_string(TABLE_SIZE) + ".txt";
    OutFile.open(outputFileName);
    if (!OutFile.is_open()) {
        cerr << "Failed to open output file: " << outputFileName << endl;
        return -1;
    }
    cerr << "Output file: " << outputFileName << endl;

    predictor = new HybridLoadValuePredictor();

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}
