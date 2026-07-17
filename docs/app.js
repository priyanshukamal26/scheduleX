// ============================================================================
// app.js — ScheduleX Front-End Logic
// ============================================================================
//
// This file does THREE things:
//   1. FORM MANAGEMENT — dynamically add/remove courses, prerequisites,
//      students with their course selections
//   2. SCHEDULING ENGINE — a JavaScript implementation of the exact same
//      algorithm as scheduler_core.hpp (BFS topological sort + greedy
//      slot assignment). This runs in the browser directly. If the WASM
//      build is available, it can use that instead — but the JS version
//      is a perfect fallback that always works.
//   3. RESULT RENDERING — takes the scheduler output and builds the
//      timetable grid, faculty/student schedules, and alert banners.
//
// Everything is heavily commented for beginners.
//
// ============================================================================


// ============================================================================
// PART 1: DOM REFERENCES
// ============================================================================
// Grab references to all the important HTML elements we'll interact with.
// This avoids calling document.getElementById() repeatedly later.

const courseRowsContainer = document.getElementById('course-rows');
const prereqRowsContainer = document.getElementById('prereq-rows');
const studentRowsContainer = document.getElementById('student-rows');

const addCourseBtn = document.getElementById('add-course-btn');
const addPrereqBtn = document.getElementById('add-prereq-btn');
const addStudentBtn = document.getElementById('add-student-btn');
const generateBtn = document.getElementById('generate-btn');
const sampleDataBtn = document.getElementById('sample-data-btn');
const clearBtn = document.getElementById('clear-btn');

const resultsPanel = document.getElementById('results-panel');
const resultAlert = document.getElementById('result-alert');
const topoOrderList = document.getElementById('topo-order-list');
const timetableGrid = document.getElementById('timetable-grid');
const facultySchedules = document.getElementById('faculty-schedules');
const studentSchedules = document.getElementById('student-schedules');


// ============================================================================
// PART 2: FORM MANAGEMENT
// ============================================================================
// Functions to dynamically add and remove rows for courses, prerequisites,
// and students.  Each row is a <div class="input-row"> with inputs inside.

// Keep a running counter for unique IDs on each type of row.
// This ensures every input has a unique HTML id attribute.
let courseCounter = 0;
let prereqCounter = 0;
let studentCounter = 0;


// --- COURSES ---
// Each course row has: name input + faculty input + remove button.

function addCourseRow(name = '', faculty = '') {
    // Create a unique ID for this row.
    const id = courseCounter++;
    
    // Build the HTML for one course row.
    const row = document.createElement('div');
    row.className = 'input-row';
    row.dataset.courseId = id;
    row.innerHTML = `
        <input type="text" id="course-name-${id}" placeholder="Course name (e.g. Data Structures)" value="${escapeHtml(name)}">
        <input type="text" id="course-faculty-${id}" placeholder="Faculty (e.g. Dr. Sharma)" value="${escapeHtml(faculty)}">
        <button type="button" class="btn-remove" onclick="removeCourseRow(this)" title="Remove this course">&times;</button>
    `;
    courseRowsContainer.appendChild(row);
    
    // Whenever courses change, update the prerequisite dropdowns and
    // student checkboxes so they reflect the current course list.
    row.querySelectorAll('input').forEach(input => {
        input.addEventListener('input', () => {
            updatePrereqDropdowns();
            updateStudentCheckboxes();
        });
    });
    
    updatePrereqDropdowns();
    updateStudentCheckboxes();
}

function removeCourseRow(button) {
    // Find the row that contains this button and remove it.
    const row = button.closest('.input-row');
    row.remove();
    
    // Update dependent UI elements.
    updatePrereqDropdowns();
    updateStudentCheckboxes();
}


// --- PREREQUISITES ---
// Each prerequisite row has: "Course A" dropdown → "is prerequisite for" → "Course B" dropdown + remove button.

