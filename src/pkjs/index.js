function getSettings() {
  var s = localStorage.getItem("memos_settings");
  if (!s) return null;
  return JSON.parse(s);
}

var DEFAULT_TAGS = ["#pebble"];

/* ---------- ACTIONS (MUST MATCH CommAction IN comm.h) ---------- */
var ACTION_CREATE        = 1;
var ACTION_LIST_RECENT   = 2;
var ACTION_LIST_TAGS     = 3;
var ACTION_LIST_BY_TAG   = 4;
var ACTION_GET           = 5;
var ACTION_ARCHIVE       = 6;
var ACTION_DELETE        = 7;
var ACTION_COMMENT       = 8;

/* ---------- LIMITS (MUST FIT THE BUFFERS IN comm.h) ---------- */
var MAX_ID_BYTES       = 47;
var MAX_TITLE_BYTES    = 63;
var MAX_SUBTITLE_BYTES = 47;
var MAX_TEXT_BYTES     = 1500;
var MAX_LIST_ITEMS     = 20;
var MAX_TAG_ITEMS      = 30;

var PAGE_SIZE   = 50;
var MAX_PAGES   = 4;
var TIMEOUT_MS  = 15000;

var MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

/* =========================================================
 * Text helpers
 * ========================================================= */

// Truncate to a maximum number of UTF-8 bytes without splitting a character
function truncateUtf8(text, maxBytes) {
  var bytes = 0;
  for (var i = 0; i < text.length; i++) {
    var code = text.charCodeAt(i);
    var len;
    if (code < 0x80) len = 1;
    else if (code < 0x800) len = 2;
    else if (code >= 0xD800 && code <= 0xDBFF) len = 4; // surrogate pair
    else len = 3;

    if (bytes + len > maxBytes) {
      return text.substring(0, i);
    }
    bytes += len;
    if (len === 4) i++; // skip the low surrogate
  }
  return text;
}

function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}

function formatDate(isoString) {
  if (!isoString) return "";
  var d = new Date(isoString);
  if (isNaN(d.getTime())) return "";
  return MONTHS[d.getMonth()] + " " + d.getDate() + ", " +
    pad2(d.getHours()) + ":" + pad2(d.getMinutes());
}

/* =========================================================
 * Memo parser
 * ========================================================= */

// Replace spoken keywords with markdown
function applyKeywords(text) {
  var content = text;
  content = content.replace(/check box[\s,.]*/gi, "- [ ] ");
  content = content.replace(/checkbox[\s,.]*/gi, "- [ ] ");
  content = content.replace(/new line[\s,.]*/gi, "\n");
  content = content.replace(/title big[\s,.]*/gi, "# ");
  content = content.replace(/title medium[\s,.]*/gi, "## ");
  content = content.replace(/title small[\s,.]*/gi, "### ");
  return content.replace(/[ \t]+\n/g, "\n");
}

