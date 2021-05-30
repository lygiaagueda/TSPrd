#ifndef TSPRD_GENETICALGORITHM_H
#define TSPRD_GENETICALGORITHM_H


#include <chrono>
#include <fstream>
#include "Instance.h"
#include "NeighborSearch.h"
#include "Timer.h"

using namespace chrono;

class GeneticAlgorithm {
private:
    const Instance &instance;
    const unsigned int mi; // minimum size of the population
    const unsigned int lambda; // how much individuals will be generated from the mi individuals
    // max population size = mi + lambda
    const unsigned nbElite; // number of elite individuals (in terms of time) to survive the next generation
    const unsigned int nClose; // number of closest solutions to consider when calculating the nCloseMean
    const unsigned int itNi; // max number of iterations without improvement to stop the algorithm
    const unsigned int itDiv; // max number of iterations without improvement to diversify the current population
    const unsigned int timeLimit; // time limit of the execution of the algorithm in seconds

    NeighborSearch ns;
    Solution *bestSolution;

    Timer<milliseconds, steady_clock> timer;
    chrono::milliseconds endTime;
    chrono::milliseconds bestSolutionFoundTime;

    vector<pair<unsigned int, unsigned int> > searchProgress; // stores (time, value) of each best solution found
    vector<pair<unsigned int, pair<unsigned int, unsigned int>> > xItNi; // stores the (TE, obj) value if itNi was "key"

    // random number generator
    mt19937 generator;
    uniform_int_distribution<int> distPopulation; // distribution for the population [0, mi)

    vector<Sequence *> *initializePopulation();

    vector<double> getBiasedFitness(vector<Solution *> *solutions) const;

    vector<unsigned int> selectParents(vector<double> &biasedFitness);

    static double solutionsDistances(Solution *s1, Solution *s2, bool symmetric);

    static Sequence *orderCrossover(const Sequence &parent1, const Sequence &parent2);

    void survivalSelection(vector<Solution *> *solutions, unsigned int Mi);

    void survivalSelection(vector<Solution *> *solutions) { // default mi
        return survivalSelection(solutions, this->mi);
    }

    void diversify(vector<Solution *> *solutions);

public:
    GeneticAlgorithm(const Instance &instance, unsigned int mi, unsigned int lambda, unsigned int nClose,
                     unsigned int nbElite, unsigned int itNi, unsigned int itDiv, unsigned int timeLimit);

    const Solution &getSolution() {
        bestSolution->validate();
        return *bestSolution;
    };

    unsigned int getExecutionTime() {
        return endTime.count();
    }

    unsigned int getBestSolutionTime() {
        return bestSolutionFoundTime.count();
    }

    void writeResult(ofstream &fout) {
        fout << "EXEC_TIME " << getExecutionTime() << endl;
        fout << "SOL_TIME " << getBestSolutionTime() << endl;
        fout << "OBJ " << bestSolution->time << endl;
        fout << "N_ROUTES " << bestSolution->routes.size() << endl;
        fout << "N_CLIENTS";
        for (auto &r: bestSolution->routes) fout << " " << (r->size() - 2);
        fout << endl << "ROUTES" << endl;
        for (auto &r: bestSolution->routes) {
            for (unsigned int c = 1; c < r->size() - 1; c++) {
                fout << r->at(c) << " ";
            }
            fout << endl;
        }
        fout << endl;
    }

    void writeSearchProgress(ofstream &spout) {
        for (const auto &x: searchProgress) {
            spout << x.first << "\t" << x.second << endl;
        }
    }

    void writeXItNi(ofstream &out) {
        for(const auto &x: xItNi) {
            out << x.first << "\t" << x.second.first << "\t" << x.second.second << endl;
        }
    }
};


#endif //TSPRD_GENETICALGORITHM_H
