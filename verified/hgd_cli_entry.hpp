#pragma once
// Unified correctness-first entrypoint for legacy executables.
// All wrappers share the same formally checked orientation implementation.
// Algorithm names identify the invocation, not an unverified optimized runtime.
#ifndef HGD_LEGACY_MODE
#define HGD_LEGACY_MODE "all"
#endif
#define main hgd_reference_main
#include "hgd_exact.cpp"
#undef main
int main(int argc, char** argv) {
    try {
        // Explicit subcommand follows hgd_exact syntax.
        if(argc >= 2) {
            std::string first(argv[1]);
            if(first == "all" || first == "query" || first == "insert" || first == "delete")
                return hgd_reference_main(argc, argv);
        }
        // Legacy binaries now require input parameters; hard-coded datasets are unsafe.
        // Usage: <executable> INPUT DELTA [K or BATCH] [VERTEX_COUNT]
        if(argc < 3) {
            std::cerr << "Usage: " << argv[0] << " INPUT DELTA"
                      << (std::string(HGD_LEGACY_MODE)=="query" ? " K" :
                          (std::string(HGD_LEGACY_MODE)=="all" ? "" : " BATCH_FILE" ))
                      << " [VERTEX_COUNT]\n"
                      << "Or: " << argv[0] << " all|query|insert|delete INPUT DELTA [K|BATCH] [VERTEX_COUNT]\n";
            return 2;
        }
        std::vector<char*> args;
        args.push_back(argv[0]);
        char mode[] = HGD_LEGACY_MODE;
        args.push_back(mode);
        for(int i=1;i<argc;++i) args.push_back(argv[i]);
        return hgd_reference_main((int)args.size(),args.data());
    } catch(const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
}
