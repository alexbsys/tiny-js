// Cycle churn inside a long loop: sweep must drop unmarked closures / self-refs
// without gouging live objects. 80 iterations > maybeClear threshold (32).
var i;
for (i = 0; i < 80; i++) {
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

var s = 0;
for (const n of [1, 2, 3, 4, 5]) s += n;

result = live.f() === 7 && live.self.n === 7 && box.get() === 1 && s === 15 && m.get(live) === 2 && m.size === 1;
