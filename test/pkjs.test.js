// End-to-end test of src/pkjs/index.js against a mock Memos server.
// Stubs the PebbleKit JS environment and simulates watch requests.
// Run with: node test/pkjs.test.js (needs Node 18+ for fetch)
process.env.TZ = 'UTC'; // dates in list subtitles are local time
const http = require('http');
const assert = require('assert');
const path = require('path');

/* ---------- mock Memos server ---------- */
let memos = [];
for (let i = 1; i <= 60; i++) {
  memos.push({ name: 'memos/m' + i, content: (i % 3 === 0 ? '# Title ' + i + '\nbody #work' : '- [ ] task ' + i + ' #home'),
    tags: i % 3 === 0 ? ['work'] : ['home'], createTime: '2026-10-1' + (i % 9) + 'T12:0' + (i % 6) + ':00Z', state: 'NORMAL' });
}
memos.push({ name: 'memos/p1', content: 'project note', tags: ['proj/a'], createTime: '2026-10-18T09:00:00Z', state: 'NORMAL' });
memos.push({ name: 'memos/md1', tags: ['idea'], createTime: '2026-10-01T09:00:00Z', state: 'NORMAL', content: [
  '# Big title', '', '## Sub *title*', 'Some **bold** and `code` with [a link](http://x.y).',
  '- [ ] open task', '- [x] done task', '* bullet', '1. first', '> quoted', '---',
  '```', 'let x = 1;', '- [ ] in code', '```', '![pic](http://img)', '#idea #pebble'].join('\n') });
memos.push({ name: 'memos/c1', content: 'a comment', parent: 'memos/m1', tags: [], state: 'NORMAL' });
memos.push({ name: 'memos/u1', content: 'Ünïcödé 😀 ' + 'x'.repeat(2000), tags: ['work/sub'], createTime: '2026-10-18T08:00:00Z', state: 'NORMAL' });
const created = [], comments = [];
let serverDown = false;
const server = http.createServer((req, res) => {
  if (serverDown) return req.socket.destroy();
  let body = '';
  req.on('data', c => body += c);
  req.on('end', () => {
    if (req.headers.authorization !== 'Bearer tok') { res.writeHead(401); return res.end('{}'); }
    const u = new URL(req.url, 'http://x');
    const send = (code, obj) => { res.writeHead(code, {'Content-Type': 'application/json'}); res.end(JSON.stringify(obj)); };
    let m;
    if (req.method === 'GET' && u.pathname === '/api/v1/memos') {
      const size = +u.searchParams.get('pageSize'), start = +(u.searchParams.get('pageToken') || 0);
      const visible = memos.filter(x => x.state !== 'ARCHIVED');
      const page = visible.slice(start, start + size);
      return send(200, { memos: page, nextPageToken: start + size < visible.length ? String(start + size) : '' });
    }
    if (req.method === 'POST' && u.pathname === '/api/v1/memos') {
      created.push(JSON.parse(body));
      const memo = { name: 'memos/new' + created.length, content: JSON.parse(body).content, createTime: '2026-10-18T10:00:00Z', state: 'NORMAL' };
      memos.unshift(memo);
      return send(200, memo);
    }
    if ((m = u.pathname.match(/^\/api\/v1\/(memos\/[\w]+)\/comments$/)) && req.method === 'POST') { comments.push([m[1], JSON.parse(body)]); return send(200, {}); }
    if ((m = u.pathname.match(/^\/api\/v1\/(memos\/[\w]+)$/))) {
      const memo = memos.find(x => x.name === m[1]);
      if (!memo) return send(404, {});
      if (req.method === 'GET') return send(200, memo);
      if (req.method === 'DELETE') { memos = memos.filter(x => x !== memo); return send(200, {}); }
      if (req.method === 'PATCH') {
        const mask = u.searchParams.get('updateMask');
        assert.ok(mask === 'state' || mask === 'content', mask);
        memo[mask] = JSON.parse(body)[mask]; return send(200, memo);
      }
    }
    send(404, {});
  });
});

/* ---------- PebbleKit JS stubs ---------- */
const listeners = {};
const received = [];
let nackNext = false;
const store = {};
global.localStorage = { getItem: k => store[k] ?? null, setItem: (k, v) => store[k] = String(v) };
global.Pebble = {
  addEventListener: (n, f) => listeners[n] = f,
  sendAppMessage: (msg, ok, fail) => setTimeout(() => {
    if (nackNext) { nackNext = false; return fail({}); }
    received.push(msg); ok({});
  }, 1),
  openURL: () => {},
};
global.XMLHttpRequest = class {
  open(method, url) { this.method = method; this.url = url; this.headers = {}; }
  setRequestHeader(k, v) { this.headers[k] = v; }
  send(body) {
    fetch(this.url, { method: this.method, headers: this.headers, body: body || undefined })
      .then(r => r.text().then(t => { this.status = r.status; this.responseText = t; this.onload(); }))
      .catch(() => this.onerror());
  }
};
require(path.join(__dirname, '..', 'src', 'pkjs', 'index.js'));

