/*
#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>

std::ofstream outfile;

struct StrideEntry {
    UINT64 lastValue = 0;
    INT64 stride = 0;
    bool valid = false;
};

const size_t TABLE_SIZE = 1024;
std::unordered_map<ADDRINT, StrideEntry> strideTable;

UINT64 totalLoads = 0, totalHits = 0, totalMisses = 0;

VOID StridePredictor(ADDRINT ip, ADDRINT addr, UINT32 size) {
    if (size > sizeof(UINT64)) return;

    UINT8 buffer[8] = {0};
    if (PIN_SafeCopy(buffer, reinterpret_cast<VOID *>(addr), size) != size)
        return;

    UINT64 actual_value = 0;
    for (UINT32 i = 0; i < size; ++i)
        actual_value |= (static_cast<UINT64>(buffer[i]) << (i * 8));

    totalLoads++;
    ADDRINT index = ip % TABLE_SIZE;

    StrideEntry entry;
    std::unordered_map<ADDRINT, StrideEntry>::iterator it = strideTable.find(index);
    if (it != strideTable.end()) {
        entry = it->second;
    }

    UINT64 predicted_value = entry.lastValue + entry.stride;
    bool hit = entry.valid && predicted_value == actual_value;

    if (hit) {
        totalHits++;
    } else {
        totalMisses++;
    }

    if (entry.valid) {
        entry.stride = actual_value - entry.lastValue;
    }

    entry.lastValue = actual_value;
    entry.valid = true;

    // ✅ Avoid operator[] to prevent STL internal warning
    strideTable.insert_or_assign(index, entry);

    outfile << std::hex << "IP: 0x" << ip << " | Addr: 0x" << addr
            << " | Pred: 0x" << predicted_value
            << " | Actual: 0x" << actual_value
            << " | " << (hit ? "HIT" : "MISS") << std::endl;
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StridePredictor,
                       IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v) {
    outfile << std::dec;
    outfile << "\n--- Summary ---\n";
    outfile << "Total Loads:  " << totalLoads << "\n";
    outfile << "Correct Predictions: " << totalHits << "\n";
    outfile << "Incorrect Predictions: " << totalMisses << "\n";
    outfile << "Hit Rate:     " << (totalLoads ? (100.0 * totalHits / totalLoads) : 0.0) << " %\n";
    outfile.close();
}

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) {
        std::cerr << "PIN Init Failed" << std::endl;
        return 1;
    }

    outfile.open("stride_prediction_output.txt");
    if (!outfile.is_open()) {
        std::cerr << "Could not open output file." << std::endl;
        return 1;
    }

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    return 0;
}

*/
/*
#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <sstream>

std::ofstream outfile;

struct StrideEntry {
    UINT64 lastValue = 0;
    INT64 stride = 0;
    bool valid = false;
};

const size_t TABLE_SIZE = 1024;
std::unordered_map<ADDRINT, StrideEntry> strideTable;

UINT64 totalLoads = 0, totalHits = 0, totalMisses = 0;

VOID StridePredictor(ADDRINT ip, ADDRINT addr, UINT32 size) {
    if (size > sizeof(UINT64)) return;

    UINT8 buffer[8] = {0};
    if (PIN_SafeCopy(buffer, reinterpret_cast<VOID *>(addr), size) != size)
        return;

    UINT64 actual_value = 0;
    for (UINT32 i = 0; i < size; ++i)
        actual_value |= (static_cast<UINT64>(buffer[i]) << (i * 8));

    totalLoads++;
    ADDRINT index = ip % TABLE_SIZE;

    StrideEntry entry;
    std::unordered_map<ADDRINT, StrideEntry>::iterator it = strideTable.find(index);
    if (it != strideTable.end()) {
        entry = it->second;
    }

    UINT64 predicted_value = entry.lastValue + entry.stride;
    bool hit = entry.valid && predicted_value == actual_value;

    if (hit) {
        totalHits++;
    } else {
        totalMisses++;
    }

    if (entry.valid) {
        entry.stride = static_cast<INT64>(actual_value) - static_cast<INT64>(entry.lastValue);
    }

    entry.lastValue = actual_value;
    entry.valid = true;

    strideTable.insert_or_assign(index, entry);

    // Include stride in output
    std::ostringstream logLine;
    logLine << std::hex
            << "IP: 0x" << ip
            << " | Addr: 0x" << addr
            << " | Pred: 0x" << predicted_value
            << " | Actual: 0x" << actual_value
            << " | Stride: " << std::dec << entry.stride
            << " | " << (hit ? "HIT" : "MISS") << std::endl;

    outfile << logLine.str();
    std::cerr << logLine.str();
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StridePredictor,
                       IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v) {
    std::ostringstream summary;
    summary << std::dec;
    summary << "\n--- Summary ---\n";
    summary << "Total Loads:           " << totalLoads << "\n";
    summary << "Correct Predictions:   " << totalHits << "\n";
    summary << "Incorrect Predictions: " << totalMisses << "\n";
    summary << "Hit Rate:              "
            << (totalLoads ? (100.0 * totalHits / totalLoads) : 0.0) << " %\n";

    outfile << summary.str();
    std::cerr << summary.str();

    outfile.close();
}

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) {
        std::cerr << "PIN Init Failed" << std::endl;
        return 1;
    }

    outfile.open("stride_prediction_output.txt");
    if (!outfile.is_open()) {
        std::cerr << "Could not open output file." << std::endl;
        return 1;
    }

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    return 0;
}
    */
