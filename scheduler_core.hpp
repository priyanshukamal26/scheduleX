// ============================================================================
// scheduler_core.hpp
// ============================================================================
//
// This is the BRAIN of the entire project.
// It contains ALL the scheduling logic — graph building, BFS topological sort,
// clash detection, and greedy exam-slot assignment — as pure functions.
//
// KEY DESIGN RULE: this file has ZERO input/output (no cin, no cout, no
// console printing). It only takes data in and returns data out.  That's
// what lets us compile the exact same algorithm for:
//   1. The terminal (g++)           — course_scheduler.cpp calls these
//   2. The browser via WASM (em++)  — wasm_bridge.cpp calls these
//
// Everything uses plain vectors and structs — no classes, no inheritance,
// no templates beyond what the STL already provides.  Beginner-friendly.
//
// ============================================================================

#ifndef SCHEDULER_CORE_HPP
#define SCHEDULER_CORE_HPP

#include <vector>
#include <string>
#include <algorithm>

using namespace std;

// ============================================================================
// DATA STRUCTURES
// ============================================================================
// These structs are just containers for holding related data together.
// Think of them as "named groups of variables".

// Represents one course: it has a name and one faculty member in charge.
struct Course {
    string name;
    string faculty;
};

// Everything the user enters before we can generate a schedule.
// One struct to rule them all — makes it easy to pass around.
struct SchedulerInput {
    // The list of all courses offered.
    vector<Course> courses;

    // The prerequisite graph:
    //   adj[u] = list of courses that DEPEND on course u
    //            (i.e., course u must come before these courses)
    //   prereqOf[v] = list of courses that course v DEPENDS ON
    //                 (i.e., these must be done before v)
    //   inDegree[v] = how many prerequisites course v has
    //
    // Example: if "Data Structures" requires "Intro to Programming",
    //   and "Intro to Programming" is course 0, "Data Structures" is course 1:
    //     adj[0] = {1}        (course 1 depends on course 0)
    //     prereqOf[1] = {0}   (course 1 needs course 0 first)
    //     inDegree[1] = 1     (course 1 has 1 prerequisite)
    vector<vector<int>> adj;
    vector<vector<int>> prereqOf;
    vector<int> inDegree;

    // Student data:
    //   studentNames[i]   = name of student i
    //   studentCourses[i] = list of course indices that student i is taking
    vector<string> studentNames;
    vector<vector<int>> studentCourses;

    // Logistics for the exam:
    int hallCapacity;     // max students one exam hall can seat
    int hallsAvailable;   // how many halls can run exams simultaneously
    int sessionsPerDay;   // exam sessions per day (e.g., 2 = morning + afternoon)
    int maxDays;          // how many calendar days are available for exams
};

// The result of running the scheduler — everything we need to display.
struct SchedulerResult {
    // Did the scheduling succeed? false if there's a cycle in prerequisites.
    bool success;

    // If there IS a cycle, this lists the names of courses stuck in it.
    vector<string> cycleCoursesNames;

    // The topological order — the "safe" order to schedule exams in.
    // Each entry is a course index, in the order BFS/Kahn's determined.
    vector<int> topoOrder;

    // For each course, which slot(s) it was assigned to.
    //   startSlot[courseIndex] = first slot assigned
    //   endSlot[courseIndex]   = last slot assigned (same as start unless batched)
    vector<int> startSlot;
    vector<int> endSlot;

    // What courses are in each slot.
    //   slotCourses[slotIndex] = list of course indices in that slot
    vector<vector<int>> slotCourses;

    // How many students are enrolled in each course.
    vector<int> enrolled;

    // How many consecutive sessions each course needs (>1 if too big for one hall).
    vector<int> sessionsNeeded;

    // How many total days the generated schedule actually uses.
    int totalDaysUsed;

    // Did the schedule fit within the available days?
    bool fitsInAvailableDays;
};


// ============================================================================
// HELPER: day/session label
// ============================================================================
// Converts a raw slot number (0, 1, 2, ...) into something readable.
// Example: slot 3 with 2 sessions/day -> "Day 2 Afternoon"
//          slot 5 with 3 sessions/day -> "Day 2 Session 3"

string slotLabel(int slot, int sessionsPerDay) {
    // Which day is this slot on? (integer division)
    int day = slot / sessionsPerDay + 1;
    // Which session within that day? (remainder)
    int session = slot % sessionsPerDay + 1;

    // Special case: if there are exactly 2 sessions/day, call them
    // Morning and Afternoon instead of Session 1 / Session 2.
    if (sessionsPerDay == 2) {
        return "Day " + to_string(day) + " " + (session == 1 ? "Morning" : "Afternoon");
    }
    return "Day " + to_string(day) + " Session " + to_string(session);
}


// ============================================================================
// FUNCTION 1: Build the clash matrix
// ============================================================================
// Two courses "clash" if they can't be in the same exam slot.
// Reasons for clashing:
//   1. Same faculty teaches both (faculty can't be in two halls at once)
//   2. At least one student is enrolled in both (student can't write two exams)
//
// Returns an n×n matrix where clash[a][b] = true means a and b can't share a slot.

