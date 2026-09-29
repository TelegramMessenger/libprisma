import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

/**
 * Compiles every pattern of a grammars.dat with java.util.regex and writes down what each one
 * matches in a corpus, for probe.cpp to be diffed against. Between them they hold the table to
 * the one thing it promises: that a pattern means the same to every client that reads it.
 *
 * The corpus is deliberately ASCII. Boost matches bytes and java.util.regex matches chars, so
 * anything above U+007F diverges on offsets alone and would drown out the escape bugs this is
 * looking for.
 */
public class PatternProbe {

    /** A pattern record, "/source/flags,alias,inside", as LanguageTree.cpp reads it. */
    private static final class Entry {
        String source;
        int flags;
    }

    private static final class Reader {
        private final byte[] bytes;
        private int offset;

        Reader(byte[] bytes) {
            this.bytes = bytes;
        }

        int uint8() {
            return bytes[offset++] & 0xFF;
        }

        int uint16() {
            return uint8() | (uint8() << 8);
        }

        String string() {
            int length = uint8();
            if (length >= 254) {
                length = uint8() | (uint8() << 8) | (uint8() << 16);
            }
            String value = new String(bytes, offset, length, StandardCharsets.ISO_8859_1);
            offset += length;
            return value;
        }
    }

    private static List<Entry> read(String path) throws IOException {
        Reader reader = new Reader(Files.readAllBytes(Paths.get(path)));
        List<Entry> entries = new ArrayList<>();

        for (int count = reader.uint16(); count > 0; count--) {
            String item = reader.string();
            int begin = item.indexOf('/');
            int end = item.lastIndexOf('/');
            if (begin < 0 || end <= begin) {
                continue;
            }

            String options = item.substring(end + 1);
            int aliasBegin = options.indexOf(',');

            Entry entry = new Entry();
            entry.source = item.substring(begin + 1, end);
            for (int i = 0; i < aliasBegin; ++i) {
                switch (options.charAt(i)) {
                    case 'i': entry.flags |= Pattern.CASE_INSENSITIVE; break;
                    case 'm': entry.flags |= Pattern.MULTILINE; break;
                    default: break;  // l and y are libprisma's, for Prism lookbehind and greedy
                }
            }
            entries.add(entry);
        }

        return entries;
    }

    /**
     * Outside MULTILINE, $ means end of input in ECMAScript but "end of input, or before a
     * line terminator that ends it" in java.util.regex. \z is the Java spelling of the first.
     * A Java client has to do this; it cannot go in the table, where \z would read as a z.
     */
    private static String endOfInput(String source) {
        if (source.indexOf('$') < 0) {
            return source;
        }

        StringBuilder result = new StringBuilder(source.length());
        boolean inClass = false;
        for (int i = 0; i < source.length(); ++i) {
            char c = source.charAt(i);
            if (c == '\\' && i + 1 < source.length()) {
                result.append(c).append(source.charAt(++i));
            } else if (c == '[') {
                inClass = true;
                result.append(c);
            } else if (c == ']') {
                inClass = false;
                result.append(c);
            } else if (c == '$' && !inClass) {
                result.append("\\z");
            } else {
                result.append(c);
            }
        }
        return result.toString();
    }

    public static void main(String[] args) throws IOException {
        List<Entry> entries = read(args[0]);
        String corpus = new String(Files.readAllBytes(Paths.get(args[1])), StandardCharsets.ISO_8859_1)
            .replace(String.valueOf((char) 13), "");

        StringBuilder out = new StringBuilder();
        int failed = 0;

        for (int i = 0; i < entries.size(); ++i) {
            Entry entry = entries.get(i);
            String source = entry.source;
            if ((entry.flags & Pattern.MULTILINE) == 0) {
                source = endOfInput(source);
            }

            Pattern pattern;
            try {
                pattern = Pattern.compile(source, entry.flags | Pattern.UNIX_LINES);
            } catch (PatternSyntaxException e) {
                if (failed++ < 20) {
                    System.err.println("pattern " + i + ": " + e.getDescription());
                    System.err.println("    " + entry.source.substring(0, Math.min(140, entry.source.length())));
                }
                continue;
            }

            out.append(i);
            java.util.regex.Matcher matcher = pattern.matcher(corpus);
            int at = 0;
            while (matcher.find(at) && out.length() < Integer.MAX_VALUE) {
                out.append(' ').append(matcher.start()).append(':').append(matcher.end());
                at = matcher.end() > matcher.start() ? matcher.end() : matcher.start() + 1;
                if (at > corpus.length()) {
                    break;
                }
            }
            out.append('\n');
        }

        Files.write(Paths.get(args[2]), out.toString().getBytes(StandardCharsets.US_ASCII));
        System.out.println("compiled " + (entries.size() - failed) + " of " + entries.size() + " patterns");

        if (failed > 0) {
            System.err.println(failed + " patterns java.util.regex will not take. "
                + "Teach normalizeEscapes in generate.js to rewrite them.");
            System.exit(1);
        }
    }
}
