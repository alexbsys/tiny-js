# TinyJS

A compact JavaScript interpreter written in C++. This tree is a maintained fork of [42TinyJS](https://github.com/ArminJO/42tiny-js) (itself a fork of Gordon Williams’ original TinyJS). It is meant to be **embedded**: you compile it as a static library, register native functions, and run scripts inside a host process.

It is **not** Node.js, **not** a browser, and **not** a JIT. There is no event loop, no `window`, no `process`, and no `async`/`await`. What you get is a single-file-class interpreter (`CTinyJS`) with a surprisingly large language surface, a small standard library, and an explicit C++ embedding API.

For a focused write-up of the later language and runtime upgrades (ES5 work, `int64` / `n` literals, arrows, `Map`, typed arrays, garbage collection, threading rules), see **[doc/JS_ENGINE.md](doc/JS_ENGINE.md)**.

---

## What this engine is good at

| | |
|---|---|
| **Embeddable** | One `CTinyJS` instance, `execute()` / `evaluate()`, `addNative()` for C++ callbacks. |
| **Most of ES5** | Objects, arrays, functions, `JSON`, `Date`, `RegExp`, `call` / `apply` / `bind`, getters and setters. |
| **Useful extras** | `let` / `const`, generators (`yield`), arrow functions, `for…of`, optional chaining, nullish coalescing, exact 64-bit integers, `Map`, `ArrayBuffer` / `Uint8Array` / `DataView`. |
| **Predictable** | No hidden I/O. File `require()` only works if the host installs a reader. Threads are a C++ concern, not a JavaScript API. |

---

## Feature map

### Language

| Area | Supported | Notes |
|---|---|---|
| **Declarations** | `var`, `let`, `const`, `function` | `let` / `const` are block-scoped. Function declarations are hoisted. `'use strict'` is **ignored**. |
| **Control flow** | `if` / `else`, `switch`, `for`, `while`, `do…while`, `break` / `continue` (including labels), `return`, `throw` | `switch` allows fall-through. |
| **Exceptions** | `try` / `catch` / `finally` | Catch bindings may be a simple name or a destructuring pattern. |
| **`with`** | Yes | Same sloppy-mode footgun as ES5. |
| **Functions** | Declarations, expressions, `Function` constructor, `arguments`, `arguments.callee` | `call`, `apply`, `bind` are on `Function.prototype`. |
| **Arrows** | `(a, b) => a + b`, `x => x`, block bodies | Lexical `this`. Default parameter syntax is **not** parsed. |
| **Object literals** | `{ a: 1 }`, trailing commas, `get` / `set` | A bare `{…}` at statement start is a **block**, not an object. Use `({…})`. |
| **Array literals** | `[1, , 3]`, trailing commas | Holes exist; `join` and `JSON.stringify` treat them as ES5 does. |
| **Destructuring** | Object and array patterns | Works in `var` / `let` / `const`, assignment, `for…of`, and `catch`. |
| **Iteration** | `for`, `for (k in obj)`, `for (v of iterable)` | `for…in` yields keys (skips array holes). `for…of` yields values. Strings yield characters. |
| **Optional chaining** | `obj?.prop`, `obj?.[key]`, `fn?.()` | Short-circuits on `null` / `undefined`. |
| **Nullish coalescing** | `a ?? b` | Substitutes only for `null` / `undefined`. `0`, `false`, and `''` are kept. |
| **Generators** | `function` bodies that contain `yield` | `next()`, `send()`, `close()`, `throw()`. Needs threading (see below). |
| **`new`** | `new C(…)` | The instance `__proto__` is always taken from `C.prototype`. |
| **Operators** | Arithmetic, bitwise, shifts, `&&` `\|\|`, `==` `===`, `in`, `instanceof`, `typeof`, `void`, `delete`, `++` `--`, comma, ternary | `typeof` of an undeclared name is `'undefined'`. `typeof null` is `'object'`. |
| **Comments** | `//` and `/* */` | |
| **Regexp literals** | `/pattern/flags` | Disabled at compile time with `NO_REGEXP`. |

### Types

| Type | How it appears in scripts | Notes |
|---|---|---|
| **undefined / null / boolean** | `undefined`, `null`, `true`, `false` | |
| **Number (IEEE-754)** | `3.14`, `1e2`, `Number(…)` | Ordinary floating-point. |
| **Number (int32)** | Small integer literals | Interned in the range **−1 … 1024** (one shared object per value). |
| **int64 / BigInt-shaped** | `9007199254740993n`, `BigInt('…')`, integers outside 32-bit range | Exact signed 64-bit arithmetic until the value no longer fits, then promotion to double. **Not** the ES2020 `1n + 1` type-error model: `1n + 1` is allowed. |
| **String** | `'…'` and `"…"` | No template literals. |
| **Object / Array / Function** | literals, `new`, constructors | |
| **Date / RegExp / Error** | constructors | Full list below. |
| **ArrayBuffer / Uint8Array / DataView** | constructors | Byte buffers for host and script use. |
| **Map** | `new Map()` | Linear key lookup (`===`). Fine for small maps. |

Special numeric values: `NaN`, `Infinity`, `-0`.

### Standard library

| Object | What is registered |
|---|---|
| **Global** | `eval`, `parseInt`, `parseFloat`, `isNaN`, `isFinite`, `require` (needs a host reader), `trace`, `charToInt` |
| **Object** | `keys`, `create`, `defineProperty`, `defineProperties`, `getOwnPropertyDescriptor`, `getOwnPropertyNames`, `getPrototypeOf`, `preventExtensions` / `isExtensible`, `seal` / `isSealed`, `freeze` / `isFrozen`, `hasOwnProperty`, `propertyIsEnumerable`, `isPrototypeOf`, `valueOf`, `toString` |
| **Array** | `isArray`, `push`, `pop`, `shift`, `unshift`, `splice`, `slice`, `concat`, `reverse`, `sort`, `indexOf`, `lastIndexOf`, `every`, `some`, `forEach`, `map`, `filter`, `reduce`, `reduceRight`, `join`, plus legacy `contains` / `remove`. Writable `length` (shrinking deletes tail indexes). |
| **String** | `fromCharCode`, `charAt`, `charCodeAt`, `concat`, `indexOf`, `lastIndexOf`, `localeCompare`, `match`, `replace`, `search`, `slice`, `split`, `substr`, `substring`, `toLowerCase` / `toUpperCase` (and `toLocale*`), `trim` / `trimLeft` / `trimRight`, legacy `quote` |
| **Number** | `NaN`, `MAX_VALUE`, `MIN_VALUE`, `POSITIVE_INFINITY`, `NEGATIVE_INFINITY`, `valueOf`, `toString`, **`toFixed`** |
| **Boolean** | `valueOf`, `toString` |
| **Function** | `call`, `apply`, `bind`, `toString` |
| **Error** | `Error`, `EvalError`, `RangeError`, `ReferenceError`, `SyntaxError`, `TypeError` (`message`, `name`, `fileName`, `lineNumber`, `column`) |
| **Math** | `E`, `LN2`, `LN10`, `LOG2E`, `LOG10E`, `PI`, `SQRT1_2`, `SQRT2`, `abs`, `round`, `ceil`, `floor`, `min`, `max`, `sign`, `random` (alias `rand`), `sin` / `cos` / `tan` / inverses / hyperbolics, `log`, `log10`, `exp`, `pow`, `sqrt`, `sqr`, `toDegrees`, `toRadians`, legacy `range` |
| **Date** | `now`, `parse`, `UTC`, local and UTC getters/setters, `toISOString`, `toJSON`, `toString`, `toUTCString` / `toGMTString` |
| **RegExp** | `test`, `exec`; flags `g`, `i`, `m`, `y` |
| **JSON** | `parse` (strict; **reviver is ignored**), `stringify` (compact; **replacer and space are ignored**; honors `toJSON`; cyclic objects throw) |
| **Map** | `set`, `get`, `has`, `delete`, `clear`, `size`, `for…of` yields `[key, value]` |
| **Iterator** | `Iterator(obj, mode)`, `.next()`, global `StopIteration` |
| **BigInt** | `BigInt(value)` |
| **Typed data** | `ArrayBuffer`, `Uint8Array` (index, `set`, `buffer`, `length`), `DataView` (`get`/`set` Int/Uint 8/16/32, optional little-endian) |

42TinyJS extras that are still there on purpose: `Object.dump`, `Object.clone`, `Object.seel` (typo alias of `seal`), `Array.contains`, `Array.remove`, `Math.rand`, `Math.range`, `String.quote`, `charToInt`, `trace`.

---

## What is not here

These do **not** parse or are **not** registered. Do not expect them to appear later as silent no-ops.

| Missing | What happens |
|---|---|
| `async` / `await`, `Promise` | Syntax error / `typeof Promise === 'undefined'` |
| `class`, template literals `` `…` ``, rest/spread `...`, default parameters | Do not parse |
| ES modules (`import` / `export`) | Not present |
| `'use strict'` | The string is a no-op; sloppy mode stays on |
| `Set`, `WeakMap`, `Symbol`, `Proxy`, `Reflect` | Not registered |
| `Object.assign` / `entries` / `values` | Not registered |
| `String.includes` / `startsWith` / `repeat` | Not registered (use `indexOf` / a loop) |
| `Array.find` / `includes` / `from` | Not registered (use `filter` / `indexOf`) |
| `Number.isNaN` / `isInteger` | Not registered (use global `isNaN`) |
| Workers, `SharedArrayBuffer`, a JavaScript `Thread` object | Threading is C++ only |
| Browser / Node globals | No `window`, `document`, `console` (unless the host adds them), `setTimeout`, `Buffer` |

---

## A few examples

```javascript
// ES5-style objects and arrays
function Point(x, y) {
  this.x = x;
  this.y = y;
}
Point.prototype.length = function () {
  return Math.sqrt(this.x * this.x + this.y * this.y);
};
var p = new Point(3, 4);
p.length();                 // 5
[1, 2, 3].map(function (n) { return n * 2; }).join(',');  // "2,4,6"

// Arrows, optional chaining, nullish coalescing
var rows = [{ n: 1 }, null, { n: 3 }];
var sum = 0;
for (const row of rows) {
  sum += row?.n ?? 0;
}
// sum === 4

var add = (a, b) => a + b;
[10, 20].reduce((acc, n) => acc + n, 0);  // 30

// Exact integers beyond 2^53
var exact = 9007199254740993n + 2n;
String(exact);              // "9007199254740995"
String(BigInt('9007199254740993'));

// Date and JSON
var iso = new Date(Date.UTC(2026, 0, 2, 3, 4, 5)).toISOString();
JSON.parse('{"ok":true}').ok;
JSON.stringify({ a: 1, skip: undefined });  // '{"a":1}'

// Map + destructuring
var m = new Map();
m.set('a', 1).set('b', 2);
for (const [key, value] of m) {
  key + value;
}

// Generator: any function whose body contains yield
// (needs threading enabled at compile time; there is no function* keyword)
function countdown(n) {
  while (n > 0) {
    var reset = yield n;
    n = reset ? 10 : n - 1;
  }
}
```

`eval` of an object literal needs parentheses, same as browsers:

```javascript
eval('{ foo: 42 }');     // a block + label, not an object
eval('({ foo: 42 })');   // the object you wanted
```

---

## Embedding (C++)

```cpp
#include "TinyJS.h"

void print(const CFunctionsScopePtr &c, void *) {
  std::string text = c->getArgument("text")->toString();
  std::printf("%s\n", text.c_str());
  c->setReturnVar(c->constScriptVar(Undefined));
}

int main() {
  CTinyJS js;
  js.addNative("function print(text)", &print, 0);
  js.execute("print('hello ' + (1 + 2));", "example.js");
  std::string six = js.evaluate("2 * 3");
  return 0;
}
```

Useful entry points on `CTinyJS`:

| Method | Role |
|---|---|
| `execute(code, file)` | Run statements. Triggers a garbage-collection pass when it returns. |
| `evaluate(code)` | Run an expression and return its string form. |
| `evaluateComplex(code)` | Same, but keep a `CScriptVarLinkPtr` to the value. |
| `addNative("function name(a,b)", fn, userdata)` | Register a C++ function (also `Class.method` form). |
| `getRoot()` | Global object. |
| `setRequireReadFnc(fn)` | How `require("file")` reads source. |
| `setDebugEnabled` / `setDebugHook` | Optional debugger (breakpoints, step, stack). |

Inside a native: `c->getArgument(0)` or `c->getArgument("name")`, `c->getArgumentsLength()`, `c->setReturnVar(…)`, `c->throwError(TypeError, "…")`.

**One instance is not safe for concurrent JavaScript.** Create one `CTinyJS` per native thread, or take a lock around every `execute` / `evaluate` on a shared instance. See [doc/JS_ENGINE.md](doc/JS_ENGINE.md#multithreading).

---

## Build

```text
cmake -S . -B build
cmake --build build --config Release --target tiny-js --target tiny-js-tests --target tiny-js-perf
```

| Target | What it is |
|---|---|
| `tiny-js` | Static library |
| `tiny-js-tests` | Language test runner (`run_tests.cpp`) |
| `tiny-js-perf` | Microbenchmarks (`run_perf.cpp`, `--json` for machine output) |
| `tiny-js-script` | Tiny interactive shell (`Script.cpp`) |

C++17 is required. `find_package(Threads)` is used so generators and the pool allocator can lock on Linux and Android; on MSVC it is a no-op.

### Tests

The runner looks for `tests/testNNN.js` and `tests/42tests/testNNN.js` **relative to the current working directory**. Run it from this directory:

```text
# from the tiny-js source root
./build/tiny-js-tests          # or build/bin/Release/tiny-js-tests.exe
./tiny-js-tests tests/test041.js
./tiny-js-tests -k             # wait for Enter when finished
```

Each file sets a global `result` (boolean). `print(text)` is injected by the runner.

### Compile-time switches (`config.h`)

| Flag | Effect |
|---|---|
| `NO_POOL_ALLOCATOR` | Use ordinary `new` / `delete` |
| `NO_REGEXP` | Drop `RegExp` and string methods that need it |
| `NO_GENERATORS` | Drop `yield` / generator objects |
| `NO_THREADING` | No mutexes; **also disables generators** |
| `NO_CXX_THREADS` | Use Win32 or pthreads instead of `std::thread` |
| `HAVE_BOOST_REGEX` / `HAVE_TR1_REGEX` | Choose a regex backend other than `std::regex` |
| `PREVENT_REDECLARATION_IN_FUNCTION_SCOPES` | Stricter `let` redeclaration in function/root scopes |

---

## Runtime notes (short)

- **Garbage collection** is mark-sweep. It runs after top-level `evaluate` / `execute` and also after every 1024 heap allocations (so tight loops do not grow without bound).
- **Small integers −1…1024** are interned. Do not mutate them; the engine will not.
- **Number reuse:** a unique (unaliased) number object may be overwritten in place on `s = s + i`, `+=`, and `++`. If another binding still points at the same object, a new number is allocated. Details and corner cases: [doc/JS_ENGINE.md](doc/JS_ENGINE.md).
- **`Map` key lookup is O(n).** It is correct for `===` keys, including objects by identity. It is not a hash table.
- **Generators** are implemented with a real operating-system thread per generator object, synchronized with semaphores. That is why `NO_THREADING` turns them off.

---

## Debugger

When `setDebugEnabled(true)` is set, the engine can stop on statements, calls, returns, exceptions, and an explicit pause. The host installs `setDebugHook` and then uses `debugContinue`, `debugStepIn`, `debugStepOver`, `debugStepOut`, `addBreakpoint(file, line)`, `stack()`, and `variables()`. Line and column numbers are **1-based**.

---

## License

MIT. Original TinyJS by Gordon Williams; 42TinyJS changes by Armin Diedering; later engine work is in this tree. See the copyright headers in the source files.

---

## Further reading

- **[doc/JS_ENGINE.md](doc/JS_ENGINE.md)** — what was added on top of classic TinyJS / 42TinyJS, ES5 coverage, extras, explicit non-goals, and the multithreading model, with examples.