vector<vector<bool>> buildClashMatrix(
    const vector<Course>& courses,
    const vector<vector<int>>& studentCourses
) {
    int n = (int)courses.size();

    // Start with no clashes — everything is false by default.
    vector<vector<bool>> clash(n, vector<bool>(n, false));

    // Reason 1: same faculty
    // Compare every pair of courses. If the faculty name matches, mark as clash.
    for (int a = 0; a < n; a++) {
        for (int b = a + 1; b < n; b++) {
            if (courses[a].faculty == courses[b].faculty) {
                clash[a][b] = true;
                clash[b][a] = true;  // symmetric — if a clashes with b, b clashes with a
            }
        }
    }

    // Reason 2: shared student
    // For each student, every pair of courses they're taking clashes.
    for (int i = 0; i < (int)studentCourses.size(); i++) {
        for (int a = 0; a < (int)studentCourses[i].size(); a++) {
            for (int b = a + 1; b < (int)studentCourses[i].size(); b++) {
                int c1 = studentCourses[i][a];
                int c2 = studentCourses[i][b];
                clash[c1][c2] = true;
                clash[c2][c1] = true;
            }
        }
    }

    return clash;
}


// ============================================================================
// FUNCTION 2: Count how many students are enrolled in each course
// ============================================================================
// Simple counting: for every course a student is taking, increment that
// course's enrollment counter.

vector<int> computeEnrollment(const vector<vector<int>>& studentCourses, int n) {
    vector<int> enrolled(n, 0);

    for (int i = 0; i < (int)studentCourses.size(); i++) {
        for (int c : studentCourses[i]) {
            enrolled[c]++;
        }
    }

    return enrolled;
}


// ============================================================================
// FUNCTION 3: Figure out how many exam sessions each course needs
// ============================================================================
// If a course has more students than one hall can seat, we split it into
// multiple back-to-back sessions (batches).
//
// Example: 250 students, hall seats 100 → needs ceil(250/100) = 3 sessions.
// The exam happens in 3 consecutive slots, with ~83-84 students each.

vector<int> computeSessionsNeeded(const vector<int>& enrolled, int hallCapacity) {
    int n = (int)enrolled.size();
    vector<int> needed(n, 1);  // every course needs at least 1 session

    for (int i = 0; i < n; i++) {
        if (enrolled[i] > hallCapacity) {
            // Ceiling division: (a + b - 1) / b gives ceil(a/b)
            needed[i] = (enrolled[i] + hallCapacity - 1) / hallCapacity;
        }
    }

    return needed;
}


// ============================================================================
// FUNCTION 4: Topological sort using BFS (Kahn's algorithm)
// ============================================================================
// This is the core graph algorithm. It finds an ordering of courses where
// every prerequisite comes before the course that needs it.
//
// How Kahn's algorithm works:
//   1. Find all courses with no prerequisites (in-degree = 0). These are "ready".
//   2. Pick one ready course, add it to the output order.
//   3. For each course that depended on the one we just picked:
//      reduce its in-degree by 1. If it hits 0, it's now ready too.
//   4. Repeat until no more ready courses.
//   5. If we processed all n courses, great! If not, there's a cycle.
//
// Our twist: when multiple courses are ready at the same time, we pick the
// one with the MOST dependents first. This gets "bottleneck" courses
// (the ones blocking the most other courses) out of the way early,
// which tends to produce shorter schedules.
//
// Returns:
//   .first  = the topological order (list of course indices), possibly incomplete
//   .second = list of course indices stuck in a cycle (empty if no cycle)

pair<vector<int>, vector<int>> topologicalSort(
    const vector<vector<int>>& adj,
    const vector<int>& inDegree,
    int n
) {
    // Make a working copy of in-degrees because we'll be modifying them
    // as we "remove" courses from the graph.
    vector<int> remaining = inDegree;

    // Step 1: find all courses with no prerequisites.
    vector<int> ready;
    for (int i = 0; i < n; i++) {
        if (remaining[i] == 0) {
            ready.push_back(i);
        }
    }

    // This will hold our final ordering.
    vector<int> order;

    // Step 2-4: keep picking ready courses until none are left.
    while (!ready.empty()) {
        // Find the ready course with the most dependents (our "smart pick").
        int bestPos = 0;
        for (int i = 1; i < (int)ready.size(); i++) {
            if (adj[ready[i]].size() > adj[ready[bestPos]].size()) {
                bestPos = i;
            }
        }

        // Remove it from the ready list (swap with last element and pop — fast).
        int u = ready[bestPos];
        ready[bestPos] = ready.back();
        ready.pop_back();

        // Add it to our output order.
        order.push_back(u);

        // Step 3: "remove" this course from the graph — for each course that
        // depended on it, reduce their in-degree.
        for (int v : adj[u]) {
            remaining[v]--;
            if (remaining[v] == 0) {
                ready.push_back(v);  // this course is now ready!
            }
        }
    }

    // Step 5: check for cycles.
    // If we couldn't process all courses, some are stuck in a cycle.
    vector<int> stuck;
    if ((int)order.size() != n) {
        // Mark which courses DID get processed.
        vector<bool> done(n, false);
        for (int i : order) done[i] = true;

        // Everything else is stuck.
        for (int i = 0; i < n; i++) {
            if (!done[i]) stuck.push_back(i);
        }
    }

    return {order, stuck};
}


