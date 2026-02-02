# Vail CW School Integration Spec
## For Summit Device Linking & Progress Sync

**Document Version:** 1.1
**Date:** February 2026
**Audience:** Vail CW School Web Development Team

**Revision History:**
- 1.1: Updated to match Summit device implementation (checkCode uses GET, added device-side storage schema, HTTP headers, token exchange details)
- 1.0: Initial specification

---

## Executive Summary

This document specifies the backend services needed in Vail CW School to support integration with Vail Summit hardware devices. The integration enables:

1. **Device Linking** - Connect a Summit device to a user's CW School account
2. **Progress Sync** - Bidirectional synchronization of training progress
3. **Practice Time Reporting** - Track practice time from device sessions

The Summit device team will implement the device-side client. Your team needs to implement:
- 5 Firebase Cloud Functions (APIs)
- Firestore collections for device and progress data
- Web UI for device management

---

## 0. Configuration Requirements

The Summit device firmware requires the following configuration values from the CW School team:

| Value | Purpose | Example |
|-------|---------|---------|
| `CWSCHOOL_FUNCTIONS_BASE` | Cloud Functions base URL | `https://us-central1-vail-cw-school.cloudfunctions.net` |
| `CWSCHOOL_FIREBASE_API_KEY` | Firebase Web API Key | `AIzaSy...` |

These are configured in `src/network/cwschool_link.h` on the device side.

---

## 1. Device Linking Flow

### Overview

Users link their Summit device to their CW School account using a short alphanumeric code. This avoids requiring users to type passwords on a small device keyboard.

### Sequence Diagram

```
┌─────────────┐         ┌──────────────────┐         ┌─────────────────┐
│   Summit    │         │  Cloud Functions │         │   Web App       │
│   Device    │         │  (Firebase)      │         │   (Browser)     │
└──────┬──────┘         └────────┬─────────┘         └────────┬────────┘
       │                         │                            │
       │ 1. POST /requestCode    │                            │
       │ ───────────────────────>│                            │
       │                         │                            │
       │ {code: "ABC123",        │                            │
       │  expires_in: 600}       │                            │
       │ <───────────────────────│                            │
       │                         │                            │
       │                         │    2. User visits          │
       │                         │    vail.school/link-device │
       │                         │ <──────────────────────────│
       │                         │                            │
       │                         │    3. User enters code     │
       │                         │    POST /linkDevice        │
       │                         │ <──────────────────────────│
       │                         │                            │
       │                         │──┐                         │
       │                         │  │ 4. Generate custom      │
       │                         │  │    token for device     │
       │                         │<─┘                         │
       │                         │                            │
       │                         │    {success: true}         │
       │                         │ ──────────────────────────>│
       │                         │                            │
       │ 5. GET /checkCode       │                            │
       │    (polling every 5s)   │                            │
       │ ───────────────────────>│                            │
       │                         │                            │
       │ {status: "linked",      │                            │
       │  custom_token: "..."}   │                            │
       │ <───────────────────────│                            │
       │                         │                            │
       │ 6. Exchange custom      │                            │
       │    token for ID token   │                            │
       │ ───────────────────────>│ Firebase Auth              │
       │ <───────────────────────│                            │
       │                         │                            │
       ▼                         ▼                            ▼
   Device stores              Link complete              Shows success
   tokens locally                                        message
```

### Link Code Requirements

- **Format:** 6 alphanumeric characters (A-Z, 0-9, excluding confusables like 0/O, 1/I/L)
- **Valid characters:** `ABCDEFGHJKMNPQRSTUVWXYZ23456789` (32 chars)
- **Expiry:** 10 minutes from generation
- **Single use:** Code invalidated after successful link

---

## 2. Required Cloud Functions

### 2.1 `api_summit_requestCode`

**Type:** HTTPS REST endpoint (POST)
**Auth:** None required (device not yet linked)

**Request:**
```
POST /api_summit_requestCode
Content-Type: application/json

{
  "device_name": "VAIL Summit",
  "device_type": "vail_summit",
  "firmware_version": "0.41",
  "device_id": "ESP32-AABBCCDD1122"  // Optional on first link, included on re-link
}
```

Note: The `device_id` field is optional on first link (server generates new ID) but should be included when re-linking an existing device.

**Response (Success - 200):**
```json
{
  "code": "ABC123",
  "link_url": "https://vail.school/link-device",
  "expires_in": 600,
  "device_id": "ESP32-AABBCCDD1122"
}
```

**Response (Error - 429):**
```json
{
  "error": "rate_limited",
  "message": "Too many requests. Try again in 60 seconds.",
  "retry_after": 60
}
```