/*
    #include "pin.H"
    #include <iostream>
    #include <fstream>
    #include <unordered_map>
    
    std::ofstream outfile;
    
    struct StrideEntry {
        UINT64 lastValue = 0;
        INT64 stride = 0;
        bool valid = false;
    };
    
    const size_t TABLE_SIZE = 1024;
    std::unordered_map<ADDRINT, StrideEntry> strideTable;
    
    UINT64 totalLoads = 0, totalHits = 0, totalMisses = 0;
    
    VOID StridePredictor(ADDRINT ip, ADDRINT addr, UINT32 size) {
        if (size > sizeof(UINT64)) return;
    
        UINT8 buffer[8] = {0};
        if (PIN_SafeCopy(buffer, reinterpret_cast<VOID *>(addr), size) != size)
            return;
    
        UINT64 actual_value = 0;
        for (UINT32 i = 0; i < size; ++i)
            actual_value |= (static_cast<UINT64>(buffer[i]) << (i * 8));
    
        totalLoads++;
        ADDRINT index = ip % TABLE_SIZE;
    
        StrideEntry entry;
        std::unordered_map<ADDRINT, StrideEntry>::iterator it = strideTable.find(index);
        if (it != strideTable.end()) {
            entry = it->second;
        }
    
        UINT64 predicted_value = entry.lastValue + entry.stride;
        bool hit = entry.valid && predicted_value == actual_value;
    
        if (hit) {
            totalHits++;
        } else {
            totalMisses++;
        }
    
        if (entry.valid) {
            entry.stride = actual_value - entry.lastValue;
        }
    
        entry.lastValue = actual_value;
        entry.valid = true;
    
        // Avoid operator[] to prevent STL internal warning
        strideTable.insert_or_assign(index, entry);
    
        outfile << std::hex << "IP: 0x" << ip << " | Addr: 0x" << addr
                << " | Pred: 0x" << predicted_value
                << " | Actual: 0x" << actual_value
                << " | Stride: " << std::dec << entry.stride
                << " | " << (hit ? "HIT" : "MISS") << std::endl;
    }
    
    VOID Instruction(INS ins, VOID *v) {
        if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StridePredictor,
                           IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
        }
    }
    
    VOID Fini(INT32 code, VOID *v) {
        outfile << std::dec;
        outfile << "\n--- Summary ---\n";
        outfile << "Total Loads:  " << totalLoads << "\n";
        outfile << "Correct Predictions: " << totalHits << "\n";
        outfile << "Incorrect Predictions: " << totalMisses << "\n";
        outfile << "Hit Rate:     " << (totalLoads ? (100.0 * totalHits / totalLoads) : 0.0) << " %\n";
    
        // Also print to terminal
        std::cerr << "\n--- Summary ---\n";
        std::cerr << "Total Loads:  " << totalLoads << "\n";
        std::cerr << "Correct Predictions: " << totalHits << "\n";
        std::cerr << "Incorrect Predictions: " << totalMisses << "\n";
        std::cerr << "Hit Rate:     " << (totalLoads ? (100.0 * totalHits / totalLoads) : 0.0) << " %\n";
    
        outfile.close();
    }
    
    int main(int argc, char *argv[]) {
        if (PIN_Init(argc, argv)) {
            std::cerr << "PIN Init Failed" << std::endl;
            return 1;
        }
    
        outfile.open("stride_prediction_output.txt");
        if (!outfile.is_open()) {
            std::cerr << "Could not open output file." << std::endl;
            return 1;
        }
    
        INS_AddInstrumentFunction(Instruction, 0);
        PIN_AddFiniFunction(Fini, 0);
    
        PIN_StartProgram();
        return 0;
    }
    
*/