// ============================================================================
// FUNCTION 5: Assign exam slots (the actual scheduling)
// ============================================================================
// Goes through courses in topological order and greedily assigns each one
// to the earliest slot (or block of consecutive slots) that:
//   1. Comes AFTER all its prerequisites' slots are done
//   2. Isn't already full (hallsAvailable limit per slot)
//   3. Doesn't clash with anything already in that slot
//
// This greedy approach works well because the topological order already
// ensures prerequisite ordering, and the clash matrix prevents conflicts.

struct SlotAssignment {
    vector<int> startSlot;              // startSlot[course] = first slot assigned
    vector<int> endSlot;                // endSlot[course] = last slot assigned
    vector<vector<int>> slotCourses;    // slotCourses[slot] = courses in that slot
};

SlotAssignment assignSlots(
    const vector<int>& topoOrder,
    const vector<vector<int>>& prereqOf,
    const vector<vector<bool>>& clash,
    const vector<int>& sessionsNeeded,
    int hallsAvailable
) {
    int n = (int)topoOrder.size();

    SlotAssignment result;
    result.startSlot.assign(n, -1);  // -1 means "not yet assigned"
    result.endSlot.assign(n, -1);

    // Process each course in topological order.
    for (int idx = 0; idx < n; idx++) {
        int c = topoOrder[idx];

        // Rule 1: find the earliest possible slot.
        // It must come after ALL of this course's prerequisites are done.
        int earliest = 0;
        for (int p : prereqOf[c]) {
            // The prerequisite's exam ends at endSlot[p], so the earliest
            // we can start is the slot AFTER that.
            if (result.endSlot[p] + 1 > earliest) {
                earliest = result.endSlot[p] + 1;
            }
        }

        // How many consecutive slots does this course need?
        int need = sessionsNeeded[c];

        // Try slot positions starting from 'earliest', moving forward until
        // we find a valid block.
        int candidate = earliest;
        while (true) {
            // Make sure the slotCourses array is big enough.
            while ((int)result.slotCourses.size() < candidate + need) {
                result.slotCourses.push_back(vector<int>());
            }

            // Check if every slot in this block is valid.
            bool ok = true;
            for (int off = 0; off < need && ok; off++) {
                int slotIdx = candidate + off;

                // Rule 2: is this slot already full?
                if ((int)result.slotCourses[slotIdx].size() >= hallsAvailable) {
                    ok = false;
                    break;
                }

                // Rule 3: does this course clash with anything already in this slot?
                for (int other : result.slotCourses[slotIdx]) {
                    if (clash[c][other]) {
                        ok = false;
                        break;
                    }
                }
            }

            // If all slots in the block are valid, we're done!
            if (ok) break;

            // Otherwise, try the next position.
            candidate++;
        }

        // Assign this course to the block of slots.
        result.startSlot[c] = candidate;
        result.endSlot[c] = candidate + need - 1;
        for (int off = 0; off < need; off++) {
            result.slotCourses[candidate + off].push_back(c);
        }
    }

    return result;
}


// ============================================================================
// FUNCTION 6: The main orchestrator — runs the entire scheduling pipeline
// ============================================================================
// Takes one SchedulerInput, returns one SchedulerResult.
// This is the ONLY function the CLI and WASM bridge need to call.

SchedulerResult runScheduler(const SchedulerInput& input) {
    SchedulerResult result;
    int n = (int)input.courses.size();

    // --- Step 1: Topological sort ---
    auto [order, stuck] = topologicalSort(input.adj, input.inDegree, n);

    // If there's a cycle, we can't schedule. Return early with the bad news.
    if (!stuck.empty()) {
        result.success = false;
        for (int i : stuck) {
            result.cycleCoursesNames.push_back(input.courses[i].name);
        }
        return result;
    }

    result.success = true;
    result.topoOrder = order;

    // --- Step 2: Build the clash matrix ---
    auto clash = buildClashMatrix(input.courses, input.studentCourses);

    // --- Step 3: Count enrollments and figure out session needs ---
    result.enrolled = computeEnrollment(input.studentCourses, n);
    result.sessionsNeeded = computeSessionsNeeded(result.enrolled, input.hallCapacity);

    // --- Step 4: Assign exam slots ---
    auto assignment = assignSlots(
        order, input.prereqOf, clash,
        result.sessionsNeeded, input.hallsAvailable
    );
    result.startSlot = assignment.startSlot;
    result.endSlot = assignment.endSlot;
    result.slotCourses = assignment.slotCourses;

    // --- Step 5: Check if it fits ---
    int totalSlots = (int)result.slotCourses.size();
    result.totalDaysUsed = (totalSlots + input.sessionsPerDay - 1) / input.sessionsPerDay;
    result.fitsInAvailableDays = (result.totalDaysUsed <= input.maxDays);

    return result;
}

#endif // SCHEDULER_CORE_HPP