**Backend Logic:**
1. Generate cryptographically random 6-char code
2. Check for collision with existing active codes
3. Store in Firestore `deviceCodes/{code}`:
   ```json
   {
     "code": "ABC123",
     "device_id": "ESP32-AABBCCDD1122",
     "device_type": "vail_summit",
     "device_name": "VAIL Summit",
     "firmware_version": "0.41",
     "status": "pending",
     "created_at": Timestamp,
     "expires_at": Timestamp (created + 10 min)
   }
   ```
4. Return code and metadata

**Rate Limiting:** Max 5 requests per device_id per hour

---

### 2.2 `api_summit_checkCode`

**Type:** HTTPS REST endpoint (GET)
**Auth:** None required

**Request:** GET with query parameters
```
GET /api_summit_checkCode?code=ABC123&device_id=ESP32-AABBCCDD1122
```

**Response (Pending - 200):**
```json
{
  "status": "pending"
}
```

**Response (Linked - 200):**
```json
{
  "status": "linked",
  "custom_token": "eyJhbGciOiJSUzI1NiIs...",
  "device_id": "ESP32-AABBCCDD1122",
  "user": {
    "uid": "firebase-auth-uid",
    "callsign": "W1ABC",
    "display_name": "John Smith"
  }
}
```

**Response (Expired - 410):**
```json
{
  "status": "expired",
  "error": "Code has expired"
}
```

**Response (Invalid - 404):**
```json
{
  "status": "invalid",
  "error": "Code not found"
}
```

**Backend Logic:**
1. Parse `code` and `device_id` from query parameters
2. Look up code in Firestore
3. Verify device_id matches (prevents code theft)
4. Check expiry time
5. If status is "linked", return custom_token, device_id, and user info
6. If status is "pending" and not expired, return pending
7. If expired, delete document and return expired status

**Polling:** Device will call this every 5 seconds for up to 10 minutes

---

### 2.3 `api_summit_linkDevice`

**Type:** HTTPS Callable
**Auth:** Required (user must be logged in)

**Request:**
```json
{
  "code": "ABC123"
}
```

**Response (Success - 200):**
```json
{
  "success": true,
  "device_id": "ESP32-AABBCCDD1122",
  "device_name": "VAIL Summit"
}
```

**Response (Invalid Code - 400):**
```json
{
  "success": false,
  "error": "invalid_code",
  "message": "Invalid or expired code"
}
```

**Response (Already Linked - 409):**
```json
{
  "success": false,
  "error": "already_linked",
  "message": "This device is already linked to another account"
}
```

**Backend Logic:**
1. Verify user is authenticated
2. Look up code, verify status is "pending" and not expired
3. Generate Firebase custom token: `admin.auth().createCustomToken(uid, { device_id, device_type })`
4. Update Firestore `deviceCodes/{code}`:
   ```json
   {
     "status": "linked",
     "linked_at": Timestamp,
     "user_uid": "auth-uid",
     "custom_token": "generated-token"
   }
   ```
5. Add device to user's device list (see Firestore Schema below)
6. Return success

---

### 2.4 `api_summit_listDevices`

**Type:** HTTPS Callable
**Auth:** Required

**Request:** None (uses auth context)

**Response (200):**
```json
{
  "devices": [
    {
      "device_id": "ESP32-AABBCCDD1122",
      "device_name": "VAIL Summit",
      "device_type": "vail_summit",
      "firmware_version": "0.41",
      "linked_at": "2026-01-30T12:00:00Z",
      "last_sync_at": "2026-02-01T08:30:00Z"
    }
  ]
}
```

---

### 2.5 `api_summit_unlinkDevice`

**Type:** HTTPS Callable
**Auth:** Required

**Request:**
```json
{
  "device_id": "ESP32-AABBCCDD1122"
}
```

**Response (Success - 200):**
```json
{
  "success": true
}
```

**Backend Logic:**
1. Verify user owns this device
2. Delete from `users/{uid}/devices/{device_id}`
3. Optionally: revoke any active refresh tokens for this device

---

## 3. Progress Sync APIs

### 3.1 `api_summit_syncProgress`

**Type:** HTTPS Callable
**Auth:** Required (device uses ID token from custom token exchange)

This is the main bidirectional sync endpoint. Device sends its progress, server merges with cloud data, returns merged result.

