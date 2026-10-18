// Layout of the settings page, built by Clay (see vendor/clay.js). The
// message keys name the settings; they are read in index.js and are not
// sent to the watch as they are.
module.exports = [
  {
    "type": "heading",
    "defaultValue": "Ostrakon"
  },
  {
    "type": "text",
    "defaultValue": "Your self-hosted Memos server on your wrist."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Connection"
      },
      {
        "type": "input",
        "messageKey": "SETTING_URL",
        "label": "Memos Server URL",
        "description": "Example: https://memos.example.com",
        "attributes": {
          "placeholder": "https://memos.example.com",
          "type": "url"
        }
      },
      {
        "type": "input",
        "messageKey": "SETTING_TOKEN",
        "label": "Access Token",
        "description": "Create one in Memos under Settings, Access Tokens.",
        "attributes": {
          "placeholder": "Your access token",
          "type": "password"
        }
      },
      {
        "type": "button",
        "id": "testConnection",
        "defaultValue": "Test connection"
      },
      {
        "type": "text",
        "id": "testResult",
        "defaultValue": ""
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Memos"
      },
      {
        "type": "input",
        "messageKey": "SETTING_TAGS",
        "label": "Default Tags",
        "description": "Added to every new memo, separated by spaces.",
        "defaultValue": "#pebble",
        "attributes": {
          "placeholder": "#pebble #notes"
        }
      },
      {
        "type": "input",
        "messageKey": "SETTING_TODO_TAG",
        "label": "To-do Tag",
        "description": "The app launcher shows the open tasks of all memos with this tag.",
        "defaultValue": "#todo",
        "attributes": {
          "placeholder": "#todo"
        }
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Watch"
      },
      {
        "type": "toggle",
        "messageKey": "VIBRATE",
        "label": "Vibrate on success and errors",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];
