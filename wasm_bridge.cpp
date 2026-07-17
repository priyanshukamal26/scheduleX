// ============================================================================
// wasm_bridge.cpp — Emscripten WebAssembly Bridge
// ============================================================================
//
// This file exposes the scheduling functions from scheduler_core.hpp to
// JavaScript running in the browser. It uses Emscripten's "embind" library,
// which automatically converts between C++ types and JavaScript types.
//
// How it works:
//   1. JavaScript calls Module.runSchedulerFromJS(coursesJSON, ...)
//   2. This function receives the data, packs it into a SchedulerInput struct
//   3. Calls runScheduler() — the exact same C++ code as the CLI version
//   4. Packs the result into a JSON-like string and returns it to JavaScript
//
// How to compile (requires Emscripten SDK installed):
//   em++ -std=c++17 -O2 --bind wasm_bridge.cpp -o docs/scheduler.js \
//        -s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME="createSchedulerModule" \
//        -s ALLOW_MEMORY_GROWTH=1
//
// This produces two files:
//   docs/scheduler.js    — JavaScript glue code that loads the WASM
//   docs/scheduler.wasm  — the compiled WebAssembly binary
//
// ============================================================================

#include <emscripten/bind.h>
#include <sstream>
#include "scheduler_core.hpp"

using namespace emscripten;


// ============================================================================
// Helper: Simple JSON builder
// ============================================================================
// We build a JSON string manually to send back to JavaScript.
// This avoids needing any JSON library — just string concatenation.
// (For a small project like this, it's perfectly fine.)

// Escapes special characters in a string so it's safe inside JSON.
string jsonEscape(const string& s) {
    string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}


// ============================================================================
// The main function JavaScript will call
// ============================================================================
// Takes all the input as simple types (strings and ints) and a few
// "flattened" arrays, runs the scheduler, and returns the result as
// a JSON string that JavaScript can parse with JSON.parse().
//
// Why not use embind's automatic struct binding? Because embind's
// vector<vector<int>> support needs careful registration and is fragile.
// A JSON string is simpler, more debuggable, and works perfectly here.

string runSchedulerFromJS(
    // Course data: parallel arrays of names and faculty
    val courseNames,       // JavaScript array of strings
    val courseFaculty,     // JavaScript array of strings
    // Prerequisites: parallel arrays of [prerequisite, dependent] pairs
    val prereqFrom,        // JavaScript array of ints (0-based)
    val prereqTo,          // JavaScript array of ints (0-based)
    // Student data
    val studentNamesList,  // JavaScript array of strings
    val studentCourseData, // JavaScript array of arrays of ints (0-based)
    // Logistics
    int hallCapacity,
    int hallsAvailable,
    int sessionsPerDay,
    int maxDays
) {
    // --- Unpack JavaScript arrays into C++ vectors ---

    int n = courseNames["length"].as<int>();

    SchedulerInput input;
    input.courses.resize(n);
    for (int i = 0; i < n; i++) {
        input.courses[i].name = courseNames[i].as<string>();
        input.courses[i].faculty = courseFaculty[i].as<string>();
    }

    // Build the prerequisite graph.
    input.adj.assign(n, vector<int>());
    input.prereqOf.assign(n, vector<int>());
    input.inDegree.assign(n, 0);

    int numPrereqs = prereqFrom["length"].as<int>();
    for (int i = 0; i < numPrereqs; i++) {
        int p = prereqFrom[i].as<int>();
        int d = prereqTo[i].as<int>();
        if (p >= 0 && p < n && d >= 0 && d < n && p != d) {
            input.adj[p].push_back(d);
            input.prereqOf[d].push_back(p);
            input.inDegree[d]++;
        }
    }

    // Unpack students.
    int numStudents = studentNamesList["length"].as<int>();
    input.studentNames.resize(numStudents);
    input.studentCourses.resize(numStudents);
    for (int i = 0; i < numStudents; i++) {
        input.studentNames[i] = studentNamesList[i].as<string>();
        val courses = studentCourseData[i];
        int numCourses = courses["length"].as<int>();
        for (int j = 0; j < numCourses; j++) {
            input.studentCourses[i].push_back(courses[j].as<int>());
        }
    }

    input.hallCapacity = hallCapacity > 0 ? hallCapacity : 1000000;
    input.hallsAvailable = hallsAvailable > 0 ? hallsAvailable : 1;
    input.sessionsPerDay = sessionsPerDay > 0 ? sessionsPerDay : 1;
    input.maxDays = maxDays;

    // --- Run the scheduler (same code as the CLI!) ---
    SchedulerResult result = runScheduler(input);

    // --- Build a JSON string with the results ---
    ostringstream json;
    json << "{";

    json << "\"success\":" << (result.success ? "true" : "false");

    if (!result.success) {
        // Cycle detected — list the stuck courses.
        json << ",\"cycleCourses\":[";
        for (int i = 0; i < (int)result.cycleCoursesNames.size(); i++) {
            if (i > 0) json << ",";
            json << "\"" << jsonEscape(result.cycleCoursesNames[i]) << "\"";
        }
        json << "]";
    } else {
        // Topological order.
        json << ",\"topoOrder\":[";
        for (int i = 0; i < (int)result.topoOrder.size(); i++) {
            if (i > 0) json << ",";
            json << result.topoOrder[i];
        }
        json << "]";

        // Start and end slots for each course.
        json << ",\"startSlot\":[";
        for (int i = 0; i < (int)result.startSlot.size(); i++) {
            if (i > 0) json << ",";
            json << result.startSlot[i];
        }
        json << "]";

        json << ",\"endSlot\":[";
        for (int i = 0; i < (int)result.endSlot.size(); i++) {
            if (i > 0) json << ",";
            json << result.endSlot[i];
        }
        json << "]";

        // Slot contents.
        json << ",\"slotCourses\":[";
        for (int i = 0; i < (int)result.slotCourses.size(); i++) {
            if (i > 0) json << ",";
            json << "[";
            for (int j = 0; j < (int)result.slotCourses[i].size(); j++) {
                if (j > 0) json << ",";
                json << result.slotCourses[i][j];
            }
            json << "]";
        }
        json << "]";

        // Enrollment and session counts.
        json << ",\"enrolled\":[";
        for (int i = 0; i < (int)result.enrolled.size(); i++) {
            if (i > 0) json << ",";
            json << result.enrolled[i];
        }
        json << "]";

        json << ",\"sessionsNeeded\":[";
        for (int i = 0; i < (int)result.sessionsNeeded.size(); i++) {
            if (i > 0) json << ",";
            json << result.sessionsNeeded[i];
        }
        json << "]";

        json << ",\"totalDaysUsed\":" << result.totalDaysUsed;
        json << ",\"fitsInAvailableDays\":" << (result.fitsInAvailableDays ? "true" : "false");
    }

    json << "}";
    return json.str();
}


// ============================================================================
// Register the function with Emscripten's embind
// ============================================================================
// This tells Emscripten: "make runSchedulerFromJS callable from JavaScript".

EMSCRIPTEN_BINDINGS(scheduler_module) {
    function("runSchedulerFromJS", &runSchedulerFromJS);
}
