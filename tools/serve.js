/* Minimal static server for the Mini Kernel Simulator frontend.
 *
 * The browser needs HTTP (not file://) so the page can fetch the committed
 * sample files and app definitions. No dependencies, no build step.
 *
 *   node tools/serve.js            -> http://127.0.0.1:8088/web/index.html
 *   PORT=9000 node tools/serve.js  -> custom port
 *   NO_OPEN=1 node tools/serve.js  -> do not open a browser
 */
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

const root = path.resolve(__dirname, '..');
const port = Number(process.env.PORT) || 8088;
const open = process.platform === 'win32' && !process.env.NO_OPEN;

const types = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.txt': 'text/plain; charset=utf-8',
  '.md': 'text/markdown; charset=utf-8',
  '.csv': 'text/csv; charset=utf-8',
  '.log': 'text/plain; charset=utf-8',
  '.c': 'text/plain; charset=utf-8',
  '.h': 'text/plain; charset=utf-8',
  '.png': 'image/png',
  '.ico': 'image/x-icon'
};

const server = http.createServer((request, response) => {
  let name;
  try {
    name = decodeURIComponent(request.url.split('?')[0]);
  } catch (error) {
    response.writeHead(400); response.end('bad request'); return;
  }
  if (name === '/' || name === '') name = '/web/index.html';
  const file = path.resolve(root, '.' + name);
  if (file !== root && !file.startsWith(root + path.sep)) {
    response.writeHead(403); response.end('forbidden'); return;
  }
  fs.readFile(file, (error, data) => {
    if (error) {
      response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
      response.end('not found: ' + name + '\n\nThe frontend expects repository paths such as ' +
                   '/sample_files/notes.txt and /apps/app-manifest.json.');
      return;
    }
    response.writeHead(200, {
      'Content-Type': types[path.extname(file).toLowerCase()] || 'application/octet-stream',
      'Cache-Control': 'no-store'
    });
    response.end(data);
  });
});

server.on('error', error => {
  if (error.code === 'EADDRINUSE')
    console.error('Port ' + port + ' is busy. Try: $env:PORT=9000; node tools/serve.js');
  else console.error(error.message);
  process.exit(1);
});

server.listen(port, '127.0.0.1', () => {
  const url = 'http://127.0.0.1:' + port + '/web/index.html';
  console.log('Mini Kernel Simulator frontend');
  console.log('  serving: ' + root);
  console.log('  open:    ' + url);
  console.log('  stop:    Ctrl+C');
  if (open) exec('start "" "' + url + '"');
});
