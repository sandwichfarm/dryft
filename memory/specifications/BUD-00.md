# BUD-00
Basic definitions

`draft` `mandatory`

## Language Interpretation

According to RFC 2119, the keywords "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" are to be interpreted as follows:

- MUST, REQUIRED or SHALL: mean that the definition is an absolute requirement of the specification.
- MUST NOT or SHALL NOT: mean that the definition is an absolute prohibition of the specification.
- SHOULD or RECOMMENDED: mean that there may exist valid reasons in particular circumstances to ignore a particular item, but the full implications must be understood and carefully weighed before choosing a different course.
- SHOULD NOT or NOT RECOMMENDED: mean that there may exist valid reasons in particular circumstances when the particular behavior is acceptable or even useful, but the full implications should be understood and the case carefully weighed before implementing any behavior described with this label.
- MAY or OPTIONAL: mean that an item is truly optional. One vendor may choose to include the item because a particular marketplace requires it or because the vendor feels that it enhances the product while another vendor may omit the same item. An implementation which does not include a particular option MUST be prepared to interoperate with another implementation which does include the option, though perhaps with reduced functionality. In the same vein an implementation which does include a particular option MUST be prepared to interoperate with another implementation which does not include the option (except, of course, for the feature the option provides.)

## Definition

BUDs or "Blossom Upgrade Documents" are short documents that outline an additional requirement or feature that a blossom server MUST or MAY implement.

## Definitions

Blobs are raw binary data addressed by the sha256 hash of the data.