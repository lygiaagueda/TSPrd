#include <chrono>
#include <random>
#include <queue>
#include <algorithm>
#include "GeneticAlgorithm.h"

void freePopulation(vector<Sequence *> *population) {
    for (auto p: *population) {
        delete p;
    }
    population->clear();
}

GeneticAlgorithm::GeneticAlgorithm(
        const Instance &instance, unsigned int mi, unsigned int lambda, unsigned int nClose, unsigned int nbElite,
        unsigned int itNi, unsigned int itDiv, unsigned int timeLimit
) : instance(instance), mi(mi), lambda(lambda), nClose(nClose), nbElite(nbElite), itNi(itNi), itDiv(itDiv),
    timeLimit(timeLimit), ns(instance) {
    srand(time(nullptr));

    beginTime = steady_clock::now();
    const steady_clock::time_point maxTime = beginTime + seconds(timeLimit);

    vector<Sequence *> *population = initializePopulation();
    // represents the population for the genetic algorithm
    // the population is simple the big tours (tours sequence) ignoring visits to the depot
    vector<Solution *> *solutions = Solution::solutionsFromSequences(instance, population);
    // represents the solution itself, with a set of routes
    // during the genetic algorithm execution, we frequently transforms the population in solutions
    // applying the split algorithm which find the optimal depot visits for each sequence;
    // and we transform the solutions in a population, removing the depot visits from the solutions
    // and constructing a vector with only the clients visit sequence

    auto *bestSolution = new Solution(vector<vector<unsigned int> >(), 999999);

    int iterations_not_improved = 0;

    while (iterations_not_improved < this->itNi && steady_clock::now() < maxTime) {
        vector<double> biasedFitness = getBiasedFitness(solutions);

        while (solutions->size() < mi + lambda) {
            // SELECAO DOS PARENTES PARA CROSSOVER
            vector<unsigned int> p = selectParents(biasedFitness);

            Sequence *child = GeneticAlgorithm::orderCrossover(*population->at(p[0]), *population->at(p[1]));
            auto *sol = new Solution(instance, *child);
            delete child;

            // EDUCACAO
            ns.educate(sol);
            solutions->push_back(sol);

            if (sol->time < bestSolution->time) {
                bestSolutionFoundTime = steady_clock::now();
                delete bestSolution;
                bestSolution = sol->copy();
                // cout << "Best Solution Found: " << bestSolution->time << endl;

                iterations_not_improved = 0;
            } else {
                iterations_not_improved++;
                if (iterations_not_improved % this->itDiv == 0) {
                    diversify(solutions);
                } else if (iterations_not_improved == this->itNi) {
                    break;
                }
            }
            if (steady_clock::now() > maxTime) break; // time limit
        }

        survivalSelection(solutions);

        //recalculate population
        freePopulation(population);
        for (Solution *s: *solutions) {
            population->push_back(s->toSequence());
        }
    }

    endTime = steady_clock::now();

    this->solution = bestSolution;
}

vector<Sequence *> *GeneticAlgorithm::initializePopulation() {
    // initial population will be of size 2*mi generated randomly
    vector<unsigned int> clients(instance.nClients()); // represents the sequence of clients visiting (big tour)
    // will be shuffled for each element of the population
    iota(clients.begin(), clients.end(), 1);

    auto rand = default_random_engine(chrono::system_clock::now().time_since_epoch().count());

    auto pop = new vector<Sequence *>(mi + lambda);
    pop->resize(2 * mi);
    // reduces the size of the vector, but keeps enough space to store the max population without needing reallocation

    for (int i = 0; i < 2 * mi; i++) {
        auto sequence = new Sequence(clients);
        shuffle(sequence->begin(), sequence->end(), rand);
        pop->at(i) = sequence;
    }

    return pop;
}

/**
 *
 * @param s1 first solution to be compared
 * @param s2 second solution to be compared
 * @return distance between the solutions
 *
 * the distance is calculated based in how many arcs the routes from the solutions have in common
 */
