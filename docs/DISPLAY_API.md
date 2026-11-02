# Display control API

The authenticated Web Admin server exposes a small REST API for the panel's
current display. It uses the same HTTP Basic Authentication credentials as
Web Admin. Send and receive JSON with UTF-8 encoding. Requests that change the
display must be sent while a firmware update is not in progress.

## Read current display state

```http
GET /api/display
```

Returns the active tab, all enabled tabs, and any active popup message:

```json
{
  "ok": true,
  "active_tab": { "id": "overview", "title": "Overview" },
  "tabs": [{ "id": "overview", "title": "Overview" }],
  "message": {
    "active": false,
    "text": "",
    "dismissible": false,
    "remaining_ms": 0
  }
}
```

`remaining_ms` is zero for an inactive or manually dismissed message. For a
timed message it counts down to zero. A manual message has
`dismissible: true`.

## Select a tab

```http
PUT /api/display/tab
Content-Type: application/json

{"id":"media"}
```

The request is idempotent: selecting the active tab again leaves the same tab
active. `id` must match an enabled module ID such as `overview`, `room`,
`media`, `weather`, `calendar`, `climate`, `security`, or `settings`. The
response is the current display state.

## Show or replace a popup message

```http
PUT /api/display/message
Content-Type: application/json

{"text":"Dinner is ready","duration_ms":30000}
```

`text` must contain 1–160 UTF-8 bytes. `duration_ms` is `0` for manual
dismissal, or from `1000` through `86400000` for an automatic timeout. The
message appears over the current panel screen. A person can dismiss a manual
message using its on-screen button.

## Dismiss a popup message

```http
DELETE /api/display/message
```

Deleting an inactive message also succeeds. Both message operations return the
current display state.

## Errors

Errors use an appropriate HTTP status (`400` for malformed JSON, `404` for an
unknown or disabled tab, `413` for a body over 512 bytes, `422` for invalid
fields, and `409` while an OTA update is active) and a JSON body:

```json
{"ok":false,"error":"The message explains the problem."}
```

The browser-based Web Admin uses these same endpoints in its **Live display**
section.