let reqId = 0;
function request(action, target, text) {
  const id = ++reqId;
  const before = received.length;
  listeners.appmessage({ payload: { REQUEST_ID: id, ACTION: action, ITEM_ID: target, TEXT: text } });
  return new Promise(resolve => {
    const t = setInterval(() => {
      const msgs = received.slice(before).filter(m => m.REQUEST_ID === id);
      const head = msgs[0];
      if (!head) return;
      if (head.STATUS !== 0 || head.LIST_COUNT === undefined || msgs.length === head.LIST_COUNT + 1) {
        clearInterval(t); resolve(msgs);
      }
    }, 5);
  });
}
const bytes = s => Buffer.byteLength(s, 'utf8');

// fail instead of hanging if a request never gets an answer
setTimeout(() => { console.error('Test timed out'); process.exit(1); }, 30000).unref();

(async () => {
  await new Promise(r => server.listen(0, r));
  const port = server.address().port;

  // not configured
  let r = await request(2);
  assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Not configured']);

  // legacy URL form with /api/v1/memos is accepted
  store.memos_settings = JSON.stringify({ url: `http://127.0.0.1:${port}/api/v1/memos`, token: 'tok', tags: '#pebble #watch' });

  // recent: 20 items, comments skipped, limits respected
  r = await request(2);
  assert.strictEqual(r[0].LIST_COUNT, 20);
  const items = r.slice(1);
  assert.ok(items.every(m => m.ITEM_ID !== 'memos/c1'));
  assert.ok(items.every((m, i) => m.ITEM_INDEX === i && bytes(m.ITEM_TITLE) <= 63 && bytes(m.ITEM_SUBTITLE) <= 47));
  assert.strictEqual(items[0].ITEM_TITLE, 'task 1');
  assert.strictEqual(items[2].ITEM_TITLE, 'Title 3');
  console.log('recent ok:', items[0], items[2].ITEM_TITLE);

  // tags with counts, hierarchical tag counted
  r = await request(3);
  const tags = r.slice(1).map(m => [m.ITEM_ID, m.ITEM_TITLE, m.ITEM_SUBTITLE]);
  console.log('tags:', tags);
  assert.deepStrictEqual(tags[0], ['home', '#home', '40 memos']);

  // by tag: across pages, limited to 20; "proj" also matches "proj/a"
  r = await request(4, 'work');
  assert.strictEqual(r[0].LIST_COUNT, 20);
  assert.ok(r.slice(1).every(m => +m.ITEM_ID.slice(7) % 3 === 0));
  r = await request(4, 'proj');
  assert.deepStrictEqual(r.slice(1).map(m => m.ITEM_ID), ['memos/p1']);
  r = await request(4, 'nothing');
  assert.strictEqual(r[0].LIST_COUNT, 0);

  // get: utf-8 safe truncation to 1500 bytes
  r = await request(5, 'memos/u1');
  assert.strictEqual(r[0].STATUS, 0);
  assert.ok(bytes(r[0].TEXT) <= 1500 && r[0].TEXT.startsWith('pÜnïcödé 😀'));
  assert.ok(!/�/.test(Buffer.from(r[0].TEXT).toString()));

  // markdown is converted into typed lines for the watch
  r = await request(5, 'memos/md1');
  assert.deepStrictEqual(r[0].TEXT.split('\n'), [
    '1Big title', '2Sub title', 'pSome bold and code with a link.',
    'copen task', 'xdone task', 'bbullet', 'n1. first', 'qquoted', '-',
    'klet x = 1;', 'k- [ ] in code', 'p[pic]', 't#idea #pebble']);

  // tasks of one memo in document order, ids are memo name and task index;
  // checkboxes in code blocks are not tasks
  r = await request(9, 'memos/md1');
  assert.deepStrictEqual(r.slice(1).map(t => [t.ITEM_ID, t.ITEM_TITLE, t.ITEM_FLAGS]),
    [['memos/md1#0', 'open task', 0], ['memos/md1#1', 'done task', 1]]);
  r = await request(9, 'memos/m1');
  assert.deepStrictEqual(r.slice(1).map(t => t.ITEM_TITLE), ['task 1']);
  r = await request(9, 'memos/u1');
  assert.strictEqual(r[0].LIST_COUNT, 0);
  r = await request(9, 'memos/nope');
  assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Server error 404']);

  // toggling patches the memo content; code blocks are not tasks
  r = await request(10, 'memos/md1#0', 'open task');
  assert.deepStrictEqual([r[0].STATUS, r[0].ITEM_FLAGS], [0, 1]);
  let md1 = memos.find(m => m.name === 'memos/md1').content;
  assert.ok(md1.includes('- [x] open task') && md1.includes('- [x] done task') && md1.includes('- [ ] in code'));
  r = await request(10, 'memos/md1#1', 'done task');
  assert.deepStrictEqual([r[0].STATUS, r[0].ITEM_FLAGS], [0, 0]);
  md1 = memos.find(m => m.name === 'memos/md1').content;
  assert.ok(md1.includes('- [ ] done task'));
  r = await request(10, 'memos/md1#0', 'something else');
  assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Memo changed, reload tasks']);
  r = await request(10, 'memos/md1#5', 'x');
  assert.strictEqual(r[0].STATUS, 1);
  assert.ok(memos.find(m => m.name === 'memos/md1').content.includes('- [x] open task'));

  // create with keywords and tags; empty memos are rejected
  r = await request(1, null, 'title big shopping new line check box milk hashtag Food');
  assert.strictEqual(r[0].STATUS, 0);
  console.log('created:', JSON.stringify(created[0].content));
  assert.strictEqual(created[0].content, '# shopping\n- [ ] milk\n\n#pebble #watch #food');
  for (const empty of ['', '   ', 'hashtag work', 'check box', 'title big']) {
    r = await request(1, null, empty);
    assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Memo is empty'], empty);
  }
  assert.strictEqual(created.length, 1);

  // comment
  r = await request(8, 'memos/m2', 'looks good');
  assert.strictEqual(r[0].STATUS, 0);
  assert.deepStrictEqual(comments[0], ['memos/m2', { content: 'looks good' }]);
  r = await request(8, 'memos/m2', '  ');
  assert.strictEqual(r[0].STATUS, 1);

  // archive and delete remove memos from the recent list
  assert.strictEqual((await request(6, 'memos/m1'))[0].STATUS, 0);
  assert.strictEqual((await request(7, 'memos/m2'))[0].STATUS, 0);
  r = await request(2);
  assert.ok(!r.slice(1).some(m => m.ITEM_ID === 'memos/m1' || m.ITEM_ID === 'memos/m2'));
  r = await request(7, 'memos/nope');
  assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Server error 404']);

  // without a connection lists and memos come from the cache
  serverDown = true;
  r = await request(2);
  assert.strictEqual(r[0].STATUS, 0);
  assert.strictEqual(r[0].TEXT, 'Offline: cached memos');
  assert.strictEqual(r[0].LIST_COUNT, 20);
  assert.ok(!r.slice(1).some(m => m.ITEM_ID === 'memos/m1' || m.ITEM_ID === 'memos/m2'));
  r = await request(4, 'idea');
  assert.deepStrictEqual(r.slice(1).map(m => m.ITEM_ID), ['memos/md1']);
  r = await request(3);
  assert.strictEqual(r[0].TEXT, 'Offline: cached memos');
  r = await request(5, 'memos/md1');
  assert.ok(r[0].TEXT.startsWith('1Big title'));
  r = await request(5, 'memos/new1'); // created memos are cached, too
  assert.ok(r[0].TEXT.startsWith('1shopping'));
  r = await request(9, 'memos/md1');
  assert.deepStrictEqual([r[0].TEXT, r[0].LIST_COUNT], ['Offline: cached memos', 2]);
  r = await request(5, 'memos/unknown');
  assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Network error']);
  r = await request(6, 'memos/m3');
  assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Network error']);
  localStorage.setItem('memos_cache', '[]');
  r = await request(2);
  assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Network error']);
  serverDown = false;

  // a NACK is retried and nothing is lost
  nackNext = true;
  r = await request(2);
  assert.strictEqual(r.length, 21);

  // wrong token
  store.memos_settings = JSON.stringify({ url: `http://127.0.0.1:${port}/`, token: 'bad' });
  r = await request(2);
  assert.deepStrictEqual([r[0].STATUS, r[0].TEXT], [1, 'Server error 401']);

  // the vibration setting is pushed to the watch on start and on change;
  // it defaults to on for settings saved before the option existed
  const before = received.length;
  listeners.ready({});
  listeners.webviewclosed({ response: encodeURIComponent(JSON.stringify({ url: 'x', token: 'y', vibrate: false })) });
  await new Promise(res => setTimeout(res, 50));
  assert.deepStrictEqual(received.slice(before), [{ VIBRATE: 1 }, { VIBRATE: 0 }]);

  console.log('ALL OK');
  server.close();
})().catch(e => { console.error(e); process.exit(1); });