function addPrereqRow(fromIdx = -1, toIdx = -1) {
    const id = prereqCounter++;
    
    // Get the current list of courses for the dropdown options.
    const courses = getCurrentCourses();
    const options = courses.map((c, i) => 
        `<option value="${i}">${escapeHtml(c.name || `Course ${i + 1}`)}</option>`
    ).join('');
    
    const row = document.createElement('div');
    row.className = 'input-row';
    row.dataset.prereqId = id;
    row.innerHTML = `
        <select id="prereq-from-${id}">${options}</select>
        <span style="color: var(--text-muted); font-size: 0.85rem; white-space: nowrap;">&#8594; is prerequisite for &#8594;</span>
        <select id="prereq-to-${id}">${options}</select>
        <button type="button" class="btn-remove" onclick="removePrereqRow(this)" title="Remove">&times;</button>
    `;
    prereqRowsContainer.appendChild(row);
    
    // If specific indices were provided (e.g., from sample data), select them.
    if (fromIdx >= 0) row.querySelector(`#prereq-from-${id}`).value = fromIdx;
    if (toIdx >= 0) row.querySelector(`#prereq-to-${id}`).value = toIdx;
}

function removePrereqRow(button) {
    button.closest('.input-row').remove();
}


// --- STUDENTS ---
// Each student row has: name input + a list of checkboxes (one per course) + remove button.

function addStudentRow(name = '', selectedCourses = []) {
    const id = studentCounter++;
    
    const courses = getCurrentCourses();
    
    // Build checkbox items for each course.
    const checkboxes = courses.map((c, i) => {
        const checked = selectedCourses.includes(i) ? 'checked' : '';
        const label = c.name || `Course ${i + 1}`;
        return `<label><input type="checkbox" value="${i}" ${checked}> ${escapeHtml(label)}</label>`;
    }).join('');
    
    const row = document.createElement('div');
    row.className = 'input-row';
    row.dataset.studentId = id;
    row.style.flexDirection = 'column';
    row.style.alignItems = 'stretch';
    row.innerHTML = `
        <div style="display: flex; gap: var(--space-sm); align-items: center;">
            <input type="text" id="student-name-${id}" placeholder="Student name" value="${escapeHtml(name)}" style="flex: 1;">
            <button type="button" class="btn-remove" onclick="removeStudentRow(this)" title="Remove">&times;</button>
        </div>
        <div class="student-courses-list" id="student-courses-${id}">
            ${checkboxes || '<span style="color: var(--text-muted); font-size: 0.85rem;">Add courses first</span>'}
        </div>
    `;
    studentRowsContainer.appendChild(row);
}

function removeStudentRow(button) {
    button.closest('.input-row').remove();
}


// --- UPDATING DEPENDENT UI ---

// Get the current list of courses from the form.
// Returns an array of {name, faculty} objects.
function getCurrentCourses() {
    const rows = courseRowsContainer.querySelectorAll('.input-row');
    const courses = [];
    rows.forEach(row => {
        const nameInput = row.querySelector('input[id^="course-name-"]');
        const facultyInput = row.querySelector('input[id^="course-faculty-"]');
        courses.push({
            name: nameInput ? nameInput.value.trim() : '',
            faculty: facultyInput ? facultyInput.value.trim() : ''
        });
    });
    return courses;
}

// Rebuild all prerequisite dropdown <option> lists to match current courses.
function updatePrereqDropdowns() {
    const courses = getCurrentCourses();
    const options = courses.map((c, i) =>
        `<option value="${i}">${escapeHtml(c.name || `Course ${i + 1}`)}</option>`
    ).join('');
    
    // For each prerequisite row, save the current selection, rebuild options,
    // then restore the selection if it's still valid.
    const rows = prereqRowsContainer.querySelectorAll('.input-row');
    rows.forEach(row => {
        const fromSelect = row.querySelector('select[id^="prereq-from-"]');
        const toSelect = row.querySelector('select[id^="prereq-to-"]');
        
        const fromVal = fromSelect.value;
        const toVal = toSelect.value;
        
        fromSelect.innerHTML = options;
        toSelect.innerHTML = options;
        
        // Restore previous selection if the index still exists.
        if (fromVal < courses.length) fromSelect.value = fromVal;
        if (toVal < courses.length) toSelect.value = toVal;
    });
}

