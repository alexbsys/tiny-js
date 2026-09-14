# JavaScript engine upgrades

This document describes **what this TinyJS tree added or changed** relative to classic TinyJS and 42TinyJS, and how to use those features safely. It is written for people who write scripts or embed the interpreter. It does not describe any particular host application.

The day-to-day language reference (full built-in tables, build flags, embedding API) lives in the [README](../README.md). This file is the “what is different, and why it matters” guide.

---

## 1. Why this document exists

The original TinyJS was a very small interpreter. 42TinyJS already added a large amount of ES5-oriented machinery: `let` / `const`, getters and setters, `try` / `catch` / `finally`, `for…in`, generators, a property descriptor API, and more.

This tree keeps that base and then:

1. Closes the remaining **ES5** holes that real scripts trip over (array methods, `JSON`, `Date`, `typeof`, `new` + prototype, literal postfix).
2. Adds a **deliberate superset** that is not ES5: exact 64-bit integers, arrow functions, optional chaining, nullish coalescing, `for…of`, `Map`, `ArrayBuffer` / `Uint8Array` / `DataView`, `Number.prototype.toFixed`.
3. Makes the **runtime** behave under load: mark-sweep garbage collection that does not destroy objects still held from C++, interned small integers, in-place reuse of unique number objects.
4. Documents **non-goals** so nobody has to discover them by accident. The most important non-goal is **`async` / `await`**. There is no Promise job queue and no event loop in the engine.

If a feature is not listed here as added, and is not in the README’s built-in table, assume it is absent.

---

## 2. Design rules that will not change

These are product decisions, not temporary gaps.

