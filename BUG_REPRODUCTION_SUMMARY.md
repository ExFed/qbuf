# The Real Issue: Assert Side Effects with -DNDEBUG

## The Aha Moment

After deep investigation, we discovered the "GCC optimization bug" was actually a **test code anti-pattern**: 

**Putting function side effects inside `assert()` statements.**

## The Problem

### The Anti-Pattern

Test code throughout `cpp/tests/test_spsc.cpp` uses patterns like:

```cpp
assert(q.try_enqueue(10));      // Side effect inside assert!
assert(q.try_dequeue().has_value());  // Side effect inside assert!
```

### What `-DNDEBUG` Does

When you compile with `-DNDEBUG`, the C preprocessor replaces:

```cpp
#define assert(x) ((void)0)  // assert becomes a no-op
```

So this line:
```cpp
assert(q.try_enqueue(10));
```

Becomes:
```cpp
((void)0);  // The try_enqueue() call is GONE!
```

## The Reproduction

### Without `-DNDEBUG` (Debug Build)
- All `try_enqueue()` calls execute ✓
- Queues fill with data ✓
- Tests pass ✓

### With `-DNDEBUG` (Release Build)
1. Test 1: `try_enqueue()` calls don't execute → queue stays empty
2. Test 2: `try_enqueue()` calls don't execute → queue stays empty
3. Test 3: Tries to dequeue from empty queue → `std::bad_optional_access` ✗

## Why This Looks Like a Compiler Bug

The behavior **looks** like a compiler bug because:
- Same code, different behavior
- Only happens with specific flags
- Silent failure (no warnings)

But it's actually **correct behavior**:
- `-DNDEBUG` is designed to remove assertions
- The compiler is doing exactly what it should
- The bug is in the test code, not the compiler

## The Fix

**Never put side effects inside `assert()` statements:**

```cpp
// ❌ WRONG:
assert(q.try_enqueue(value));

// ✓ CORRECT:
bool success = q.try_enqueue(value);
assert(success);

// ✓ ALSO CORRECT:
if (!q.try_enqueue(value)) {
    throw std::runtime_error("enqueue failed");
}
```

## Key Takeaway

This is a **common C++ anti-pattern** that catches many developers off guard. The `assert()` macro is designed to disappear entirely with `-DNDEBUG`, so any code that depends on side effects inside assertions will break.

**Always test your code with `-DNDEBUG` defined to catch these issues early.**
