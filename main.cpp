#include <iostream>
#include <algorithm>
#include <fstream>
#include "Solution.h"
#include "GeneticAlgorithm.h"
#include "Grasp.h"

using namespace std;

int main(int argc, char **argv) {

    // genetic algorithm parameters
    unsigned int mi = 25;
    unsigned int lambda = 100;
    auto nbElite = (unsigned int) (0.4 * mi);
    auto nClose = (unsigned int) (0.2 * mi);
    unsigned int itNi = 2000; // max iterations without improvement to stop the algorithm
    auto itDiv = (unsigned int) (0.4 * itNi); // iterations without improvement to diversify

    // grasp parameters
    unsigned int itNiGrasp = 1000;
    double alpha = 0.2;

    auto timeLimit = (unsigned int) (10 * 60 * (1976.0 / 1201.0)); // in seconds

    string instanceFile = argv[1];
    Instance instance(instanceFile);

    auto alg = GeneticAlgorithm(instance, mi, lambda, nClose, nbElite, itNi, itDiv, timeLimit);
//    auto alg = Grasp(instance, itNiGrasp, alpha, timeLimit);
    Solution s = alg.getSolution();
    s.validate();

    cout << "RESULT " << s.time << endl;
    cout << "EXEC_TIME " << alg.getExecutionTime() << endl;
    cout << "SOL_TIME " << alg.getBestSolutionTime() << endl;

    // output to file
    unsigned long long timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    string outFile = to_string(timeStamp) + "/" + instanceFile;
    if (argc > 2)
        outFile = string(argv[2]) + "/" + instanceFile;
    string id = "1";
    if (argc > 3)
        id = string(argv[3]);
    string baseOutFile = "output/" + outFile + "_" + id;
    outFile = baseOutFile + ".txt";
    string dir = outFile.substr(0, outFile.find_last_of('/'));
    system(("mkdir -p " + dir).c_str());

    ofstream fout(outFile, ios::out);
    alg.writeResult(fout);
    fout.close();

    // output search progress
    string spFile = baseOutFile + "_SP.txt";
    ofstream spout(spFile, ios::out);
    alg.writeSearchProgress(spout);
    spout.close();

    string xItNiFile = baseOutFile + "_itni.txt";
    ofstream itNiOut(xItNiFile, ios::out);
    alg.writeXItNi(itNiOut);
    itNiOut.close();
    return 0;
}