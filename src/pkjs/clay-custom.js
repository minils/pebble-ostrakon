// Runs inside the settings page (Clay copies this function into it, so it
// must not use anything from outside). Wires up the "Test connection"
// button. The server may not allow requests from this page (CORS); the
// watch also reports the connection status after saving.
module.exports = function () {
  var clayConfig = this;

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var result = clayConfig.getItemById('testResult');

    function show(text, color) {
      result.set('<span style="color:' + color + '">' + text + '</span>');
    }

    clayConfig.getItemById('testConnection').on('click', function () {
      var url = String(clayConfig.getItemByMessageKey('SETTING_URL').get() || '')
        .trim().replace(/\/+$/, '').replace(/\/api\/v1\/memos$/, '');
      var token = String(clayConfig.getItemByMessageKey('SETTING_TOKEN').get() || '').trim();
      if (!url || !token) {
        show('Enter the server URL and token first.', '#ff8a80');
        return;
      }

      show('Testing...', '#cccccc');
      var xhr = new XMLHttpRequest();
      xhr.open('GET', url + '/api/v1/memos?pageSize=1', true);
      xhr.setRequestHeader('Authorization', 'Bearer ' + token);
      xhr.timeout = 10000;
      xhr.onload = function () {
        if (xhr.status >= 200 && xhr.status < 300) {
          show('Connected. URL and token work.', '#69f0ae');
        } else if (xhr.status === 401 || xhr.status === 403) {
          show('The server rejected the token (' + xhr.status + ').', '#ff8a80');
        } else if (xhr.status === 404) {
          show('No Memos API found at this URL (404).', '#ff8a80');
        } else {
          show('Server error ' + xhr.status + '.', '#ff8a80');
        }
      };
      xhr.onerror = xhr.ontimeout = function () {
        show('Could not test from this page. Save, and the watch will ' +
             'show whether the connection works.', '#cccccc');
      };
      xhr.send();
    });
  });
};