double GeneticAlgorithm::solutionsDistances(Solution *s1, Solution *s2) {
    const unsigned int N = s1->N + 1;
    // each position i of the vector represents the client that comes after the client i in the solution
    // s1Arcs[i] = x -> (i, x) ∈ Arcs(s1)
    vector<unsigned int> s1Arcs(N), s2Arcs(N);
    // destination of arcs with the origin in the depot (first client of each route)
    set<unsigned int> s1Depots, s2Depots; // x: (0, x) ∈ arcs

    for (const vector<unsigned int> &route: s1->routes) { // calcula os arcos em s1
        s1Depots.insert(route[1]);
        for (int i = 2; i < route.size() - 1; i++) {
            s1Arcs[route[i - 1]] = route[i];
        }
    }
    for (const vector<unsigned int> &route: s2->routes) { // calcula os arcos em s2
        s2Depots.insert(route[1]);
        for (int i = 2; i < route.size() - 1; i++) {
            s2Arcs[route[i - 1]] = route[i];
        }
    }

    unsigned int I = 0; // number of arcs that exists in both solutions
    unsigned int U = 0; // number of arcs in Arcs(s1) U Arcs(s2)
    for (unsigned int x: s1Depots) { // compara arcos que saem do deposito
        I += s2Depots.erase(x);
    }
    U += s1Depots.size() + s2Depots.size();
    // at that point we removed from s2 all the arcs in the intersection

    for (int i = 1; i < s1Arcs.size(); i++) { // comparam restante das arcos
        int equal = s1Arcs[i] == s2Arcs[i];
        I += equal;

        // when the arcs are different, there are 2 more arcs in the union of arcs of the solutions
        // but when they are equal, there are only 1 more arc
        U += 2 - equal;
    }


    return 1 - ((double) I / U);
}

/**
 *
 * @param d matrix of the distances of the solutions
 * @param n_close how many closest solutions should the algorithm consider to calculate the mean
 * @param i which solution to calculate the mean
 * @return the mean of the distances of the solution i to the 'nClose' closest solutions
 */
double nCloseMean(const vector<vector<double> > &d, unsigned int n_close, unsigned int i) {
    const vector<double> &di = d[i];
    // criamos uma fila de prioridade para armazenar as n_close menores distancias
    priority_queue<double> pq;
    // insere as distancias dos primeiros n_close clientes
    int j;
    for (j = 0; pq.size() < n_close; j++) {
        if (i == j)
            continue;
        pq.push(di[j]);
    }

    // para cada distancia restante verifica se eh menor que alguma anterior
    for (; j < d.size(); j++) {
        if (i == j)
            continue;

        double dij = di[j];
        if (dij < pq.top()) {
            pq.pop();
            pq.push(dij);
        }
    }

    // calcula a media das menores distancias;
    double sum = 0;
    for (j = 0; j < n_close; j++) {
        sum += pq.top();
        pq.pop();
    }

    return sum / n_close;
}

vector<double> GeneticAlgorithm::getBiasedFitness(vector<Solution *> *solutions) const {
    int N = solutions->size(); // nbIndiv
    vector<double> biasedFitness(N);

    vector<vector<double> > d(N, vector<double>(N)); // guarda a distancia entre cada par de cromossomo
    for (int i = 0; i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            d[i][j] = solutionsDistances(solutions->at(i), solutions->at(j));
            d[j][i] = d[i][j];
        }
    }

    vector<double> nMean(N);
    for (int i = 0; i < N; i++) {
        nMean[i] = nCloseMean(d, nClose, i);
    }

    vector<unsigned int> sortedIndex(N);
    iota(sortedIndex.begin(), sortedIndex.end(), 0);
    sort(sortedIndex.begin(), sortedIndex.end(), [&nMean](int i, int j) {
        return nMean[i] > nMean[j];
    });

    vector<unsigned int> rankDiversity(N); // rank of the solution with respect to the diversity contribution
    // best solutions (higher nMean distance) have the smaller ranks
    for (int i = 0; i < rankDiversity.size(); i++) {
        rankDiversity[sortedIndex[i]] = i + 1;
    }

    // rank with respect to the time of the solution
    // best solutions (smaller times) have the smaller ranks
    sort(sortedIndex.begin(), sortedIndex.end(), [&solutions](int i, int j) {
        return solutions->at(i)->time < solutions->at(j)->time;
    });
    vector<unsigned int> rankFitness(N);
    for (int i = 0; i < rankFitness.size(); i++) {
        rankFitness[sortedIndex[i]] = i + 1;
    }

    // calculate the biased fitness with the equation 4 of the vidal article
    // best solutions have smaller biased fitness
    biasedFitness.resize(solutions->size());
    for (int i = 0; i < biasedFitness.size(); i++) {
        biasedFitness[i] = rankFitness[i] + (1 - ((double) nbElite / N)) * rankDiversity[i];
    }

    return biasedFitness;
}

