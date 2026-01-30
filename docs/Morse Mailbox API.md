# Morse Mailbox External API

**Version:** 1.0
**Base URL:** `https://us-central1-morse-mailbox.cloudfunctions.net/api/v1`

This API allows external devices and services to interact with Morse Mailbox accounts to send and receive morse code messages.

---

## Table of Contents

1. [Authentication](#authentication)
2. [Device Registration](#device-registration)
3. [Messages](#messages)
4. [Users](#users)
5. [Device Management](#device-management)
6. [Data Types](#data-types)
7. [Error Handling](#error-handling)
8. [Rate Limits](#rate-limits)

---

## Authentication

The API uses a device code flow for initial authentication, then Firebase ID tokens for subsequent requests.

### Authentication Flow

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Your Device    │     │  Morse Mailbox  │     │  User (Browser) │
│  (ESP32, etc)   │     │  Cloud API      │     │                 │
└────────┬────────┘     └────────┬────────┘     └────────┬────────┘
         │                       │                       │
         │ 1. POST /device/requestCode                   │
         │ ─────────────────────>│                       │
         │                       │                       │
         │ { code: "ABC123" }    │                       │
         │ <─────────────────────│                       │
         │                       │                       │
         │   [Display code]      │                       │
         │                       │                       │
         │                       │ 2. User visits URL    │
         │                       │    logs in, enters    │
         │                       │    code "ABC123"      │
         │                       │<──────────────────────│
         │                       │                       │
         │ 3. GET /device/checkCode?code=ABC123          │
         │ ─────────────────────>│                       │
         │                       │                       │
         │ { custom_token: "..." }                       │
         │ <─────────────────────│                       │
         │                       │                       │
         │ 4. Exchange custom token for ID token         │
         │    (Firebase Auth REST API)                   │
         │                       │                       │
         │ 5. Use ID token for all API calls             │
         │ ─────────────────────>│                       │
```

### Token Exchange

After receiving a `custom_token`, exchange it for an ID token using Firebase Auth REST API:

```http
POST https://identitytoolkit.googleapis.com/v1/accounts:signInWithCustomToken?key={FIREBASE_API_KEY}
Content-Type: application/json

{
  "token": "<custom_token>",
  "returnSecureToken": true
}
```

**Response:**
```json
{
  "idToken": "<id_token>",
  "refreshToken": "<refresh_token>",
  "expiresIn": "3600"
}
```

### Using the ID Token

Include the ID token in the `Authorization` header for all authenticated requests:

```http
Authorization: Bearer <id_token>
```

### Token Refresh

ID tokens expire after 1 hour. Refresh using:

```http
POST https://securetoken.googleapis.com/v1/token?key={FIREBASE_API_KEY}
Content-Type: application/x-www-form-urlencoded

grant_type=refresh_token&refresh_token=<refresh_token>
```

---

## Device Registration

### Request Device Code

Request a 6-character code to begin device linking.

```http
POST /device/requestCode
Content-Type: application/json
```

**Request Body:**
```json
{
  "device_name": "Shack ESP32",
  "device_type": "esp32",
  "firmware_version": "1.0.0"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `device_name` | string | Yes | Human-readable device name |
| `device_type` | string | Yes | Device type identifier (e.g., "esp32", "vail_summit") |
| `firmware_version` | string | No | Device firmware version |

**Response: `200 OK`**
```json
{
  "code": "ABC123",
  "link_url": "https://morsemailbox.com/link-device?code=ABC123",
  "expires_in": 900,
  "poll_interval": 5
}
```

| Field | Type | Description |
|-------|------|-------------|
| `code` | string | 6-character alphanumeric code to display to user |
| `link_url` | string | URL user should visit to complete linking |
| `expires_in` | number | Seconds until code expires (default: 900 = 15 min) |
| `poll_interval` | number | Recommended seconds between checkCode polls |

---

### Check Device Code Status

Poll this endpoint to check if the user has completed linking.

```http
GET /device/checkCode?code={code}
```

**Query Parameters:**
| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `code` | string | Yes | The 6-character code from requestCode |

**Response (pending): `200 OK`**
```json
{
  "status": "pending",
  "expires_in": 845
}
```

**Response (linked): `200 OK`**
```json
{
  "status": "linked",
  "custom_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
  "device_id": "device_abc123xyz",
  "user": {
    "user_id": "firebase_uid_here",
    "callsign": "W1ABC",
    "morse_mailbox_id": "MM-00042"
  }
}
```

**Response (expired): `410 Gone`**
```json
{
  "error": "code_expired",
  "message": "Device code has expired. Request a new code."
}
```

---

### Refresh Device Token

Refresh an expired custom token. Use when API calls return `401 Unauthorized`.

```http
POST /device/refreshToken
Content-Type: application/json
X-Device-ID: {device_id}
Authorization: Bearer <expired_or_valid_id_token>
```

**Request Body:**
```json
{
  "device_id": "device_abc123xyz"
}
```

**Response: `200 OK`**
```json
{
  "custom_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_in": 3600
}
```

---

## Messages

### Send Message

Send a morse code message with raw timing data.

```http
POST /messages/send
Content-Type: application/json
Authorization: Bearer <id_token>
```

**Request Body:**
```json
{
  "recipient": "W1ABC",
  "morse_timing": [
    { "timestamp": 0, "type": "keydown" },
    { "timestamp": 120, "type": "keyup" },
    { "timestamp": 240, "type": "keydown" },
    { "timestamp": 360, "type": "keyup" },
    { "timestamp": 600, "type": "keydown" },
    { "timestamp": 960, "type": "keyup" }
  ],
  "device_id": "device_abc123xyz"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `recipient` | string | Yes | Recipient's callsign OR Morse Mailbox ID (MM-XXXXX) |
| `morse_timing` | array | Yes | Array of timing events (see [MorseTimingEvent](#morsetimingevent)) |
| `device_id` | string | No | Device identifier for tracking |

**Validation Rules:**
- `morse_timing` must have at least 2 events
- `morse_timing` must have at most 10,000 events
- Maximum duration: 300,000ms (5 minutes)
- Timestamps must be monotonically increasing (each >= previous)
- Events should alternate between `keydown` and `keyup`
- Cannot send to yourself

**Response: `201 Created`**
```json
{
  "success": true,
  "message_id": "msg_xyz789abc",
  "recipient": {
    "user_id": "recipient_uid",
    "callsign": "W1ABC",
    "morse_mailbox_id": "MM-00042"
  }
}
```

---

### Get Inbox

Retrieve inbox messages (messages received by the authenticated user).

```http
GET /messages/inbox
Authorization: Bearer <id_token>
```

**Query Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `limit` | number | 20 | Max messages to return (1-50) |
| `after_id` | string | - | Cursor for pagination (message ID) |
| `status` | string | "all" | Filter: "unread", "read", "archived", "all" |

**Response: `200 OK`**
```json
{
  "messages": [
    {
      "id": "msg_xyz789",
      "sender": {
        "user_id": "sender_uid",
        "callsign": "W1XYZ",
        "morse_mailbox_id": "MM-00015"
      },
      "status": "unread",
      "sent_at": "2026-01-29T10:30:00.000Z",
      "duration_ms": 4500,
      "event_count": 42
    },
    {
      "id": "msg_abc123",
      "sender": {
        "user_id": "sender_uid2",
        "callsign": "K2DEF",
        "morse_mailbox_id": "MM-00089"
      },
      "status": "read",
      "sent_at": "2026-01-28T15:20:00.000Z",
      "read_at": "2026-01-28T16:00:00.000Z",
      "duration_ms": 3200,
      "event_count": 28
    }
  ],
  "has_more": true,
  "next_cursor": "msg_abc123"
}
```

**Note:** `morse_timing` is NOT included in inbox responses to minimize payload size. Use [Get Message](#get-message) to fetch full timing data.

---

### Get Sent Messages

Retrieve messages sent by the authenticated user.

```http
GET /messages/sent
Authorization: Bearer <id_token>
```

**Query Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `limit` | number | 20 | Max messages to return (1-50) |
| `after_id` | string | - | Cursor for pagination |

**Response: `200 OK`**
```json
{
  "messages": [
    {
      "id": "msg_sent123",
      "recipient": {
        "user_id": "recipient_uid",
        "callsign": "W1ABC",
        "morse_mailbox_id": "MM-00042"
      },
      "status": "read",
      "sent_at": "2026-01-29T09:00:00.000Z",
      "read_at": "2026-01-29T09:15:00.000Z",
      "duration_ms": 2800,
      "event_count": 24
    }
  ],
  "has_more": false,
  "next_cursor": null
}
```

---

### Get Message

Retrieve a specific message including full timing data.

```http
GET /messages/{messageId}
Authorization: Bearer <id_token>
```

**Path Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `messageId` | string | The message ID |

**Response: `200 OK`**
```json
{
  "id": "msg_xyz789",
  "sender": {
    "user_id": "sender_uid",
    "callsign": "W1XYZ",
    "morse_mailbox_id": "MM-00015"
  },
  "recipient": {
    "user_id": "recipient_uid",
    "callsign": "W1ABC",
    "morse_mailbox_id": "MM-00042"
  },
  "morse_timing": [
    { "timestamp": 0, "type": "keydown" },
    { "timestamp": 120, "type": "keyup" },
    { "timestamp": 240, "type": "keydown" },
    { "timestamp": 360, "type": "keyup" }
  ],
  "status": "unread",
  "playback_count": 0,
  "playback_speed": 1.0,
  "private_notes": null,
  "transcription": null,
  "sent_at": "2026-01-29T10:30:00.000Z",
  "read_at": null,
  "archived_at": null
}
```

**Authorization:** User must be sender or recipient.

---

### Update Message

Update message status, playback count, notes, or transcription.

```http
PATCH /messages/{messageId}
Content-Type: application/json
Authorization: Bearer <id_token>
```

**Path Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `messageId` | string | The message ID |

**Request Body (all fields optional):**
```json
{
  "status": "read",
  "playback_count": 3,
  "playback_speed": 0.75,
  "private_notes": "Practice this one more",
  "transcription": "CQ CQ DE W1XYZ"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | "read" or "archived" |
| `playback_count` | number | New playback count value |
| `playback_speed` | number | Last used playback speed (0.5-2.0) |
| `private_notes` | string | Personal notes (recipient only) |
| `transcription` | string | User's morse transcription (recipient only) |

**Authorization:** Only the recipient can update messages.

**Response: `200 OK`**
```json
{
  "success": true,
  "message_id": "msg_xyz789"
}
```

---

### Get Thread

Retrieve conversation thread between authenticated user and another user.

```http
GET /messages/thread/{userId}
Authorization: Bearer <id_token>
```

**Path Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `userId` | string | The other user's UID, callsign, or MM-ID |

**Query Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `limit` | number | 50 | Max messages to return |
| `before_id` | string | - | Cursor for pagination (older messages) |

**Response: `200 OK`**
```json
{
  "other_user": {
    "user_id": "other_uid",
    "callsign": "W1XYZ",
    "morse_mailbox_id": "MM-00015"
  },
  "friendship": {
    "tier": 3,
    "tier_name": "Good Friend",
    "messages_sent": 12,
    "messages_received": 15,
    "messages_exchanged": 12
  },
  "messages": [
    {
      "id": "msg_001",
      "direction": "received",
      "status": "read",
      "sent_at": "2026-01-28T09:00:00.000Z",
      "duration_ms": 3200,
      "event_count": 28
    },
    {
      "id": "msg_002",
      "direction": "sent",
      "status": "read",
      "sent_at": "2026-01-28T09:15:00.000Z",
      "duration_ms": 2800,
      "event_count": 24
    }
  ],
  "has_more": false
}
```

---

## Users

### Search Users

Search for users by callsign or Morse Mailbox ID.

```http
GET /users/search
Authorization: Bearer <id_token>
```

**Query Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `q` | string | (required) | Search query (min 2 characters) |
| `limit` | number | 10 | Max results (1-20) |

**Response: `200 OK`**
```json
{
  "users": [
    {
      "user_id": "uid123",
      "callsign": "W1ABC",
      "morse_mailbox_id": "MM-00042",
      "location": "California",
      "friendship_tier": 2
    },
    {
      "user_id": "uid456",
      "callsign": "W1AXY",
      "morse_mailbox_id": "MM-00089",
      "location": null,
      "friendship_tier": null
    }
  ]
}
```

**Note:** `friendship_tier` is included if the authenticated user has an existing friendship with that user.

---

### Get User Profile

Get a user's public profile.

```http
GET /users/{identifier}
Authorization: Bearer <id_token>
```

**Path Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `identifier` | string | User's UID, callsign, or Morse Mailbox ID |

**Examples:**
```
GET /users/W1ABC
GET /users/MM-00042
GET /users/firebase_uid_here
```

**Response: `200 OK`**
```json
{
  "user_id": "uid123",
  "callsign": "W1ABC",
  "morse_mailbox_id": "MM-00042",
  "bio": "CW enthusiast from California. Always happy to QSO!",
  "location": "California",
  "timezone": "America/Los_Angeles",
  "created_at": "2026-01-15T00:00:00.000Z",
  "friendship": {
    "tier": 2,
    "tier_name": "Friend",
    "messages_exchanged": 7
  }
}
```

**Note:** `friendship` is only included if the authenticated user has an existing friendship.

---

## Device Management

These endpoints are for managing linked devices from the web application.

### List Devices

List all devices linked to the authenticated user's account.

```http
GET /devices
Authorization: Bearer <id_token>
```

**Response: `200 OK`**
```json
{
  "devices": [
    {
      "device_id": "device_abc123",
      "device_name": "Shack ESP32",
      "device_type": "esp32",
      "firmware_version": "1.0.0",
      "created_at": "2026-01-20T00:00:00.000Z",
      "last_active_at": "2026-01-29T10:00:00.000Z",
      "revoked": false
    },
    {
      "device_id": "device_xyz789",
      "device_name": "Vail Summit",
      "device_type": "vail_summit",
      "firmware_version": "2.1.0",
      "created_at": "2026-01-25T00:00:00.000Z",
      "last_active_at": "2026-01-29T08:30:00.000Z",
      "revoked": false
    }
  ]
}
```

---

### Update Device

Update a device's name.

```http
PATCH /devices/{deviceId}
Content-Type: application/json
Authorization: Bearer <id_token>
```

**Request Body:**
```json
{
  "device_name": "New Device Name"
}
```

**Response: `200 OK`**
```json
{
  "success": true,
  "device_id": "device_abc123"
}
```

---

### Revoke Device

Revoke a device's access. The device will no longer be able to make API calls.

```http
DELETE /devices/{deviceId}
Authorization: Bearer <id_token>
```

**Response: `200 OK`**
```json
{
  "success": true,
  "message": "Device revoked"
}
```

---

## Data Types

### MorseTimingEvent

Represents a single key state change event.

```typescript
interface MorseTimingEvent {
  timestamp: number;  // Milliseconds since recording started (0 = first event)
  type: "keydown" | "keyup";
}
```

**Example:** A simple "E" (dit) followed by "T" (dah):
```json
[
  { "timestamp": 0, "type": "keydown" },
  { "timestamp": 80, "type": "keyup" },
  { "timestamp": 200, "type": "keydown" },
  { "timestamp": 440, "type": "keyup" }
]
```

**Timing Guidelines:**
- Standard dit length at 20 WPM ≈ 60ms
- Dah = 3 × dit
- Inter-element space = 1 dit
- Inter-character space = 3 dits
- Inter-word space = 7 dits

---

### Message Status

| Status | Description |
|--------|-------------|
| `unread` | Message has not been opened by recipient |
| `read` | Message has been opened/played |
| `archived` | Message has been archived by recipient |

---

### Friendship Tiers

| Tier | Name | Required Exchanges |
|------|------|--------------------|
| 1 | Acquaintance | 1-4 |
| 2 | Friend | 5-9 |
| 3 | Good Friend | 10-19 |
| 4 | Close Friend | 20-49 |
| 5 | Best Friend | 50+ |

**Note:** "Exchanges" = minimum of messages sent and received between two users.

---

## Error Handling

### Error Response Format

All errors return a consistent JSON structure:

```json
{
  "error": "error_code",
  "message": "Human-readable error description",
  "details": {}
}
```

### HTTP Status Codes

| Code | Description |
|------|-------------|
| `200` | Success |
| `201` | Created (new resource) |
| `400` | Bad Request (invalid input) |
| `401` | Unauthorized (invalid/expired token) |
| `403` | Forbidden (valid token but no permission) |
| `404` | Not Found |
| `410` | Gone (expired resource, e.g., device code) |
| `429` | Too Many Requests (rate limited) |
| `500` | Internal Server Error |

### Common Error Codes

| Error Code | HTTP Status | Description |
|------------|-------------|-------------|
| `invalid_token` | 401 | Token is invalid or expired |
| `device_revoked` | 403 | Device has been revoked |
| `code_expired` | 410 | Device linking code has expired |
| `code_not_found` | 404 | Device linking code not found |
| `user_not_found` | 404 | Recipient user not found |
| `message_not_found` | 404 | Message not found |
| `invalid_timing` | 400 | Invalid morse_timing data |
| `rate_limited` | 429 | Too many requests |
| `cannot_send_to_self` | 400 | Cannot send message to yourself |

---

## Rate Limits

| Endpoint | Limit | Window |
|----------|-------|--------|
| `POST /device/requestCode` | 5 | 1 hour per IP |
| `GET /device/checkCode` | 100 | 1 hour per IP |
| `POST /messages/send` | 60 | 1 hour per device |
| `GET /messages/*` | 120 | 1 hour per device |
| `GET /users/*` | 60 | 1 hour per device |

**Rate Limit Headers:**
```http
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 45
X-RateLimit-Reset: 1706531400
```

---

## Example: ESP32 Integration

### Minimal Implementation Flow

```cpp
// 1. Request device code
POST /device/requestCode
{ "device_name": "My ESP32", "device_type": "esp32" }
// Response: { "code": "ABC123", ... }

// 2. Display code to user, poll for completion
GET /device/checkCode?code=ABC123
// Poll every 5 seconds until status == "linked"

// 3. Store credentials
// Save: custom_token, device_id, user info

// 4. Exchange custom token for ID token
POST https://identitytoolkit.googleapis.com/v1/accounts:signInWithCustomToken
// Save: id_token, refresh_token

// 5. Make API calls
GET /messages/inbox
Authorization: Bearer <id_token>

// 6. When token expires (401), refresh and retry
POST https://securetoken.googleapis.com/v1/token
// Get new id_token
```

### Sending a Message from ESP32

```cpp
// After recording morse timing data...
POST /messages/send
Authorization: Bearer <id_token>
Content-Type: application/json

{
  "recipient": "W1ABC",
  "morse_timing": [
    { "timestamp": 0, "type": "keydown" },
    { "timestamp": 80, "type": "keyup" },
    // ... more events
  ],
  "device_id": "device_abc123"
}
```

---

## Changelog

### v1.0 (2026-01-29)
- Initial API specification
- Device code authentication flow
- Message send/receive endpoints
- User search and profile endpoints
- Device management endpoints