// Rebuild all student course checkboxes to match current courses.
function updateStudentCheckboxes() {
    const courses = getCurrentCourses();
    
    const rows = studentRowsContainer.querySelectorAll('.input-row');
    rows.forEach(row => {
        const checkboxList = row.querySelector('.student-courses-list');
        if (!checkboxList) return;
        
        // Save which courses were previously checked.
        const previouslyChecked = new Set();
        checkboxList.querySelectorAll('input[type="checkbox"]:checked').forEach(cb => {
            previouslyChecked.add(parseInt(cb.value));
        });
        
        // Rebuild the checkboxes.
        if (courses.length === 0) {
            checkboxList.innerHTML = '<span style="color: var(--text-muted); font-size: 0.85rem;">Add courses first</span>';
            return;
        }
        
        checkboxList.innerHTML = courses.map((c, i) => {
            const checked = previouslyChecked.has(i) ? 'checked' : '';
            const label = c.name || `Course ${i + 1}`;
            return `<label><input type="checkbox" value="${i}" ${checked}> ${escapeHtml(label)}</label>`;
        }).join('');
    });
}


// HTML escape helper — prevents XSS from user-typed names.
function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}


// ============================================================================
// PART 3: SCHEDULING ENGINE (JavaScript)
// ============================================================================
// This is a direct port of the C++ algorithm from scheduler_core.hpp.
// Same logic, same variable names, same comments — just JavaScript syntax.
// If you've read the C++ version, this should look very familiar.


// Build the clash matrix: clash[a][b] = true if courses a and b can't share a slot.
function buildClashMatrix(courses, studentCourses) {
    const n = courses.length;
    
    // Initialize an n×n matrix filled with false.
    const clash = Array.from({length: n}, () => Array(n).fill(false));
    
    // Reason 1: same faculty teaches both courses.
    for (let a = 0; a < n; a++) {
        for (let b = a + 1; b < n; b++) {
            if (courses[a].faculty && courses[b].faculty &&
                courses[a].faculty === courses[b].faculty) {
                clash[a][b] = true;
                clash[b][a] = true;
            }
        }
    }
    
    // Reason 2: at least one student is enrolled in both.
    for (let i = 0; i < studentCourses.length; i++) {
        for (let a = 0; a < studentCourses[i].length; a++) {
            for (let b = a + 1; b < studentCourses[i].length; b++) {
                const c1 = studentCourses[i][a];
                const c2 = studentCourses[i][b];
                clash[c1][c2] = true;
                clash[c2][c1] = true;
            }
        }
    }
    
    return clash;
}


// Count how many students are enrolled in each course.
function computeEnrollment(studentCourses, n) {
    const enrolled = Array(n).fill(0);
    for (let i = 0; i < studentCourses.length; i++) {
        for (const c of studentCourses[i]) {
            enrolled[c]++;
        }
    }
    return enrolled;
}


// Figure out how many exam sessions each course needs (batching for big courses).
function computeSessionsNeeded(enrolled, hallCapacity) {
    return enrolled.map(e => 
        e > hallCapacity ? Math.ceil(e / hallCapacity) : 1
    );
}


// Topological sort using BFS (Kahn's algorithm) with "most-dependents-first" priority.
// Returns { order: [...], stuck: [...] }
function topologicalSort(adj, inDegree, n) {
    // Working copy of in-degrees.
    const remaining = [...inDegree];
    
    // Find all courses with no prerequisites.
    const ready = [];
    for (let i = 0; i < n; i++) {
        if (remaining[i] === 0) ready.push(i);
    }
    
    const order = [];
    
    while (ready.length > 0) {
        // Pick the ready course with the most dependents.
        let bestPos = 0;
        for (let i = 1; i < ready.length; i++) {
            if (adj[ready[i]].length > adj[ready[bestPos]].length) {
                bestPos = i;
            }
        }
        
        // Remove it from the ready list (swap with last, pop).
        const u = ready[bestPos];
        ready[bestPos] = ready[ready.length - 1];
        ready.pop();
        
        order.push(u);
        
        // Reduce in-degree for all courses that depended on u.
        for (const v of adj[u]) {
            remaining[v]--;
            if (remaining[v] === 0) ready.push(v);
        }
    }
    
    // Check for cycles — any courses not processed are stuck.
    const stuck = [];
    if (order.length !== n) {
        const done = new Set(order);
        for (let i = 0; i < n; i++) {
            if (!done.has(i)) stuck.push(i);
        }
    }
    
    return { order, stuck };
}


