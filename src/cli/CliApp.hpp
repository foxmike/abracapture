#pragma once

#include <QStringList>

class CliApp {
public:
    CliApp() = default;

    int run(int argc, char* argv[]);
    int parseArguments(const QStringList& arguments);
    int listSources();
    int runRecord(const QStringList& recordArgs);
};
