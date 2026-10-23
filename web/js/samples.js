/* Repository sample loader for the Mini Kernel Simulator frontend.
 *
 * The simulator keeps its own in-memory file system, but the *source* files are
 * the ones committed under sample_files/ and apps/. This loader fetches them,
 * validates every manifest path, and either returns usable fixtures or a list of
 * exact missing/invalid paths. It never substitutes invented contents.
 *
 * The page is served over HTTP because browsers block fetch() from file:// URLs.
 * Run `node tools/serve.js` (or `run_frontend.bat`) and open the printed URL.
 */
(function (root) {
  'use strict';

  var ALLOCATIONS = ['contiguous', 'linked', 'indexed'];

  function extension(path) {
    var name = String(path).split('/').pop();
    var dot = name.lastIndexOf('.');
    return dot > 0 ? name.slice(dot + 1).toLowerCase() : '';
  }

  function fileType(path) {
    var ext = extension(path);
    var map = {
      txt: 'text', md: 'markdown', csv: 'csv', json: 'json', c: 'C source',
      h: 'C header', log: 'log', js: 'javascript', html: 'html', css: 'css'
    };
    return map[ext] || (ext ? ext.toUpperCase() : 'file');
  }

  function validate(manifest, applications, samples, knownKinds) {
    var errors = [];
    if (!manifest || manifest.version !== 1) errors.push('manifest: missing or unsupported version');
    if (!manifest || !Array.isArray(manifest.applications) || manifest.applications.length < 5)
      errors.push('manifest: expected at least five application definitions');
    if (!manifest || !Array.isArray(manifest.samples) || manifest.samples.length < 6)
      errors.push('manifest: expected at least six sample-file entries');
    if (applications.length !== (manifest && manifest.applications ? manifest.applications.length : 0))
      errors.push('manifest: application definitions did not all load');

    var kinds = {};
    applications.forEach(function (entry) {
      var app = entry.definition;
      var where = entry.path;
      if (!app) { errors.push(where + ': empty definition'); return; }
      if (knownKinds && knownKinds.indexOf(app.kind) < 0)
        errors.push(where + ': unknown application kind "' + app.kind + '"');
      if (!app.name) errors.push(where + ': missing name');
      if (!app.description) errors.push(where + ': missing description');
      if (!Array.isArray(app.workload) || !app.workload.length) errors.push(where + ': missing workload');
      if (!(app.defaultPriority >= 1 && app.defaultPriority <= 3))
        errors.push(where + ': defaultPriority must be 1..3');
      if (!(app.memoryUnits > 0)) errors.push(where + ': memoryUnits must be positive');
      if (!Array.isArray(app.operations) || !app.operations.length) errors.push(where + ': missing operations');
      if (!Array.isArray(app.inputFiles) || !Array.isArray(app.outputFiles))
        errors.push(where + ': inputFiles/outputFiles must be arrays');
      (app.inputFiles || []).concat(app.outputFiles || []).forEach(function (path) {
        if (typeof path !== 'string' || path.charAt(0) !== '/')
          errors.push(where + ': file path must be absolute: ' + path);
      });
      if (kinds[app.kind]) errors.push(where + ': duplicate application kind ' + app.kind);
      kinds[app.kind] = true;
      if (entry.path !== manifest.applications.filter(function (item) {
        return item.kind === app.kind;
      })[0].definition) {
        errors.push(where + ': manifest kind/definition mismatch');
      }
    });

    var paths = {};
    samples.forEach(function (sample, index) {
      var where = 'samples[' + index + ']';
      if (!sample.source) errors.push(where + ': missing source path');
      if (!sample.contents || !sample.contents.length)
        errors.push(where + ': file is empty (' + sample.source + ')');
      if (!sample.vfsPath || sample.vfsPath.indexOf('/docs/') !== 0)
        errors.push(where + ': vfsPath must be under /docs');
      if (ALLOCATIONS.indexOf(sample.allocation) < 0)
        errors.push(where + ': unknown allocation "' + sample.allocation + '"');
      if (paths[sample.vfsPath]) errors.push(where + ': duplicate vfsPath ' + sample.vfsPath);
      paths[sample.vfsPath] = true;
    });

    return errors;
  }

  function buildAppEntry(meta, app, contents) {
    var readme = meta.definition.replace(/app\.json$/, 'README.md');
    return {
      kind: app.kind,
      name: app.name,
      description: app.description,
      definition: app,
      path: meta.definition,
      readmePath: readme,
      priority: app.defaultPriority,
      arrival: app.defaultArrival || 0,
      memory: app.memoryUnits,
      burst: app.defaultBurst || 0,
      workload: app.workload,
      inputFiles: app.inputFiles || [],
      outputFiles: app.outputFiles || [],
      operations: app.operations || [],
      repoBytes: contents ? contents.length : 0
    };
  }

  async function fetchText(url) {
    var response = await fetch(url, { cache: 'no-cache' });
    if (!response.ok) throw new Error('HTTP ' + response.status + ' for ' + url);
    return response.text();
  }

  async function load(baseUrl, options) {
    options = options || {};
    var rootUrl = baseUrl || '../';
    if (rootUrl.charAt(rootUrl.length - 1) !== '/') rootUrl += '/';
    var manifestPath = 'apps/app-manifest.json';
    var result = {
      ok: false, baseUrl: rootUrl, manifest: null, applications: [], samples: [],
      repoFiles: [], errors: []
    };

    var manifestText;
    try {
      manifestText = await fetchText(rootUrl + manifestPath);
    } catch (error) {
      result.errors.push(manifestPath + ': ' + error.message);
      return result;
    }
    try {
      result.manifest = JSON.parse(manifestText);
    } catch (error) {
      result.errors.push(manifestPath + ': invalid JSON (' + error.message + ')');
      return result;
    }
    var manifest = result.manifest;

    var appJobs = (manifest.applications || []).map(async function (meta) {
      try {
        var text = await fetchText(rootUrl + meta.definition);
        result.applications.push(buildAppEntry(meta, JSON.parse(text), text));
      } catch (error) {
        result.errors.push(meta.definition + ': ' + error.message);
      }
      return meta;
    });

    var sampleJobs = (manifest.samples || []).map(async function (meta) {
      try {
        var contents = await fetchText(rootUrl + meta.source);
        result.samples.push({
          source: meta.source, vfsPath: meta.vfsPath, allocation: meta.allocation,
          description: meta.description || '', contents: contents, bytes: contents.length,
          type: fileType(meta.source), extension: extension(meta.source)
        });
        result.repoFiles.push({
          source: meta.source, vfsPath: meta.vfsPath, contents: contents,
          bytes: contents.length, type: fileType(meta.source),
          extension: extension(meta.source), allocation: meta.allocation
        });
      } catch (error) {
        result.errors.push(meta.source + ': ' + error.message);
      }
      return meta;
    });

    await Promise.all(appJobs.concat(sampleJobs));
    result.applications.sort(function (a, b) {
      return (manifest.applications || []).findIndex(function (item) { return item.kind === a.kind; }) -
             (manifest.applications || []).findIndex(function (item) { return item.kind === b.kind; });
    });
    result.samples.sort(function (a, b) {
      return a.vfsPath.localeCompare(b.vfsPath);
    });
    result.repoFiles.sort(function (a, b) {
      return a.source.localeCompare(b.source);
    });

    var validation = validate(manifest, result.applications, result.samples, options.knownKinds);
    result.errors = result.errors.concat(validation);
    result.ok = result.errors.length === 0;
    return result;
  }

  var api = { load: load, validate: validate, extension: extension, fileType: fileType };
  root.SampleLoader = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : globalThis);