// Assign exam slots greedily.
function assignSlots(topoOrder, prereqOf, clash, sessionsNeeded, hallsAvailable) {
    const n = topoOrder.length;
    const startSlot = Array(n).fill(-1);
    const endSlot = Array(n).fill(-1);
    const slotCourses = [];  // slotCourses[slot] = array of course indices
    
    for (let idx = 0; idx < n; idx++) {
        const c = topoOrder[idx];
        
        // Find the earliest possible slot (after all prerequisites).
        let earliest = 0;
        for (const p of prereqOf[c]) {
            if (endSlot[p] + 1 > earliest) {
                earliest = endSlot[p] + 1;
            }
        }
        
        const need = sessionsNeeded[c];
        let candidate = earliest;
        
        while (true) {
            // Ensure slotCourses is big enough.
            while (slotCourses.length < candidate + need) {
                slotCourses.push([]);
            }
            
            // Check if this block of slots is valid.
            let ok = true;
            for (let off = 0; off < need && ok; off++) {
                const slotIdx = candidate + off;
                
                // Is this slot already full?
                if (slotCourses[slotIdx].length >= hallsAvailable) {
                    ok = false;
                    break;
                }
                
                // Does this course clash with anything in this slot?
                for (const other of slotCourses[slotIdx]) {
                    if (clash[c][other]) {
                        ok = false;
                        break;
                    }
                }
            }
            
            if (ok) break;
            candidate++;
        }
        
        // Assign the course to this block of slots.
        startSlot[c] = candidate;
        endSlot[c] = candidate + need - 1;
        for (let off = 0; off < need; off++) {
            slotCourses[candidate + off].push(c);
        }
    }
    
    return { startSlot, endSlot, slotCourses };
}


// Main scheduler function — mirrors runScheduler() in scheduler_core.hpp.
function runScheduler(input) {
    const n = input.courses.length;
    
    // Step 1: Topological sort.
    const { order, stuck } = topologicalSort(input.adj, input.inDegree, n);
    
    if (stuck.length > 0) {
        return {
            success: false,
            cycleCourses: stuck.map(i => input.courses[i].name || `Course ${i + 1}`)
        };
    }
    
    // Step 2: Build the clash matrix.
    const clash = buildClashMatrix(input.courses, input.studentCourses);
    
    // Step 3: Count enrollments and compute session needs.
    const enrolled = computeEnrollment(input.studentCourses, n);
    const sessionsNeeded = computeSessionsNeeded(enrolled, input.hallCapacity);
    
    // Step 4: Assign exam slots.
    const assignment = assignSlots(
        order, input.prereqOf, clash, sessionsNeeded, input.hallsAvailable
    );
    
    // Step 5: Check if it fits in the available days.
    const totalSlots = assignment.slotCourses.length;
    const totalDaysUsed = Math.ceil(totalSlots / input.sessionsPerDay);
    const fitsInAvailableDays = totalDaysUsed <= input.maxDays;
    
    return {
        success: true,
        topoOrder: order,
        startSlot: assignment.startSlot,
        endSlot: assignment.endSlot,
        slotCourses: assignment.slotCourses,
        enrolled,
        sessionsNeeded,
        totalDaysUsed,
        fitsInAvailableDays
    };
}


// ============================================================================
// PART 4: COLLECTING FORM DATA
// ============================================================================
// Reads all the form inputs and packs them into a SchedulerInput-like object.