/*
#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <list>

std::ofstream outfile;

struct StrideEntry {
    UINT64 lastValue = 0;
    INT64 stride = 0;
    bool valid = false;
};

// Configurable at compile time via -DTABLE_SIZE=...
#ifndef TABLE_SIZE
#define TABLE_SIZE 1024
#endif

// Fully associative table with FIFO eviction
std::unordered_map<ADDRINT, StrideEntry> strideTable;
std::list<ADDRINT> lruQueue;  // to track insertion order for eviction

UINT64 totalLoads = 0, totalHits = 0, totalMisses = 0;

VOID StridePredictor(ADDRINT ip, ADDRINT addr, UINT32 size) {
    if (size > sizeof(UINT64)) return;

    UINT8 buffer[8] = {0};
    if (PIN_SafeCopy(buffer, reinterpret_cast<VOID *>(addr), size) != size)
        return;

    UINT64 actual_value = 0;
    for (UINT32 i = 0; i < size; ++i)
        actual_value |= (static_cast<UINT64>(buffer[i]) << (i * 8));

    totalLoads++;

    StrideEntry entry;
    auto it = strideTable.find(ip);
    if (it != strideTable.end()) {
        entry = it->second;
    }

    UINT64 predicted_value = entry.lastValue + entry.stride;
    bool hit = entry.valid && predicted_value == actual_value;

    if (hit) {
        totalHits++;
    } else {
        totalMisses++;
    }

    if (entry.valid) {
        entry.stride = actual_value - entry.lastValue;
    }

    entry.lastValue = actual_value;
    entry.valid = true;

    // Evict if table is full and this is a new entry
    if (strideTable.find(ip) == strideTable.end() && strideTable.size() >= TABLE_SIZE) {
        ADDRINT to_evict = lruQueue.front();
        lruQueue.pop_front();
        strideTable.erase(to_evict);
    }

    strideTable[ip] = entry;

    // Maintain LRU order
    if (std::find(lruQueue.begin(), lruQueue.end(), ip) == lruQueue.end()) {
        lruQueue.push_back(ip);
    }

    outfile << std::hex << "IP: 0x" << ip << " | Addr: 0x" << addr
            << " | Pred: 0x" << predicted_value
            << " | Actual: 0x" << actual_value
            << " | Stride: " << std::dec << entry.stride
            << " | " << (hit ? "HIT" : "MISS") << std::endl;
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StridePredictor,
                       IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v) {
    outfile << std::dec;
    outfile << "\n--- Summary ---\n";
    outfile << "Total Loads:  " << totalLoads << "\n";
    outfile << "Correct Predictions: " << totalHits << "\n";
    outfile << "Incorrect Predictions: " << totalMisses << "\n";
    outfile << "Hit Rate:     " << (totalLoads ? (100.0 * totalHits / totalLoads) : 0.0) << " %\n";

    std::cerr << "\n--- Summary ---\n";
    std::cerr << "Total Loads:  " << totalLoads << "\n";
    std::cerr << "Correct Predictions: " << totalHits << "\n";
    std::cerr << "Incorrect Predictions: " << totalMisses << "\n";
    std::cerr << "Hit Rate:     " << (totalLoads ? (100.0 * totalHits / totalLoads) : 0.0) << " %\n";

    outfile.close();
}

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) {
        std::cerr << "PIN Init Failed" << std::endl;
        return 1;
    }

    outfile.open("stride_prediction_output.txt");
    if (!outfile.is_open()) {
        std::cerr << "Could not open output file." << std::endl;
        return 1;
    }

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    return 0;
}
*/

