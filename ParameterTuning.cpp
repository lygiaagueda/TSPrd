#ifndef TSPRD_PARAMETERTUNING_CPP
#define TSPRD_PARAMETERTUNING_CPP

#include <map>
#include <iostream>
#include "GeneticAlgorithm.h"
#include <experimental/filesystem>
#include <regex>
#include <fstream>

namespace filesystem = experimental::filesystem;

struct Params {
    unsigned int mi;
    unsigned int lambda;
    double el;
    double nc;
    unsigned int itNi;

    unsigned int nClose() const {
        return (unsigned int) (nc * mi);
    }

    unsigned int nbElit() const {
        return (unsigned int) (el * mi);
    }

    unsigned int itDiv() const {
        return (unsigned int) (0.4 * itNi);
    }

    void print() const {
        cout << "mi: " << mi << "  lambda: " << lambda;
        cout << "  el: " << el << "  nc: " << nc << endl;
    }
};

static void copyInstances() {
    vector<string> paths = vector<string>({"TSPLIB", "aTSPLIB"});

    for (auto &p : paths) {
        string path = "instances/" + p;
        for (const auto &f: filesystem::directory_iterator(path)) {
            string filename = f.path().filename();
            if (filename.at(0) == '0') continue;

            unsigned int n = stoi(regex_replace(filename, regex("[^0-9]*([0-9]+).*"), string("$1")));

            if (n >= 150 && n <= 300) {
                filesystem::copy_file(f.path(), "instances/testSet/" + filename);
            }
        }
    }
}

static void selectInstances() {
    vector<string> instances;
    for (const auto &entry : filesystem::directory_iterator("instances/testSet")) {
        instances.push_back(entry.path());
    }

    if (instances.size() != 96)
        exit(1);

    sort(instances.begin(), instances.end());

    mt19937 generator((random_device()) ());
    uniform_int_distribution<int> dist(0, 5);

    for (int i = 0; i < instances.size(); i += 6) {
        int keep1 = dist(generator);
        int keep2;
        do {
            keep2 = dist(generator);
        } while (keep1 == keep2);
        for (int j = 0; j < 6; j++) {
            if (j != keep1 && j != keep2) {
                filesystem::remove(instances[i + j]);
            }
        }
        cout << endl << endl;
    }
}

GeneticAlgorithm runWith(const Instance &instance, const Params &params) {
    return GeneticAlgorithm(instance, params.mi, params.lambda, params.nClose(),
                            params.nbElit(), params.itNi, params.itDiv(),
                            60 * 60); //(unsigned int) (10 * 60 * (1976.0 / 1201.0))
}

void saveOptionalValues(int which = 0) {
//    vector<pair<unsigned int, string>> instances;
//    for (const auto &entry : filesystem::directory_iterator("instances/testSet")) {
//        string filename = entry.path().filename().string();
//        if(filename.at(0) != '0') {
//            unsigned int n = stoi(regex_replace(filename, regex("[^0-9]*([0-9]+).*"), string("$1")));
//            instances.emplace_back(n, filename.substr(0, filename.find_last_of('.')));
//        }
//    }
//    sort(instances.begin(), instances.end());
//    for(auto i : instances) {
//        cout << '"' <<  i.second << "\", ";
//    }

    vector<string> instances(
            {"ch150", "kroA150", "kroB150", "pr152", "u159", "rat195", "d198", "kroA200", "kroB200", "ts225", "tsp225",
             "pr226", "gil262", "pr264", "a280", "pr299"});
    vector<string> betas = {"0.5", "1", "1.5", "2", "2.5", "3"};
//    if (which == 1) {
//        betas = {"0.5", "1"};
//    } else if (which == 2) {
//        betas = {"1.5", "2"};
//    } else if (which == 3) {
//        betas = {"2.5", "3"};
//    } else if (which < 0) {
//        which = 0;
//    }


    if (which == 1) betas = {"0.5"};
    else if (which == 2) betas = {"1"};
    else if (which == 3) betas = {"1.5"};
    else if (which == 4) betas = {"2"};
    else if (which == 5) betas = {"2.5"};
    else if (which == 6) betas = {"3"};
    else if (which < 0) which = 0;

    string outfile = "instances/testSet/0ref" + to_string(which) + ".txt";
    ofstream fout(outfile, ios::out);

    for (const auto &instName: instances) {
        for (const auto &beta: betas) {
            string instanceName = instName + '_' + beta; // NOLINT(performance-inefficient-string-concatenation)
            unsigned int bestObj = numeric_limits<unsigned int>::max();
            for (int i = 0; i < 10; i++) {
                cout << "\rRunning " << instanceName << "  ";
                cout << to_string(i + 1) << "/10       ";
                fflush(stdout);
                Instance instance("testSet/" + instanceName);
                auto result = runWith(instance, {25, 100, 0.4, 0.2, 2000});
                bestObj = min(bestObj, result.getSolution().time);
            }
            fout << instanceName << " " << bestObj << endl;
            cout << endl << instanceName << " " << bestObj << endl;
        }
    }

    fout.close();
}