function collectInput() {
    // Courses
    const courses = getCurrentCourses();
    const n = courses.length;
    
    if (n === 0) {
        return { error: 'Please add at least one course.' };
    }
    
    // Check for empty course names.
    for (let i = 0; i < n; i++) {
        if (!courses[i].name) {
            return { error: `Course ${i + 1} has no name. Please fill in all course names.` };
        }
    }
    
    // Prerequisites — build the adjacency list and in-degree array.
    const adj = Array.from({length: n}, () => []);
    const prereqOf = Array.from({length: n}, () => []);
    const inDegree = Array(n).fill(0);
    
    const prereqRows = prereqRowsContainer.querySelectorAll('.input-row');
    prereqRows.forEach(row => {
        const fromSelect = row.querySelector('select[id^="prereq-from-"]');
        const toSelect = row.querySelector('select[id^="prereq-to-"]');
        const p = parseInt(fromSelect.value);
        const d = parseInt(toSelect.value);
        
        if (p >= 0 && p < n && d >= 0 && d < n && p !== d) {
            adj[p].push(d);
            prereqOf[d].push(p);
            inDegree[d]++;
        }
    });
    
    // Students
    const studentNames = [];
    const studentCourses = [];
    
    const studentRows = studentRowsContainer.querySelectorAll('.input-row');
    studentRows.forEach(row => {
        const nameInput = row.querySelector('input[id^="student-name-"]');
        const name = nameInput ? nameInput.value.trim() : '';
        studentNames.push(name || `Student ${studentNames.length + 1}`);
        
        const checked = [];
        row.querySelectorAll('.student-courses-list input[type="checkbox"]:checked').forEach(cb => {
            checked.push(parseInt(cb.value));
        });
        studentCourses.push(checked);
    });
    
    // Logistics
    const hallCapacity = parseInt(document.getElementById('hall-capacity').value) || 100;
    const hallsAvailable = parseInt(document.getElementById('halls-available').value) || 3;
    const sessionsPerDay = parseInt(document.getElementById('sessions-per-day').value) || 2;
    const maxDays = parseInt(document.getElementById('max-days').value) || 7;
    
    return {
        courses,
        adj,
        prereqOf,
        inDegree,
        studentNames,
        studentCourses,
        hallCapacity: hallCapacity > 0 ? hallCapacity : 1000000,
        hallsAvailable: hallsAvailable > 0 ? hallsAvailable : 1,
        sessionsPerDay: sessionsPerDay > 0 ? sessionsPerDay : 1,
        maxDays
    };
}


// ============================================================================
// PART 5: RESULT RENDERING
// ============================================================================
// Takes the scheduler output and builds the visual result panels.

// Helper: convert a slot number to a readable label.
function getSlotLabel(slot, sessionsPerDay) {
    const day = Math.floor(slot / sessionsPerDay) + 1;
    const session = (slot % sessionsPerDay) + 1;
    if (sessionsPerDay === 2) {
        return `Day ${day} ${session === 1 ? 'Morning' : 'Afternoon'}`;
    }
    return `Day ${day} Session ${session}`;
}