vector<unsigned int> GeneticAlgorithm::selectParents(vector<double> &biasedFitness) const {
    // seleciona o primeiro pai
    vector<unsigned int> p(2);
    unsigned int p1a = rand() % mi, p1b = rand() % mi;
    p[0] = p1a;
    if (biasedFitness[p1b] < biasedFitness[p1a])
        p[0] = p1b;


    // seleciona segundo pai, diferente do primeiro
    do {
        int p2a = rand() % mi, p2b = rand() % mi;
        p[1] = p2a;
        if (biasedFitness[p2b] < biasedFitness[p2a])
            p[1] = p2b;
    } while (p[0] == p[1]);

    return p;
}

Sequence *GeneticAlgorithm::orderCrossover(const Sequence &parent1, const Sequence &parent2) {
    int N = parent1.size();

    // choose radomly a sub sequence of the first parent that goes to the offspring
    // a and b represents the start and end index of the sequence, respectively
    int a, b;
    do {
        a = rand() % N;
        b = rand() % N;
        if (a > b) {
            swap(a, b);
        }
    } while ((a == 0 && b == N - 1) || (b - a) <= 1);

    // copy the chosen subsequence from the first parent to the offspring
    vector<bool> has(N, false);
    auto *offspring = new vector<unsigned int>(N);
    for (int i = a; i <= b; i++) {
        offspring->at(i) = parent1[i];
        has[offspring->at(i)] = true;
    }

    // copy the remaining elements keeping the relative order that they appear in the second parent
    int x = 0;
    for (int i = 0; i < a; i++) {
        while (has[parent2[x]]) // pula os elementos que já foram copiados do primeiro pai
            x++;

        offspring->at(i) = parent2[x];
        x++;
    }
    for (int i = b + 1; i < N; i++) {
        while (has[parent2[x]]) // pula os elementos que já foram copiados do primeiro pai
            x++;

        offspring->at(i) = parent2[x];
        x++;
    }

    return offspring;
}

void GeneticAlgorithm::survivalSelection(vector<Solution *> *solutions, unsigned int Mi) {
    vector<double> biasedFitness = getBiasedFitness(solutions);
    for (int i = 0; i < solutions->size(); i++) {
        solutions->at(i)->id = i;
    }

    // look for clones and give them a very low biased fitness
    // this way they will be removed from population
    const double INF = instance.nVertex() * 10;
    vector<bool> isClone(solutions->size(), false);
    for(int i = 0; i < solutions->size(); i++) {
        if(isClone[i]) continue;
        for(int j = i+1; j < solutions->size(); j++) {
            if(solutions->at(i)->equals(solutions->at(j))) {
                isClone[j] = true;
                biasedFitness[j] += INF;
            }
        }
    }

    // sort the solutions based on the biased fitness and keep only the best 'mi' solutions
    sort(solutions->begin(), solutions->end(), [&biasedFitness](Solution *s1, Solution *s2) {
        return biasedFitness[s1->id] < biasedFitness[s2->id];
    });
    solutions->resize(Mi);
}

void GeneticAlgorithm::diversify(vector<Solution *> *solutions) {
    survivalSelection(solutions, mi / 3); // keeps the mi/3 best solutions we have so far

    // generate more solutions with the same procedure that generated the initial population
    // and appends to the current solutions
    vector<Sequence *> *population = initializePopulation();

    for (auto *sequence: *population) {
        solutions->push_back(new Solution(instance, *sequence));
        delete sequence;
    }
    delete population;
}