map<string, unsigned int> readOptimalFile() {
    map<string, unsigned int> optimal;
    ifstream fin("instances/testSet/0ref.txt", ios::in);
    string instName;
    unsigned int opt;
    while (fin.good()) {
        fin >> instName >> opt;
        optimal[instName] = opt;
    }
    fin.close();
    return optimal;
}

void testParams(const Params &params, const vector<string> &instances, const map<string, unsigned int> &optimals) {
    params.print();
    const unsigned int NUMBER_EXECUTIONS = 10;
    const unsigned int TOTAL_EXECUTIONS = NUMBER_EXECUTIONS * instances.size();

    double totalGap = 0;
    unsigned totalTimeExec = 0;
    unsigned totalTimeBest = 0;

    int nInstance = 0;
    for (auto const &instanceName : instances) {
        nInstance++;
        Instance instance("testSet/" + instanceName);
        for (unsigned int i = 0; i < NUMBER_EXECUTIONS; i++) {
            cout << "\rRunning: " << nInstance << "/" << instances.size();
            cout << "  " << (i + 1) << "/" << NUMBER_EXECUTIONS << "       ";
            fflush(stdout);
            auto ga = runWith(instance, params);
            double gap = (((double) ga.getSolution().time / optimals.at(instanceName)) - 1) * 100;
            totalGap += gap;
            totalTimeExec += ga.getExecutionTime();
            totalTimeBest += ga.getBestSolutionTime();

            ofstream itNiOut("instances/testSet/itni/" + instanceName + "_" + to_string(i + 1) + ".txt", ios::out);
            ga.writeXItNi(itNiOut);
            itNiOut.close();
        }
    }

    double meanGap = totalGap / TOTAL_EXECUTIONS;
    unsigned int meanTimeExec = totalTimeExec / TOTAL_EXECUTIONS;
    unsigned int meanTimeBest = totalTimeBest / TOTAL_EXECUTIONS;

    char buffer[200];
    sprintf(buffer, "(%2d %3d %.2f %.2f) -> %8d %8d % 7.2f%%", params.mi, params.lambda, params.el, params.nc,
            meanTimeExec, meanTimeBest, meanGap);
    cout << buffer << endl;

    ofstream fout("instances/testSet/0results.txt", ios::app);
    fout << buffer << endl;
    fout.close();
}

void run(int which = -1) {
    vector<Params> paramsSet({
                                     {20, 40, 0.5, 0.3, 20000},
                             });
    map<string, unsigned int> optimal = readOptimalFile();
    vector<string> instances;
    instances.reserve(optimal.size());
    for (auto const &x : optimal) instances.push_back(x.first);

    if (which == -1) {
        for (const auto &params: paramsSet) {
            testParams(params, instances, optimal);
        }
    } else {
        testParams(paramsSet[which], instances, optimal);
    }
}

