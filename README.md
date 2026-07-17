# ScheduleX — University Course & Exam Scheduler

> **SIH · Smart Education Theme**
>
> Models courses as graph nodes with prerequisite dependencies as directed edges.
> Applies BFS-based topological ordering (Kahn's algorithm) to generate a
> clash-free exam/class timetable for students and faculty.

## Project Outcomes

1. **Resolves scheduling conflicts** using graph dependency analysis — no student or faculty member has two exams at the same time.
2. **Produces an optimized, clash-free timetable** for any sample dataset, with support for multi-hall batching.
3. **Strengthens understanding of graph traversal** in scheduling problems — the algorithm is the same in the terminal and web versions.

## Tech Stack

| Layer | Technology |
|---|---|
| Core Algorithm | C++ (arrays, vectors, graph adjacency lists, BFS) |
| Terminal Build | g++ (C++17) |
| Web Build | Emscripten (C++ → WebAssembly) or JavaScript fallback |
| Front-End | Plain HTML + CSS + JavaScript (no frameworks) |
| Hosting | GitHub Pages (static, free) |

## File Structure

```
scheduleX/
├── scheduler_core.hpp      ← Shared algorithm (pure functions, zero I/O)
├── course_scheduler.cpp    ← Terminal (CLI) entry point
├── wasm_bridge.cpp         ← Emscripten WebAssembly bridge
├── build_wasm.sh           ← Build script (Linux/Mac)
├── build_wasm.bat          ← Build script (Windows)
├── docs/                   ← Static site (served by GitHub Pages)
│   ├── index.html          ← Single-page app
│   ├── style.css           ← Dark theme design system
│   ├── app.js              ← Form logic + JS scheduler + rendering
│   ├── scheduler.js        ← (Generated) Emscripten glue
│   └── scheduler.wasm      ← (Generated) Compiled WebAssembly
└── README.md               ← This file
```

## Quick Start

### Run the Terminal Version

```bash
# Compile (requires g++ with C++17 support)
g++ -std=c++17 -o scheduler course_scheduler.cpp

# Run
./scheduler           # Linux/Mac
scheduler.exe         # Windows
```

The program will prompt you for courses, prerequisites, students, and exam logistics, then print the clash-free timetable.

### Run the Web Version Locally

```bash
# Navigate to the docs folder
cd docs

# Start a simple web server (Python 3)
python -m http.server 8080

# Open in your browser
# http://localhost:8080
```

No build step needed — the web version includes a JavaScript implementation of the same algorithm. Just open and use.

### (Optional) Compile the WASM Version

If you want the browser to run the actual C++ code via WebAssembly:

1. Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html):
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh    # Linux/Mac
   # or: emsdk_env.bat      # Windows
   ```

2. Build:
   ```bash
   # Linux/Mac
   ./build_wasm.sh

   # Windows
   build_wasm.bat
   ```

3. This produces `docs/scheduler.js` and `docs/scheduler.wasm`. Commit them and push.

## Deploy to GitHub Pages

1. Push this repo to a **public** GitHub repository.
2. Go to **Settings → Pages**.
3. Under "Source", select **Deploy from a branch**.
4. Choose branch `main` (or `master`), folder `/docs`.
5. Click **Save**.
6. Your site will be live at `https://<username>.github.io/<repo-name>/`.

**No server, no backend, no costs — static hosting, free forever.**

## How the Algorithm Works

1. **Graph Modeling**: Each course is a node. Prerequisite relationships are directed edges (A → B means "A must come before B").

2. **Clash Matrix**: Two courses "clash" (can't share a time slot) if they share a faculty member or if any student is enrolled in both.

3. **BFS Topological Sort (Kahn's Algorithm)**: Starting from courses with no prerequisites, we repeatedly pick the ready course with the most dependents and "remove" it from the graph, reducing in-degrees. This produces a valid ordering where every prerequisite comes first. If any courses remain, there's a cycle.

4. **Greedy Slot Assignment**: Following the topological order, each course gets the earliest available time slot that (a) comes after all its prerequisites, (b) isn't full (hall limit), and (c) doesn't clash with anything already scheduled in that slot.

5. **Batching**: If a course has more students than one hall can seat, it's split into consecutive sessions (batches).

## Credits

- **Priyanshu Kamal** — [GitHub](https://github.com/priyanshukamal26/) · [LinkedIn](https://www.linkedin.com/in/priyanshukamal/)
- **PARTNER_NAME** — [GitHub](PARTNER_GITHUB) · [LinkedIn](PARTNER_LINKEDIN)
