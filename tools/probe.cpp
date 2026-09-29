// Compiles every pattern of a grammars.dat with Boost, the way libprisma does, and writes down
// what each one matches in a corpus. PatternProbe.java does the same with java.util.regex and
// the two files are diffed, which is what holds the table to meaning one thing everywhere.
//
// The corpus is deliberately ASCII: Boost matches bytes where java.util.regex matches chars, so
// anything above U+007F diverges on offsets alone and would drown out the escape bugs.
#include <boost/regex.hpp>

#include "../libprisma/UnicodeEscapes.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

    std::string readAll(const char* path) {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    struct Reader {
        const std::string& bytes;
        size_t offset = 0;

        uint8_t uint8() {
            return offset < bytes.size() ? static_cast<uint8_t>(bytes[offset++]) : 0;
        }

        uint16_t uint16() {
            uint16_t low = uint8();
            return static_cast<uint16_t>(low | (uint8() << 8));
        }

        std::string string() {
            size_t length = uint8();
            if (length >= 254) {
                size_t a = uint8(), b = uint8(), c = uint8();
                length = a | (b << 8) | (c << 16);
            }
            std::string value = bytes.substr(offset, length);
            offset += length;
            return value;
        }
    };

    struct Entry {
        std::string source;
        boost::regex_constants::syntax_option_type flags;
    };

    // "/source/flags,alias,inside", the record LanguageTree::parsePatterns reads
    std::vector<Entry> read(const std::string& table) {
        Reader reader{ table };
        std::vector<Entry> entries;

        for (uint16_t count = reader.uint16(); count > 0; --count) {
            const std::string item = reader.string();
            const size_t begin = item.find_first_of('/');
            const size_t end = item.find_last_of('/');
            if (begin == std::string::npos || end <= begin) {
                continue;
            }

            const std::string options = item.substr(end + 1);
            const size_t aliasBegin = options.find_first_of(',');

            Entry entry;
            entry.source = item.substr(begin + 1, end - (begin + 1));
            entry.flags = boost::regex_constants::ECMAScript | boost::regex_constants::no_mod_m;

            for (size_t i = 0; i < aliasBegin && i < options.size(); ++i) {
                switch (options[i]) {
                case 'i': entry.flags |= boost::regex_constants::icase; break;
                case 'm': entry.flags &= ~boost::regex_constants::no_mod_m; break;
                default: break;  // l and y are libprisma's, for Prism lookbehind and greedy
                }
            }

            entries.push_back(entry);
        }

        return entries;
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: probe <grammars.dat> <corpus> <out>\n");
        return 2;
    }

    const std::vector<Entry> entries = read(readAll(argv[1]));

    std::string corpus = readAll(argv[2]);
    corpus.erase(std::remove(corpus.begin(), corpus.end(), '\r'), corpus.end());

    std::ofstream out(argv[3], std::ios::binary);
    int failed = 0;

    for (size_t i = 0; i < entries.size(); ++i) {
        boost::regex re;
        try {
            re.assign(libprisma::widenUnicodeClasses(entries[i].source), entries[i].flags);
        } catch (const std::exception& e) {
            if (failed++ < 20) {
                fprintf(stderr, "pattern %zu: %s\n    %.140s\n", i, e.what(), entries[i].source.c_str());
            }
            continue;
        }

        out << i;
        auto it = boost::sregex_iterator(corpus.begin(), corpus.end(), re,
                                         boost::regex_constants::match_not_dot_newline);
        for (auto end = boost::sregex_iterator(); it != end; ++it) {
            out << ' ' << it->position() << ':' << (it->position() + it->length());
        }
        out << '\n';
    }

    printf("compiled %zu of %zu patterns\n", entries.size() - failed, entries.size());

    if (failed > 0) {
        fprintf(stderr, "%d patterns Boost will not take.\n", failed);
        return 1;
    }
    return 0;
}