function renderResults(result, input) {
    // Make the results panel visible.
    resultsPanel.classList.add('visible');
    
    // Scroll to the results.
    resultsPanel.scrollIntoView({ behavior: 'smooth', block: 'start' });
    
    // --- Alert banner ---
    if (!result.success) {
        // Cycle detected!
        resultAlert.innerHTML = `
            <div class="alert-banner error">
                &#9888; Can't build a schedule — there's a cycle in the prerequisites. 
                These courses are stuck depending on each other: 
                <strong>${result.cycleCourses.map(escapeHtml).join(', ')}</strong>
            </div>
        `;
        // Hide the other result sections since we can't show a schedule.
        document.getElementById('result-topo').style.display = 'none';
        document.getElementById('result-timetable').style.display = 'none';
        document.getElementById('result-faculty').style.display = 'none';
        document.getElementById('result-students').style.display = 'none';
        return;
    }
    
    // Show all result sections.
    document.getElementById('result-topo').style.display = '';
    document.getElementById('result-timetable').style.display = '';
    document.getElementById('result-faculty').style.display = '';
    document.getElementById('result-students').style.display = '';
    
    // Status banner: fits or doesn't fit.
    if (result.fitsInAvailableDays) {
        resultAlert.innerHTML = `
            <div class="alert-banner success">
                &#9989; Schedule generated successfully! Fits in your ${input.maxDays} available day(s) 
                (actually uses ${result.totalDaysUsed}).
            </div>
        `;
    } else {
        resultAlert.innerHTML = `
            <div class="alert-banner warning">
                &#9888; This schedule needs ${result.totalDaysUsed} day(s), but you only have 
                ${input.maxDays}. Try adding more halls, more sessions per day, or reducing clashes.
            </div>
        `;
    }
    
    // --- Topological order ---
    topoOrderList.innerHTML = '';
    result.topoOrder.forEach((courseIdx, rank) => {
        const li = document.createElement('li');
        li.innerHTML = `<span class="topo-num">${rank + 1}.</span> ${escapeHtml(input.courses[courseIdx].name)}`;
        topoOrderList.appendChild(li);
    });
    
    // --- Timetable grid ---
    renderTimetableGrid(result, input);
    
    // --- Faculty schedule ---
    renderFacultySchedule(result, input);
    
    // --- Student schedule ---
    renderStudentSchedule(result, input);
}


function renderTimetableGrid(result, input) {
    const sessionsPerDay = input.sessionsPerDay;
    const totalDays = result.totalDaysUsed;
    const totalSlots = result.slotCourses.length;
    
    // Set the CSS grid columns: [day label] + [one column per session]
    timetableGrid.style.gridTemplateColumns = `auto ${'1fr '.repeat(sessionsPerDay)}`;
    timetableGrid.innerHTML = '';
    
    // Header row: empty corner + session labels
    const corner = document.createElement('div');
    corner.className = 'timetable-header-cell';
    corner.textContent = 'Day';
    timetableGrid.appendChild(corner);
    
    for (let se = 0; se < sessionsPerDay; se++) {
        const header = document.createElement('div');
        header.className = 'timetable-header-cell';
        if (sessionsPerDay === 2) {
            header.textContent = se === 0 ? 'Morning' : 'Afternoon';
        } else {
            header.textContent = `Session ${se + 1}`;
        }
        timetableGrid.appendChild(header);
    }
    
    // Data rows: one per day.
    for (let d = 0; d < totalDays; d++) {
        // Day label.
        const dayLabel = document.createElement('div');
        dayLabel.className = 'timetable-day-label';
        dayLabel.textContent = `Day ${d + 1}`;
        timetableGrid.appendChild(dayLabel);
        
        // One cell per session.
        for (let se = 0; se < sessionsPerDay; se++) {
            const slot = d * sessionsPerDay + se;
            const cell = document.createElement('div');
            cell.className = 'timetable-cell';
            
            if (slot < totalSlots && result.slotCourses[slot].length > 0) {
                // Add a chip for each course in this slot.
                for (const c of result.slotCourses[slot]) {
                    const chip = document.createElement('span');
                    let label = input.courses[c].name;
                    
                    // If this course is batched, show which batch.
                    if (result.sessionsNeeded[c] > 1) {
                        const batchNum = slot - result.startSlot[c] + 1;
                        label += ` (B${batchNum}/${result.sessionsNeeded[c]})`;
                        chip.className = 'course-chip batch';
                    } else {
                        chip.className = 'course-chip';
                    }
                    
                    chip.textContent = label;
                    cell.appendChild(chip);
                }
            } else {
                cell.classList.add('empty');
                cell.textContent = '—';
            }
            
            timetableGrid.appendChild(cell);
        }
    }
}


