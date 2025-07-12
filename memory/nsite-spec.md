# NIP-XX
## Pubkey Static Websites

`draft` `optional`

This NIP describes how to serve static websites from a nostr pubkey.

Static Website Events
A static website event is a parameterized replaceable event of kind 34128 with a `d` tag as the absolute path from the root of the website (starting with a `/`).

> This event is not versioned, if a user wants to create multiple versions of their website they should use different pubkeys.

```jsonc
{
  "kind": 34128,
  "tags": [
    ["d", "/index.html"],
    ["x", "186ea5fd14e88fd1ac49351759e7ab906fa94892002b60bf7f5a428f28ca1c99"] // file hash
    // ...
  ]
}
```

## Resolving the website path

Host servers can use a variety of methods to resolve the pubkey into a user's website. For example:

- NIP-05 IDs
- `<npub>` subdomain names (recommended)
- DNS records
- Hardcoded names

Subdomains of the format `<npub>.<domain>` are recommended because they are the only format that doesn't require additional requests or a database. For example `npub10phxfsms72rhafrklqdyhempujs9h67nye0p67qe424dyvcx0dkqgvap0e.npub-sites.com`.

Once the server has the pubkey it should:

1. Fetch the user's NIP-65 relay list
2. Use the relays to search for replaceable events of kind 34128
3. Match the `d` tag to the requested path
4. If no event is found for the path `/index.html` then the server should be used as a fallback for root (`/`) and directories (`/posts/`)

## Fetching static website file

> Host servers should proxy requests to avoid leaking the user's IP address to file servers and other relays.

Once the server has found the event it should:

1. Fetch the file from the user's file server using the BUD-03 User Server List protocol
2. If there is no BUD-03 User Server list available the server should return 404

Once the server has the file it should check to make sure the sha256 hash from the `x` tag matches the sha256 hash of the content.

## Handling not found

If a website event is not found for a path the server should look for the `/404.html` path to fallback to.