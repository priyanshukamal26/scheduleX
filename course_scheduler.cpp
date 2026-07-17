// ============================================================================
// course_scheduler.cpp — Terminal (CLI) Version
// ============================================================================
//
// University Course & Exam Scheduler
// (Graph + BFS-based ordering, clash-free)
//
// This file handles ALL the user interaction (reading input, printing output).
// The actual scheduling logic lives in scheduler_core.hpp — we just call
// runScheduler() and display what it returns.
//
// How to compile:
//   g++ -std=c++17 -o scheduler course_scheduler.cpp
//
// How to run:
//   ./scheduler        (Linux/Mac)
//   scheduler.exe      (Windows)
//
// ============================================================================

#include <iostream>
#include <iomanip>
#include <limits>
#include "scheduler_core.hpp"

using namespace std;


// ============================================================================
// HELPER: Print the timetable as a neat Day × Session grid
// ============================================================================
// Instead of just listing "course X → slot 5", this prints an actual
// table you can read like a calendar.

void printTimetableGrid(
    const SchedulerResult& result,
    const vector<Course>& courses,
    int sessionsPerDay
) {
    int totalSlots = (int)result.slotCourses.size();
    int totalDays = result.totalDaysUsed;

    // Column widths for pretty printing.
    int dayColWidth = 10;
    int sessionColWidth = 28;

    // Print the header row: "Day   Morning   Afternoon" (or Session 1, 2, 3...)
    cout << left << setw(dayColWidth) << "Day";
    for (int se = 0; se < sessionsPerDay; se++) {
        string colName;
        if (sessionsPerDay == 2) {
            colName = (se == 0) ? "Morning" : "Afternoon";
        } else {
            colName = "Session " + to_string(se + 1);
        }
        cout << left << setw(sessionColWidth) << colName;
    }
    cout << "\n";

    // Print a separator line.
    for (int x = 0; x < dayColWidth + sessionColWidth * sessionsPerDay; x++) {
        cout << "-";
    }
    cout << "\n";

    // Print each day's row.
    for (int d = 0; d < totalDays; d++) {
        cout << left << setw(dayColWidth) << ("Day " + to_string(d + 1));

        for (int se = 0; se < sessionsPerDay; se++) {
            int slot = d * sessionsPerDay + se;
            string cell = "-";  // dash means "nothing scheduled here"

            if (slot < totalSlots && !result.slotCourses[slot].empty()) {
                cell = "";
                for (int k = 0; k < (int)result.slotCourses[slot].size(); k++) {
                    int c = result.slotCourses[slot][k];
                    if (k > 0) cell += ", ";
                    cell += courses[c].name;

                    // If this course needed multiple sessions (batching),
                    // show which batch this is, e.g. "(B1/3)".
                    if (result.sessionsNeeded[c] > 1) {
                        int batchNum = slot - result.startSlot[c] + 1;
                        cell += " (B" + to_string(batchNum) + "/"
                              + to_string(result.sessionsNeeded[c]) + ")";
                    }
                }
            }

            cout << left << setw(sessionColWidth) << cell;
        }
        cout << "\n";
    }
}


// ============================================================================
// HELPER: Print faculty-wise schedule
// ============================================================================
// Groups exams by faculty member, so each professor can see their schedule.

