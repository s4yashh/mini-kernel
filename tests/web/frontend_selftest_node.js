// Runs the browser self-test suite under Node against the real repository files.
// Usage: node tests/web/frontend_selftest_node.js
'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

const root = path.join(__dirname, '..', '..');
const html = fs.readFileSync(path.join(root, 'web', 'index.html'), 'utf8');
const begin = html.indexOf('/* KERNEL-CORE:BEGIN');
const end = html.indexOf('/* KERNEL-CORE:END */');
assert(begin > 0 && end > begin, 'KERNEL-CORE markers not found');

const sandbox = { console };
vm.createContext(sandbox);
vm.runInContext(html.slice(begin, end) + '\nthis.KernelCore = KernelCore;', sandbox, { filename: 'kernel-core.js' });
const core = sandbox.KernelCore;
const suite = require(path.join(root, 'web', 'js', 'selftest.js'));
const loader = require(path.join(root, 'web', 'js', 'samples.js'));

function readText(relative) {
  return fs.readFileSync(path.join(root, relative), 'utf8');
}

const manifest = JSON.parse(readText('apps/app-manifest.json'));
const applications = manifest.applications.map(meta => {
  const definition = JSON.parse(readText(meta.definition));
  const contents = readText(meta.definition);
  return {
    kind: definition.kind,
    name: definition.name,
    description: definition.description,
    definition: definition,
    path: meta.definition,
    readmePath: meta.definition.replace(/app\.json$/, 'README.md'),
    priority: definition.defaultPriority,
    arrival: definition.defaultArrival || 0,
    memory: definition.memoryUnits,
    burst: definition.defaultBurst || 0,
    workload: definition.workload,
    inputFiles: definition.inputFiles || [],
    outputFiles: definition.outputFiles || [],
    operations: definition.operations || [],
    repoBytes: contents.length
  };
});

const samples = manifest.samples.map(meta => {
  const contents = readText(meta.source);
  return {
    source: meta.source,
    vfsPath: meta.vfsPath,
    allocation: meta.allocation,
    description: meta.description || '',
    contents: contents,
    bytes: contents.length,
    type: loader.fileType(meta.source),
    extension: loader.extension(meta.source)
  };
});

// Every manifest path must exist, validate, and be usable.
const missing = [];
manifest.applications.forEach(meta => {
  if (!fs.existsSync(path.join(root, meta.definition))) missing.push(meta.definition);
  if (!fs.existsSync(path.join(root, meta.definition.replace(/app\.json$/, 'README.md'))))
    missing.push(meta.definition.replace(/app\.json$/, 'README.md'));
});
manifest.samples.forEach(meta => {
  if (!fs.existsSync(path.join(root, meta.source))) missing.push(meta.source);
});
assert.deepStrictEqual(missing, [], 'manifest paths missing: ' + missing.join(', '));

const validation = loader.validate(manifest, applications, samples, core.APP_KINDS);
assert.deepStrictEqual(validation, [], 'manifest validation failed:\n' + validation.join('\n'));

// The same suite the browser runs.
const report = suite.run(core, {
  applications: applications,
  samples: samples,
  repoFiles: samples.map(sample => ({
    source: sample.source, vfsPath: sample.vfsPath, contents: sample.contents,
    bytes: sample.bytes, type: sample.type, extension: sample.extension,
    allocation: sample.allocation
  }))
}, { reset: function () {} });

report.results.forEach(item => {
  console.log((item.ok ? 'PASS ' : 'FAIL ') + item.name);
  if (!item.ok) {
    console.log('     expected: ' + item.expected);
    console.log('     actual:   ' + item.actual);
    if (item.message) console.log('     note:     ' + item.message);
  }
});
console.log('\n' + report.passed + ' passed, ' + report.failed + ' failed, ' +
            report.total + ' total (' + applications.length + ' apps, ' +
            samples.length + ' sample files)');
if (report.failed) process.exitCode = 1;