| Rule | Meaning |
|---|---|
| **No `'use strict'`** | The directive is ignored. Implicit globals, `with`, `arguments.callee`, and octal integer literals stay available. |
| **No `async` / `await` / `Promise`** | Scripts are synchronous. A native may block the calling thread; the engine will not schedule a continuation later. |
| **Keep exact integers** | The `n` suffix (`123n`) and `BigInt()` stay. They are how you write values that IEEE-754 cannot represent exactly. |
| **One interpreter instance, one thread at a time** | JavaScript itself cannot spawn threads. See [§8 Multithreading](#8-multithreading). |
| **Engine, not environment** | No `window`, no `document`, no `console`, no `setTimeout`, no filesystem, no network. The host may add those as natives. |

---

## 3. ES5 support

The goal is: **a script written against the ES5 language you actually use in the field should run**, with the documented exceptions below.

### 3.1 What now matches ES5

#### Arrays

Every ES5 `Array.prototype` method is registered, plus `Array.isArray`.

```javascript
var a = [1, 2, 3];
a.push(4);                         // 4
a.pop();                           // 4
[10, 20, 30].length;               // 3  (postfix on a literal works)
[1, 2, 3].join();                  // "1,2,3"  (default separator is comma)
[1, 2, 3].join('-');               // "1-2-3"
[1, 2, 3].map(function (n) { return n * 2; });   // [2, 4, 6]
[1, 2, 3, 4].filter(function (n) { return n % 2 === 0; });
[1, 2, 3, 4].reduce(function (s, n) { return s + n; }, 0);  // 10
Array.isArray(a);                  // true
Array.isArray({});                 // false
```

`length` is a writable accessor:

```javascript
var a = [1, 2, 3, 4];
a.length = 2;                      // a is [1, 2]; indexes 2 and 3 are gone
a.length = 5;                      // length is 5; new slots are holes
var holes = new Array(4);          // length 4, no own indexes
holes[5] = 1;                      // length becomes 6
```

`Array.prototype.toString` is `join(',')` with **no spaces**. Holes become empty segments: `[1,,3].toString()` is `"1,,3"`.

Legacy 42TinyJS methods `contains` and `remove` are still present so older scripts keep working.

#### Objects and `new`

`new Constructor()` always sets the instance `[[Prototype]]` from `Constructor.prototype`. Inherited methods are visible.

```javascript
function Box(n) { this.n = n; }
Box.prototype.get = function () { return this.n; };
var b = new Box(7);
b.get();                           // 7
b instanceof Box;                  // true
Object.getPrototypeOf(b) === Box.prototype;
```

ES5 object reflection that is actually registered:

```javascript
Object.keys({ a: 1, b: 2 });
Object.create(proto);
Object.defineProperty(obj, 'x', { value: 1, writable: true, enumerable: true, configurable: true });
Object.getOwnPropertyDescriptor(obj, 'x');
Object.getOwnPropertyNames(obj);
Object.getPrototypeOf(obj);
Object.preventExtensions(obj);
Object.seal(obj);
Object.freeze(obj);
Object.isExtensible(obj);
Object.isSealed(obj);
Object.isFrozen(obj);
obj.hasOwnProperty('x');
obj.propertyIsEnumerable('x');
Box.prototype.isPrototypeOf(b);
```

`Object.seel` remains as a misspelled alias of `Object.seal` (42TinyJS compatibility).

Getters and setters in object literals work:

```javascript
var o = {
  _n: 1,
  get n() { return this._n; },
  set n(v) { this._n = v; }
};
o.n = 5;
o.n;                               // 5
```

#### Functions

```javascript
function add(a, b) { return a + b; }
add.call(null, 1, 2);              // 3
add.apply(null, [1, 2]);           // 3
var plusOne = add.bind(null, 1);
plusOne(4);                        // 5

function f(a) {
  return arguments[0] + arguments.length + arguments.callee.length;
}
```

`arguments.callee` exists because there is no strict mode.

#### `typeof` and `null`

```javascript
typeof missingName;                // "undefined"  (does not throw)
typeof null;                       // "object"     (ES5 historical result)
typeof [];                         // "object"
typeof function () {};             // "function"
```

Internally the engine still knows that a null value is null (`===` against `null` works). Only `typeof` is special-cased.

#### JSON

`JSON.parse` is a **strict JSON** lexer, not `eval`.

```javascript
JSON.parse('{"a":1,"b":[true,null]}');
JSON.parse("{a:1}");               // throws  (identifiers are not JSON)
JSON.parse("{'a':1}");             // throws  (single quotes are not JSON)
JSON.parse('{"a":1,}');            // throws  (trailing comma)
```

The second argument (**reviver**) is accepted and **ignored**.

`JSON.stringify` produces compact JSON:

```javascript
JSON.stringify({ a: 1, b: undefined, c: function () {} });
// '{"a":1}'   — undefined and functions are omitted

JSON.stringify([1, , 3]);
// '[1,null,3]'  — holes become null

JSON.stringify({ toJSON: function () { return { ok: true }; } });
// '{"ok":true}'
```

The **replacer** and **space** arguments are accepted and **ignored**. There is no pretty-print indent. A cyclic object throws `TypeError`.

#### Date

A full `Date` built-in is registered (this was missing in earlier trees).

```javascript
Date.now();                        // milliseconds since Unix epoch (exact int64)
Date.parse('2026-01-02T03:04:05Z');
Date.UTC(2026, 0, 2, 3, 4, 5);

var d = new Date(Date.UTC(2026, 0, 2, 3, 4, 5));
d.getUTCFullYear();                // 2026
d.getUTCMonth();                   // 0
d.toISOString();                   // "2026-01-02T03:04:05.000Z"
d.toJSON();                        // same as toISOString
JSON.stringify({ t: d });          // uses toJSON
```

Also present: local and UTC getters and setters for year, month, date, day, hours, minutes, seconds, milliseconds; `getTime` / `setTime` / `valueOf`; `toString`, `toUTCString`, `toGMTString`.

Invalid dates stringify as `"Invalid Date"`. `toISOString` on an invalid date throws `RangeError`. Instant values are stored as signed 64-bit milliseconds, so they do not lose precision the way an IEEE double would around year 275760.

`Date` as a function (without `new`) returns an ISO string of “now”, which matches common ES5 implementations.

#### Strings and numbers (ES5 pieces)

```javascript
String.fromCharCode(65, 66);       // "AB"  (static, variable arguments)
"  x  ".trim();                    // "x"
(1.5).toFixed(2);                  // "1.50"
(1).toFixed();                     // "1"
```

`toFixed` accepts a digit count from 0 to 100 inclusive. Out of range throws.

#### Regular expressions

```javascript
/ab+c/i.test('ABBC');
'hello'.replace(/l/g, 'L');
```

`RegExp.prototype.test` and `exec` are registered. Flags: `g`, `i`, `m`, `y`. The whole feature can be compiled out with `NO_REGEXP`.

### 3.2 ES5 things that still diverge

These are intentional or inherited. They are not “forgotten”.

| Topic | What you should do |
|---|---|
| **`'use strict'`** | Do not rely on it. Implicit assignment still creates a global. `with` still works. |
| **`var` hoisting** | `var` exists, and simple function-body cases work. Some inner/`typeof` edge cases can differ from a browser. Prefer `let` / `const` if you care about block scope, or declare before first use. |
| **`eval('{a:1}')`** | This is a **block**, not an object, in ES5 as well. Write `eval('({a:1})')`. |
| **`JSON.parse` reviver / `JSON.stringify` replacer and space** | Arguments are ignored. Transform the value in script if you need a reviver or indent. |
| **`eval` and some parse errors** | `eval(badSyntax)` may return `undefined` instead of throwing. Prefer `try { JSON.parse(...) }` or `try { /* statements */ } catch (e) { }` around real code, not around `eval` as a syntax probe. |
| **Annex B / browser-only APIs** | Not a goal. No `escape` / `unescape` documentation, no host objects. |

---

## 4. Additional language features (beyond ES5)

These are supported on purpose. They are not “preview” flags.

### 4.1 Exact 64-bit integers (`n` suffix and `BigInt`)

IEEE-754 numbers cannot represent every integer above `2^53` (`9007199254740992`). This engine can:

```javascript
9007199254740993 !== 9007199254740992;          // true (unsuffixed integer stayed exact)
9007199254740993n + 2n;                         // 9007199254740995n
String(9007199254740993n);                      // "9007199254740993"
String(9223372036854775807);                    // "9223372036854775807"  (2^63-1)

BigInt(7);                                      // 7n-shaped value
BigInt('9007199254740993');
String(BigInt('9007199254740993') + 2);         // "9007199254740995"
```

How to read this:

- A literal with a trailing **`n`** is an integer token (`LEX_BIGINT`) and is stored with an internal “big integer” flag.
- `BigInt(x)` builds the same kind of value from a string or from an integer.
- `Number('9007199254740993')` is still IEEE-754 and will round. Use `BigInt` or the `n` suffix when you need the exact value.
- Mixed arithmetic is **allowed**. This is **not** ES2020 BigInt, where `1n + 1` is a `TypeError`. Here `1n + 1` produces an integer result.
- When a result no longer fits in signed 64-bit (`−2^63 … 2^63−1`), the engine **promotes to double**. It does not wrap silently like a machine `int64_t` add.

`parseInt` on a decimal string that fits in int64 stays exact. Values that overflow int64 become double.

### 4.2 Arrow functions

```javascript
(() => 1)();
(x => x + 1)(4);                   // 5
((a, b) => a + b)(2, 3);           // 5

var f = function (x) {
  var y = x + 1;
  return y * 2;
};
// same idea as a block arrow:
var g = x => { var y = x + 1; return y * 2; };
g(3);                              // 8

[1, 2, 3].map(x => x + 1).join(',');     // "2,3,4"
[1, 2, 3, 4].filter(x => x % 2 === 0);   // [2, 4]
```

Lexical `this`:

```javascript
var o = {
  n: 3,
  f: function () {
    return (() => this.n)();
  }
};
o.f();                             // 3

var detached = o.f();              // if f instead returns () => this.n
// a returned arrow still sees the `this` of the call that created it
```

A block arrow without `return` yields `undefined`. Default parameter syntax (`(x = 1) => x`) does **not** parse.

### 4.3 Optional chaining `?.`

```javascript
var o = { a: { b: 1 }, f: function (x) { return x + 1; } };
o.a?.b;                            // 1
o.z?.b;                            // undefined
o?.f(10);                          // 11

var n = null;
n?.foo.bar;                        // undefined  (does not read .bar)
n?.['a'];                          // undefined
n?.missing(expensive());           // undefined; expensive() is not called
```

`obj.missing()` without `?.` still throws if `missing` is `null` or `undefined`.

### 4.4 Nullish coalescing `??`

```javascript
null ?? 7;                         // 7
undefined ?? 8;                    // 8
0 ?? 9;                            // 0
false ?? 9;                        // false
'' ?? 9;                           // ""
null ?? undefined ?? 4;            // 4
```

`??` only replaces `null` and `undefined`. It is the right operator when `0` and `''` are valid data.

### 4.5 `for…of` and destructuring

```javascript
var sum = 0;
for (const n of [1, 2, 3]) sum += n;             // 6

for (const [a, b] of [[1, 2], [3, 4]]) {
  a + b;
}

for (const { value: frame } of [{ value: 5 }, { value: 7 }]) {
  frame;
}

var s = '';
for (const ch of 'ab') s += ch;                  // "ab"
```

`var`, `let`, and `const` are all accepted in the loop head. Object and array destructuring also work in ordinary declarations and in `catch`.

`for…of` uses the engine’s iterator protocol (`.next()` plus the global `StopIteration` object), not `Symbol.iterator`. Anything that implements that protocol can be iterated, including `Map` and arrays.

### 4.6 `Map`

```javascript
var m = new Map();
m.set('a', 1).set('b', 2);
m.get('a');                        // 1
m.has('a');                        // true
m.size;                            // 2
m.delete('a');
m.clear();

m.set(1, 'number');
m.set('1', 'string');              // two different keys (===)
var key = {};
m.set(key, 7);
m.get(key);                        // 7
m.get({});                         // undefined  (different object)

for (const [k, v] of m) {
  k;
  v;
}
```

Implementation note: keys are stored in a pair of arrays and compared with `===`. Lookup is **linear in the number of keys**. That is correct and simple. It is the wrong structure if you insert tens of thousands of keys in a tight loop.

There is no `Map.prototype.forEach`, `keys()`, `values()`, or `entries()`. Use `for…of`.

### 4.7 `ArrayBuffer`, `Uint8Array`, `DataView`

```javascript
var buf = new ArrayBuffer(8);
buf.byteLength;                    // 8

var u8 = new Uint8Array(4);
u8[0] = 200;
u8[0];                             // 200
u8.set([0x1FF], 0);                // clamped to 255
u8.buffer === /* the underlying ArrayBuffer */;

var bytes = new Uint8Array([1, 2, 3]);
bytes[2];                          // 3

var view = new DataView(new ArrayBuffer(4));
view.setUint32(0, 0x01020304, true);   // little-endian
view.getUint8(0);                  // 4
view.setInt32(0, -1, true);
view.getInt32(0, true);            // -1

var slice = new DataView(buf, 2, 2);   // offset 2, length 2
```

`Uint8Array` is the only typed array. There is no `Uint16Array`, `Float32Array`, or `Uint8ClampedArray` (clamping happens only in `Uint8Array.prototype.set`).

### 4.8 `let`, `const`, generators (already in 42TinyJS, still supported)

```javascript
{
  let x = 1;
  const y = 2;
  x = 3;
  // y = 4;   // const reassignment is rejected
}

function fibonacci() {
  var fn1 = 1;
  var fn2 = 1;
  while (true) {
    var current = fn2;
    fn2 = fn1;
    fn1 = fn1 + current;
    var reset = yield current;
    if (reset) {
      fn1 = 1;
      fn2 = 1;
    }
  }
}
var g = fibonacci();
g.next();                          // 1
g.next();                          // 1
g.next();                          // 2
g.send(true);                      // restart; returns 1
```

A function is treated as a generator if its body contains `yield`. Generator objects also have `close` and `throw`.

**Generators require threading** at compile time. Defining `NO_THREADING` in `config.h` also defines `NO_GENERATORS`. See [§8](#8-multithreading).

---

## 5. What is not supported (and will stay that way unless explicitly scheduled)

If you are coming from a browser or from Node.js, this list is the important one. None of these parse, or they parse as something else, or `typeof Name === 'undefined'`.

### 5.1 Asynchronous JavaScript

There is **no** `async function`, **no** `await`, **no** `Promise`, **no** microtask queue, **no** `queueMicrotask`, **no** `setTimeout` / `setImmediate`.

A script runs from the first statement to the last statement (or until it throws) on the native thread that called `execute` / `evaluate`. If a native function blocks — for example it reads a socket — the script is stuck until that native returns.

There is no supported way to write:

```javascript
// THIS DOES NOT EXIST
async function load() {
  const text = await fetch(url);
  return text;
}
```

If the host needs “later”, it must do that in C++ (or in whatever environment wraps the engine) and then call back into JavaScript when the result is ready.

### 5.2 Syntax that does not parse

| Construct | Status |
|---|---|
| `class Foo { … }` | Does not parse. Use constructor functions and `.prototype`. |
| Template literals `` `hello ${name}` `` | Does not parse. Use `+`. |
| Rest / spread `function f(...a)` / `[...arr]` / `{...obj}` | Does not parse. |
| Default parameters `function f(x = 1)` | Does not parse. |
| `import` / `export` / `import()` | Not present. |

### 5.3 Standard library that is not registered

| Name | Substitute |
|---|---|
| `Promise` | None in the engine. |
| `Set` | `Map` with dummy values, or an array + `indexOf`. |
| `Symbol`, `Proxy`, `Reflect` | None. |
| `Object.assign` / `Object.entries` / `Object.values` | Write a loop over `Object.keys`. |
| `String.prototype.includes` / `startsWith` / `repeat` | `indexOf`, a prefix check, a loop. |
| `Array.prototype.find` / `includes` / `Array.from` | `filter`[0], `indexOf`, a loop. |
| `Number.isNaN` / `Number.isInteger` | Global `isNaN`, or `x === (x \| 0)` for 32-bit integers. |
| `WeakMap` / `WeakSet` / `WeakRef` | None. |
| Other typed arrays (`Int16Array`, `Float64Array`, …) | `DataView` on an `ArrayBuffer`. |

### 5.4 Environment

The engine does not invent a console, timers, URL, filesystem, or cryptographic API. `require("file.js")` only works if the embedder installed `setRequireReadFnc`. `print` exists in the **test runner**, not in the library itself — register it yourself if you want it.

---

## 6. Runtime upgrades (memory and numbers)

These changes are invisible in correct scripts and visible in long-running loops and in C++ embeddings that hold objects across garbage collections.

### 6.1 Mark-sweep garbage collection

Objects that are no longer reachable from JavaScript are collected.

- A full collection runs when a top-level `execute` / `evaluate` returns.
- An additional collection is **polled on loop back-edges** after every **1024 allocations**. Collection is **not** “every N loop trips”. A loop that allocates will collect; a loop that only mutates interned integers may not.

Roots include:

- the scope chain (globals, function frames, `let` blocks);
- built-in prototypes and interned constants;
- generator objects that are still running;
- **C++ `CScriptVarPtr` locals** that hold an object even if no JavaScript variable points at it anymore.

The last point matters for `for…of`. The iterator is often alive only as a C++ pointer. An earlier sweep that ignored those pointers could destroy the iterator and produce errors such as `next is not a function`. The collector now counts incoming heap edges; if a C++ pointer holds extra references, the object is treated as live.

You do not call the collector from JavaScript. There is no `gc()` built-in.

### 6.2 Interned small integers

Every distinct integer in **−1 … 1024** shares one immutable number object (created on first use).

```javascript
var a = 1;
var b = 1;
// a and b refer to the same interned object
a = a + 1;
b;                                 // still 1
```

This is why `1` is cheap to create in a loop, and why the engine **must never overwrite** an interned object in place. Doing so would change the value of `1` for every variable in the process.

### 6.3 In-place reuse of unique numbers

Allocating a new number object on every `s = s + i` in a long loop is expensive. When the left-hand side is a **non-interned** number that **nobody else references**, the engine may write the new value into the existing object.

This is used for:

- `s = s + expr` (and `-`, `*`, `/`, `%`, and shifts) when the rewrite is safe (it is **not** applied when `?`, `&&`, `||`, or `??` bind to the whole addition);
- compound assignment `s += expr`;
- prefix and postfix `++` / `--`.

It is **not** used when another binding still points at the same object:

```javascript
var a = 200000;
var b = a;                         // alias
a = a + 1;
b;                                 // 200000
a;                                 // 200001

var held = 200000;
function remember(x) { return function () { return x; }; }
var get = remember(held);
held = held + 1;
get();                             // 200000

var s = 200000;
function readS() { return s; }
s = readS(s + 1);                  // if you pass s+1 into a call, s is not mutated first
```

If you only have one accumulator and you write `s = s + i` thousands of times, reuse kicks in once `s` leaves the interned range. The numeric result is the same either way.

---

## 7. Everyday pitfalls (with the upgraded engine)

### Object literals versus blocks

```javascript
eval('{ ok: true }');              // block + label
eval('({ ok: true })');            // object
```

The same rule applies at the start of a statement in a file. `{ foo: 1 }` at statement position is a block.

### Strict JSON is stricter than JavaScript

If you have been passing “JSON-ish” text (single quotes, trailing commas, unquoted keys) to `JSON.parse`, it will now throw. That is correct ES5. Use real JSON, or parse with a custom native.

### `??` and `?.` versus `||`

```javascript
var count = 0;
count || 10;                       // 10   (0 is falsy)
count ?? 10;                       // 0    (0 is not nullish)

var node = null;
node && node.child.name;           // null, but easy to get wrong
node?.child.name;                  // undefined, no throw
```

### Integer display versus `Number()`

```javascript
String(9007199254740993n);         // exact
Number('9007199254740993');        // rounded IEEE value
```

If a host gives you a decimal string that must stay exact (timestamps in milliseconds, 64-bit identifiers), prefer `parseInt`, `BigInt`, or the `n` suffix — not `Number(string)`.

### `Map` is not a hash table

A few hundred keys is fine. A few hundred thousand keys in a hot path will be slow. That is a known limitation, not a usage error on your side.

---

## 8. Multithreading

### 8.1 What JavaScript can and cannot do

JavaScript in this engine **cannot** start a thread, join a thread, or post a message to another worker. There is no `Worker`, no `SharedArrayBuffer`, no atomics API, and no `Thread` constructor.

All of the threading code in the tree is **C++ infrastructure**:

- mutexes around the object pool;
- a real operating-system thread **inside each generator object**, so `yield` can suspend that function without turning the whole interpreter into a bytecode coroutine.

From a script author’s point of view, a generator looks single-threaded: you call `next()`, you get a value, you call `next()` again. Under the hood the generator body is running on another native thread and is released only at `yield`. You do not manage that thread.

### 8.2 Rules for the embedder

These are the rules that keep the process defined:

1. **Do not call `execute`, `evaluate`, `evaluateComplex`, or any other entry that runs script on the same `CTinyJS` instance from two native threads at once.** The instance has a single token stream, a single scope stack, and a single garbage collector. There is no internal “big lock” around those.

2. **You may create many `CTinyJS` instances** and run them in parallel, one instance per native thread. That is the supported way to use several cores.

3. **If several native threads must talk to one instance**, serialize every call yourself (one mutex around all `execute` / `evaluate` / native-driven re-entry).

4. **Natives that you register must be safe for the thread that is currently running script on that instance.** If a native itself starts a thread and that thread calls back into the same `CTinyJS`, you have broken rule 1.

5. **C++ `CScriptVarPtr` values are reference-counted.** Passing them across threads without a lock is a data race. Treat a `CScriptVarPtr` as belonging to the instance that created it.

6. **Turning off threading.** In `config.h`:
   - `NO_THREADING` — no mutexes, and generators are compiled out (`NO_GENERATORS` is forced on).
   - `NO_GENERATORS` — keep mutexes (the pool allocator can still lock) but drop `yield`.
   - `NO_CXX_THREADS` — use Win32 or POSIX threads instead of `std::thread`.

   On platforms that need it (Linux, many Android NDKs), CMake links `Threads::Threads` (`-pthread`).

### 8.3 How this interacts with generators

```javascript
function tick() {
  var n = 0;
  while (true) {
    yield n;
    n = n + 1;
  }
}
var t = tick();
t.next();                          // 0
t.next();                          // 1
```

Each `tick()` **object** owns a native thread. Creating thousands of live generators means thousands of threads. That is almost never what you want on a microcontroller or in a process with a small thread budget. If you need that environment, compile with `NO_GENERATORS` and do not use `yield`.

`for…of` over an array does **not** start a thread. Only functions that contain `yield` do.

### 8.4 How this interacts with garbage collection

The collector is not concurrent. It runs on the thread that is executing script (or finishing `execute`). It will not run “in the background” on another core.

Because a generator’s JavaScript frames live on another thread until the next `yield`, the collector treats active generators as roots. Do not destroy a `CTinyJS` instance while a generator created by that instance is still running.

### 8.5 Picture

```text
Native thread A                 Native thread B
─────────────────               ─────────────────
CTinyJS instance 1              CTinyJS instance 2
  execute(...)                    execute(...)
  optional generator threads      optional generator threads
  (belong to instance 1 only)     (belong to instance 2 only)

        ✗  A must not call into instance 2’s objects
        ✗  B must not call execute() on instance 1
        ✓  A and B may run at the same time on *different* instances
```

---

## 9. Debugger (optional)

The engine can stop on every statement, on calls and returns, on exceptions, and on an explicit pause. This is a C++ API (`setDebugEnabled`, `setDebugHook`, `debugContinue`, `debugStepIn`, `debugStepOver`, `debugStepOut`, breakpoints, stack snapshot, locals snapshot). Line and column numbers are **1-based**.

There is no built-in `debugger;` statement documented as a public language feature. The host decides when the hook fires.

When debug is disabled (the default), the extra branches are cheap and script behavior is unchanged.

---

## 10. Feature checklist

Use this as a one-page contract.

| Feature | In this tree | Origin |
|---|---|---|
| ES5 objects, functions, `call` / `apply` / `bind` | Yes | 42TinyJS + fixes |
| ES5 array methods + `Array.isArray` + `length` setter | Yes | Upgrade |
| Literal postfix `[1,2,3].length` | Yes | Upgrade |
| `new` sets `[[Prototype]]` | Yes | Upgrade |
| `typeof` undeclared / `typeof null` | Yes (ES5) | Upgrade |
| Strict `JSON.parse` / compact `JSON.stringify` | Yes | Upgrade |
| `Date` (including `now`, ISO, UTC) | Yes | Upgrade |
| `Number.prototype.toFixed` | Yes | Upgrade |
| `let` / `const` / getters / setters / `for…in` | Yes | 42TinyJS |
| Generators / `yield` | Yes, if threading is on | 42TinyJS |
| Arrow functions | Yes | Upgrade |
| `?.` and `??` | Yes | Upgrade |
| `for…of` + destructuring | Yes | Upgrade |
| `Map` | Yes (linear) | Upgrade |
| `ArrayBuffer` / `Uint8Array` / `DataView` | Yes | Upgrade |
| Exact int64 / `n` / `BigInt()` | Yes | Upgrade (kept) |
| Interned small ints, unique-number reuse, mark-sweep GC | Yes | Upgrade |
| `'use strict'` | **No** | Non-goal |
| `async` / `await` / `Promise` | **No** | Non-goal |
| `class`, templates, rest/spread, default params, modules | **No** | Non-goal |
| JS-level threads / workers | **No** | Non-goal |

---

## 11. Related files

| Path | Contents |
|---|---|
| [README.md](../README.md) | Full language and library tables, build, embedding sketch |
| `config.h` | Compile-time switches (`NO_THREADING`, `NO_GENERATORS`, `NO_REGEXP`, …) |
| `tests/test039.js` … `tests/test045.js` | ES5 probe, arrows / `?.` / `??` / `toFixed`, typed arrays / `BigInt`, `for…of` / `Map`, GC, number-reuse aliases |
| `tests/42tests/` | 42TinyJS tests, including generators |
| `TinyJS_Date.cpp` | `Date` implementation |
| `TinyJS_Threading.cpp` | Mutexes, native threads, generator coroutines |
| `TinyJS_Debug.cpp` | Debugger |
