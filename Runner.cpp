#include <iostream>
#include <cstdio>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <chrono>
#include <string>
#include <array>
#include <map>
#include <sys/stat.h>

using namespace std;

struct Instance {
    string name; // instance name
    string file; // file name with dir
    string beta; // beta used to generate instance
    unsigned int optimal; // optimal value of solution
};

string execute(const char *cmd) {
    array<char, 128> buffer{};
    string result;
    shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}

// read the integer after the string s appear
map<string, string> readValues(stringstream &in) {
    string key, value;
    map<string, string> values;
    while (in.good()) {
        in >> key >> value;
        values[key] = value;
    }
    return values;
}

map<string, unsigned int> readOptimalFile(const string &location) {
    map<string, unsigned int> optimal;
    ifstream fin("instances/" + location, ios::in);
    string instName;
    unsigned int opt;
    while (fin.good()) {
        fin >> instName >> opt;
        optimal[instName] = opt;
    }
    fin.close();
    return optimal;
}

bool pathExists(const string &s) {
    struct stat buffer;
    return (stat(s.c_str(), &buffer) == 0);
}

string devToFormattedString(double dev) {
    char buffer[20];
    sprintf(buffer, "%.2f", dev);
    int stringEnd;
    for (stringEnd = 0; buffer[stringEnd] != '\0'; stringEnd++);

    if (buffer[stringEnd - 1] == '0') {
        buffer[stringEnd - 1] = ' ';
        if (buffer[stringEnd - 2] == '0') {
            buffer[stringEnd - 2] = ' ';
            buffer[stringEnd - 3] = ' ';
        }
    }

    return string(buffer);
}

void runInstances(const vector<Instance> &instances, const string &executionId, const string &outputFolder) {
//    unsigned long long timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(
//            std::chrono::system_clock::now().time_since_epoch()).count();

    static const unsigned int NUMBER_EXECUTIONS = 1;

    char buffer[512];
    sprintf(buffer, "output/%s/%s.txt", outputFolder.c_str(), executionId.c_str()); // output file
    ofstream fout(buffer, ios::out);

    cout << fixed;
    cout.precision(2);

    cout << "betas  Instance   TE(ms)  TI(ms)    opt       BestObj   BestDev       MeanObj   MeanDev    ResultModel    GapModelHeuristc    GapOptimalModel TimeModel   QuantRoutes" << endl;
    fout << "beta   Instance   TE(ms)  TI(ms)    opt       BestObj   BestDev       MeanObj   MeanDev    ResultModel    GapModelHeuristc    GapOptimalModel TimeModel   QuantRoutes" << endl;

    unsigned int better = 0, worse = 0, same = 0;
    unsigned int betterModelHeuristc = 0, worseModelHeuristc = 0, sameModelHeuristc = 0;
    unsigned int betterOptimalModel = 0, worseOptimalModel = 0, sameOptimalModel = 0;



    for (auto &instance: instances) {
        unsigned int sumObj = 0;
        unsigned int bestObj = numeric_limits<unsigned int>::max();
        unsigned int sumExecutionTime = 0;
        unsigned int sumBestSolutionTime = 0;
        unsigned int resultModel = 0;
        unsigned int executionTimeModel = 0;
        unsigned int routesInModel = 0;

        for (unsigned int i = 0; i < NUMBER_EXECUTIONS; i++) {
            sprintf(buffer, "./TSPrd %s %s %d", instance.file.c_str(), outputFolder.c_str(), i+1);
            stringstream stream(execute(buffer));

            map<string, string> values = readValues(stream);

            if (values.count("ERROR") > 0) {
                string error = "Error: " + values["ERROR"];
                sprintf(buffer, "%3s  %10s   %s", instance.beta.c_str(), instance.name.c_str(), error.c_str());
                cout << buffer << endl;
                fout << buffer << endl;
                continue;
            }

            unsigned int result = stoi(values["RESULT"]);
            unsigned int executionTime = stoi(values["EXEC_TIME"]);
            unsigned int bestSolutionTime = stoi(values["SOL_TIME"]);
            unsigned int resultModel = stoi(values["RESULT_MODEL"]);
            unsigned int executionTimeModel = stoi(values["EXEC_TIME_MODEL"]);
            unsigned int routesInModel = stoi(values["COUNT_ROUTES"]);



            sumObj += result;
            bestObj = min(bestObj, result);
            sumExecutionTime += executionTime;
            sumBestSolutionTime += bestSolutionTime;
        }

        const double meanObj = (double) sumObj / NUMBER_EXECUTIONS;
        const unsigned int meanExecutionTime = sumExecutionTime / NUMBER_EXECUTIONS;
        const unsigned int meanBestSolutionTime = sumBestSolutionTime / NUMBER_EXECUTIONS;

        if (meanObj < instance.optimal) better++;
        else if (meanObj > instance.optimal) worse++;
        else same++;

        double gapModelHeuristic = resultModel - meanObj;
        double gapOptimalModel = resultModel - instance.optimal;

        if (resultModel < meanObj) betterModelHeuristc++;
        else if (resultModel > meanObj) worseModelHeuristc++;
        else sameModelHeuristc++;

        if (resultModel < instance.optimal) betterOptimalModel++;
        else if (resultModel > instance.optimal) worseOptimalModel++;
        else sameOptimalModel++;


        double deviationMean = (((double) meanObj / instance.optimal) - 1) * 100;
        double deviationBest = (((double) bestObj / instance.optimal) - 1) * 100;

        sprintf(buffer, "%3s    %10s    %6d %6d %6d %6d % 6.2f%%    %9.2f   % 6.2f%%  %6d   %6.2f   %6.2f   %6.2d   %6d", instance.beta.c_str(),
                instance.name.c_str(), meanExecutionTime, meanBestSolutionTime, instance.optimal, bestObj,
                deviationBest, meanObj, deviationMean, resultModel, gapModelHeuristic, gapOptimalModel, executionTimeModel, routesInModel);

        cout << buffer << endl;
        fout << buffer << endl;
        fout.flush();
    }
    cout << "Better: " << better << "  |  Worse: " << worse << "  |  Same: " << same << endl;
    cout << "BetterModelHeuristc: " << betterModelHeuristc << "  |  WorseModelHeuristc: " << worseModelHeuristc << "  |  SameModelHeuristc: " << sameModelHeuristc << endl;
    cout << "BetterOptimalModel: " << betterOptimalModel << "  |  WorseOptimalModel: " << worseOptimalModel << "  |  SameOptimalModel: " << sameOptimalModel << endl;

    fout << "Better: " << better << "  |  Worse: " << worse << "  |  Same: " << same << endl;
    fout << "BetterModelHeuristc: " << betterModelHeuristc << "  |  WorseModelHeuristc: " << worseModelHeuristc << "  |  SameModelHeuristc: " << sameModelHeuristc << endl;
    fout << "BetterOptimalModel: " << betterOptimalModel << "  |  WorseOptimalModel: " << worseOptimalModel << "  |  SameOptimalModel: " << sameOptimalModel << endl;

    fout.close();
}

