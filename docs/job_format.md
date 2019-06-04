# VGD2 job format

All integers are little-endian. The fixed header is 16 bytes:

| Field | Size | Meaning |
|---|---:|---|
| magic | 4 | ASCII `VGD2` |
| version | 2 | must be `2` |
| flags | 2 | batch execution options |
| record count | 4 | maximum 4096 |
| nonce | 4 | deterministic batch salt |

Each record has an 8-byte prefix followed by its payload:

| Field | Size | Meaning |
|---|---:|---|
| opcode | 1 | command identifier |
| flags | 1 | command-local options |
| payload length | 2 | maximum 16384 |
| epoch | 4 | monotonic edit epoch |
| payload | variable | command-specific fields |

The first meaningful record is normally `BeginDocument`. Later records create resources and pages, install styles, mutate text and images, manipulate transactions, schedule rendering, or request derived work. Unknown opcodes are ignored after their payload has been safely consumed, which keeps forward-compatible jobs replayable.

The fuzzer corpus contains complete, valid jobs. Mutations therefore begin near deep state transitions instead of spending most executions guessing the fixed header.
