#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: embed <input> <output> <variable>\n";
        return 1;
    }

    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open input file: " << argv[1] << "\n";
        return 1;
    }

    std::ofstream output(argv[2], std::ios::trunc);
    if (!output) {
        std::cerr << "Failed to open output file: " << argv[2] << "\n";
        return 1;
    }

    output << "#pragma once\n\n";
    output << "#include <string_view>\n\n";
    output << "namespace mwrender::detail {\n\n";
    output << "inline const unsigned char " << argv[3] << "_data[] = {";

    unsigned char c;
    std::size_t count = 0;
    while (input.read(reinterpret_cast<char*>(&c), 1)) {
        if (count % 12 == 0) {
            output << "\n    ";
        }
        output << "0x" << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(c) << ", ";
        ++count;
    }

    output << "\n};\n\n";
    output << "inline const std::string_view " << argv[3]
           << "(reinterpret_cast<const char*>(" << argv[3] << "_data), "
           << std::dec << count << ");\n\n";
    output << "} // namespace mwrender::detail\n";

    return 0;
}