void runSolomonInstances(const string &outputFolder) {
    map<string, unsigned int> optimal = readOptimalFile("Solomon/0ptimal.txt");

    vector<unsigned int> ns({10, 15});
    //vector<unsigned int> ns({10, 15, 20, 50, 100});
    vector<string> names({"C101"});
    //vector<string> names({"C101", "C201", "R101", "RC101"});
    vector<string> betas({"0.5"});
    //vector<string> betas({"0.5", "1", "1.5", "2", "2.5", "3"});


    vector<Instance> instances;
    instances.reserve(betas.size() * names.size());
    for (auto &n: ns) {
        instances.clear();
        for (auto &beta: betas) {
            for (auto &name: names) {
                string file =
                        to_string(n) + "/" + name + "_" + beta; // NOLINT(performance-inefficient-string-concatenation)
                Instance instance = {name, "Solomon/" + file, beta, optimal[file]};
                instances.push_back(instance);
            }
        }

        cout << "for n = " << n << endl;
        runInstances(instances, "Solomon" + to_string(n), outputFolder);
    }
}

void runTSPLIBInstances(const string &outputFolder, int which = -1) {
    map<string, unsigned int> optimal = readOptimalFile("TSPLIB/0ptimal.txt");

    vector<string> names(
            {"eil51", "berlin52", "st70", "eil76", "pr76", "rat99", "kroA100", "kroB100", "kroC100", "kroD100",
             "kroE100", "rd100", "eil101", "lin105", "pr107", "pr124", "bier127", "ch130", "pr136", "pr144", "ch150",
             "kroA150", "kroB150", "pr152", "u159", "rat195", "d198", "kroA200", "kroB200", "ts225", "tsp225", "pr226",
             "gil262", "pr264", "a280", "pr299", "lin318", "rd400", "fl417", "pr439", "pcb442", "d493"});
    vector<string> betas({"0.5", "1", "1.5", "2", "2.5", "3"});
    if(which > 0) {
        betas = {betas[which-1]};
    }

    vector<Instance> instances(names.size() + betas.size());
    instances.resize(0); // resize but keep allocated memory

    for (auto &beta: betas) {
        for (auto &name: names) {
            string file = name + "_" + beta; // NOLINT(performance-inefficient-string-concatenation)
            Instance instance = {name, "TSPLIB/" + file, beta, optimal[file]};
            instances.push_back(instance);
        }
    }

    runInstances(instances, "TSPLIB", outputFolder + "_" + betas.back());
}

void runATSPLIBInstances(const string &outputFolder) {
    map<string, unsigned int> optimal = readOptimalFile("aTSPLIB/0ptimal.txt");

    vector<string> names({"ftv33", "ft53", "ftv70", "kro124p", "rbg403"});
    vector<string> betas({"0.5", "1", "1.5", "2", "2.5", "3"});

    vector<Instance> instances(names.size() + betas.size());
    instances.resize(0); // resize but keep allocated memory

    for (auto &name: names) {
        for (auto &beta: betas) {
            string file = name + "_" + beta; // NOLINT(performance-inefficient-string-concatenation)
            Instance instance = {name, "aTSPLIB/" + file, beta, optimal[file]};
            instances.push_back(instance);
        }
    }

    runInstances(instances, "aTSPLIB", outputFolder);
}

int main(int argc, char **argv) {
    string outputFolder = "Results";
    bool allowExistentFolder = true;

    int which;
    if (argc > 1) {
        which = stoi(argv[1]);
    } else {
        throw invalid_argument("missing argument");
    }

    if(pathExists("output/" + outputFolder)) {
        if(!allowExistentFolder) throw invalid_argument("output dir already exists!");
    } else {
        system(("mkdir -p output/" + outputFolder).c_str());
    }

    if(which == 0) {
        runSolomonInstances(outputFolder);
        //runATSPLIBInstances(outputFolder);
    } else {
        //runTSPLIBInstances(outputFolder, which);
    }
    return 0;
}

struct RouteDate {
    vector<vector<unsigned int> > route;
    unsigned int releaseTime;
    unsigned int duration;
};