#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <list>
#include <vector>
#include <algorithm>
#include <string>

std::ofstream outfile;

struct StrideEntry {
    UINT64 lastValue = 0;
    INT64 stride = 0;
    bool valid = false;
};

UINT64 TABLE_SIZE = 1024; // Default, overridden via -table_size
std::unordered_map<ADDRINT, StrideEntry> strideTable;
std::list<ADDRINT> lruQueue;

UINT64 totalLoads = 0, totalHits = 0, totalMisses = 0;

VOID StridePredictor(ADDRINT ip, ADDRINT addr, UINT32 size) {
    if (size > sizeof(UINT64)) return;

    UINT8 buffer[8] = {0};
    if (PIN_SafeCopy(buffer, reinterpret_cast<VOID *>(addr), size) != size)
        return;

    UINT64 actual_value = 0;
    for (UINT32 i = 0; i < size; ++i)
        actual_value |= (static_cast<UINT64>(buffer[i]) << (i * 8));

    totalLoads++;

    StrideEntry entry;
    auto it = strideTable.find(ip);
    if (it != strideTable.end()) {
        entry = it->second;
    }

    UINT64 predicted_value = entry.lastValue + entry.stride;
    bool hit = entry.valid && predicted_value == actual_value;

    if (hit) {
        totalHits++;
    } else {
        totalMisses++;
    }

    if (entry.valid) {
        entry.stride = actual_value - entry.lastValue;
    }

    entry.lastValue = actual_value;
    entry.valid = true;

    if (strideTable.find(ip) == strideTable.end() && strideTable.size() >= TABLE_SIZE) {
        ADDRINT to_evict = lruQueue.front();
        lruQueue.pop_front();
        strideTable.erase(to_evict);
    }

    strideTable.insert_or_assign(ip, entry);

    // Maintain FIFO order for eviction
    if (std::find(lruQueue.begin(), lruQueue.end(), ip) == lruQueue.end()) {
        lruQueue.push_back(ip);
    }

    outfile << std::hex << "IP: 0x" << ip << " | Addr: 0x" << addr
            << " | Pred: 0x" << predicted_value
            << " | Actual: 0x" << actual_value
            << " | Stride: " << std::dec << entry.stride
            << " | " << (hit ? "HIT" : "MISS") << std::endl;
}

VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)StridePredictor,
                       IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v) {
    outfile << std::dec;
    outfile << "\n--- Summary ---\n";
    outfile << "Total Loads:  " << totalLoads << "\n";
    outfile << "Correct Predictions: " << totalHits << "\n";
    outfile << "Incorrect Predictions: " << totalMisses << "\n";
    outfile << "Hit Rate:     " << (totalLoads ? (100.0 * totalHits / totalLoads) : 0.0) << " %\n";

    std::cerr << "\n--- Summary ---\n";
    std::cerr << "Total Loads:  " << totalLoads << "\n";
    std::cerr << "Correct Predictions: " << totalHits << "\n";
    std::cerr << "Incorrect Predictions: " << totalMisses << "\n";
    std::cerr << "Hit Rate:     " << (totalLoads ? (100.0 * totalHits / totalLoads) : 0.0) << " %\n";

    outfile.close();
}

int main(int argc, char *argv[]) {
    // Extract custom args and remove them from argv before calling PIN_Init
    std::vector<char *> pin_args;
    pin_args.push_back(argv[0]);  // binary name

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-table_size" && i + 1 < argc) {
            TABLE_SIZE = std::stoull(argv[i + 1]);
            std::cerr << "TABLE_SIZE set to: " << TABLE_SIZE << std::endl;
            ++i; // Skip value
        } else {
            pin_args.push_back(argv[i]);
        }
    }

    int new_argc = pin_args.size();
    char **new_argv = &pin_args[0];

    if (PIN_Init(new_argc, new_argv)) {
        std::cerr << "PIN Init Failed" << std::endl;
        return 1;
    }

    outfile.open("stride_prediction_output.txt");
    if (!outfile.is_open()) {
        std::cerr << "Could not open output file." << std::endl;
        return 1;
    }

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    return 0;
}