// Returns the final memo content, or null if there is no actual text
function parseMemo(text, defaultTags) {
  var tags = defaultTags.slice();
  var content = applyKeywords(text);

  /* ---- Extract spoken hashtags ---- */
  var tagRegex = /hashtag\s+(\w+)/gi;
  var match;
  while ((match = tagRegex.exec(text)) !== null) {
    tags.push("#" + match[1].toLowerCase());
  }
  content = content.replace(tagRegex, "").trim();

  // Only markdown leftovers (e.g. a lone checkbox) count as empty, too
  if (content.replace(/[-#\[\]\s]/g, "") === "") return null;

  return content + "\n\n" + tags.join(" ");
}

function memoTags(memo) {
  if (memo.tags && memo.tags.length) return memo.tags;
  if (memo.property && memo.property.tags) return memo.property.tags;
  var tags = [];
  var regex = /(^|\s)#([^\s#]+)/g;
  var match;
  while ((match = regex.exec(memo.content || "")) !== null) {
    tags.push(match[2]);
  }
  return tags;
}

function hasTag(memo, tag) {
  var tags = memoTags(memo);
  for (var i = 0; i < tags.length; i++) {
    // hierarchical tags like "work/project" also match "work"
    if (tags[i] === tag || tags[i].indexOf(tag + "/") === 0) return true;
  }
  return false;
}

// First line with actual text, without markdown prefixes
function memoTitle(memo) {
  var lines = (memo.content || "").split("\n");
  for (var i = 0; i < lines.length; i++) {
    var line = lines[i]
      .replace(/^\s*(#+|[-*+]|\d+\.)\s+/, "")
      .replace(/^\[[ xX]\]\s*/, "")
      .trim();
    // inline tags are noise in a one-line title
    var withoutTags = line.replace(/(^|\s)#[^\s#]+/g, "").trim();
    if (withoutTags !== "") line = withoutTags;
    if (line !== "") return truncateUtf8(line, MAX_TITLE_BYTES);
  }
  return "(empty)";
}

function memoSubtitle(memo) {
  return truncateUtf8(formatDate(memo.displayTime || memo.createTime),
                      MAX_SUBTITLE_BYTES);
}

/* =========================================================
 * Outgoing message queue (one message in flight at a time)
 * ========================================================= */
var outbox = [];
var outboxBusy = false;

function enqueue(payload) {
  outbox.push({ payload: payload, retries: 0 });
  pumpOutbox();
}

function pumpOutbox() {
  if (outboxBusy || outbox.length === 0) return;
  outboxBusy = true;
  var entry = outbox[0];

  Pebble.sendAppMessage(entry.payload, function () {
    outbox.shift();
    outboxBusy = false;
    pumpOutbox();
  }, function () {
    entry.retries++;
    if (entry.retries > 2) {
      console.log("Memos: Dropping message after retries");
      outbox.shift();
    }
    outboxBusy = false;
    setTimeout(pumpOutbox, 200);
  });
}

function reply(req, fields) {
  var payload = { "REQUEST_ID": req.requestId, "ACTION": req.action };
  for (var key in fields) {
    payload[key] = fields[key];
  }
  enqueue(payload);
}

function replyOk(req, fields) {
  fields = fields || {};
  fields["STATUS"] = 0;
  reply(req, fields);
}

function replyError(req, message) {
  console.log("Memos: Request failed:", message);
  reply(req, { "STATUS": 1, "TEXT": truncateUtf8(message, MAX_TITLE_BYTES) });
}

function replyList(req, items) {
  replyOk(req, { "LIST_COUNT": items.length });
  for (var i = 0; i < items.length; i++) {
    reply(req, {
      "ITEM_INDEX": i,
      "ITEM_ID": truncateUtf8(items[i].id, MAX_ID_BYTES),
      "ITEM_TITLE": items[i].title,
      "ITEM_SUBTITLE": items[i].subtitle
    });
  }
}

/* =========================================================
 * Memos API
 * ========================================================= */

// Accepts both "https://host" and the legacy "https://host/api/v1/memos"
function apiBase(url) {
  return url.replace(/\/+$/, "").replace(/\/api\/v1\/memos$/, "");
}

function apiRequest(method, path, body, callback) {
  var settings = getSettings();
  if (!settings || !settings.url || !settings.token) {
    callback("Not configured");
    return;
  }

  var xhr = new XMLHttpRequest();
  xhr.open(method, apiBase(settings.url) + "/api/v1/" + path, true);
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.setRequestHeader("Authorization", "Bearer " + settings.token);
  xhr.timeout = TIMEOUT_MS;

  xhr.onload = function () {
    if (xhr.status < 200 || xhr.status >= 300) {
      callback("Server error " + xhr.status);
      return;
    }
    var data = {};
    try {
      if (xhr.responseText) data = JSON.parse(xhr.responseText);
    } catch (err) {
      callback("Invalid response");
      return;
    }
    callback(null, data);
  };
  xhr.onerror = function () { callback("Network error"); };
  xhr.ontimeout = function () { callback("Request timed out"); };

  xhr.send(body ? JSON.stringify(body) : null);
}

// Collect memos page by page until `limit` memos pass `accept`
function fetchMemos(accept, limit, callback) {
  var result = [];

  function fetchPage(pageToken, pagesLeft) {
    var path = "memos?pageSize=" + PAGE_SIZE +
      (pageToken ? "&pageToken=" + encodeURIComponent(pageToken) : "");

    apiRequest("GET", path, null, function (err, data) {
      if (err) {
        callback(err);
        return;
      }
      var memos = data.memos || [];
      for (var i = 0; i < memos.length && result.length < limit; i++) {
        var memo = memos[i];
        // comments are memos with a parent, archived ones are not listed
        if (memo.parent) continue;
        if (memo.state === "ARCHIVED" || memo.rowStatus === "ARCHIVED") continue;
        if (accept(memo)) result.push(memo);
      }

      if (result.length < limit && data.nextPageToken && pagesLeft > 1) {
        fetchPage(data.nextPageToken, pagesLeft - 1);
      } else {
        callback(null, result);
      }
    });
  }

  fetchPage(null, MAX_PAGES);
}

function memoItems(memos) {
  return memos.map(function (memo) {
    return { id: memo.name, title: memoTitle(memo), subtitle: memoSubtitle(memo) };
  });
}

/* =========================================================
 * Request handlers
 * ========================================================= */

function handleCreate(req) {
  var settings = getSettings() || {};
  var defaultTags = settings.tags ? settings.tags.split(/\s+/) : DEFAULT_TAGS;
  var content = parseMemo(req.text || "", defaultTags);
  if (content === null) {
    replyError(req, "Memo is empty");
    return;
  }
  apiRequest("POST", "memos", { content: content }, function (err) {
    if (err) replyError(req, err);
    else replyOk(req);
  });
}

function handleListRecent(req) {
  fetchMemos(function () { return true; }, MAX_LIST_ITEMS, function (err, memos) {
    if (err) replyError(req, err);
    else replyList(req, memoItems(memos));
  });
}

function handleListByTag(req) {
  fetchMemos(function (memo) { return hasTag(memo, req.target); }, MAX_LIST_ITEMS,
    function (err, memos) {
      if (err) replyError(req, err);
      else replyList(req, memoItems(memos));
    });
}

function handleListTags(req) {
  // there is no stable tag endpoint across Memos versions, so count
  // the tags of the most recent memos instead
  fetchMemos(function () { return true; }, PAGE_SIZE * MAX_PAGES,
    function (err, memos) {
      if (err) {
        replyError(req, err);
        return;
      }
      var counts = {};
      memos.forEach(function (memo) {
        memoTags(memo).forEach(function (tag) {
          counts[tag] = (counts[tag] || 0) + 1;
        });
      });
      var tags = Object.keys(counts).sort(function (a, b) {
        return counts[b] - counts[a] || a.localeCompare(b);
      }).slice(0, MAX_TAG_ITEMS);

      replyList(req, tags.map(function (tag) {
        return {
          id: tag,
          title: truncateUtf8("#" + tag, MAX_TITLE_BYTES),
          subtitle: counts[tag] + (counts[tag] === 1 ? " memo" : " memos")
        };
      }));
    });
}

function handleGet(req) {
  apiRequest("GET", req.target, null, function (err, memo) {
    if (err) replyError(req, err);
    else replyOk(req, { "TEXT": truncateUtf8(memo.content || "", MAX_TEXT_BYTES) });
  });
}

function handleArchive(req) {
  apiRequest("PATCH", req.target + "?updateMask=state", { state: "ARCHIVED" },
    function (err) {
      if (!err) {
        replyOk(req);
        return;
      }
      // Memos versions before 0.22 call the field rowStatus
      apiRequest("PATCH", req.target + "?updateMask=row_status",
        { rowStatus: "ARCHIVED" }, function (legacyErr) {
          if (legacyErr) replyError(req, err);
          else replyOk(req);
        });
    });
}

function handleDelete(req) {
  apiRequest("DELETE", req.target, null, function (err) {
    if (err) replyError(req, err);
    else replyOk(req);
  });
}

function handleComment(req) {
  var content = applyKeywords(req.text || "").trim();
  if (content.replace(/[-#\[\]\s]/g, "") === "") {
    replyError(req, "Comment is empty");
    return;
  }
  apiRequest("POST", req.target + "/comments", { content: content }, function (err) {
    if (err) replyError(req, err);
    else replyOk(req);
  });
}

var HANDLERS = {};
HANDLERS[ACTION_CREATE]      = handleCreate;
HANDLERS[ACTION_LIST_RECENT] = handleListRecent;
HANDLERS[ACTION_LIST_TAGS]   = handleListTags;
HANDLERS[ACTION_LIST_BY_TAG] = handleListByTag;
HANDLERS[ACTION_GET]         = handleGet;
HANDLERS[ACTION_ARCHIVE]     = handleArchive;
HANDLERS[ACTION_DELETE]      = handleDelete;
HANDLERS[ACTION_COMMENT]     = handleComment;

/* =========================================================
 * AppMessage handler
 * ========================================================= */
Pebble.addEventListener("appmessage", function (e) {
  var payload = e.payload || {};
  var req = {
    requestId: payload["REQUEST_ID"],
    action: payload["ACTION"],
    target: payload["ITEM_ID"], // memo name or tag the request refers to
    text: payload["TEXT"]
  };

  var handler = HANDLERS[req.action];
  if (!handler) {
    console.log("Memos: Unknown action", req.action);
    return;
  }
  console.log("Memos: Request", req.action, req.target || "");

  try {
    handler(req);
  } catch (err) {
    console.log("Memos: Handler error", err);
    replyError(req, "Phone error");
  }
});

/* =========================================================
 * Lifecycle
 * ========================================================= */
Pebble.addEventListener("ready", function () {
  console.log("Memos: PebbleKit JS ready");
});

Pebble.addEventListener("showConfiguration", function() {
  console.log("Memos: Opening config page");
  var configHtml = require("./config");
  var current = getSettings() || {};
  Pebble.openURL("data:text/html;charset=utf-8," +
    encodeURIComponent(configHtml) + "#" +
    encodeURIComponent(JSON.stringify(current)));
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (!e.response) return;

  try {
    var settings = JSON.parse(decodeURIComponent(e.response));
    localStorage.setItem("memos_settings", JSON.stringify(settings));
    console.log("Memos: Settings saved", settings);
  } catch (err) {
    console.log("Memos: Failed to parse settings", err);
  }
});