**Request:**
```json
{
  "v": 1,
  "device_id": "ESP32-AABBCCDD1122",
  "last_sync_time": 1706900000000,
  "session": {
    "start_time": 1706898200000,
    "duration_sec": 1800,
    "mode": "vail_course"
  },
  "vail_course": {
    "current_module": "letters-3",
    "current_lesson": 2,
    "completed_lessons": ["letters-1-1", "letters-1-2", "letters-2-1"],
    "char_mastery": {
      "E": { "mastery": 850, "attempts": 120, "correct": 108, "avg_ttr_ms": 450 },
      "T": { "mastery": 780, "attempts": 95, "correct": 82, "avg_ttr_ms": 520 },
      "A": { "mastery": 620, "attempts": 45, "correct": 35, "avg_ttr_ms": 680 }
    },
    "achievements": 7
  },
  "practice_time": {
    "today_sec": 1800,
    "total_sec": 72000,
    "history": {
      "2026-02-01": 1800,
      "2026-01-31": 2400
    }
  }
}
```

**Response (200):**
```json
{
  "sync_time": 1706900500000,
  "conflict_resolution": "merged",
  "merged_progress": {
    "vail_course": {
      "current_module": "letters-3",
      "current_lesson": 2,
      "completed_lessons": ["letters-1-1", "letters-1-2", "letters-2-1", "letters-2-2"],
      "module_progress": {
        "letters-1": { "unlocked": true, "lessons_completed": 2, "total_lessons": 2 },
        "letters-2": { "unlocked": true, "lessons_completed": 2, "total_lessons": 3 },
        "letters-3": { "unlocked": true, "lessons_completed": 1, "total_lessons": 3 }
      },
      "char_mastery": {
        "E": { "mastery": 920, "attempts": 180, "correct": 165 },
        "T": { "mastery": 850, "attempts": 140, "correct": 125 },
        "A": { "mastery": 720, "attempts": 85, "correct": 68 }
      }
    },
    "stats": {
      "total_practice_sec": 95000,
      "today_practice_sec": 3600,
      "current_streak": 5,
      "longest_streak": 14,
      "total_xp": 4520
    }
  },
  "xp_earned": 85,
  "new_achievements": ["STREAK_7"]
}
```

**Merge Rules:**
| Field | Resolution Strategy |
|-------|---------------------|
| `completed_lessons` | Union (completed on either = completed) |
| `char_mastery.mastery` | Take maximum value |
| `char_mastery.attempts/correct` | Sum deltas since last sync |
| `practice_time.total_sec` | Add session duration |
| `practice_time.history` | Merge maps, sum overlapping dates |
| `achievements` | Bitwise OR |
| `current_module/lesson` | Take furthest progression |

---

### 3.2 `api_summit_getProgress`

**Type:** HTTPS Callable
**Auth:** Required

Read-only endpoint for initial sync when device has no local data.

**Request:**
```json
{
  "device_id": "ESP32-AABBCCDD1122"
}
```

**Response:** Same format as `merged_progress` above

---

## 4. Device-Side Storage Schema

The Summit device stores CW School credentials in ESP32 NVS (flash) under the `cwschool` namespace:

| Key | Type | Description |
|-----|------|-------------|
| `linked` | bool | Whether device is linked to an account |
| `device_id` | string | Unique device identifier |
| `id_token` | string | Firebase ID token (for authenticated API calls) |
| `refresh_tkn` | string | Firebase refresh token |
| `token_exp` | ulong | Token expiry time (millis timestamp) |
| `uid` | string | Firebase Auth UID of linked user |
| `callsign` | string | User's callsign (if set) |
| `display` | string | User's display name |

---

## 5. Firestore Schema

### Collection: `deviceCodes`

Temporary storage for active link codes. Clean up expired entries periodically.

```
deviceCodes/{code}
├── code: string                    "ABC123"
├── device_id: string               "ESP32-AABBCCDD1122"
├── device_type: string             "vail_summit"
├── device_name: string             "VAIL Summit"
├── firmware_version: string        "0.41"
├── status: string                  "pending" | "linked" | "expired"
├── created_at: timestamp
├── expires_at: timestamp
├── linked_at: timestamp | null
├── user_uid: string | null
└── custom_token: string | null     (only set when linked)
```

**Indexes:**
- `status` + `expires_at` (for cleanup query)

**TTL:** Set Firestore TTL policy to auto-delete documents 24 hours after `expires_at`

---

### Collection: `users/{uid}/devices`

Track which devices are linked to each user.

```
users/{uid}/devices/{device_id}
├── device_id: string               "ESP32-AABBCCDD1122"
├── device_type: string             "vail_summit"
├── device_name: string             "VAIL Summit"
├── firmware_version: string        "0.41"
├── linked_at: timestamp
├── last_sync_at: timestamp
└── last_seen_at: timestamp
```

