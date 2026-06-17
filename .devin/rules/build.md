---
trigger: always_on
---

# Build Instructions

Always use the `bd` build script to build the project. Never invoke `cmake` directly.

```bash
# Configure
./bd configure

# Build
./bd build

# Configure and build in one step
./bd all

# Clean build directory
./bd clean
```

The binary is output to `./build/alpha-centauri`.
