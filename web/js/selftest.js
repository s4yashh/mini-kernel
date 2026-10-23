/* Browser verification suite for the Mini Kernel Simulator frontend.
 *
 * These tests exercise the real JavaScript simulation core and the real
 * repository fixtures (sample files + app definitions). They do not check DOM
 * elements. The same suite runs from Node (tests/web/frontend_selftest_node.js).
 */
(function (root) {
  'use strict';

  function fmt(value) {
    if (value === undefined) return '';
    if (typeof value === 'string') return value.length > 300 ? value.slice(0, 300) + '…' : value;
    try { return JSON.stringify(value); } catch (error) { return String(value); }
  }

  function result(ok, expected, actual, message) {
    return { ok: ok, expected: fmt(expected), actual: fmt(actual), message: message || '' };
  }

  function same(actual, expected, message) {
    var a = JSON.stringify(actual), e = JSON.stringify(expected);
    return result(a === e, e, a, message);
  }

  function findSample(samples, path) {
    return samples.filter(function (sample) { return sample.vfsPath === path; })[0];
  }

  function regexCount(text, needle) {
    var escaped = needle.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    return (text.match(new RegExp(escaped, 'g')) || []).length;
  }

  function textStats(text) {
    var lines = text.split('\n');
    if (lines.length && lines[lines.length - 1] === '') lines.pop();
    var words = (text.match(/\S+/g) || []).length;
    return { lines: lines.length, words: words, characters: text.length };
  }

  function sampleList(samples) {
    return samples.map(function (sample) {
      return {
        vfsPath: sample.vfsPath, source: sample.source,
        contents: sample.contents, allocation: sample.allocation
      };
    });
  }

  function spec(kind, pid, priority, arrival, memory) {
    return { pid: pid, kind: kind, priority: priority, arrival: arrival, memory: memory, burst: 0 };
  }

  function run(core, fixtures, hooks) {
    hooks = hooks || {};
    var samples = fixtures.samples || [];
    var apps = fixtures.applications || [];
    var results = [];

    function test(name, fn) {
      try {
        var outcome = fn() || {};
        if (outcome.ok === undefined) outcome = result(false, 'a result', 'nothing', 'test produced no result');
        results.push({ name: name, ok: !!outcome.ok, expected: outcome.expected || '',
                       actual: outcome.actual || '', message: outcome.message || '' });
      } catch (error) {
        results.push({ name: name, ok: false, expected: 'no exception',
                       actual: error.message, message: 'unexpected exception' });
      }
    }

    function freshFs() {
      return core.fsFromSamples(sampleList(samples), 128);
    }

    test('1. application definitions load', function () {
      var names = apps.map(function (app) { return app.kind + ':' + app.name; });
      var valid = apps.length >= 5 && apps.every(function (app) {
        return app.kind && app.name && app.description &&
               Array.isArray(app.workload) && app.workload.length > 0 &&
               app.priority >= 1 && app.priority <= 3 &&
               app.memory > 0 && Array.isArray(app.outputFiles);
      });
      return result(valid, '>=5 valid definitions', apps.length + ' definitions: ' + names.join(', '));
    });

    test('2. required sample files load with content', function () {
      var required = ['/docs/notes.txt', '/docs/report.txt', '/docs/numbers.csv',
                      '/docs/source.c', '/docs/config.json', '/docs/system.log'];
      var missing = required.filter(function (path) { return !findSample(samples, path); });
      var empty = samples.filter(function (sample) { return !sample.contents || !sample.contents.length; });
      return result(samples.length >= 6 && !missing.length && !empty.length,
        '>=6 files, none missing/empty',
        samples.length + ' files; missing=[' + missing.join(',') + ']; empty=' + empty.length);
    });

    test('3. virtual file system initialises from samples', function () {
      var fs = freshFs();
      var missing = samples.filter(function (sample) { return !fs.files[sample.vfsPath]; });
      var sources = Object.keys(fs.files).map(function (path) { return fs.files[path].source; });
      var ok = !missing.length && core.fsUsed(fs) > 0 &&
               sources.every(function (source) { return source === 'repository sample'; });
      return result(ok, 'all samples present with source=repository sample',
        missing.length + ' missing, used=' + core.fsUsed(fs) + ' blocks, sources=' +
        Array.from(new Set(sources)).join(','));
    });

    test('4. reads return the committed contents', function () {
      var fs = freshFs();
      var mismatches = [];
      samples.forEach(function (sample) {
        var actual = core.fsRead(fs, sample.vfsPath);
        if (actual !== sample.contents) mismatches.push(sample.vfsPath);
      });
      return result(!mismatches.length, 'exact byte-for-byte match',
        mismatches.length ? 'mismatch: ' + mismatches.join(', ') : samples.length + ' files matched');
    });

    test('5. editing a virtual copy leaves the repository sample unchanged', function () {
      var fs = freshFs();
      var sample = findSample(samples, '/docs/notes.txt');
      var original = sample.contents;
      var written = core.fsWrite(fs, '/docs/notes.txt', 'EDITED ' + original, 'contiguous');
      var entry = fs.files['/docs/notes.txt'];
      var ok = written && core.fsRead(fs, '/docs/notes.txt') !== original &&
               sample.contents === original && entry.source === 'repository sample (edited)' &&
               entry.origin === sample.source;
      return result(ok, 'virtual file edited, fixture unchanged, source marked edited',
        'written=' + written + ', source=' + entry.source + ', fixtureIntact=' + (sample.contents === original));
    });

    test('6. copy preserves content with independent blocks', function () {
      var fs = freshFs();
      var before = core.fsUsed(fs);
      var copied = core.fsCopy(fs, '/docs/report.txt', '/apps/report-copy.txt', 'contiguous');
      var source = fs.files['/docs/report.txt'];
      var target = fs.files['/apps/report-copy.txt'];
      var disjoint = target && target.blocks.every(function (block) { return source.blocks.indexOf(block) < 0; });
      var added = target ? target.blocks.length : 0;
      return result(copied && target && target.contents === source.contents && disjoint &&
                    core.fsUsed(fs) === before + added,
        'copy equal, different blocks, used += ' + added,
        copied ? 'copy=' + (target && target.contents === source.contents) +
                 ', disjoint=' + disjoint + ', used=' + core.fsUsed(fs) : 'copy failed');
    });

    test('7. rename and move update the path', function () {
      var fs = freshFs();
      var contents = fs.files['/docs/notes.txt'].contents;
      var origin = fs.files['/docs/notes.txt'].origin;
      var moved = core.fsRename(fs, '/docs/notes.txt', '/apps/moved-notes.txt');
      var ok = moved && !fs.files['/docs/notes.txt'] &&
               fs.files['/apps/moved-notes.txt'].contents === contents &&
               fs.files['/apps/moved-notes.txt'].origin === origin;
      return result(ok, 'old path removed, new path holds the same contents',
        'moved=' + moved + ', oldExists=' + !!fs.files['/docs/notes.txt'] +
        ', origin kept=' + (fs.files['/apps/moved-notes.txt'] &&
                            fs.files['/apps/moved-notes.txt'].origin === origin));
    });

    test('8. deleting a file releases its blocks', function () {
      var fs = freshFs();
      var file = fs.files['/docs/system.log'];
      var expected = core.fsUsed(fs) - file.blocks.length;
      var deleted = core.fsDelete(fs, '/docs/system.log');
      return same(core.fsUsed(fs), expected, deleted ? '' : 'delete failed');
    });

    test('9. file search counts the real log contents', function () {
      var sample = findSample(samples, '/docs/system.log');
      var expected = regexCount(sample.contents, 'ERROR');
      var kernel = core.runKernel([spec('file-search', 1, 2, 0, 7)],
        { samples: sampleList(samples), cpu: 'priority', io: 'priority', memory: 96 });
      var output = kernel.runtimes[0].result;
      var file = kernel.fs.files['/apps/search.txt'];
      var want = 'found ' + expected + ' occurrence(s) of ERROR';
      var ok = output === want && file && file.contents.indexOf(expected + ' match') >= 0;
      return result(ok, want + ' and "' + expected + ' match(es)" in the file', output);
    });

    test('10. file analyzer computes real statistics', function () {
      var sample = findSample(samples, '/docs/numbers.csv');
      var stats = textStats(sample.contents);
      var want = 'lines=' + stats.lines + ' words=' + stats.words +
                 ' characters=' + stats.characters;
      var kernel = core.runKernel([spec('file-analyzer', 1, 3, 0, 6)],
        { samples: sampleList(samples), cpu: 'priority', io: 'priority', memory: 96 });
      return same(kernel.runtimes[0].result, want);
    });

    test('11. priority scheduling picks the highest priority', function () {
      var kernel = core.runKernel([
        spec('counter', 1, 3, 0, 8), spec('calculator', 2, 1, 0, 6)
      ], { samples: sampleList(samples), cpu: 'priority', io: 'priority', memory: 96 });
      return same(kernel.gantt[0], 2, 'the priority-1 calculator must run first');
    });

    test('12. preemption interrupts the running process', function () {
      var kernel = core.runKernel([
        spec('counter', 1, 3, 0, 8), spec('calculator', 2, 1, 2, 6)
      ], { samples: sampleList(samples), cpu: 'priority', io: 'priority', memory: 96 });
      var preempted = kernel.decisions.some(function (entry) {
        return entry.text.indexOf('preempts') >= 0;
      });
      return result(kernel.gantt[2] === 2 && preempted,
        'tick 2 runs P2 and a preemption is logged',
        'tick2=' + kernel.gantt[2] + ', preemptionLogged=' + preempted);
    });

    test('13. FCFS and priority I/O orders are correct', function () {
      var fcfs = core.simulateRequests(core.demoRequests(), 'fcfs');
      var priority = core.simulateRequests(core.demoRequests(), 'priority');
      function order(run) {
        return run.requests.slice().sort(function (a, b) { return a.order - b.order; })
          .map(function (request) { return request.id; });
      }
      return result(JSON.stringify(order(fcfs)) === JSON.stringify([1, 2, 3, 4, 5, 6, 7]) &&
                    JSON.stringify(order(priority)) === JSON.stringify([1, 6, 7, 2, 3, 4, 5]),
        'FCFS [1,2,3,4,5,6,7]; priority [1,6,7,2,3,4,5]',
        'FCFS [' + order(fcfs) + ']; priority [' + order(priority) + ']');
    });

    test('14. I/O blocks a process and completion wakes it', function () {
      var kernel = core.runKernel([spec('file-search', 1, 2, 0, 7)],
        { samples: sampleList(samples), cpu: 'priority', io: 'priority', memory: 96 });
      var blocked = kernel.transitions.some(function (change) {
        return change.pid === 1 && change.from === 'RUNNING' && change.to === 'WAITING';
      });
      var woke = kernel.transitions.some(function (change) {
        return change.pid === 1 && change.from === 'WAITING' && change.to === 'READY';
      });
      return result(blocked && woke, 'RUNNING->WAITING and WAITING->READY',
        'blocked=' + blocked + ', woke=' + woke);
    });

    test('15. First/Best/Worst Fit allocate valid partitions', function () {
      function map() {
        return [{ start: 0, size: 30, pid: 0 },
                { start: 30, size: 20, pid: 0 },
                { start: 50, size: 25, pid: 0 }];
      }
      var first = map(), best = map(), worst = map();
      core.memoryAllocate(first, 1, 18, 'First Fit');
      core.memoryAllocate(best, 1, 18, 'Best Fit');
      core.memoryAllocate(worst, 1, 18, 'Worst Fit');
      function startOf(regions) {
        return regions.filter(function (region) { return region.pid === 1; })[0].start;
      }
      var chosen = [startOf(first), startOf(best), startOf(worst)];
      var contiguous = first.every(function (region, index) {
        return index === 0 || region.start === first[index - 1].start + first[index - 1].size;
      });
      var conserved = core.memoryUsed(first) + core.memoryFree(first) === 75;
      return result(JSON.stringify(chosen) === JSON.stringify([0, 30, 0]) && contiguous && conserved,
        'starts 0/30/0, contiguous, used+free=75',
        'starts ' + chosen.join('/') + ', contiguous=' + contiguous + ', conserved=' + conserved);
    });

    test('16. deadlock detection and recovery stay consistent', function () {
      var state = core.deadlockSample();
      var detection = core.deadlockDetect(state);
      var recovered = core.deadlockRecover(state, 'least');
      var after = core.deadlockDetect(recovered.state);
      var released = recovered.victim >= 0 &&
        recovered.state.allocation[recovered.victim].every(function (units) { return units === 0; });
      return result(JSON.stringify(detection.deadlocked) === '[0,1]' &&
                    after.deadlocked.length === 0 && released &&
                    recovered.state.available[0] === 1,
        'P0,P1 deadlocked; recovery leaves no deadlock and returns [1,0]',
        'deadlocked=' + JSON.stringify(detection.deadlocked) +
        ', after=' + JSON.stringify(after.deadlocked) +
        ', available=' + JSON.stringify(recovered.state.available));
    });

    test('17. reset restores the initial demonstration state', function () {
      function signature(fs) {
        return {
          used: core.fsUsed(fs),
          files: Object.keys(fs.files).length,
          notes: core.fsRead(fs, '/docs/notes.txt')
        };
      }
      var before = signature(freshFs());
      if (hooks.reset) hooks.reset();
      var after = signature(freshFs());
      var live = hooks.snapshot ? hooks.snapshot() : null;
      var liveOk = !live || (live.used === after.used && live.files === after.files &&
                             live.notes === after.notes);
      return result(JSON.stringify(before) === JSON.stringify(after) && liveOk,
        'fresh VFS has the same files/blocks; live view matches after Reset',
        JSON.stringify(live || after));
    });

    test('18. missing files and invalid requests fail safely', function () {
      var fs = freshFs();
      var readThrew = false;
      try { core.fsRead(fs, '/docs/missing.txt'); }
      catch (error) { readThrew = error.message.indexOf('/docs/missing.txt') >= 0; }
      var used = core.fsUsed(fs);
      var invalidWrites = [
        core.fsWrite(fs, 'relative.txt', 'x', 'contiguous'),
        core.fsWrite(fs, '/docs', 'x', 'contiguous'),
        core.fsWrite(fs, '/docs/' + new Array(400).join('x'), new Array(4000).join('x'), 'contiguous')
      ];
      var emptyPick = core.pickRequest([], 'fcfs').index === -1;
      var duplicateThrew = false;
      try {
        core.runKernel([spec('counter', 1, 3, 0, 8), spec('calculator', 1, 1, 0, 6)],
          { samples: sampleList(samples), memory: 96 });
      } catch (error) { duplicateThrew = error.message.indexOf('duplicate') >= 0; }
      var ok = readThrew && invalidWrites.every(function (written) { return written === false; }) &&
               core.fsUsed(fs) === used && emptyPick && duplicateThrew &&
               !core.fsRename(fs, '/docs/missing.txt', '/apps/x.txt') &&
               !core.fsCopy(fs, '/docs/missing.txt', '/apps/x.txt', 'contiguous');
      return result(ok, 'errors thrown/rejected, storage unchanged',
        'readThrew=' + readThrew + ', writes=' + invalidWrites.join(',') +
        ', used=' + used + ', emptyPick=' + emptyPick + ', duplicateRejected=' + duplicateThrew);
    });

    var passed = results.filter(function (item) { return item.ok; }).length;
    return {
      passed: passed,
      failed: results.length - passed,
      total: results.length,
      results: results
    };
  }

  var api = { run: run, fmt: fmt };
  root.KernelSelfTest = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : globalThis);