---

### Collection: `users/{uid}/progress`

Extend existing progress document to include device sync metadata.

```
users/{uid}/progress/current
├── ... (existing progress fields) ...
│
├── device_sync: {
│   ├── last_sync_time: number (timestamp ms)
│   ├── last_device_id: string
│   └── pending_merge: boolean
│ }
│
└── vail_course: {                  (NEW - matches web course structure)
    ├── current_module: string
    ├── current_lesson: number
    ├── completed_lessons: string[]
    ├── module_progress: {
    │   [module_id]: {
    │     unlocked: boolean
    │     lessons_completed: number
    │   }
    │ }
    └── char_mastery: {
        [char]: {
          mastery: number (0-1000)
          attempts: number
          correct: number
          avg_ttr_ms: number
          confused_with: { [char]: count }
        }
      }
    }
```

---

## 6. Security Rules

```javascript
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {

    // Device codes - public read for checking, no direct writes
    match /deviceCodes/{code} {
      allow read: if true;
      allow write: if false;  // Only via Cloud Functions
    }

    // User devices - only owner can read
    match /users/{userId}/devices/{deviceId} {
      allow read: if request.auth != null && request.auth.uid == userId;
      allow write: if false;  // Only via Cloud Functions
    }

    // User progress - only owner can read, write via functions
    match /users/{userId}/progress/{doc} {
      allow read: if request.auth != null && request.auth.uid == userId;
      allow write: if false;  // Only via Cloud Functions
    }
  }
}
```

---

## 7. Web UI Requirements

### 6.1 Device Link Page (`/link-device`)

**Route:** `https://vail.school/link-device`

**UI Flow:**
1. If not logged in → redirect to login with return URL
2. Show text input for 6-character code
3. On submit → call `api_summit_linkDevice`
4. Show success message with device name
5. Optionally redirect to device management

**Mockup:**
```
┌─────────────────────────────────────────────┐
│           Link Your VAIL Summit             │
│                                             │
│  Enter the code displayed on your device:   │
│                                             │
│  ┌─────────────────────────────────┐        │
│  │  A B C 1 2 3                    │        │
│  └─────────────────────────────────┘        │
│                                             │
│           [ Link Device ]                   │
│                                             │
│  Code expires in 8:42                       │
└─────────────────────────────────────────────┘
```

### 6.2 Device Management (in Account Settings)

**Location:** Account Settings → Linked Devices

**Features:**
- List all linked devices with name, type, last seen
- Unlink button per device (with confirmation)
- Show sync status (last sync time)

**Mockup:**
```
┌─────────────────────────────────────────────┐
│  Linked Devices                             │
├─────────────────────────────────────────────┤
│  ┌─────────────────────────────────────┐    │
│  │ 📟 VAIL Summit                      │    │
│  │    Firmware: v0.41                  │    │
│  │    Last synced: 2 hours ago         │    │
│  │    Linked: Jan 30, 2026             │    │
│  │                        [ Unlink ]   │    │
│  └─────────────────────────────────────┘    │
│                                             │
│  [ + Link New Device ]                      │
└─────────────────────────────────────────────┘
```

---

## 8. XP & Achievement Integration

### XP Awards from Device Activity

| Activity | XP |
|----------|-----|
| Correct character (first 100/day) | 1 |
| Complete lesson | 50 |
| Complete module | 200 |
| Daily practice (any duration) | 10 |
| Streak milestone (7 days) | 100 |
| Streak milestone (30 days) | 500 |
| First device link | 50 |

### Device-Specific Achievements

| Achievement ID | Criteria |
|----------------|----------|
| `SUMMIT_LINKED` | First device linked |
| `SUMMIT_FIRST_LESSON` | Complete first lesson on device |
| `SUMMIT_STREAK_7` | 7-day streak with device practice |
| `SUMMIT_PRACTICE_1HR` | 1 hour total practice on device |
| `SUMMIT_PRACTICE_10HR` | 10 hours total practice on device |

---

## 9. HTTP Headers for Authenticated Requests

For all authenticated API calls, the device includes these headers:

```
Authorization: Bearer <firebase_id_token>
X-Device-ID: <device_id>
Content-Type: application/json
```

The `X-Device-ID` header allows the backend to verify the request comes from the claimed device.

---

## 10. Error Handling

### HTTP Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Bad request (invalid input) |
| 401 | Unauthorized (invalid/expired token) |
| 403 | Forbidden (not allowed for this user) |
| 404 | Not found (invalid code/device) |
| 409 | Conflict (device already linked) |
| 410 | Gone (code expired) |
| 429 | Rate limited |
| 500 | Server error |

