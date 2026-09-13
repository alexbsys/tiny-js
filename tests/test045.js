// In-place number reuse: unique accumulators may mutate; aliases must not.
var i;

var loopS = 200000;
for (i = 0; i < 2000; i++) loopS = loopS + i;
var loopOk = loopS === 2199000;

var plusEq = 200000;
for (i = 0; i < 200; i++) plusEq += i;
var plusEqOk = plusEq === 219900;

var inc = 200000;
for (i = 0; i < 50; i++) inc++;
var incOk = inc === 200050;

// Aliases: same unique number in two bindings — increment must not clobber the other.
var a = 200000;
var b = a;
a = a + 1;
var aliasAssign = b === 200000 && a === 200001;

var a2 = 200000;
var b2 = a2;
a2 += 1;
var aliasPlusEq = b2 === 200000 && a2 === 200001;

var a3 = 200000;
var b3 = a3;
a3++;
var aliasInc = b3 === 200000 && a3 === 200001;

var o = { n: 200000 };
var ox = o.n;
o.n = o.n + 1;
var aliasProp = ox === 200000 && o.n === 200001;

var arr = [200000];
var ax = arr[0];
arr[0] = arr[0] + 1;
var aliasArr = ax === 200000 && arr[0] === 200001;

function hold(x) {
	return function () { return x; };
}
var held = 200000;
var getter = hold(held);
held = held + 1;
var aliasClosure = getter() === 200000 && held === 200001;

// Right-hand read of an alias must stay stable while the target changes.
var t = 200000;
var s = 1;
s = t + 1;
var assignOther = t === 200000 && s === 200001;

// Nested + as a call argument must not mutate the variable before the call.
var argS = 200000;
function retS(x) { return argS; }
argS = retS(argS + 1);
var argOk = argS === 200000;

var addF = 200000;
function readS() { return addF; }
addF = addF + readS();
var addFOk = addF === 400000;

// Interned small ints stay shared; aliases must see the old value.
var ia = 1;
var ib = ia;
ia = ia + 1;
var internAlias = ib === 1 && ia === 2;

// BigInt alias
var ba = 9007199254740993n;
var bb = ba;
ba = ba + 1n;
var bigAlias = bb === 9007199254740993n && ba === 9007199254740994n;

// String + must not clobber a numeric alias.
var sa = 200000;
var sb = sa;
sa = sa + "x";
var strAlias = sb === 200000 && sa === "200000x";

// Ternary / logic bind tighter than a rewrite to +=.
var cond = 200000;
cond = cond + 1 ? 7 : 9;
var condOk = cond === 7;

var logic = 200000;
logic = logic + 1 && 3;
var logicOk = logic === 3;

result = loopOk && plusEqOk && incOk
	&& aliasAssign && aliasPlusEq && aliasInc && aliasProp && aliasArr && aliasClosure
	&& assignOther && argOk && addFOk && internAlias && bigAlias && strAlias
	&& condOk && logicOk;
