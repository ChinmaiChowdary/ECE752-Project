#include "pin.H"
#include <iostream>
#include <unordered_map>

// Stores the last value loaded by each static load instruction (indexed by instruction address)
std::unordered_map<ADDRINT, UINT64> last_value_map;

// Counters for prediction statistics
UINT64 total_loads = 0;
UINT64 correct_predictions = 0;

/*!
 * Predicts the value using the last seen value and compares it with the actual loaded value.
 *
 * @param ip   The instruction pointer (address of the load instruction).
 * @param addr The effective memory address being read.
 * @param size The size in bytes of the memory being read.
 */
VOID PredictAndCheck(ADDRINT ip, ADDRINT addr, UINT32 size) {
    // Avoid unsupported large memory loads
    if (size > sizeof(UINT64)) return;

    // Safely copy memory from target program
    UINT8 buffer[8] = {0};
    if (PIN_SafeCopy(buffer, reinterpret_cast<VOID *>(addr), size) != size)
        return;

    // Reconstruct loaded value (little-endian)
    UINT64 actual_value = 0;
    for (UINT32 i = 0; i < size; ++i) {
        actual_value |= (static_cast<UINT64>(buffer[i]) << (i * 8));
    }

    total_loads++;

    // Predict and compare
    auto it = last_value_map.find(ip);
    if (it != last_value_map.end()) {
        UINT64 predicted_value = it->second;
        if (predicted_value == actual_value) {
            correct_predictions++;
        }
    }

    // Update last seen value
    last_value_map[ip] = actual_value;
}

/*!
 * Instruments memory-read (load) instructions.
 */
VOID Instruction(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PredictAndCheck,
                       IARG_INST_PTR,
                       IARG_MEMORYREAD_EA,
                       IARG_MEMORYREAD_SIZE,
                       IARG_END);
    }
}

/*!
 * Finalization routine — prints prediction stats.
 */
VOID Fini(INT32 code, VOID *v) {
    std::cerr << "====== Load Value Prediction Results ======" << std::endl;
    std::cerr << "Total Load Instructions Executed: " << total_loads << std::endl;
    std::cerr << "Correct Predictions:              " << correct_predictions << std::endl;
    std::cerr << "Prediction Accuracy:              "
              << (total_loads > 0 ? (100.0 * correct_predictions / total_loads) : 0.0)
              << " %" << std::endl;
    std::cerr << "===========================================" << std::endl;
}

/*!
 * Main entry point for the PIN tool.
 */
int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) {
        std::cerr << "PIN initialization failed." << std::endl;
        return 1;
    }

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram(); // Never returns
    return 0;
}