### Error Response Format

```json
{
  "error": "error_code",
  "message": "Human-readable message",
  "details": { }
}
```

### Token Exchange (Custom Token → ID Token)

After receiving a custom token from `checkCode`, the device exchanges it for Firebase ID/refresh tokens:

**Endpoint:** `POST https://identitytoolkit.googleapis.com/v1/accounts:signInWithCustomToken?key={API_KEY}`

**Request:**
```json
{
  "token": "<custom_token_from_checkCode>",
  "returnSecureToken": true
}
```

**Response:**
```json
{
  "idToken": "<firebase_id_token>",
  "refreshToken": "<firebase_refresh_token>",
  "expiresIn": "3600"
}
```

The device stores both tokens and uses `idToken` for authenticated API calls.

### Token Refresh

Device uses Firebase Auth REST API to refresh expired tokens:

**Endpoint:** `POST https://securetoken.googleapis.com/v1/token?key={API_KEY}`

**Request:** (`application/x-www-form-urlencoded`)
```
grant_type=refresh_token&refresh_token=<stored_refresh_token>
```

**Response:**
```json
{
  "id_token": "<new_firebase_id_token>",
  "refresh_token": "<new_refresh_token>",
  "expires_in": "3600"
}
```

- Device handles HTTP 401 by attempting token refresh, then retrying the failed request
- If refresh fails (e.g., refresh token revoked), device must re-link
- Tokens are refreshed when expiring within 5 minutes (300 seconds safety margin)

---

## 11. Rate Limits

| Endpoint | Limit |
|----------|-------|
| `requestCode` | 5/hour per device |
| `checkCode` | 120/10min per code (polling) |
| `linkDevice` | 10/hour per user |
| `syncProgress` | 60/hour per device |
| `getProgress` | 30/hour per device |

---

## 12. Testing Checklist

### Device Linking
- [ ] Code generation returns valid 6-char code
- [ ] Code expires after 10 minutes
- [ ] Linking with valid code succeeds
- [ ] Linking with expired code fails (410)
- [ ] Linking with invalid code fails (404)
- [ ] Device receives custom token after link
- [ ] Custom token can be exchanged for ID token
- [ ] Multiple devices can link to same account
- [ ] Unlinking removes device from list
- [ ] Rate limits enforced

### Progress Sync
- [ ] Initial sync (no local data) returns cloud progress
- [ ] Device progress merges with cloud progress
- [ ] Completed lessons union correctly
- [ ] Character mastery takes max
- [ ] Practice time accumulates correctly
- [ ] Streak calculated from merged history
- [ ] XP awarded for new achievements
- [ ] Concurrent syncs from web and device resolve correctly

### Edge Cases
- [ ] Offline device queues sync, flushes on reconnect
- [ ] Expired token triggers re-auth flow
- [ ] Large progress payload (40 characters) fits response
- [ ] Clock skew between device and server handled

---

## Appendix A: Character Set

Characters tracked for mastery (40 total):

```
Letters: A B C D E F G H I J K L M N O P Q R S T U V W X Y Z (26)
Numbers: 0 1 2 3 4 5 6 7 8 9 (10)
Punctuation: . , ? / (4)
```

Character encoding for Firestore keys (some chars not allowed in keys):
- `.` → `_DOT_`
- `,` → `_COMMA_`
- `?` → `_QUESTION_`
- `/` → `_SLASH_`

---

## Appendix B: Module Definitions

Modules that need to be synced (matching web course):

| Module ID | Name | Characters | Prerequisites |
|-----------|------|------------|---------------|
| letters-1 | Letters 1 | E, T | None |
| letters-2 | Letters 2 | A, N, I | letters-1 |
| letters-3 | Letters 3 | R, S, O | letters-2 |
| letters-4 | Letters 4 | H, D, L | letters-3 |
| letters-5 | Letters 5 | C, U, M | letters-4 |
| letters-6 | Letters 6 | W, F, Y | letters-5 |
| letters-7 | Letters 7 | P, G, K | letters-6 |
| letters-8 | Letters 8 | V, B, X, J, Q, Z | letters-7 |
| numbers | Numbers | 0-9 | letters-6 |
| punctuation | Punctuation | . , ? / | numbers |
| words-common | Common Words | (words) | punctuation |
| callsigns | Callsigns | (patterns) | words-common |

---

## Contact

**Summit Device Team:** (your contact)
**CW School Web Team:** (web team contact)

Questions about this spec? Reach out before implementing.
