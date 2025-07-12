# BUD-03
User Server List

`draft` `mandatory`

This BUD defines a `kind:10063` replaceable event specifying the list of servers that a user uploads content blobs to.

```json
{
  "kind": 10063,
  "content": "A nostr file storage server list",
  "tags": [
    ["server", "https://file-storage.example.com"],
    ["server", "https://cdn.jb55.com"]
  ]
}
```

The event MUST have at least one `server` tag where the value is the root endpoint of the server.

The servers should be ordered by reliability and users trust in the server. With the first server being the most trusted / reliable for retrieving that users content

## Client Upload

When uploading files clients SHOULD check if the user has a blossom server list with a `kind:10063` event and attempt to upload the blob to at least the first `server` listed.

Optionally, clients can upload the blob to multiple servers for redundancy.

Clients MAY choose to skip uploading any servers if they are known to be offline or unreliable. However if the client finds that none of the servers in a users list are online or accepting their upload then the client SHOULD display an actionable error to the user that their file could not be uploaded.

## Client Retrieval

When retrieving a blob, clients should query the users file server list with a `kind:10063` event. Use the returned list to attempt a download of the file

If the file is not located on any of the users servers or the user does not have a server list. Clients MAY attempt to look for the file on a selection of popular servers

## SHA256 from URL

Clients often need to extract the sha256 hash (address) of a blob from any random URL pointing to that blob. To do this clients SHOULD find the last 64 character hex string in the URL.

The following regular expression can be used to find this

```
/[0-9a-f]{64}/gi
```

Test cases
```
https://cdn.jb55.com/s/voiceover-1668197260.mp4 (no hash)
https://blossom.primal.net/b3b1d17731c825f066a52cfcfd619d6790b253ae82464a896c3ada9d204a91b5.png (hash=b3b1d17731c825f066a52cfcfd619d6790b253ae82464a896c3ada9d204a91b5)
https://www.europeanpressprize.com/wp-content/uploads/2020/08/nostradamus-1024x1024.jpg (no hash)
https://cdn.satellite.earth/b708326a0ed5a950052bbec3a1a0c4547e17b817b76fc5c17877f9958b48b1dc.mp4 (hash=b708326a0ed5a950052bbec3a1a0c4547e17b817b76fc5c17877f9958b48b1dc)
https://cdn.satellite.earth/b708326a0ed5a950052bbec3a1a0c4547e17b817b76fc5c17877f9958b48b1dc (hash=b708326a0ed5a950052bbec3a1a0c4547e17b817b76fc5c17877f9958b48b1dc)
https://i.nostr.build/j8UMniCDV4oYLm5Q.jpg (no hash)
```