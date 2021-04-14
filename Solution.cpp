#include <limits>
#include <algorithm>
#include <iostream>
#include "Solution.h"
#include "Split.h"

using namespace std;

Solution::Solution(
        vector<vector<unsigned int> > routes, unsigned int time, const Instance *instance
) : routes(std::move(routes)), time(time), instance(instance) {
    this->N = 0;
    for (auto &r: routes) {
        this->N += r.size() - 2; // excluding the depot at the start and end of the route
    }
}

Solution::Solution(const Instance &instance, Sequence &sequence, set<unsigned int> *depotVisits) : N(sequence.size()),
                                                                                                   instance(&instance) {
    set<unsigned int> depotVisitsTmp; // clients at the end of each route
    if (depotVisits == nullptr) {
        depotVisits = &depotVisitsTmp;
        Split::split(depotVisitsTmp, instance.getW(), instance.getRD(), sequence);
        // the split function return the time of the best routes and insert the last element of each route
        // in the depotVisits set
    }

    // create the routes given the depotVisits set
    routes.push_back(vector<unsigned int>({0}));
    for (unsigned int i : sequence) {
        routes.back().push_back(i);

        const bool is_in = depotVisits->find(i) != depotVisits->end();
        if (is_in) {
            routes.back().push_back(0);
            routes.push_back(vector<unsigned int>({0}));
        }
    }
    routes.back().push_back(0);

    time = update(); // calculate the times
}

unsigned int Solution::update() {
    routeRD.resize(routes.size());
    routeTime.resize(routes.size());

    for (int r = 0; r < routes.size(); r++) {
        auto &route = routes[r];
        routeRD[r] = 0;
        routeTime[r] = 0;

        for (int i = 1; i < route.size(); i++) {
            // calculate time to perform route
            routeTime[r] += instance->time(route[i - 1], route[i]);

            // and verify the maximum release date of the route
            unsigned int rdi = instance->releaseDateOf(route[i]);
            if (rdi > routeRD[r]) {
                routeRD[r] = rdi;
            }
        }
    }

    return updateStartingTimes();
}

// must be called when changes are made to the release date and times of the routes
unsigned int Solution::updateStartingTimes(unsigned int from) {
    routeStart.resize(routes.size());

    for (unsigned int r = from; r < routes.size(); r++) {
        // calculate the starting time of route = max between release time and finishing time of the previous route
        // the first route always have the release time as starting time
        routeStart[r] = r == 0 ? routeRD[r] : max(routeRD[r], routeStart[r - 1] + routeTime[r - 1]); //
    }
    this->time = routeStart.back() + routeTime.back();
    return time;
}

// verify if r is a empty route, and if so, delete it from routes
// return whether had a empty route
bool Solution::removeEmptyRoutes() {
    bool hasEmpty = false;
    for (int r = (int) routes.size() - 1; r >= 0; r--) {
        if (routes[r].size() == 2) { // just the depot at start and end
            routes.erase(routes.begin() + r);
            routeRD.erase(routeRD.begin() + r);
            routeTime.erase(routeTime.begin() + r);
            routeStart.erase(routeStart.begin() + r);
            hasEmpty = true;
        }
    }
    return hasEmpty;
}

Solution *Solution::copy() const {
    vector<vector<unsigned int> > r(this->routes);
    auto sol = new Solution(r, this->time, instance);
    sol->routeRD = this->routeRD;
    sol->routeTime = this->routeTime;
    sol->routeStart = this->routeStart;
    return sol;
}

void Solution::mirror(Solution *s) {
    this->routes = s->routes;
    this->routeRD = s->routeRD;
    this->routeTime = s->routeTime;
    this->routeStart = s->routeStart;
    this->time = s->time;
    this->id = s->id;
    this->N = s->N;
}

Sequence *Solution::toSequence() const {
    auto *s = new Sequence(this->N);
    int i = 0;
    for (const vector<unsigned int> &route: routes) {
        for (int j = 1; j < route.size() - 1; j++) {
            s->at(i) = route[j];
            i++;
        }
    }
    return s;
}

void Solution::printRoutes() {
    for (int i = 0; i < routes.size(); i++) {
        cout << "Route " << i + 1 << ": " << routes[i][0];
        for (int j = 1; j < routes[i].size(); j++) {
            cout << " -> " << routes[i][j];
        }
        cout << endl;
    }
}

void Solution::validate() {
    // check that all the routes are non-empty and start and end at the depot
    for (auto &route: routes) {
        if (route.size() <= 2) {
            throw logic_error("found empty route");
        }

        if (route.front() != 0) {
            throw logic_error("route does not start at depot");
        }

        if (route.back() != 0) {
            throw logic_error("route does not end at depot");
        }
    }

    // check if all clients are visited once
    vector<bool> visited(instance->nVertex(), false);
    visited[0] = true;
    for (auto &route: routes) {
        for (unsigned int i = 1; i < route.size() - 1; i++) {
            if (visited[route[i]])
                throw logic_error("client " + to_string(i) + " visited more than once");
            visited[route[i]] = true;
        }
    }

    // check all routes release date
    for (unsigned int r = 0; r < routes.size(); r++) {
        unsigned int rd = 0;
        for (unsigned int i = 1; i < routes[r].size(); i++) {
            rd = max(rd, instance->releaseDateOf(routes[r][i]));
        }
        if (routeRD[r] != rd) {
            throw logic_error("route " + to_string(r) + " has incorrect release date");
        }
    }

    // check all routes times
    for (unsigned int r = 0; r < routes.size(); r++) {
        unsigned int rtime = 0;
        for (int i = 1; i < routes[r].size(); i++) {
            rtime += instance->time(routes[r][i - 1], routes[r][i]);
        }
        if (routeTime[r] != rtime) {
            throw logic_error("route " + to_string(r) + " has incorrect time");
        }
    }

    // check all routes starting times
    for (unsigned int r = 0; r < routes.size(); r++) {
        unsigned int start = r == 0 ? routeRD[r] : max(routeRD[r], routeStart[r - 1] + routeTime[r - 1]);
        if (routeStart[r] != start) {
            throw logic_error("route " + to_string(r) + " has incorrect starting time");
        }
    }

    // check completion time
    if (time != routeStart.back() + routeTime.back()) {
        throw logic_error("incorrect solution time");
    }
}

bool Solution::equals(Solution *other) const {
    if (this->time != other->time || this->routes.size() != other->routes.size())
        return false;

    for (unsigned int r = 0; r < this->routes.size(); r++) {
        if (this->routes[r].size() != other->routes[r].size()
            || this->routeRD[r] != other->routeRD[r]
            || this->routeTime[r] != other->routeTime[r])
            return false;

        for(unsigned int c = 1; c < this->routes[r].size() - 1; c++) {
            if(this->routes[r][c] != other->routes[r][c])
                return false;
        }
    }
    return true;
}

// given a set of sequences, create a solution from each sequence
vector<Solution *> *Solution::solutionsFromSequences(
        const Instance &instance, vector<Sequence *> *sequences
) {
    auto *solutions = new vector<Solution *>(sequences->size());
    for (int i = 0; i < solutions->size(); i++) {
        solutions->at(i) = new Solution(instance, *(sequences->at(i)));
    }
    return solutions;
}