function renderFacultySchedule(result, input) {
    facultySchedules.innerHTML = '';
    
    const n = input.courses.length;
    
    // Collect unique faculty names.
    const uniqueFaculty = [];
    for (let i = 0; i < n; i++) {
        if (input.courses[i].faculty && !uniqueFaculty.includes(input.courses[i].faculty)) {
            uniqueFaculty.push(input.courses[i].faculty);
        }
    }
    
    for (const fac of uniqueFaculty) {
        // Get this faculty's courses, sorted by slot.
        const items = [];
        for (let i = 0; i < n; i++) {
            if (input.courses[i].faculty === fac) {
                items.push({ courseIdx: i, slot: result.startSlot[i] });
            }
        }
        items.sort((a, b) => a.slot - b.slot);
        
        // Build a collapsible group.
        const group = document.createElement('div');
        group.className = 'schedule-group';
        
        const header = document.createElement('div');
        header.className = 'schedule-group-header';
        header.innerHTML = `<span class="chevron">&#9654;</span> ${escapeHtml(fac)}`;
        header.addEventListener('click', () => {
            header.classList.toggle('open');
            itemsDiv.classList.toggle('open');
        });
        
        const itemsDiv = document.createElement('div');
        itemsDiv.className = 'schedule-group-items';
        
        for (const item of items) {
            const div = document.createElement('div');
            div.className = 'schedule-item';
            let label = getSlotLabel(item.slot, input.sessionsPerDay);
            let extra = '';
            if (result.sessionsNeeded[item.courseIdx] > 1) {
                extra = ` (needs ${result.sessionsNeeded[item.courseIdx]} sessions)`;
            }
            div.innerHTML = `<span class="slot-label">${label}</span> &#8594; ${escapeHtml(input.courses[item.courseIdx].name)}${extra}`;
            itemsDiv.appendChild(div);
        }
        
        group.appendChild(header);
        group.appendChild(itemsDiv);
        facultySchedules.appendChild(group);
    }
}


function renderStudentSchedule(result, input) {
    studentSchedules.innerHTML = '';
    
    for (let i = 0; i < input.studentNames.length; i++) {
        const items = input.studentCourses[i].map(c => ({
            courseIdx: c,
            slot: result.startSlot[c]
        }));
        items.sort((a, b) => a.slot - b.slot);
        
        const group = document.createElement('div');
        group.className = 'schedule-group';
        
        const header = document.createElement('div');
        header.className = 'schedule-group-header';
        header.innerHTML = `<span class="chevron">&#9654;</span> ${escapeHtml(input.studentNames[i])}`;
        header.addEventListener('click', () => {
            header.classList.toggle('open');
            itemsDiv.classList.toggle('open');
        });
        
        const itemsDiv = document.createElement('div');
        itemsDiv.className = 'schedule-group-items';
        
        if (items.length === 0) {
            const div = document.createElement('div');
            div.className = 'schedule-item';
            div.textContent = 'Not enrolled in any courses';
            itemsDiv.appendChild(div);
        } else {
            for (const item of items) {
                const div = document.createElement('div');
                div.className = 'schedule-item';
                const label = getSlotLabel(item.slot, input.sessionsPerDay);
                div.innerHTML = `<span class="slot-label">${label}</span> &#8594; ${escapeHtml(input.courses[item.courseIdx].name)}`;
                itemsDiv.appendChild(div);
            }
        }
        
        group.appendChild(header);
        group.appendChild(itemsDiv);
        studentSchedules.appendChild(group);
    }
}


// ============================================================================
// PART 6: SAMPLE DATA
// ============================================================================
// A realistic 6-course dataset so users can try the scheduler instantly.