void runItNi(int which = -1) {
    vector<string> instancesNames(
            {"ch150", "kroA150", "kroB150", "pr152", "u159", "rat195", "d198", "kroA200", "kroB200", "ts225", "tsp225",
             "pr226", "gil262", "pr264", "a280", "pr299"});
    vector<string> betas = {"0.5", "1", "1.5", "2", "2.5", "3"};

    if (which == 1) betas = {"0.5"};
    else if (which == 2) betas = {"1"};
    else if (which == 3) betas = {"1.5"};
    else if (which == 4) betas = {"2"};
    else if (which == 5) betas = {"2.5"};
    else if (which == 6) betas = {"3"};
    else if (which < 0) which = 0;

    Params params = {20, 40, 0.5, 0.3, 50000};

    vector<string> instances;
    for (const auto &a: instancesNames) {
        for (const auto &b: betas) {
            instances.push_back(a + "_" + b); // NOLINT(performance-inefficient-string-concatenation)
        }
    }

    map<string, unsigned int> optimals = readOptimalFile();

    testParams(params, instances, optimals);
}

void meanItNi() {
    const unsigned int ITS = 50000;

    map<string, unsigned int> optimals = readOptimalFile();
    vector<double> sumGaps(ITS + 1, 0.0);
    vector<unsigned int> sumTimes(ITS + 1, 0);

    unsigned int executionsCount = 0;

    for (const auto &entry : filesystem::directory_iterator("instances/testSet/itni")) {
        executionsCount++;
        string filename = entry.path().filename().string();
        string instance = filename.substr(0, filename.find_last_of('_'));

        unsigned int n = 0, te, obj, lastN = 0;
        double gap;
        ifstream fin(entry.path(), ios::in);
        while (fin.good() || n == ITS) {
            fin >> n >> te >> obj;
            if (n == lastN) break; else lastN = n;
            gap = (((double) obj / optimals.at(instance)) - 1) * 100;
            sumGaps[n] += gap;
            sumTimes[n] += te;
        }

        te = max(te, (unsigned int) (10 * 60 * (1976.0 / 1201.0)));
        for (n = n + 1; n <= ITS; n++) {
            sumGaps[n] += gap;
            sumTimes[n] += te;
        }

        fin.close();

    }


    char buffer[512];
    vector<string> gaps(ITS + 1);
    vector<unsigned int> tes(ITS + 1);
    for (unsigned int i = 1; i < sumGaps.size(); i++) {
        double meanGap = sumGaps[i] / executionsCount;
        sprintf(buffer, "%.2f", meanGap);
        gaps[i] = string(buffer);
        replace(gaps[i].begin(), gaps[i].end(), '.', ',');

        tes[i] = sumTimes[i] / executionsCount;
    }

    ofstream fout("instances/testSet/0itni.csv", ios::out);
    ofstream fout2("instances/testSet/0itni2.csv", ios::out);
    for (unsigned int i = 1; i < gaps.size(); i++) {
        sprintf(buffer, "%d;%s;%d", i, gaps[i].c_str(), tes[i]);
        string outs(buffer);
        fout2 << outs << endl;
        if (i == 0 || i == ITS || gaps[i] != gaps[i + 1]) {
            fout << outs << endl;
        }
    }
    fout2.close();
    fout.close();
}

int main(int argc, char **argv) {
    cout << fixed;
    cout.precision(2);
//    copyInstances();
//    meanItNi();

    int which = -1;
    if (argc > 1) {
        which = stoi(argv[1]);
    } else {
        cout << "Runing all params" << endl;
    }

//    saveOptionalValues(which);
    runItNi(which);
    return 0;
}

#endif //TSPRD_PARAMETERTUNING_CPP
