// Regression tests for the browser simulation core.
// Run with: node tests/web/kernel_core_test.js
// The core lives in web/index.html between the KERNEL-CORE markers, so the page
// stays a single self-contained file while its logic remains testable in Node.
'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

const file = path.join(__dirname, '..', '..', 'web', 'index.html');
const html = fs.readFileSync(file, 'utf8');
const begin = html.indexOf('/* KERNEL-CORE:BEGIN');
const end = html.indexOf('/* KERNEL-CORE:END */');
assert(begin > 0 && end > begin, 'KERNEL-CORE markers not found in web/index.html');

const sandbox = { console };
vm.createContext(sandbox);
vm.runInContext(html.slice(begin, end) + '\nthis.KernelCore = KernelCore;', sandbox, { filename: 'kernel-core.js' });
const K = sandbox.KernelCore;
assert(K && typeof K.runKernel === 'function', 'KernelCore did not load');

let checks = 0;
function ok(condition, message) { assert(condition, message); checks++; }
function near(actual, expected, message) {
  ok(Math.abs(actual - expected) < 0.001, message + ' (expected ' + expected + ', got ' + actual + ')');
}
function orderOf(run) {
  return run.requests.slice().sort((a, b) => a.order - b.order).map(r => r.id);
}
const LEGAL = {
  NEW: ['READY', 'TERMINATED'], READY: ['RUNNING', 'TERMINATED'],
  RUNNING: ['READY', 'WAITING', 'TERMINATED'], WAITING: ['READY', 'TERMINATED'],
  TERMINATED: []
};

// --- application catalogue ---------------------------------------------------
const catalog = K.catalog();
ok(catalog.length >= 5, 'at least five applications in the catalogue');
ok(new Set(catalog.map(a => a.pid)).size === catalog.length, 'application PIDs are unique');
catalog.forEach(app => {
  ok(app.burst > 0, app.kind + ' has CPU work');
  ok(app.priority >= 1 && app.priority <= 3, app.kind + ' uses the 1..3 priority convention');
  ok(app.steps.length >= 2, app.kind + ' has a multi-step program');
});

// --- full kernel run ---------------------------------------------------------
const run = K.runKernel(catalog.map(a => ({
  pid: a.pid, kind: a.kind, name: a.name, priority: a.priority,
  arrival: a.arrival, memory: a.memory, burst: a.burst
})), { cpu: 'priority', io: 'priority', memory: 96 });

ok(run.runtimes.every(rt => rt.done), 'every application finished');
ok(run.snapshots.length === run.elapsed + 1, 'one snapshot per clock tick plus t=0');
ok(run.gantt.length === run.elapsed, 'Gantt covers every tick');
ok(run.requests.length > 0, 'applications submitted file requests');
ok(run.requests.every(r => r.status === 'COMPLETED'), 'every file request completed');
const orders = run.requests.map(r => r.order).sort((a, b) => a - b);
orders.forEach((order, index) => ok(order === index + 1, 'service order is 1..N'));

run.transitions.forEach(change => {
  ok((LEGAL[change.from] || []).includes(change.to),
     'legal transition ' + change.from + ' -> ' + change.to + ' (P' + change.pid + ')');
});

run.runtimes.forEach(rt => {
  ok(rt.response >= 0, 'response time recorded for P' + rt.pid);
  ok(rt.finish > rt.arrival, 'completion after arrival for P' + rt.pid);
  const tat = rt.finish - rt.arrival;
  const wt = tat - rt.app.burst - rt.ioWait;
  ok(wt >= 0, 'non-negative waiting time for P' + rt.pid);
  ok(rt.app.burst === rt.cpuTicks, 'burst equals executed CPU ticks for P' + rt.pid);
});
ok(run.decisions.some(d => /priority/.test(d.text)), 'scheduling decisions are explained');

// --- real file results -------------------------------------------------------
const files = run.fs.files;
ok(files['/apps/calculator.txt'] && files['/apps/calculator.txt'].contents === '14 + 28 = 42',
   'calculator saved its result');
ok(/2 match/.test(files['/apps/search.txt'].contents), 'file search counted the ERROR lines');
ok(/500500/.test(files['/apps/counter.txt'].contents), 'counter saved 1..1000 = 500500');
ok(/lines=6/.test(files['/apps/analysis.txt'].contents), 'file analyzer measured the CSV');
ok(files['/apps/report-copy.txt'].contents === files['/docs/report.txt'].contents,
   'file copy produced an identical copy');
ok(/edited by text-editor/.test(files['/apps/notes.txt'].contents), 'text editor wrote the edited note');
ok(K.fsUsed(run.fs) > 39, 'application output consumed disk blocks');

// --- preemptive priority -----------------------------------------------------
const preempt = K.runKernel([
  { pid: 1, kind: 'counter', priority: 3, arrival: 0, memory: 8, burst: 5 },
  { pid: 2, kind: 'calculator', priority: 1, arrival: 2, memory: 6, burst: 2 }
], { cpu: 'priority', io: 'priority' });
ok(preempt.gantt[0] === 1, 'the low-priority counter starts first');
ok(preempt.gantt[2] === 2, 'the priority-1 calculator preempts it at t=2');
ok(preempt.decisions.some(d => /preempts/.test(d.text)), 'the preemption is explained');
ok(preempt.runtimes[0].dispatches >= 2, 'the preempted process is dispatched again');

