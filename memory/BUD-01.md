# BUD-01
Server required endpoints

`draft` `mandatory`

## Cross-Origin Headers

All endpoints MUST set the `Access-Control-Allow-Origin` header to `*` (wildcard) to allow requests from any origin.

For endpoints that support additional methods to `GET` or `HEAD` the server MUST set additional headers in response to a preflight request with the `OPTIONS` method.

```
Access-Control-Allow-Methods: PUT,DELETE
Access-Control-Allow-Headers: Content-Type,X-Reason,X-Auth-<suffix>
```

All preflight requests MAY return a `Access-Control-Max-Age: 86400` (1 day) header to allow clients to cache the CORS response.

## Authorization events

This BUD defines kind `24242` as an authorization event that clients should create to be granted access to a servers endpoints.

The authorization event MUST contain a `t` tags with one or more verbs for the request. 
The verbs `get`, `upload`, `list` and `delete` correspond to `GET /[sha256]`, `PUT /upload`, `GET /list` and `DELETE /[sha256]` respectively.

These events MUST also have an `expiration` tag with a unix timestamp after which the event should be considered expired.

The `created_at` of the event should be close to the current time (within ~60 seconds), a server MAY reject stale events.

The authorization event MUST also contain a human readable `content` describing the purpose of the event. _for example: "Upload bitcoin.pdf", "Delete bca723e0"_

```jsonc
{
  "kind": 24242,
  "content": "Upload bitcoin.pdf",
  "tags": [
    ["t", "upload"],
    ["t", "delete"],
    ["expiration", "<later-timestamp>"]
  ]
}
```

Servers MUST perform the following checks in order to validate the event

- Event `kind` MUST be `24242`
- Event `created_at` MUST be within a reasonable time from the current time (within ~60 seconds)
- Event MUST have a `t` tag matching the request
- Event MUST have an `expiration` tag with a valid future timestamp

Additionally each endpoint MUST implement specific checks if the endpoint supports authorization events

Servers MUST NOT authenticate users with other kinds of events such as `kind:01` notes to prevent users from accidentally signing access to the server.

Additionally servers MUST NOT create a unique authorization event for any specific server as these authorization events are created for users to create standard (non-server-specific) access to any blossom server.

When requiring authorization the server MUST set a `WWW-Authenticate: Nostr` header on the request to tell the client the authorization event should be signed by a nostr key.

### Pubkey format

All pubkeys MUST be in hex format

## GET /{sha256}

`GET` endpoint to get a blob

Required: NO

If the blob is found then the server should return:

- `200` status code
- A response body containing the blob data

Servers MAY support content negotiation using a file extension. For example `GET /6080a6cfe50d72c94fb614c8738edd9de919f5dcd45cf2ab88c7c08a26c1b731.mp3`

The server MUST respond with the correct mime-type header when returning the blob. If the server does not know the mime-type then it should try to infer it from the blobs content. Otherwise it SHOULD return `application/octet-stream`

The server SHOULD set the following header to allow clients to use the `Range` header when requesting blobs

```
accept-ranges: bytes
```

The server MAY require authorization for this endpoint and SHOULD use the `X-Reason` header to tell the client why it requires auth.

Servers MAY support redirection. If the server doesn't have the blob but knows its on another server (like a CDN), instead of streaming the blob through the server, it can send a HTTP 302 redirect response pointing the client to the other server.

## HEAD /{sha256}

`HEAD` endpoint to check if a blob exists

Required: NO

This should respond the same as `GET /{sha256}` but with no response body

## Partial Requests

Both `GET` and `HEAD` endpoints SHOULD support partial requests. 

If the client sends a request with a `Range` header (example: `range: bytes=200-1023`) then the server SHOULD respond with:

- status code `206` Partial Content
- The `content-range` header indicating the range (example: `content-range: bytes 200-1023/1024`)
- A response body containing the requested range from the blob

## Authorization Validation

When requiring authorization for the `GET` and `HEAD` endpoints the server MUST perform additional checks:

- Event MUST have a matching `t` tag with value `get`
- Additionally the server MAY perform any kind of checks on the given event kind (access control, rate limiting, etc.)