void printFacultySchedule(
    const SchedulerResult& result,
    const vector<Course>& courses,
    int sessionsPerDay
) {
    int n = (int)courses.size();

    // First, collect all unique faculty names.
    vector<string> uniqueFaculty;
    for (int i = 0; i < n; i++) {
        bool found = false;
        for (int j = 0; j < (int)uniqueFaculty.size(); j++) {
            if (uniqueFaculty[j] == courses[i].faculty) {
                found = true;
                break;
            }
        }
        if (!found) uniqueFaculty.push_back(courses[i].faculty);
    }

    // For each faculty member, list their courses in chronological order.
    for (int f = 0; f < (int)uniqueFaculty.size(); f++) {
        cout << "\n" << uniqueFaculty[f] << ":\n";

        // Collect this faculty's courses and their slots.
        vector<int> slots, courseIndices;
        for (int i = 0; i < n; i++) {
            if (courses[i].faculty == uniqueFaculty[f]) {
                slots.push_back(result.startSlot[i]);
                courseIndices.push_back(i);
            }
        }

        // Sort by slot (simple bubble sort — fine for small lists).
        int cnt = (int)slots.size();
        for (int a = 0; a < cnt; a++) {
            for (int b = 0; b < cnt - a - 1; b++) {
                if (slots[b] > slots[b + 1]) {
                    int t = slots[b]; slots[b] = slots[b + 1]; slots[b + 1] = t;
                    t = courseIndices[b]; courseIndices[b] = courseIndices[b + 1]; courseIndices[b + 1] = t;
                }
            }
        }

        // Print each course with its time slot.
        for (int k = 0; k < cnt; k++) {
            int c = courseIndices[k];
            cout << "  " << slotLabel(result.startSlot[c], sessionsPerDay)
                 << " -> " << courses[c].name;
            if (result.sessionsNeeded[c] > 1) {
                cout << " (needs " << result.sessionsNeeded[c] << " sessions)";
            }
            cout << "\n";
        }
    }
}


// ============================================================================
// HELPER: Print student-wise schedule
// ============================================================================
// Each student sees when their exams are, in chronological order.

void printStudentSchedule(
    const SchedulerResult& result,
    const vector<string>& studentNames,
    const vector<vector<int>>& studentCourses,
    const vector<Course>& courses,
    int sessionsPerDay
) {
    int s = (int)studentNames.size();

    for (int i = 0; i < s; i++) {
        cout << "\n" << studentNames[i] << ":\n";

        // Collect this student's courses and their slots.
        vector<int> slots, courseIndices;
        for (int c : studentCourses[i]) {
            slots.push_back(result.startSlot[c]);
            courseIndices.push_back(c);
        }

        // Sort by slot (bubble sort).
        int cnt = (int)slots.size();
        for (int a = 0; a < cnt; a++) {
            for (int b = 0; b < cnt - a - 1; b++) {
                if (slots[b] > slots[b + 1]) {
                    int t = slots[b]; slots[b] = slots[b + 1]; slots[b + 1] = t;
                    t = courseIndices[b]; courseIndices[b] = courseIndices[b + 1]; courseIndices[b + 1] = t;
                }
            }
        }

        if (cnt == 0) {
            cout << "  not enrolled in anything\n";
        }
        for (int k = 0; k < cnt; k++) {
            int c = courseIndices[k];
            cout << "  " << slotLabel(result.startSlot[c], sessionsPerDay)
                 << " -> " << courses[c].name << "\n";
        }
    }
}


// ============================================================================
// MAIN — reads input, calls the scheduler, prints results
// ============================================================================