const nonPreempt = K.runKernel([
  { pid: 1, kind: 'counter', priority: 3, arrival: 0, memory: 8, burst: 5 },
  { pid: 2, kind: 'calculator', priority: 1, arrival: 2, memory: 6, burst: 2 }
], { cpu: 'nonpreemptive', io: 'priority' });
ok(nonPreempt.gantt[2] === 1, 'non-preemptive priority lets the running process continue');

const tie = K.runKernel([
  { pid: 2, kind: 'counter', priority: 2, arrival: 0, memory: 8, burst: 3 },
  { pid: 1, kind: 'calculator', priority: 2, arrival: 0, memory: 6, burst: 2 }
], { cpu: 'priority', io: 'priority' });
ok(tie.gantt[0] === 1, 'equal priority and arrival break the tie by lower PID');

// --- I/O request scheduling --------------------------------------------------
const base = K.demoRequests();
const fcfs = K.simulateRequests(base, 'fcfs');
const priority = K.simulateRequests(base, 'priority');
ok(JSON.stringify(orderOf(fcfs)) === JSON.stringify([1, 2, 3, 4, 5, 6, 7]),
   'FCFS serves the fixed request set in submission order');
ok(JSON.stringify(orderOf(priority)) === JSON.stringify([1, 6, 7, 2, 3, 4, 5]),
   'priority serves the editor write and the search read before background reads');
const editorFcfs = fcfs.requests.find(r => r.app === 'text-editor');
const editorPriority = priority.requests.find(r => r.app === 'text-editor');
ok(editorFcfs.wait === 16 && editorPriority.wait === 2,
   'the editor write waits 16 ticks under FCFS and 2 under priority');
ok(priority.log.length === base.length, 'every selection has a reason');
ok(priority.log[1].text.indexOf('priority') >= 0, 'the reason mentions the priority policy');
ok(K.serviceTicks(113) === 8 && K.serviceTicks(0) === 1, 'service duration follows the block count');

const equal = K.pickRequest([
  { id: 1, status: 'PENDING', priority: 2, submitted: 3 },
  { id: 2, status: 'PENDING', priority: 2, submitted: 1 },
  { id: 3, status: 'PENDING', priority: 1, submitted: 5 }
], 'priority', 10);
ok(equal.index === 2, 'the lowest priority number wins');

// --- simulated file system ---------------------------------------------------
const sim = K.seedFs(K.newFs(64));
ok(sim.files['/docs/notes.txt'] !== undefined, 'sample files are seeded');
ok(K.fsUsed(sim) === 39, 'seed files use exactly 39 blocks');
const before = K.fsUsed(sim);
ok(K.fsWrite(sim, '/apps/one.txt', '0123456789abcdefghij', 'contiguous', 'user'), 'write a contiguous file');
ok(sim.files['/apps/one.txt'].blocks.length === 2, '20 bytes need two blocks');
ok(K.fsRead(sim, '/apps/one.txt') === '0123456789abcdefghij', 'read the file back');
ok(K.fsAppend(sim, '/apps/one.txt', 'abc', 'contiguous'), 'append grows the file');
ok(K.fsRead(sim, '/apps/one.txt') === '0123456789abcdefghijabc', 'appended contents are present');
ok(K.fsCopy(sim, '/apps/one.txt', '/apps/two.txt', 'indexed'), 'copy a file');
ok(sim.files['/apps/two.txt'].blocks.length === 3, 'indexed copy uses two data blocks plus an index block');
ok(K.fsRename(sim, '/apps/two.txt', '/apps/renamed.txt'), 'rename a file');
ok(!sim.files['/apps/two.txt'] && sim.files['/apps/renamed.txt'], 'the rename moved the entry');
ok(!K.fsRename(sim, '/apps/renamed.txt', '/apps/one.txt'), 'rename onto an existing file is rejected');
ok(!K.fsWrite(sim, '/apps', 'x', 'contiguous', 'user'), 'a directory name cannot be used as a file');
ok(!K.fsWrite(sim, 'relative.txt', 'x', 'contiguous', 'user'), 'relative paths are rejected by the core');
ok(!K.fsWrite(sim, '/missing/x.txt', 'x', 'contiguous', 'user'), 'missing parent directories are rejected');
ok(!K.fsWrite(sim, '/apps/one.txt', 'hack', 'contiguous', 'guest'), 'only the owner may overwrite');
ok(K.fsWrite(sim, '/apps/one.txt', 'owned', 'contiguous', 'admin'), 'admin may overwrite');
ok(K.fsDelete(sim, '/apps/renamed.txt'), 'delete releases the copy');
ok(K.fsDelete(sim, '/apps/one.txt'), 'delete releases the original');
ok(K.fsUsed(sim) === before, 'deleting the new files restores the original block usage');

let threw = false;
try { K.fsRead(sim, '/apps/missing.txt'); } catch (error) { threw = true; }
ok(threw, 'reading a missing file throws');

const small = K.seedFs(K.newFs(16));
const used = K.fsUsed(small);
ok(K.fsFree(small) === 16 - used, 'free-block accounting is consistent');
ok(!K.fsWrite(small, '/apps/huge.txt', new Array(500).join('x'), 'contiguous', 'user'),
   'a request larger than the free space is rejected');
ok(K.fsUsed(small) === used, 'a rejected write does not consume blocks');

console.log('All ' + checks + ' browser kernel-core checks passed.');
