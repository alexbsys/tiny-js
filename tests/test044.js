// Cycle churn + long loops: sweep must drop unmarked closures / self-refs
// without gouging live objects or for-of iterators (C++-held, not in JS scopes).
var i;
for (i = 0; i < 2000; i++) {
	var trash = { n: i };
	trash.self = trash;
	trash.f = function () { return trash.n; };
	(function (x) {
		x.g = function () { return x; };
	})({ k: i });
}

var live = { n: 7 };
live.self = live;
live.f = function () { return live.n; };

var box = { n: 1 };
box.get = function () { return (() => this.n)(); };

var m = new Map();
m.set("a", 1);
m.set(live, 2);
m.delete("a");

var m2 = new Map();
for (i = 0; i < 600; i++) m2.set(i, i);
var mapSum = 0;
for (const pair of m2) mapSum += pair[1];

var s = 0;
for (const n of [1, 2, 3, 4, 5]) s += n;

result = live.f() === 7 && live.self.n === 7 && box.get() === 1 && s === 15
	&& m.get(live) === 2 && m.size === 1 && mapSum === 179700 && m2.size === 600;