int main() {
    cout << "University Course & Exam Scheduler\n";
    cout << "(graph + BFS based, clash-free)\n\n";
    cout << "Note: names can have spaces, just press enter after each one.\n\n";

    // We'll build up this struct with everything the user enters.
    SchedulerInput input;

    // ---------- Step 1: Read courses ----------
    int n;
    cout << "How many courses: ";
    cin >> n;
    // Clear the leftover newline from cin so getline works correctly next.
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    if (n <= 0) {
        cout << "No courses, nothing to schedule. Exiting.\n";
        return 0;
    }

    input.courses.resize(n);
    for (int i = 0; i < n; i++) {
        cout << "\nCourse " << (i + 1) << " name: ";
        getline(cin, input.courses[i].name);
        cout << "Faculty in charge: ";
        getline(cin, input.courses[i].faculty);
    }

    // ---------- Step 2: Read prerequisites (build the graph) ----------
    // Initialize the graph structures.
    input.adj.assign(n, vector<int>());
    input.prereqOf.assign(n, vector<int>());
    input.inDegree.assign(n, 0);

    int m;
    cout << "\nHow many prerequisite relationships: ";
    cin >> m;

    if (m > 0) {
        cout << "Enter pairs as: <prerequisite course #> <course that needs it>\n";
        cout << "(course numbers are 1 to " << n << ")\n";
    }

    for (int i = 0; i < m; i++) {
        int p, d;
        cout << "Relationship " << (i + 1) << ": ";
        cin >> p >> d;
        p--; d--;  // convert from 1-based user input to 0-based internal indexing

        // Validate the input.
        if (p < 0 || p >= n || d < 0 || d >= n || p == d) {
            cout << "  bad input, skipping this one\n";
            continue;
        }

        input.adj[p].push_back(d);       // p must come before d
        input.prereqOf[d].push_back(p);   // d depends on p
        input.inDegree[d]++;              // d has one more prerequisite
    }

    // ---------- Step 3: Read students ----------
    int s;
    cout << "\nHow many students: ";
    cin >> s;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    input.studentNames.resize(max(s, 0));
    input.studentCourses.resize(max(s, 0));

    for (int i = 0; i < s; i++) {
        cout << "\nStudent " << (i + 1) << " name: ";
        getline(cin, input.studentNames[i]);

        int k;
        cout << "How many courses are they taking: ";
        cin >> k;

        if (k > 0) cout << "Enter " << k << " course numbers (1 to " << n << "): ";
        for (int j = 0; j < k; j++) {
            int c;
            cin >> c;
            c--;
            if (c >= 0 && c < n) {
                input.studentCourses[i].push_back(c);
            } else {
                cout << "  (ignoring invalid course number)\n";
            }
        }

        // Flush the input buffer before the next student's getline.
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    // ---------- Step 4: Read exam logistics ----------
    cout << "\nA few more things before we build the timetable:\n";

    cout << "Max students one exam hall can seat: ";
    cin >> input.hallCapacity;
    if (input.hallCapacity <= 0) input.hallCapacity = 1000000;  // basically unlimited

    cout << "How many halls can run exams at the same time: ";
    cin >> input.hallsAvailable;
    if (input.hallsAvailable <= 0) input.hallsAvailable = 1;

    cout << "Exam sessions per day (2 = morning/afternoon): ";
    cin >> input.sessionsPerDay;
    if (input.sessionsPerDay <= 0) input.sessionsPerDay = 1;

    cout << "How many days do you actually have for exams: ";
    cin >> input.maxDays;

    // ---------- Step 5: Run the scheduler! ----------
    SchedulerResult result = runScheduler(input);

    // ---------- Step 6: Display results ----------

    // Check for cycles first.
    if (!result.success) {
        cout << "\nCan't build a schedule - there's a cycle in the prerequisites.\n";
        cout << "These courses are stuck depending on each other (directly or indirectly):\n";
        for (const string& name : result.cycleCoursesNames) {
            cout << "  - " << name << "\n";
        }
        return 0;
    }

    // Print the topological order.
    cout << "\nTopological order (BFS/Kahn's):\n";
    for (int i = 0; i < n; i++) {
        cout << "  " << (i + 1) << ". " << input.courses[result.topoOrder[i]].name << "\n";
    }

    // Check if the schedule fits in the available days.
    cout << "\n";
    if (!result.fitsInAvailableDays) {
        cout << "Heads up: this schedule needs " << result.totalDaysUsed
             << " day(s), but you only said " << input.maxDays
             << " are available. Try more halls, more sessions/day, "
             << "or check for avoidable clashes.\n";
    } else {
        cout << "Good news - this fits in your available " << input.maxDays
             << " day(s) (actually uses " << result.totalDaysUsed << ").\n";
    }

    // Print the timetable grid.
    cout << "\nExam Timetable:\n\n";
    printTimetableGrid(result, input.courses, input.sessionsPerDay);

    // Print faculty-wise schedule.
    cout << "\nFaculty-wise schedule:\n";
    printFacultySchedule(result, input.courses, input.sessionsPerDay);

    // Print student-wise schedule.
    cout << "\nStudent-wise schedule:\n";
    printStudentSchedule(result, input.studentNames, input.studentCourses,
                         input.courses, input.sessionsPerDay);

    cout << "\nDone. No student or faculty member has two exams in the same session,\n";
    cout << "and every course comes strictly after its prerequisites.\n";

    return 0;
}
