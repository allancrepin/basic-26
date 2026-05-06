#include "runner.h"

int main(int argc, char* argv[]) {

    if (argc == 2) {
        run_file(argv[1]);
        return 0;
    }

    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [program.ls|program.b26]\n";
        return 1;
    }

    run({ "5 + 2 + (3 - 4 * 2)" }, 2);
    run({ "10 / 2 + 3" }, 8);
    run({ "100 / (4 * 5)" }, 5);
    run({ "-3 + 10" }, 7);
    run({ "2 * (3 + 4) * -1" }, -14);

    run({ "ARRAY A(10);", "SET A(2) = 5;", "A(2)" }, 5);
    run({ "ARRAY A(10);", "SET A(4) = 3 * 3;", "A(4)" }, 9);
    run({ "ARRAY A(8);", "SET A(3) = 8;", "SET A(4) = 2;", "let b = A(3) + A(4);", "b" }, 10);
    run({ "let mul = 3;", "ARRAY data(4);", "SET data(2) = 8;", "data(2) * mul" }, 24);

    run({ "let x = 10\n", "x + 5" }, 15);
    run({ "let a = 3\n", "let b = 4", "a * a + b * b" }, 25);
    run({ "let base = 7", "let scale = 6", "base * scale" }, 42);
    run({ "let n = 100", "let d = 4", "n / d - 3" }, 22);
    run({ "let x = 2;", "let x = x * 3;", "x" }, 6);

    run({ "let a = 5",
        "let b = 20",
        "LABEL START",
        "let a = a + 1",
        "IF a < b THEN GOTO START",
        "a" },
        20);

    run({ R"(STRING a = "HELLO")", R"(STRING b = " WORLD")", R"(a = a + b)", "a(1)" }, 'E');
    run({ R"(STRING s = "ABC")", R"(SET s(1) = "U")", "s(1)" }, 'U');


    run({ R"(PRINT "HELLO")" });
    run({ R"(PRINT "FOO" + "BAR")" });
    run({ R"(STRING s = "WORLD")", R"(PRINT "HELLO " + s)" });
    run({ R"(STRING a = "AB")", R"(STRING b = "CD")", "PRINT a + b" });

    run({ "PRINT 42" });
    run({ "PRINT 3 + 4" });
    run({ "PRINT 10 / 2 + 3" });
    run({ "let x = 99", "PRINT x" });
    run({ "let a = 6", "let b = 7", "PRINT a * b" });

    run({ "let i = 0",
          "LABEL LOOP",
          "let i = i + 1",
          "PRINT i",
          "IF i < 5 THEN GOTO LOOP",
          "PRINT \"SUCCESS\"" });

    run({ R"(print "hello")" });


    run({ R"(print "hello")", "let x = 0", "print 1", "INPUT x", "print 2", "PRINT x"});
    

    return 0;
}