function loadSampleData() {
    // Clear existing data.
    clearAll();
    
    // Add 6 courses with faculty.
    const sampleCourses = [
        { name: 'Intro to Programming', faculty: 'Dr. Sharma' },
        { name: 'Data Structures', faculty: 'Dr. Sharma' },
        { name: 'Discrete Maths', faculty: 'Dr. Gupta' },
        { name: 'Database Systems', faculty: 'Dr. Patel' },
        { name: 'Algorithms', faculty: 'Dr. Gupta' },
        { name: 'Operating Systems', faculty: 'Dr. Patel' }
    ];
    
    for (const c of sampleCourses) {
        addCourseRow(c.name, c.faculty);
    }
    
    // Add prerequisites (0-based indices):
    //   Intro to Programming → Data Structures
    //   Intro to Programming → Database Systems
    //   Data Structures → Algorithms
    //   Discrete Maths → Algorithms
    const samplePrereqs = [
        { from: 0, to: 1 },
        { from: 0, to: 3 },
        { from: 1, to: 4 },
        { from: 2, to: 4 }
    ];
    
    for (const p of samplePrereqs) {
        addPrereqRow(p.from, p.to);
    }
    
    // Add 5 students with their course enrollments.
    const sampleStudents = [
        { name: 'Aarav', courses: [0, 1, 2] },
        { name: 'Priya', courses: [0, 2, 3] },
        { name: 'Rohan', courses: [1, 4, 5] },
        { name: 'Sneha', courses: [0, 3, 5] },
        { name: 'Kiran', courses: [2, 4, 5] }
    ];
    
    for (const s of sampleStudents) {
        addStudentRow(s.name, s.courses);
    }
    
    // Set logistics.
    document.getElementById('hall-capacity').value = 100;
    document.getElementById('halls-available').value = 3;
    document.getElementById('sessions-per-day').value = 2;
    document.getElementById('max-days').value = 7;
}


// ============================================================================
// PART 7: CLEAR ALL
// ============================================================================

function clearAll() {
    courseRowsContainer.innerHTML = '';
    prereqRowsContainer.innerHTML = '';
    studentRowsContainer.innerHTML = '';
    courseCounter = 0;
    prereqCounter = 0;
    studentCounter = 0;
    
    // Reset logistics to defaults.
    document.getElementById('hall-capacity').value = 100;
    document.getElementById('halls-available').value = 3;
    document.getElementById('sessions-per-day').value = 2;
    document.getElementById('max-days').value = 7;
    
    // Hide results.
    resultsPanel.classList.remove('visible');
}


// ============================================================================
// PART 8: EVENT LISTENERS
// ============================================================================

// "Add" buttons.
addCourseBtn.addEventListener('click', () => addCourseRow());
addPrereqBtn.addEventListener('click', () => addPrereqRow());
addStudentBtn.addEventListener('click', () => addStudentRow());

// "Load Sample Data" button.
sampleDataBtn.addEventListener('click', loadSampleData);

// "Clear All" button.
clearBtn.addEventListener('click', clearAll);

// "Generate Schedule" button.
generateBtn.addEventListener('click', () => {
    // Collect all form data.
    const input = collectInput();
    
    // Check for validation errors.
    if (input.error) {
        resultsPanel.classList.add('visible');
        resultAlert.innerHTML = `<div class="alert-banner error">&#9888; ${escapeHtml(input.error)}</div>`;
        document.getElementById('result-topo').style.display = 'none';
        document.getElementById('result-timetable').style.display = 'none';
        document.getElementById('result-faculty').style.display = 'none';
        document.getElementById('result-students').style.display = 'none';
        resultsPanel.scrollIntoView({ behavior: 'smooth', block: 'start' });
        return;
    }
    
    // Run the scheduler!
    const result = runScheduler(input);
    
    // Display the results.
    renderResults(result, input);
});


// ============================================================================
// PART 9: SCROLL ANIMATIONS (Intersection Observer)
// ============================================================================
// Elements with the class "fade-in" start invisible and animate in
// when they scroll into the viewport.

const observerOptions = {
    root: null,           // observe relative to the viewport
    rootMargin: '0px',
    threshold: 0.1        // trigger when 10% of the element is visible
};

const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.classList.add('visible');
            // Stop observing once it's animated in (no need to re-trigger).
            observer.unobserve(entry.target);
        }
    });
}, observerOptions);

// Observe all elements with the .fade-in class.
document.querySelectorAll('.fade-in').forEach(el => observer.observe(el));


// ============================================================================
// PART 10: INITIAL STATE
// ============================================================================
// Start with one empty course row so the form doesn't look blank.
addCourseRow();
